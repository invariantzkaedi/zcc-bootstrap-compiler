#!/usr/bin/env python3
import argparse, json, pathlib, datetime

ap = argparse.ArgumentParser()
ap.add_argument("--bench-summary", required=False, default="out/bench/summary.json")
ap.add_argument("--correctness-ok", required=False, default="true")
ap.add_argument("--perf-ok", required=False, default="true")
ap.add_argument("--flake-rate", required=False, default="0.0")
ap.add_argument("--out", required=True)
args = ap.parse_args()

bench = {}
p = pathlib.Path(args.bench_summary)
if p.exists():
    bench = json.loads(p.read_text(encoding="utf-8"))

details = sorted(bench.get("details", []), key=lambda d: d.get("runtime_delta_pct", 0.0))
worst = details[:5]

table = ["| Benchmark | Runtime Delta % | Compile Overhead % | p-value |",
         "|---|---:|---:|---:|"]
for d in worst:
    table.append(f"| {d.get('benchmark','-')} | {d.get('runtime_delta_pct',0):.2f} | {d.get('compile_overhead_pct',0):.2f} | {d.get('runtime_pvalue_mannwhitney',1.0):.4f} |")
if len(table) == 2:
    table.append("| (none) | 0.00 | 0.00 | 1.0000 |")

correctness_ok = args.correctness_ok.lower() == "true"
perf_ok = args.perf_ok.lower() == "true"
flake = float(args.flake_rate)

score = 100
if not correctness_ok: score -= 50
if not perf_ok: score -= 30
if flake > 5.0: score -= 10
if bench.get("runtime_geomean_delta_pct", 0.0) < 0: score -= 10
score = max(0, min(100, score))

template = pathlib.Path("docs/status/optimizer_command_center.md").read_text(encoding="utf-8")
rendered = (template
    .replace("{{RELEASE_SCORE}}", str(score))
    .replace("{{CORRECTNESS_STATUS}}", "✅ PASS" if correctness_ok else "❌ FAIL")
    .replace("{{PERF_STATUS}}", "✅ PASS" if perf_ok else "❌ FAIL")
    .replace("{{FLAKE_RATE}}", f"{flake:.2f}%")
    .replace("{{UPDATED_AT}}", datetime.datetime.utcnow().isoformat() + "Z")
    .replace("{{TOP_REGRESSIONS_TABLE}}", "\n".join(table))
)

pathlib.Path(args.out).write_text(rendered, encoding="utf-8")
print(f"Wrote {args.out}")
