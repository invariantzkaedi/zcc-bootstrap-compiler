#!/usr/bin/env python3
from __future__ import annotations
import argparse, json, os, subprocess, sys, time, uuid
from dataclasses import dataclass, asdict
from datetime import datetime, timezone
from pathlib import Path

MANIFEST_PATH = "tools/legendary_experiment_manifest.json"

REQUIRED_METRICS = [
    "falseBeatRate","missedTrueBeatRate","minBeatIntervalMs","syncErrorP95Ms",
    "arbitrationCorrectnessPct","nondeterministicSortCount",
    "constraintViolations","popEvents","nanCount","infCount",
    "recalibrationConvergenceSec","flashFrequencyHzMaxObserved","luminanceDeltaMaxObserved",
    "reentryJerkP95","reactiveArtifactCount","determinismMatchPct","poseHashMismatchCount",
    "memorySlopeMBPerHour","criticalErrors","degradationLockEvents","fpsP50"
]

@dataclass
class MetricResult:
    name: str
    value: float

def now_utc():
    return datetime.now(timezone.utc).isoformat()

def ensure_required_metrics(payload: dict):
    missing = [m for m in REQUIRED_METRICS if m not in payload]
    if missing:
        raise RuntimeError(f"Missing required runtime metrics: {missing}")

def run_runtime_probe(runtime_cmd: str, exp: dict, seed: int, out_dir: Path) -> dict:
    """
    Expects runtime_cmd to write a JSON file with required metrics.
    Command receives:
      --experiment-id
      --seed
      --duration-sec
      --output-json
      --inputs-json
      --actions-json
    """
    import shlex
    probe_out = out_dir / f"{exp['id']}__seed{seed}.metrics.json"
    cmd = shlex.split(runtime_cmd)
    cmd += [
        "--experiment-id", exp["id"],
        "--seed", str(seed),
        "--duration-sec", str(exp["durationSec"]),
        "--output-json", str(probe_out),
        "--inputs-json", json.dumps(exp.get("inputs", {})),
        "--actions-json", json.dumps(exp.get("actions", [])),
    ]
    # support quoted command
    if os.name == "nt":
        def esc(x):
            if " " in x or "\\" in x or "/" in x or '"' in x:
                return '"' + x.replace('"', '\\"') + '"'
            return x
        cmd_str = " ".join(esc(c) for c in cmd)
        result = subprocess.run(cmd_str, shell=True, capture_output=True, text=True)
    else:
        result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        raise RuntimeError(f"Runtime probe failed for {exp['id']} seed {seed}\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}")

    if not probe_out.exists():
        raise RuntimeError(f"Runtime probe did not produce output file: {probe_out}")

    payload = json.loads(probe_out.read_text(encoding="utf-8"))
    ensure_required_metrics(payload)
    return payload

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
    lines += [f"- Total runs: **{total}**", f"- Passed: **{passed}**", f"- Failed: **{total-passed}**", ""]
    lines += ["| Experiment | Seed | Status | Failures |", "|---|---:|---|---|"]
    for r in results:
        fail = "; ".join(r["failures"]) if r["failures"] else "-"
        lines.append(f"| {r['id']} | {r['seed']} | {r['status']} | {fail} |")
    (out_dir / "summary.md").write_text("\n".join(lines), encoding="utf-8")

def write_environment(out_dir: Path, synthetic: bool, runtime_cmd: str):
    env = {
        "generatedAt": now_utc(),
        "python": sys.version,
        "platform": sys.platform,
        "syntheticMode": synthetic,
        "runtimeProbeCommand": runtime_cmd
    }
    (out_dir / "environment.json").write_text(json.dumps(env, indent=2), encoding="utf-8")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", default=MANIFEST_PATH)
    ap.add_argument("--runtime-probe-cmd", required=True, help="Executable that returns REAL runtime metrics JSON.")
    ap.add_argument("--allow-synthetic", action="store_true", help="Only for local dev. CI must never use this.")
    args = ap.parse_args()

    root_dir = Path(__file__).parent.parent
    manifest = json.loads((root_dir / args.manifest).read_text(encoding="utf-8"))
    run_id = datetime.now().strftime("%Y%m%d_%H%M%S") + "_" + uuid.uuid4().hex[:8]
    out_dir = root_dir / manifest["global"]["artifactsDir"] / run_id
    out_dir.mkdir(parents=True, exist_ok=True)

    synthetic = False  # forced real mode
    write_environment(out_dir, synthetic=synthetic, runtime_cmd=args.runtime_probe_cmd)

    all_results = []
    for exp in manifest["experiments"]:
        for seed in manifest["seedSet"]:
            measured = run_runtime_probe(args.runtime_probe_cmd, exp, seed, out_dir)
            ok, failures = check_pass(exp, measured)

            all_results.append({
                "id": exp["id"],
                "status": "PASS" if ok else "FAIL",
                "metrics": [asdict(MetricResult(k, float(v))) for k, v in measured.items()],
                "failures": failures,
                "durationSec": exp["durationSec"],
                "seed": seed
            })

    payload = {
        "suiteName": manifest["suiteName"],
        "generatedAt": now_utc(),
        "runId": run_id,
        "results": all_results
    }
    (out_dir / "results.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")
    write_summary(all_results, out_dir)
    print(f"[DONE] Wrote REAL artifacts to: {out_dir}")

if __name__ == "__main__":
    main()
