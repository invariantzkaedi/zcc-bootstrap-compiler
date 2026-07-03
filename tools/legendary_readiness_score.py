#!/usr/bin/env python3
from __future__ import annotations
import json, sys
from pathlib import Path

WEIGHTS = {
    "safety": 30.0,
    "sync": 20.0,
    "performance": 20.0,
    "determinism": 15.0,
    "accessibility": 10.0,
    "expressiveness": 5.0
}

def get_metric(run, name, default=None):
    for m in run.get("metrics", []):
        if m["name"] == name:
            return m["value"]
    return default

def score_run(run):
    s = 0.0
    nan = get_metric(run, "nanCount", 999)
    inf = get_metric(run, "infCount", 999)
    cv  = get_metric(run, "constraintViolations", 999)
    pop = get_metric(run, "popEvents", 999)
    if nan == 0 and inf == 0 and cv == 0 and pop == 0:
        s += WEIGHTS["safety"]

    sync = get_metric(run, "syncErrorP95Ms", 999)
    rec  = get_metric(run, "recalibrationConvergenceSec", 999)
    sync_score = 0.0
    if sync <= 26: sync_score += 0.7
    elif sync <= 35: sync_score += 0.4
    if rec <= 3: sync_score += 0.3
    elif rec <= 5: sync_score += 0.15
    s += WEIGHTS["sync"] * min(sync_score, 1.0)

    fps = get_metric(run, "fpsP50", 0)
    s += WEIGHTS["performance"] * (1.0 if fps >= 60 else 0.6 if fps >= 50 else 0.2)

    det = get_metric(run, "determinismMatchPct", 0)
    s += WEIGHTS["determinism"] * (1.0 if det >= 99.99 else 0.7 if det >= 99.5 else 0.3)

    flash = get_metric(run, "flashFrequencyHzMaxObserved", 999)
    lum   = get_metric(run, "luminanceDeltaMaxObserved", 999)
    s += WEIGHTS["accessibility"] * (1.0 if (flash <= 2.0 and lum <= 0.08) else 0.5 if flash <= 3.0 else 0.0)

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

    run_scores = [score_run(r) for r in runs]
    final = round(sum(run_scores) / len(run_scores), 2) if run_scores else 0.0
    print(f"Legendary Readiness Score: {final}/100")
    print(f"Verdict: {verdict(final)}")

if __name__ == "__main__":
    main()
