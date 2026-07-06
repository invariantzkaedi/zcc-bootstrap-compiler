#!/usr/bin/env python3
import argparse, json, sys

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--summary", required=True)
    ap.add_argument("--max-compile-overhead-pct", type=float, required=True)
    ap.add_argument("--min-runtime-geomean-pct", type=float, required=True)
    ap.add_argument("--max-regressed-benches", type=int, required=True)
    ap.add_argument("--per-bench-regress-pct", type=float, required=True)
    ap.add_argument("--alpha", type=float, default=0.05)
    args = ap.parse_args()

    s = json.load(open(args.summary, encoding="utf-8"))
    comp = s["compile_geomean_overhead_pct"]
    runp = s["runtime_geomean_delta_pct"]

    hard_reg = 0
    sig_reg = 0
    for d in s["details"]:
        if d["runtime_delta_pct"] < args.per_bench_regress_pct:
            hard_reg += 1
            if d.get("runtime_pvalue_mannwhitney", 1.0) < args.alpha:
                sig_reg += 1

    print(f"compile_geomean_overhead_pct={comp:.3f}%")
    print(f"runtime_geomean_delta_pct={runp:.3f}%")
    print(f"hard_regressed_benches={hard_reg}")
    print(f"significant_regressed_benches={sig_reg}")

    fail = False
    if comp > args.max_compile_overhead_pct:
        print("FAIL compile overhead threshold")
        fail = True
    if runp < args.min_runtime_geomean_pct:
        print("FAIL runtime geomean threshold")
        fail = True
    if hard_reg > args.max_regressed_benches:
        print("FAIL per-benchmark regression count threshold")
        fail = True

    if fail:
        sys.exit(1)
    print("PASS quality gate")

if __name__ == "__main__":
    main()
