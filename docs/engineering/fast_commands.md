# Fast Commands

## Full correctness
```bash
make -C tests/verify test-negative && \
make -C tests/verify-positive test-positive && \
make -C tests/opt/instcombine test-normalized && \
make -C tests/opt/sccp test-normalized
```

## Perf quick check
```bash
scripts/bench/run_robust_benchmarks.sh --base build/base/zcc-opt --cand build/cand/zcc-opt --suite benchmarks/list.txt --warmup 2 --runs 11 --trim 0.10 --out out/bench
python3 scripts/bench/evaluate_robust_thresholds.py --summary out/bench/summary.json --max-compile-overhead-pct 12 --min-runtime-geomean-pct 0 --max-regressed-benches 3 --per-bench-regress-pct -3 --alpha 0.05
```
