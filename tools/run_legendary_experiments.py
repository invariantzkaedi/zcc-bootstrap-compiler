#!/usr/bin/env python3
"""
Legendary Experiment Runner (skeleton)
- Reads experiment manifest
- Executes placeholder runs (replace with real hooks)
- Writes artifacts/results.json and summary.md
"""

from __future__ import annotations
import json, os, time, uuid, random, statistics
from dataclasses import dataclass, asdict
from datetime import datetime, timezone
from pathlib import Path

MANIFEST_PATH = "tools/legendary_experiment_manifest.json"

@dataclass
class MetricResult:
    name: str
    value: float

@dataclass
class ExperimentResult:
    id: str
    status: str
    metrics: list[MetricResult]
    failures: list[str]
    durationSec: int
    seed: int

def now_utc():
    return datetime.now(timezone.utc).isoformat()

def fake_measurements(exp_id: str, seed: int) -> dict:
    random.seed(f"{exp_id}:{seed}")
    # Replace all of this with real telemetry pull from runtime.
    return {
        "falseBeatRate": random.uniform(0.0, 1.8),
        "missedTrueBeatRate": random.uniform(0.0, 2.5),
        "minBeatIntervalMs": random.uniform(112, 150),
        "syncErrorP95Ms": random.uniform(10, 25),
        "arbitrationCorrectnessPct": 100.0,
        "nondeterministicSortCount": 0,
        "constraintViolations": 0,
        "popEvents": 0,
        "nanCount": 0,
        "infCount": 0,
        "recalibrationConvergenceSec": random.uniform(1.0, 2.8),
        "flashFrequencyHzMaxObserved": random.uniform(1.0, 1.9),
        "luminanceDeltaMaxObserved": random.uniform(0.03, 0.075),
        "reentryJerkP95": random.uniform(0.1, 0.21),
        "reactiveArtifactCount": 0,
        "determinismMatchPct": random.uniform(99.99, 100.0),
        "poseHashMismatchCount": 0,
        "memorySlopeMBPerHour": random.uniform(-2.0, 8.0),
        "criticalErrors": 0,
        "degradationLockEvents": 0,
        "fpsP50": random.uniform(62.0, 120.0)
    }

def check_pass(exp: dict, measured: dict) -> tuple[bool, list[str]]:
    failures = []
    criteria = exp.get("passCriteria", {})
    for k, v in criteria.items():
        if k.endswith("Max"):
            metric = k[:-3]
            if measured.get(metric, float("inf")) > v:
                failures.append(f"{metric}={measured.get(metric)} > {v}")
        elif k.endswith("Min"):
            metric = k[:-3]
            if measured.get(metric, float("-inf")) < v:
                failures.append(f"{metric}={measured.get(metric)} < {v}")
    return (len(failures) == 0, failures)

def write_summary(results, out_dir: Path):
    lines = ["# Legendary Experiment Summary", "", f"Generated: {now_utc()}", ""]
    total = len(results)
    passed = sum(1 for r in results if r["status"] == "PASS")
    lines.append(f"- Total runs: **{total}**")
    lines.append(f"- Passed: **{passed}**")
    lines.append(f"- Failed: **{total - passed}**")
    lines.append("")
    lines.append("| Experiment | Seed | Status | Failures |")
    lines.append("|---|---:|---|---|")
    for r in results:
        fail = "; ".join(r["failures"]) if r["failures"] else "-"
        lines.append(f"| {r['id']} | {r['seed']} | {r['status']} | {fail} |")
    (out_dir / "summary.md").write_text("\n".join(lines), encoding="utf-8")

def main():
    root_dir = Path(__file__).parent.parent
    manifest = json.loads((root_dir / MANIFEST_PATH).read_text(encoding="utf-8"))
    run_id = datetime.now().strftime("%Y%m%d_%H%M%S") + "_" + uuid.uuid4().hex[:8]
    out_dir = root_dir / manifest["global"]["artifactsDir"] / run_id
    out_dir.mkdir(parents=True, exist_ok=True)

    all_results = []
    for exp in manifest["experiments"]:
        for seed in manifest["seedSet"]:
            measured = fake_measurements(exp["id"], seed)
            ok, failures = check_pass(exp, measured)
            exp_result = ExperimentResult(
                id=exp["id"],
                status="PASS" if ok else "FAIL",
                metrics=[MetricResult(k, float(v)) for k, v in measured.items()],
                failures=failures,
                durationSec=exp["durationSec"],
                seed=seed
            )
            all_results.append({
                "id": exp_result.id,
                "status": exp_result.status,
                "metrics": [asdict(m) for m in exp_result.metrics],
                "failures": exp_result.failures,
                "durationSec": exp_result.durationSec,
                "seed": exp_result.seed
            })
            time.sleep(0.01)  # placeholder pacing

    payload = {
        "suiteName": manifest["suiteName"],
        "generatedAt": now_utc(),
        "runId": run_id,
        "results": all_results
    }
    (out_dir / "results.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")
    write_summary(all_results, out_dir)
    print(f"[DONE] Wrote artifacts to: {out_dir}")

if __name__ == "__main__":
    main()
