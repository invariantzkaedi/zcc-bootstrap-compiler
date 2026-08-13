#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

echo "[1/7] git clean check"
git diff --quiet || { echo "Uncommitted changes present"; exit 1; }

echo "[2/7] branch check"
BRANCH="$(git rev-parse --abbrev-ref HEAD)"
[[ "$BRANCH" != "main" ]] || echo "Warning: on main"

echo "[3/7] required docs"
test -f "$PR_TEMPLATE"
test -f "$POLICY_MERGE_GATE"

echo "[4/7] correctness"
bash "$PRE_PUSH_GUARD"

echo "[5/7] perf (quick)"
if [[ -x "$ZCC_BASE_OPT" && -x "$ZCC_CAND_OPT" ]]; then
  BASE="$ZCC_BASE_OPT" CAND="$ZCC_CAND_OPT" SUITE="$BENCH_SUITE" \
    bash scripts/max/full_perf_gate_local.sh
else
  echo "Skipping perf quick: baseline/candidate binaries missing"
fi

echo "[6/7] command center render"
mkdir -p "$STATUS_OUT_DIR"
python3 "$GEN_CMD_CENTER" \
  --bench-summary "$BENCH_SUMMARY" \
  $CMD_CENTER_FLAGS \
  --out "$CMD_CENTER_RENDERED" || true

echo "[7/7] PR ready"
echo "PR READY ✅"
