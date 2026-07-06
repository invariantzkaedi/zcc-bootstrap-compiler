#!/usr/bin/env python3
import argparse, json, sys

ap = argparse.ArgumentParser()
ap.add_argument("--summary", required=True)
ap.add_argument("--max-hard-regressions", type=int, default=2)
ap.add_argument("--hard-threshold-pct", type=float, default=-2.0)
args = ap.parse_args()

s = json.load(open(args.summary, encoding="utf-8"))
hard = [d for d in s.get("details", []) if d.get("runtime_delta_pct", 0.0) < args.hard_threshold_pct]

print(f"Hard regressions: {len(hard)}")
for d in sorted(hard, key=lambda x: x["runtime_delta_pct"]):
    print(f"- {d['benchmark']}: {d['runtime_delta_pct']:.2f}% (p={d.get('runtime_pvalue_mannwhitney',1.0):.4f})")

if len(hard) > args.max_hard_regressions:
    print("Watchdog FAIL: too many hard regressions")
    sys.exit(1)
print("Watchdog PASS")
