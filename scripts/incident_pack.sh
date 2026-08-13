#!/usr/bin/env bash
set -Eeuo pipefail

# -------------------------------------------------------------------
# ZCC Incident Pack Orchestrator (consolidated, FI-aware)
# -------------------------------------------------------------------

INCIDENT_ID=""
OUTDIR="artifacts/incidents"
GATES="gate1 gate2 gate3 gate4 gate5"
FAULT_INJECT="0"
BASELINE_REF="HEAD~1"
TARGET_REF="HEAD"
REPRO_CMD=""
ABI_AUDIT="1"
REDACT="0"

STATUS="ok"
FAILURE_REASON=""
FAULT_INJECT_EXIT="not-run"
FAULT_RESTORE_EXIT="not-run"

timestamp_utc() { date -u +"%Y-%m-%dT%H:%M:%SZ"; }
date_stamp() { date -u +"%Y%m%d-%H%M%S"; }
log() { printf '[incident-pack] %s\n' "$*"; }
err() { printf '[incident-pack][error] %s\n' "$*" >&2; }

normalize_bool() {
  case "${1:-0}" in
    1|true|TRUE|yes|YES|on|ON) echo "true" ;;
    *) echo "false" ;;
  esac
}

safe_run() {
  local outfile="$1"; shift
  {
    printf '$ %q ' "$@"; printf '\n'
    "$@"
  } >"$outfile" 2>&1 || return $?
}

mark_failed() {
  STATUS="failed"
  FAILURE_REASON="$1"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --incident-id) INCIDENT_ID="${2:-}"; shift 2 ;;
    --outdir) OUTDIR="${2:-}"; shift 2 ;;
    --gates) GATES="${2:-}"; shift 2 ;;
    --fault-inject) FAULT_INJECT="${2:-0}"; shift 2 ;;
    --baseline-ref) BASELINE_REF="${2:-HEAD~1}"; shift 2 ;;
    --target-ref) TARGET_REF="${2:-HEAD}"; shift 2 ;;
    --repro-cmd) REPRO_CMD="${2:-}"; shift 2 ;;
    --abi-audit) ABI_AUDIT="${2:-1}"; shift 2 ;;
    --redact) REDACT="${2:-0}"; shift 2 ;;
    -h|--help)
      cat <<'USAGE'
Usage:
  bash scripts/incident_pack.sh [options]

Options:
  --incident-id   Incident ID (default: INC-<UTC_YYYYMMDD-HHMMSS>)
  --outdir        Output root (default: artifacts/incidents)
  --gates         Space-delimited gate list (default: "gate1 gate2 gate3 gate4 gate5")
  --fault-inject  0/1 (default: 0)
  --baseline-ref  Git baseline ref (default: HEAD~1)
  --target-ref    Git target ref (default: HEAD)
  --repro-cmd     Optional repro command
  --abi-audit     0/1 (default: 1)
  --redact        0/1 (default: 0)
USAGE
      exit 0
      ;;
    *)
      err "Unknown arg: $1"
      exit 2
      ;;
  esac
done

command -v git >/dev/null || { err "git required"; exit 2; }
command -v bash >/dev/null || { err "bash required"; exit 2; }

[[ -n "$INCIDENT_ID" ]] || INCIDENT_ID="INC-$(date_stamp)"

ARTIFACT_ROOT="${OUTDIR%/}/${INCIDENT_ID}"
ENV_DIR="$ARTIFACT_ROOT/env"
GIT_DIR="$ARTIFACT_ROOT/git"
GATES_DIR="$ARTIFACT_ROOT/gates"
REPRO_DIR="$ARTIFACT_ROOT/repro"
ASM_DIR="$ARTIFACT_ROOT/asm"
FI_DIR="$ARTIFACT_ROOT/fault_injection"
TPL_DIR="$ARTIFACT_ROOT/templates"

mkdir -p "$ENV_DIR" "$GIT_DIR" "$GATES_DIR" "$REPRO_DIR" "$ASM_DIR" "$TPL_DIR"
[[ "$(normalize_bool "$FAULT_INJECT")" == "true" ]] && mkdir -p "$FI_DIR"

START_TS="$(timestamp_utc)"
EXIT_CODES="$GATES_DIR/gate_exit_codes.txt"
: > "$EXIT_CODES"

