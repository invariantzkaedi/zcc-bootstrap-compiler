#!/usr/bin/env python3
import argparse
import datetime as dt
import json
import os
import platform
import subprocess
from pathlib import Path

def sh(cmd):
    try:
        return subprocess.check_output(cmd, shell=True, text=True).strip()
    except Exception:
        return ""

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--test-name", required=True)
    ap.add_argument("--failure-type", default="unknown")
    ap.add_argument("--seed", type=int, required=True)
    ap.add_argument("--n-qubits", type=int, required=True)
    ap.add_argument("--gate-sequence-json", required=True, help="path to JSON array")
    ap.add_argument("--input-pauli-json", required=True, help="path to JSON object")
    ap.add_argument("--expected-json", required=True, help="path to JSON object")
    ap.add_argument("--actual-json", required=True, help="path to JSON object")
    ap.add_argument("--repro-command", required=True)
    args = ap.parse_args()

    gate_seq = json.loads(Path(args.gate_sequence_json).read_text())
    input_pauli = json.loads(Path(args.input_pauli_json).read_text())
    expected = json.loads(Path(args.expected_json).read_text())
    actual = json.loads(Path(args.actual_json).read_text())

    artifact = {
        "schema_version": "1.0.0",
        "timestamp_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "test_name": args.test_name,
        "failure_type": args.failure_type,
        "seed": args.seed,
        "n_qubits": args.n_qubits,
        "gate_sequence": gate_seq,
        "input_pauli": input_pauli,
        "expected": expected,
        "actual": actual,
        "repro": {
            "command": args.repro_command,
            "env": {
                "QEC_SEED": os.environ.get("QEC_SEED", ""),
                "QEC_FUZZ_SEEDS": os.environ.get("QEC_FUZZ_SEEDS", "")
            }
        },
        "tool_versions": {
            "python": platform.python_version(),
            "pytest": sh("python -m pytest --version"),
            "compiler": sh("${CC:-cc} --version | head -n1"),
            "git_commit": sh("git rev-parse --short HEAD")
        }
    }

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(artifact, indent=2, sort_keys=True))
    print(f"Wrote failure artifact: {out}")

if __name__ == "__main__":
    main()
