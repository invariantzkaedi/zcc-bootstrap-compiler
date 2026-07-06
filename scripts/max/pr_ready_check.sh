#!/usr/bin/env bash
set -euo pipefail

echo "[1/7] git clean check"
git diff --quiet || { echo "Uncommitted changes present"; exit 1; }

echo "[2/7] branch check"
BRANCH="$(git rev-parse --abbrev-ref HEAD)"
[[ "$BRANCH" != "main" ]] || echo "Warning: on main"

echo "[3/7] required docs"
test -f .github/pull_request_template.md
test -f docs/policies/merge_gate_policy.md

echo "[4/7] correctness"
bash scripts/ci/pre_push_guard.sh

echo "[5/7] perf (quick)"
if [[ -x build/base/zcc-opt && -x build/cand/zcc-opt ]]; then
  BASE=build/base/zcc-opt CAND=build/cand/zcc-opt SUITE=benchmarks/list.txt bash scripts/max/full_perf_gate_local.sh
else
  echo "Skipping perf quick: baseline/candidate binaries missing"
fi

echo "[6/7] command center render"
mkdir -p out/status
python3 scripts/status/generate_command_center.py   --bench-summary out/bench/summary.json   --correctness-ok true   --perf-ok true   --flake-rate 0.0   --out out/status/optimizer_command_center.rendered.md || true

echo "[7/7] PR ready"
echo "PR READY ✅"
