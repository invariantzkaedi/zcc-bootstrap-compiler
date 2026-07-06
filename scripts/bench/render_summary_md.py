#!/usr/bin/env python3
import json
import argparse

ap = argparse.ArgumentParser()
ap.add_argument("--summary", required=True)
ap.add_argument("--out", required=True)
args = ap.parse_args()

with open(args.summary, "r") as f:
    s = json.load(f)

md = []
md.append("## IR Opt Bench Summary")
md.append(f"- Benchmarks: **{s['bench_count']}**")
md.append(f"- Compile geomean overhead: **{s['compile_geomean_overhead_pct']:.2f}%**")
md.append(f"- Runtime geomean delta: **{s['runtime_geomean_delta_pct']:.2f}%**")
md.append("")
md.append("| Benchmark | Compile Overhead % | Runtime Delta % |")
md.append("|---|---:|---:|")
for d in s["details"]:
    md.append(f"| {d['benchmark']} | {d['compile_overhead_pct']:.2f} | {d['runtime_delta_pct']:.2f} |")

with open(args.out, "w") as f:
    f.write("\n".join(md))
