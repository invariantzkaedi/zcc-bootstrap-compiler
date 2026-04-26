#!/usr/bin/env python3

import argparse
import json
import platform
import subprocess
from pathlib import Path

def version_of(cmd):
    try:
        result = subprocess.run(
            [cmd, "--version"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=5,
        )
        return result.stdout.splitlines()[0] if result.stdout else "unknown"
    except Exception:
        return "unknown"

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--kind", required=True)
    parser.add_argument("--status", required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--seed", type=int, required=True)
    parser.add_argument("--count", type=int, required=True)
    parser.add_argument("--case-index", type=int, default=None)
    parser.add_argument("--case-file", default=None)
    parser.add_argument("--reduced-case-file", default=None)
    parser.add_argument("--oracle-output", default=None)
    parser.add_argument("--zcc-output", default=None)
    parser.add_argument("--cc-oracle", default="cc")
    parser.add_argument("--zcc", default="./zcc")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    manifest = {
        "kind": args.kind,
        "status": args.status,
        "target": args.target,
        "seed": args.seed,
        "count": args.count,
        "case_index": args.case_index,
        "case_file": args.case_file,
        "reduced_case_file": args.reduced_case_file,
        "oracle_output": args.oracle_output,
        "zcc_output": args.zcc_output,
        "cc_oracle": args.cc_oracle,
        "cc_oracle_version": version_of(args.cc_oracle),
        "zcc": args.zcc,
        "zcc_version": version_of(args.zcc),
        "host": {
            "system": platform.system(),
            "machine": platform.machine(),
            "platform": platform.platform(),
        },
    }

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()