#!/usr/bin/env bash
# F1 authoritative-safe-base production closure gate.
# Runs the uploaded/reference test suite against the production
# zkaedi_security_utils module and appends evidence to a gate ledger.

set -uo pipefail

PYTHON_BIN="${PYTHON_BIN:-python3}"
TEST_SOURCE="${TEST_SOURCE:-test_authoritative_base.py}"
REGRESSION_SOURCE="${REGRESSION_SOURCE:-test_v2_containment.py}"
TRAIN_SCRIPT="${TRAIN_SCRIPT:-train_hf_dpo_adamw_hardened_v3.py}"
LEDGER="${LEDGER:-gate_ledger.tsv}"
LOG_DIR="${LOG_DIR:-/tmp/zkaedi_gates}"
PRODUCTION_MODULE="${PRODUCTION_MODULE:-zkaedi_security_utils}"

mkdir -p "$LOG_DIR"
timestamp="$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
run_id="${RUN_ID:-f1-authoritative-$(date -u +'%Y%m%dT%H%M%SZ')}"
overall=0

record_gate() {
    local gate="$1"
    local claim="$2"
    local exit_code="$3"
    local log_path="$4"
    local verdict="PASS"
    if [[ "$exit_code" -ne 0 ]]; then
        verdict="FAIL"
        overall=1
    fi
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$timestamp" "$run_id" "$gate" "$claim" "$exit_code" "$log_path" "$verdict" \
        >> "$LEDGER"
    printf '%-4s %-5s exit=%s log=%s\n' "$gate" "$verdict" "$exit_code" "$log_path"
}

# G6P: adapt the reference suite so it imports the production validator.
g6_log="$LOG_DIR/g6_production_authoritative.log"
tmp_test="$(mktemp "$LOG_DIR/test_authoritative_production.XXXXXX.py")"
trap 'rm -f "$tmp_test"' EXIT

if [[ ! -f "$TEST_SOURCE" ]]; then
    printf 'Missing test source: %s\n' "$TEST_SOURCE" > "$g6_log"
    record_gate "G6P" "production validator authoritative controls" 2 "$g6_log"
else
    "$PYTHON_BIN" - "$TEST_SOURCE" "$tmp_test" "$PRODUCTION_MODULE" > "$LOG_DIR/g6_rewrite.log" 2>&1 <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")
target = Path(sys.argv[2])
module = sys.argv[3]

old_module_import = "import zkaedi_safe_path_patch as sp"
old_symbol_import = "from zkaedi_safe_path_patch import validate_safe_path"

if source.count(old_module_import) != 1:
    raise SystemExit("expected exactly one reference module import")

if source.count(old_symbol_import) != 1:
    raise SystemExit("expected exactly one reference symbol import")

source = source.replace(old_module_import, f"import {module} as sp")
source = source.replace(old_symbol_import, f"from {module} import validate_safe_path")

required_markers = (
    "T1a narrow-authoritative",
    "T1b narrow-authoritative",
    "T2 symlink escape",
    "T5 both additive and authoritative",
    "T13 legacy extra_safe_bases",
)

missing = [m for m in required_markers if m not in source]
if missing:
    raise SystemExit(f"test source lacks required behavioral controls: {missing}")

if "zkaedi_safe_path_patch" in source:
    raise SystemExit("reference-module import remained after rewrite")

target.write_text(source, encoding="utf-8")
PY
    rewrite_rc=$?

    if [[ "$rewrite_rc" -ne 0 ]]; then
        cat "$LOG_DIR/g6_rewrite.log" > "$g6_log"
        record_gate "G6P" "production validator authoritative controls" "$rewrite_rc" "$g6_log"
    else
        (
            set +e
            # Rewritten test runs from $LOG_DIR; put the invocation CWD on
            # sys.path so a repo-local production module resolves (venv
            # installs are unaffected).
            PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" "$PYTHON_BIN" "$tmp_test"
            rc=$?
            printf '\nEXIT_CODE=%s\n' "$rc"
            exit "$rc"
        ) > "$g6_log" 2>&1
        g6_rc=$?
        record_gate "G6P" "production validator authoritative controls" "$g6_rc" "$g6_log"
    fi
