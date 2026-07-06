#!/usr/bin/env bash
set -euo pipefail

echo "== LEGENDARY MODE =="

echo "[1/6] correctness"
make -C tests/verify test-negative
make -C tests/verify-positive test-positive
make -C tests/opt/instcombine test-normalized
make -C tests/opt/sccp test-normalized

echo "[2/6] perf"
scripts/bench/run_robust_benchmarks.sh \
  --base build/base/zcc-opt \
  --cand build/cand/zcc-opt \
  --suite benchmarks/list.txt   --warmup 3 --runs 25 --trim 0.10 --out out/bench

echo "[3/6] thresholds"
python3 scripts/bench/evaluate_robust_thresholds.py \
  --summary out/bench/summary.json \
  --max-compile-overhead-pct 8.0 \
  --min-runtime-geomean-pct 3.0 \
  --max-regressed-benches 2 \
  --per-bench-regress-pct -2.0 \
  --alpha 0.05

echo "[4/6] watchdog"
python3 scripts/bench/regression_watchdog.py \
  --summary out/bench/summary.json \
  --max-hard-regressions 2 \
  --hard-threshold-pct -2.0

echo "[5/6] command center"
python3 scripts/status/generate_command_center.py \
  --bench-summary out/bench/summary.json \
  --correctness-ok true --perf-ok true --flake-rate 0.0 \
  --out out/status/optimizer_command_center.rendered.md

echo "[6/6] done"
echo "LEGENDARY RUN COMPLETE."
