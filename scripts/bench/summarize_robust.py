#!/usr/bin/env python3
import argparse, csv, json, math, statistics
from collections import defaultdict
from scipy.stats import mannwhitneyu

def trimmed_mean(xs, trim):
    xs = sorted(xs)
    n = len(xs)
    k = int(n * trim)
    core = xs[k:n-k] if n - 2*k > 0 else xs
    return sum(core)/len(core) if core else 0.0

def geomean(xs):
    xs = [x for x in xs if x > 0]
    if not xs: return 0.0
    return math.exp(sum(math.log(x) for x in xs)/len(xs))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--samples", required=True)
    ap.add_argument("--trim", type=float, default=0.10)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    comp = defaultdict(lambda: defaultdict(list))
    run  = defaultdict(lambda: defaultdict(list))

    with open(args.samples, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            b, v = r["benchmark"], r["variant"]
            if r["compile_us"]:
                comp[b][v].append(float(r["compile_us"]))
            if r["run_us"]:
                run[b][v].append(float(r["run_us"]))

    details = []
    comp_factors = []
    speedups = []
    regressed = 0

    for b in sorted(run.keys()):
        if "base" not in run[b] or "cand" not in run[b]: 
            continue
        rb = run[b]["base"]
        rc = run[b]["cand"]
        cb = comp[b]["base"] if "base" in comp[b] else []
        cc = comp[b]["cand"] if "cand" in comp[b] else []

        rb_tm = trimmed_mean(rb, args.trim)
        rc_tm = trimmed_mean(rc, args.trim)
        cb_tm = trimmed_mean(cb, args.trim) if cb else 0.0
        cc_tm = trimmed_mean(cc, args.trim) if cc else 0.0

        speed = (rb_tm/rc_tm) if rc_tm > 0 else 0.0
        speed_pct = (speed - 1.0) * 100.0
        comp_over = ((cc_tm-cb_tm)/cb_tm*100.0) if cb_tm > 0 else 0.0

        p = mannwhitneyu(rb, rc, alternative="two-sided").pvalue if rb and rc else 1.0

        if speed_pct < 0:
            regressed += 1

        speedups.append(speed)
        comp_factors.append(1.0 + comp_over/100.0)

        details.append({
            "benchmark": b,
            "compile_base_trimmed_us": cb_tm,
            "compile_cand_trimmed_us": cc_tm,
            "compile_overhead_pct": comp_over,
            "runtime_base_trimmed_us": rb_tm,
            "runtime_cand_trimmed_us": rc_tm,
            "runtime_speedup_x": speed,
            "runtime_delta_pct": speed_pct,
            "runtime_pvalue_mannwhitney": p,
            "runtime_samples_base": len(rb),
            "runtime_samples_cand": len(rc)
        })

    summary = {
        "bench_count": len(details),
        "compile_geomean_overhead_pct": (geomean(comp_factors)-1.0)*100.0 if comp_factors else 0.0,
        "runtime_geomean_delta_pct": (geomean(speedups)-1.0)*100.0 if speedups else 0.0,
        "runtime_geomean_speedup_x": geomean(speedups) if speedups else 1.0,
        "regressed_benchmark_count": regressed,
        "details": details
    }

    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)

if __name__ == "__main__":
    main()
