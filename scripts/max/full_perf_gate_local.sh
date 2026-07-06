#!/usr/bin/env bash
set -euo pipefail

: "${BASE:=build/base/zcc-opt}"
: "${CAND:=build/cand/zcc-opt}"
: "${SUITE:=benchmarks/list.txt}"

echo "[1/5] build baseline/candidate"
make clean
make all OUT=build/base BASELINE=1
make all OUT=build/cand

echo "[2/5] run robust benchmarks"
scripts/bench/run_robust_benchmarks.sh   --base "$BASE"   --cand "$CAND"   --suite "$SUITE"   --warmup 3   --runs 25   --trim 0.10   --out out/bench

echo "[3/5] evaluate thresholds"
python3 scripts/bench/evaluate_robust_thresholds.py   --summary out/bench/summary.json   --max-compile-overhead-pct 8.0   --min-runtime-geomean-pct 3.0   --max-regressed-benches 2   --per-bench-regress-pct -2.0   --alpha 0.05

echo "[4/5] watchdog"
python3 scripts/bench/regression_watchdog.py   --summary out/bench/summary.json   --max-hard-regressions 2   --hard-threshold-pct -2.0

echo "[5/5] render summary md"
python3 scripts/bench/render_summary_md.py   --summary out/bench/summary.json   --out out/bench/summary.md

echo "local perf gate PASS"
