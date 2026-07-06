#!/usr/bin/env python3
import argparse, json, pathlib

ap = argparse.ArgumentParser()
ap.add_argument("--summary", required=True)
ap.add_argument("--out", required=True)
args = ap.parse_args()

s = json.loads(pathlib.Path(args.summary).read_text(encoding="utf-8"))
lines = []
lines.append("## ⚙️ IR Optimization Benchmark Summary")
lines.append(f"- Benchmarks: **{s.get('bench_count',0)}**")
lines.append(f"- Compile geomean overhead: **{s.get('compile_geomean_overhead_pct',0):.2f}%**")
lines.append(f"- Runtime geomean delta: **{s.get('runtime_geomean_delta_pct',0):.2f}%**")
lines.append(f"- Regressed benchmarks: **{s.get('regressed_benchmark_count',0)}**")
lines.append("")
lines.append("| Benchmark | Compile Overhead % | Runtime Delta % | p-value |")
lines.append("|---|---:|---:|---:|")
for d in s.get("details", []):
    lines.append(
        f"| {d['benchmark']} | {d.get('compile_overhead_pct',0):.2f} | "
        f"{d.get('runtime_delta_pct',0):.2f} | {d.get('runtime_pvalue_mannwhitney',1.0):.4f} |"
    )
pathlib.Path(args.out).write_text("
".join(lines), encoding="utf-8")