fi

# G7P: run the production containment regression suite
g7_log="$LOG_DIR/g7_production_regression.log"
if [[ ! -f "$REGRESSION_SOURCE" ]]; then
    printf 'Missing regression source: %s\n' "$REGRESSION_SOURCE" > "$g7_log"
    record_gate "G7P" "production containment regression suite" 2 "$g7_log"
else
    (
        set +e
        PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" "$PYTHON_BIN" -m unittest "$REGRESSION_SOURCE"
        rc=$?
        printf '\nEXIT_CODE=%s\n' "$rc"
        exit "$rc"
    ) > "$g7_log" 2>&1
    g7_rc=$?
    record_gate "G7P" "production containment regression suite" "$g7_rc" "$g7_log"
fi

# G8P: every validate_safe_path call in v3 must use authoritative_safe_bases;
# legacy extra_safe_bases is forbidden in this script.
g8_log="$LOG_DIR/g8_v3_authoritative_ast.log"
if [[ ! -f "$TRAIN_SCRIPT" ]]; then
    printf 'Missing training script: %s\n' "$TRAIN_SCRIPT" > "$g8_log"
    record_gate "G8P" "v3 validator call-site enforcement" 2 "$g8_log"
else
    "$PYTHON_BIN" - "$TRAIN_SCRIPT" > "$g8_log" 2>&1 <<'PY'
import ast
import sys
from pathlib import Path

path = Path(sys.argv[1])
tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))

calls = []
failures = []
for node in ast.walk(tree):
    if not isinstance(node, ast.Call):
        continue
    fn = node.func
    name = fn.id if isinstance(fn, ast.Name) else (
        fn.attr if isinstance(fn, ast.Attribute) else None
    )
    if name != "validate_safe_path":
        continue

    keywords = {kw.arg for kw in node.keywords if kw.arg is not None}
    calls.append((node.lineno, sorted(keywords)))

    if "authoritative_safe_bases" not in keywords:
        failures.append(
            f"line {node.lineno}: missing authoritative_safe_bases"
        )
    if "extra_safe_bases" in keywords:
        failures.append(
            f"line {node.lineno}: legacy extra_safe_bases still present"
        )

print(f"validate_safe_path calls: {len(calls)}")
for line, keywords in calls:
    print(f"line {line}: {keywords}")

if len(calls) != 8:
    failures.append(f"expected exactly 8 call sites, found {len(calls)}")

if failures:
    print("FAILURES:")
    for failure in failures:
        print(f"- {failure}")
    raise SystemExit(1)

print("PASS: all 8 call sites are authoritative and zero are legacy-additive")
PY
    g8_rc=$?
    record_gate "G8P" "v3 validator call-site enforcement" "$g8_rc" "$g8_log"
fi

# G9P: ensure the installed production function advertises the new parameter.
g9_log="$LOG_DIR/g9_production_signature.log"
"$PYTHON_BIN" - "$PRODUCTION_MODULE" > "$g9_log" 2>&1 <<'PY'
import importlib
import inspect
import sys

module = importlib.import_module(sys.argv[1])
fn = module.validate_safe_path
signature = inspect.signature(fn)
print(signature)

parameters = signature.parameters
if "authoritative_safe_bases" not in parameters:
    raise SystemExit("validate_safe_path lacks authoritative_safe_bases")

print("PASS: production validator exposes authoritative_safe_bases")
PY
g9_rc=$?
record_gate "G9P" "production validator API supports authoritative mode" "$g9_rc" "$g9_log"

printf '\nLedger: %s\n' "$LEDGER"
printf 'Run ID: %s\n' "$run_id"

if [[ "$overall" -ne 0 ]]; then
    printf 'F1 STATUS: NOT CLOSED\n'
    exit 1
fi

printf 'F1 STATUS: STRUCTURALLY CLOSED AGAINST PRODUCTION MODULE\n'
exit 0
