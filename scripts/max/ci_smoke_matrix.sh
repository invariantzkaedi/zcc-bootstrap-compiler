#!/usr/bin/env bash
set -euo pipefail

echo "== CI Smoke Matrix ==="

echo "[A] quick correctness"
make -C tests/verify test-negative
make -C tests/verify-positive test-positive

echo "[B] opt suites"
make -C tests/opt/instcombine test-normalized
make -C tests/opt/sccp test-normalized

echo "[C] watchdog (if summary exists)"
if [[ -f out/bench/summary.json ]]; then
  python3 scripts/bench/regression_watchdog.py --summary out/bench/summary.json --max-hard-regressions 2 --hard-threshold-pct -2.0
else
  echo "No out/bench/summary.json yet; skipping watchdog."
fi

echo "CI smoke matrix PASS ✅"