finalize_manifest() {
  local end_ts head_sha branch_name cmp_exit fi_verdict
  end_ts="$(timestamp_utc)"
  head_sha="$(git rev-parse --verify HEAD 2>/dev/null || echo unknown)"
  branch_name="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
  cmp_exit="not-run"
  [[ -f "$ASM_DIR/cmp.exit" ]] && cmp_exit="$(cat "$ASM_DIR/cmp.exit")"
  fi_verdict="not-run"
  [[ -d "$FI_DIR" && -f "$FI_DIR/sensitivity_verdict.txt" ]] && fi_verdict="$(head -n1 "$FI_DIR/sensitivity_verdict.txt")"

  INC_ID="$INCIDENT_ID" START_TS="$START_TS" END_TS="$end_ts" STATUS="$STATUS" REASON="$FAILURE_REASON" \
  BRANCH="$branch_name" HEAD="$head_sha" BASE_REF="$BASELINE_REF" TARG_REF="$TARGET_REF" \
  GATES="$GATES" CMP_EXIT="$cmp_exit" FI_ENABLE="${FAULT_INJECT:-0}" FI_VERDICT="$fi_verdict" \
  FI_INJ_EXIT="$FAULT_INJECT_EXIT" FI_RST_EXIT="$FAULT_RESTORE_EXIT" REPRO="$REPRO_CMD" \
  ABI_AUDIT="${ABI_AUDIT:-1}" ROOT="$ARTIFACT_ROOT" \
  python3 - <<'PY' > "$ARTIFACT_ROOT/manifest.json" || true
import os, json
obj = {
  "incident_id": os.environ.get("INC_ID", ""),
  "created_at_utc": os.environ.get("START_TS", ""),
  "completed_at_utc": os.environ.get("END_TS", ""),
  "status": os.environ.get("STATUS", ""),
  "failure_reason": os.environ.get("REASON", ""),
  "branch": os.environ.get("BRANCH", ""),
  "head": os.environ.get("HEAD", ""),
  "baseline_ref": os.environ.get("BASE_REF", ""),
  "target_ref": os.environ.get("TARG_REF", ""),
  "gates_requested": os.environ.get("GATES", "").split(),
  "selfhost_cmp_exit": os.environ.get("CMP_EXIT", ""),
  "fault_injection_enabled": int(os.environ.get("FI_ENABLE", "0") in ["1", "true", "TRUE"]),
  "fault_injection_verdict": os.environ.get("FI_VERDICT", ""),
  "fault_inject_exit": os.environ.get("FI_INJ_EXIT", ""),
  "fault_restore_exit": os.environ.get("FI_RST_EXIT", ""),
  "repro_cmd": os.environ.get("REPRO", ""),
  "abi_audit_enabled": int(os.environ.get("ABI_AUDIT", "1") in ["1", "true", "TRUE"]),
  "artifact_root": os.environ.get("ROOT", "")
}
print(json.dumps(obj, indent=2))
PY
}
trap finalize_manifest EXIT

cat > "$ARTIFACT_ROOT/README.txt" <<EOF
ZCC Incident Pack
Incident: $INCIDENT_ID
Created: $START_TS
Artifact root: $ARTIFACT_ROOT
EOF

# -------------------------------------------------------------------
# Gate command map (edit to your exact repo commands)
# -------------------------------------------------------------------
declare -A GATE_CMDS
GATE_CMDS[gate0]="git log --oneline -5 && git status --porcelain=v1"
GATE_CMDS[gate1]="make -j\$(nproc) selfhost && cmp zcc2.s zcc3.s"
GATE_CMDS[gate2]="make -j\$(nproc) test"
GATE_CMDS[gate3]="make -j\$(nproc) test-abi || make -j\$(nproc) abi-test"
GATE_CMDS[gate4]="make -j\$(nproc) test-runtime"
GATE_CMDS[gate5]="make -j\$(nproc) test-regression"

# -------------------------------------------------------------------
# Environment snapshot
# -------------------------------------------------------------------
log "Collecting environment snapshot..."
safe_run "$ENV_DIR/uname.txt" uname -a || true
[[ -f /etc/os-release ]] && cp /etc/os-release "$ENV_DIR/os-release.txt"
safe_run "$ENV_DIR/gcc-version.txt" bash -lc "gcc --version" || true
safe_run "$ENV_DIR/python-version.txt" bash -lc "python3 --version" || true
safe_run "$ENV_DIR/git-version.txt" git --version || true
safe_run "$ENV_DIR/locale.txt" locale || true
safe_run "$ENV_DIR/ulimit.txt" bash -lc "ulimit -a" || true
safe_run "$ENV_DIR/date-utc.txt" date -u || true
safe_run "$ENV_DIR/wsl-status.txt" bash -lc "command -v wsl.exe >/dev/null && wsl.exe -l -v || echo 'wsl.exe not available'" || true

