#!/usr/bin/env python3
"""
Legendary Readiness Score (0-100)
Usage:
  python tools/legendary_readiness_score.py artifacts/qa/<run_id>/results.json
"""

from __future__ import annotations
import json, sys
from pathlib import Path

WEIGHTS = {
    "safety": 30.0,        # NaN/Inf/constraint/pop
    "sync": 20.0,          # sync + recalibration
    "performance": 20.0,   # fps/frame time/endurance stability
    "determinism": 15.0,   # replay hash parity
    "accessibility": 10.0, # flash/luminance safety
    "expressiveness": 5.0  # optional artistic metrics
}

def get_metric(run, name, default=None):
    for m in run.get("metrics", []):
        if m["name"] == name:
            return m["value"]
    return default

def score_run(run):
    s = 0.0
    # Safety
    nan = get_metric(run, "nanCount", 999)
    inf = get_metric(run, "infCount", 999)
    cv  = get_metric(run, "constraintViolations", 999)
    pop = get_metric(run, "popEvents", 999)
    safety_ok = (nan == 0 and inf == 0 and cv == 0 and pop == 0)
    s += WEIGHTS["safety"] if safety_ok else 0.0

    # Sync
    sync = get_metric(run, "syncErrorP95Ms", 999)
    rec  = get_metric(run, "recalibrationConvergenceSec", 999)
    sync_score = 0.0
    if sync <= 26: sync_score += 0.7
    elif sync <= 35: sync_score += 0.4
    if rec <= 3: sync_score += 0.3
    elif rec <= 5: sync_score += 0.15
    s += WEIGHTS["sync"] * min(sync_score, 1.0)

    # Performance
    fps = get_metric(run, "fpsP50", 0)
    perf_score = 1.0 if fps >= 60 else (0.6 if fps >= 50 else 0.2)
    s += WEIGHTS["performance"] * perf_score

    # Determinism
    det = get_metric(run, "determinismMatchPct", 0)
    det_score = 1.0 if det >= 99.99 else (0.7 if det >= 99.5 else 0.3)
    s += WEIGHTS["determinism"] * det_score

    # Accessibility
    flash = get_metric(run, "flashFrequencyHzMaxObserved", 999)
    lum   = get_metric(run, "luminanceDeltaMaxObserved", 999)
    acc_score = 1.0 if (flash <= 2.0 and lum <= 0.08) else (0.5 if flash <= 3.0 else 0.0)
    s += WEIGHTS["accessibility"] * acc_score

    # Expressiveness (placeholder)
    # Replace with real expressive metrics when available.
    s += WEIGHTS["expressiveness"] * 0.8

    return round(s, 2)

def verdict(score):
    if score >= 95: return "LEGENDARY CERTIFIED"
    if score >= 85: return "PRODUCTION READY"
    if score >= 70: return "BETA READY"
    return "NOT READY"

def main():
    if len(sys.argv) < 2:
        print("Usage: python tools/legendary_readiness_score.py artifacts/qa/<run_id>/results.json")
        sys.exit(1)

    p = Path(sys.argv[1])
    data = json.loads(p.read_text(encoding="utf-8"))
    runs = data["results"]

    run_scores = []
    for r in runs:
        sc = score_run(r)
        run_scores.append(sc)

    final = round(sum(run_scores) / len(run_scores), 2) if run_scores else 0.0
    print(f"Legendary Readiness Score: {final}/100")
    print(f"Verdict: {verdict(final)}")

if __name__ == "__main__":
    main()
