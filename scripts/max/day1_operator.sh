#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

OWNER_REPO="${1:?usage: day1_operator.sh owner/repo}"
OWNER="${OWNER_REPO%/*}"
REPO="${OWNER_REPO#*/}"

echo "[1/8] preflight"
git rev-parse --is-inside-work-tree >/dev/null
gh auth status >/dev/null

echo "[2/8] local correctness quick gate"
bash "$PRE_PUSH_GUARD"

echo "[3/8] bootstrap labels/milestones/issues"
bash project/bootstrap_all.sh       "$OWNER_REPO" || true
bash project/create_kickoff_issues.sh "$OWNER_REPO" || true

echo "[4/8] ensure workflows visible"
gh workflow list --repo "$OWNER_REPO"

echo "[5/8] run M1 daily check workflow"
gh workflow run "M1 Daily Check" --repo "$OWNER_REPO" || true

echo "[6/8] open/update Day-1 issue comment"
DAY1_URL=$(gh issue list --repo "$OWNER_REPO" \
  --search "Day 1 — Verifier CFG/Terminators in:title" \
  --json number,url --jq '.[0].url')
if [[ -n "${DAY1_URL:-}" ]]; then
  gh issue comment "$DAY1_URL" --repo "$OWNER_REPO" \
    --body "Operator kickoff: local quick gate passed, bootstrap attempted, workflow dispatched."
fi

echo "[7/8] render command center locally"
mkdir -p "$STATUS_OUT_DIR"
python3 "$GEN_CMD_CENTER" \
  --bench-summary "$BENCH_SUMMARY" \
  $CMD_CENTER_FLAGS \
  --out "$CMD_CENTER_RENDERED" || true

echo "[8/8] done"
echo "Day-1 operator flow complete for $OWNER_REPO"