# -------------------------------------------------------------------
# Git snapshot
# -------------------------------------------------------------------
log "Collecting git snapshot..."
safe_run "$GIT_DIR/status.txt" git status --porcelain=v1 || mark_failed "git status failed"
safe_run "$GIT_DIR/rev-parse.txt" bash -lc "git rev-parse HEAD && git rev-parse --abbrev-ref HEAD" || mark_failed "rev-parse failed"
safe_run "$GIT_DIR/log-20.txt" git log --oneline -20 || true
safe_run "$GIT_DIR/diff-stat.txt" git diff --stat "$BASELINE_REF..$TARGET_REF" || true
safe_run "$GIT_DIR/diff.patch" git diff "$BASELINE_REF..$TARGET_REF" || true
safe_run "$GIT_DIR/changed-files.txt" git diff --name-only "$BASELINE_REF..$TARGET_REF" || true

# -------------------------------------------------------------------
# Repro capture
# -------------------------------------------------------------------
if [[ -n "$REPRO_CMD" ]]; then
  log "Running repro command..."
  echo "$REPRO_CMD" > "$REPRO_DIR/repro_command.txt"
  set +e
  bash -lc "$REPRO_CMD" > "$REPRO_DIR/repro_output.txt" 2>&1
  repro_rc=$?
  set -e
  echo "exit_code=$repro_rc" >> "$REPRO_DIR/repro_output.txt"
else
  echo "No repro command provided." > "$REPRO_DIR/repro_command.txt"
fi

# -------------------------------------------------------------------
# Gate execution via normalized runner
# -------------------------------------------------------------------
log "Executing gates: $GATES"
ANY_GATE_FAIL=0

run_gate() {
  local g="$1"
  local cmd="${GATE_CMDS[$g]:-}"
  if [[ -z "$cmd" ]]; then
    echo "$g=127" >> "$EXIT_CODES"
    printf "No command configured for %s\nexit_code=127\n" "$g" > "$GATES_DIR/$g.txt"
    ANY_GATE_FAIL=1
    return
  fi

  set +e
  bash scripts/gate_runner.sh \
    --name "$g" \
    --command "$cmd" \
    --out "$GATES_DIR" \
    --exit-map "$EXIT_CODES"
  rc=$?
  set -e

  if [[ $rc -ne 0 ]]; then
    ANY_GATE_FAIL=1
  fi
}

for g in $GATES; do
  run_gate "$g"
done

# -------------------------------------------------------------------
# Assembly artifacts + summary
# -------------------------------------------------------------------
log "Capturing assembly artifacts..."
[[ -f zcc2.s ]] && cp zcc2.s "$ASM_DIR/baseline.s" || true
[[ -f zcc3.s ]] && cp zcc3.s "$ASM_DIR/target.s" || true

if [[ -f "$ASM_DIR/baseline.s" && -f "$ASM_DIR/target.s" ]]; then
  if cmp "$ASM_DIR/baseline.s" "$ASM_DIR/target.s" > "$ASM_DIR/cmp.txt" 2>&1; then
    echo "0" > "$ASM_DIR/cmp.exit"
  else
    echo "$?" > "$ASM_DIR/cmp.exit"
  fi

  if [[ -x scripts/asm_delta_summary.py ]]; then
    python3 scripts/asm_delta_summary.py \
      "$ASM_DIR/baseline.s" "$ASM_DIR/target.s" "$ASM_DIR/asm_delta_summary.txt" || true
  else
    echo "asm_delta_summary.py missing/executable not set" > "$ASM_DIR/asm_delta_summary.txt"
  fi
else
  echo "Assembly files missing (zcc2.s/zcc3.s)." > "$ASM_DIR/cmp.txt"
  echo "not-run" > "$ASM_DIR/cmp.exit"
  echo "No asm summary." > "$ASM_DIR/asm_delta_summary.txt"
fi

