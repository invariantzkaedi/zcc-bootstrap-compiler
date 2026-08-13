#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

: "${BASE:=$ZCC_BASE}"
: "${CAND:=$ZCC_CAND}"
: "${SUITE:=$BENCH_SUITE}"

echo "[1/5] build baseline/candidate"
make clean
make all OUT=build/base BASELINE=1
make all OUT=build/cand

echo "[2/5] run robust benchmarks"
bash "$RUN_ROBUST_BENCH" \
  --base "$BASE" \
  --cand "$CAND" \
  --suite "$SUITE" \
  --warmup "$BENCH_WARMUP" \
  --runs "$BENCH_RUNS" \
  --trim "$BENCH_TRIM" \
  --out out/bench

echo "[3/5] evaluate thresholds"
python3 "$EVAL_THRESHOLDS" \
  --summary "$BENCH_SUMMARY" \
  --max-compile-overhead-pct "$THRESH_MAX_COMPILE_OVERHEAD_PCT" \
  --min-runtime-geomean-pct "$THRESH_MIN_RUNTIME_GEOMEAN_PCT" \
  --max-regressed-benches "$THRESH_MAX_REGRESSED_BENCHES" \
  --per-bench-regress-pct "$THRESH_PER_BENCH_REGRESS_PCT" \
  --alpha "$THRESH_ALPHA"

echo "[4/5] watchdog"
python3 "$WATCHDOG" \
  --summary "$BENCH_SUMMARY" \
  --max-hard-regressions "$THRESH_MAX_HARD_REGRESSIONS" \
  --hard-threshold-pct "$THRESH_HARD_PCT"

echo "[5/5] render summary md"
python3 "$RENDER_MD" \
  --summary "$BENCH_SUMMARY" \
  --out "$BENCH_MD"

echo "local perf gate PASS"
