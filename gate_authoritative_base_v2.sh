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

def check_file(content):
    tree = ast.parse(content, filename="<string>")
    failures = []
    
    # 1. Enforce direct call safety & check for extra_safe_bases keywords
    for node in ast.walk(tree):
        if isinstance(node, ast.Call):
            fn = node.func
            name = fn.id if isinstance(fn, ast.Name) else (
                fn.attr if isinstance(fn, ast.Attribute) else None
            )
            if name == "validate_safe_path":
                keywords = {kw.arg for kw in node.keywords if kw.arg is not None}
                if "authoritative_safe_bases" not in keywords:
                    failures.append(f"line {node.lineno}: direct validate_safe_path call missing authoritative_safe_bases")
                if "extra_safe_bases" in keywords:
                    failures.append(f"line {node.lineno}: legacy extra_safe_bases present")
            
            for kw in node.keywords:
                if kw.arg == "extra_safe_bases":
                    failures.append(f"line {node.lineno}: banned keyword argument 'extra_safe_bases' used")

    # 2. Track assignments for sensitive variables
    sensitive_targets = {
        "dataset_path", "output_dir", "manifest_path", 
        "validated_receipt", "validated_public_key", "validated_sig_path",
        "validated_checkpoint_dir"
    }
    
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign):
            continue
        for target in node.targets:
            name = None
            if isinstance(target, ast.Name):
                name = target.id
            if name in sensitive_targets:
                val = node.value
                is_valid_call = False
                if isinstance(val, ast.Call):
                    val_fn = val.func
                    val_name = val_fn.id if isinstance(val_fn, ast.Name) else (
                        val_fn.attr if isinstance(val_fn, ast.Attribute) else None
                    )
                    if val_name in ("validate_runtime_path", "validate_safe_path", "local_validate"):
                        is_valid_call = True
                if not is_valid_call:
                    failures.append(f"line {node.lineno}: sensitive variable '{name}' assigned value from non-validator call")
                    
    # 3. Verify local_validate definition safety (def local_validate must only call validate_safe_path)
    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef) and node.name == "local_validate":
            calls = []
            for child in ast.walk(node):
                if isinstance(child, ast.Call):
                    fn = child.func
                    name = fn.id if isinstance(fn, ast.Name) else (
                        fn.attr if isinstance(fn, ast.Attribute) else None
                    )
                    if name == "validate_safe_path":
                        calls.append(child)
            if len(calls) != 1:
                failures.append(f"line {node.lineno}: local_validate definition must call validate_safe_path exactly once")
            else:
                call_node = calls[0]
                keywords = {kw.arg for kw in call_node.keywords if kw.arg is not None}
                if "authoritative_safe_bases" not in keywords:
                    failures.append(f"line {node.lineno}: local_validate's inner validate_safe_path call missing authoritative_safe_bases")
            returns = [c for c in ast.walk(node) if isinstance(c, ast.Return)]
            for r in returns:
                if r.value is not None:
                    is_valid_return = False
                    if isinstance(r.value, ast.Call):
                        fn = r.value.func
                        name = fn.id if isinstance(fn, ast.Name) else (
                            fn.attr if isinstance(fn, ast.Attribute) else None
                        )
                        if name == "validate_safe_path":
                            is_valid_return = True
                    if not is_valid_return:
                        failures.append(f"line {r.lineno}: local_validate has unsafe return path")
                        
    return failures

path = Path(sys.argv[1])
content = path.read_text(encoding="utf-8")

# 1. Run check on original script
failures = check_file(content)
print(f"Original script AST failures: {len(failures)}")

if failures:
    print("FAILURES ON ORIGINAL:")
    for failure in failures:
        print(f"- {failure}")
    sys.exit(1)

# 2. Self-mutation test: mutate authoritative_safe_bases with extra_safe_bases
mutated = content.replace(
    "authoritative_safe_bases=[SAFE_BASE_DIR]",
    "extra_safe_bases=[SAFE_BASE_DIR]"
)
if mutated == content:
    print("FAIL: self-mutation replace target not found")
    sys.exit(1)

mutated_failures = check_file(mutated)
if not mutated_failures:
    print("FAIL: mutated script passed AST gate but should have failed")
    sys.exit(1)

print(f"PASS: G8P-positive current script passes, G8P-negative mutated script correctly fails with: {mutated_failures[0]}")
sys.exit(0)
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
