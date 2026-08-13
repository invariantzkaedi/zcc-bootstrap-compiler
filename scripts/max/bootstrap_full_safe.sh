#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

OWNER_REPO="${1:?usage: bootstrap_full_safe.sh owner/repo}"
PROJECT_NUMBER="${2:-1}"

echo "[1/6] bootstrap labels/milestones/issues"
bash project/bootstrap_all.sh       "$OWNER_REPO" || true
bash project/create_kickoff_issues.sh "$OWNER_REPO" || true

echo "[2/6] validate ProjectV2"
OWNER="${OWNER_REPO%/*}"
python3 project/validate_projectv2_setup.py \
  --owner "$OWNER" \
  --project-number "$PROJECT_NUMBER" || true

echo "[3/6] export field IDs"
python3 project/export_projectv2_field_ids.py \
  --owner "$OWNER" \
  --project-number "$PROJECT_NUMBER" \
  --out .github/projectv2_field_ids.json || true

echo "[4/6] run local quality"
bash scripts/max/full_quality_gate_local.sh

echo "[5/6] run local perf"
bash scripts/max/full_perf_gate_local.sh

echo "[6/6] generate status report"
mkdir -p "$STATUS_OUT_DIR"
python3 "$GEN_CMD_CENTER" \
  --bench-summary "$BENCH_SUMMARY" \
  $CMD_CENTER_FLAGS \
  --out "$CMD_CENTER_RENDERED" || true

echo "BOOTSTRAP FULL SAFE COMPLETE ✅"