# -------------------------------------------------------------------
# Fault injection block (FI-aware semantics)
# -------------------------------------------------------------------
if [[ "$(normalize_bool "$FAULT_INJECT")" == "true" ]]; then
  log "Fault injection enabled."

  if [[ -x scripts/fault_inject.sh ]]; then
    set +e
    FI_ALLOW_DIRTY=1 bash scripts/fault_inject.sh > "$FI_DIR/red_output.txt" 2>&1
    red_rc=$?
    set -e

    FAULT_INJECT_EXIT="$red_rc"
    echo "red_exit=$red_rc" >> "$FI_DIR/red_output.txt"

    if [[ "$red_rc" -eq 10 ]]; then
      echo "gate_sensitivity=not_proven" > "$FI_DIR/sensitivity_verdict.txt"
      mark_failed "fault injection reported gate insensitivity (exit 10)"
    elif [[ "$red_rc" -ne 0 ]]; then
      echo "gate_sensitivity=inject_failed" > "$FI_DIR/sensitivity_verdict.txt"
      mark_failed "fault injection failed (exit $red_rc)"
    else
      echo "gate_sensitivity=proven_red_observed" > "$FI_DIR/sensitivity_verdict.txt"
    fi
  else
    echo "fault_inject.sh missing/executable bit not set" > "$FI_DIR/red_output.txt"
    FAULT_INJECT_EXIT="missing-hook"
    echo "gate_sensitivity=inject_hook_missing" > "$FI_DIR/sensitivity_verdict.txt"
    mark_failed "fault injection hook missing"
  fi

  if [[ -x scripts/fault_restore.sh ]]; then
    set +e
    bash scripts/fault_restore.sh > "$FI_DIR/restore_output.txt" 2>&1
    restore_rc=$?
    set -e

    FAULT_RESTORE_EXIT="$restore_rc"
    echo "restore_exit=$restore_rc" >> "$FI_DIR/restore_output.txt"

    if [[ "$restore_rc" -eq 0 ]]; then
      echo "restore=ok" >> "$FI_DIR/sensitivity_verdict.txt"
    else
      echo "restore=failed" >> "$FI_DIR/sensitivity_verdict.txt"
      mark_failed "fault restore failed"
    fi
  else
    echo "fault_restore.sh missing/executable bit not set" > "$FI_DIR/restore_output.txt"
    FAULT_RESTORE_EXIT="missing-hook"
    echo "restore=missing" >> "$FI_DIR/sensitivity_verdict.txt"
    mark_failed "fault restore hook missing"
  fi
fi

# -------------------------------------------------------------------
# Template copy + prefill
# -------------------------------------------------------------------
log "Copying templates..."
copy_tpl() {
  local src="$1" dst="$2"
  [[ -f "$src" ]] && cp "$src" "$dst" || printf '# Missing template: %s\n' "$src" > "$dst"
}

copy_tpl "templates/INCIDENT_MICRO.md" "$TPL_DIR/INCIDENT_MICRO.md"
copy_tpl "templates/INCIDENT_REPORT.md" "$TPL_DIR/INCIDENT_REPORT.md"
copy_tpl "templates/POSTMORTEM_SCORECARD.md" "$TPL_DIR/POSTMORTEM_SCORECARD.md"
copy_tpl "templates/PR_DESCRIPTION_TEMPLATE.md" "$TPL_DIR/PR_DESCRIPTION_TEMPLATE.md"

BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
HEAD_SHA="$(git rev-parse --verify HEAD 2>/dev/null || echo unknown)"
CMP_EXIT="$(cat "$ASM_DIR/cmp.exit" 2>/dev/null || echo not-run)"
GATE_MATRIX="$(cat "$EXIT_CODES" 2>/dev/null | tr '\n' ';' | sed 's/;$//')"
DIFFSTAT_SNIPPET="$(head -n 20 "$GIT_DIR/diff-stat.txt" 2>/dev/null | tr '\n' '|' | sed 's/|$//')"
FI_VERDICT="not-run"
[[ -d "$FI_DIR" && -f "$FI_DIR/sensitivity_verdict.txt" ]] && FI_VERDICT="$(head -n1 "$FI_DIR/sensitivity_verdict.txt")"

if [[ -x scripts/template_prefill.py ]]; then
  python3 scripts/template_prefill.py \
    --templates-dir "$TPL_DIR" \
    --incident-id "$INCIDENT_ID" \
    --utc-now "$START_TS" \
    --branch "$BRANCH" \
    --head-sha "$HEAD_SHA" \
    --selfhost-cmp-exit "$CMP_EXIT" \
    --gate-matrix "$GATE_MATRIX" \
    --diffstat-snippet "$DIFFSTAT_SNIPPET" \
    --fault-inject-exit "$FAULT_INJECT_EXIT" \
    --fault-restore-exit "$FAULT_RESTORE_EXIT" \
    --fault-injection-enabled "$(normalize_bool "$FAULT_INJECT")" \
    --fault-injection-verdict "$FI_VERDICT" || true
fi

# -------------------------------------------------------------------
# Final status fold-in
# -------------------------------------------------------------------
if [[ "$ANY_GATE_FAIL" -ne 0 ]]; then
  mark_failed "one or more gates failed"
fi

if [[ "$STATUS" != "ok" ]]; then
  log "Completed with failure status. Artifact: $ARTIFACT_ROOT"
  exit 1
fi

log "Success. Artifact: $ARTIFACT_ROOT"
exit 0
