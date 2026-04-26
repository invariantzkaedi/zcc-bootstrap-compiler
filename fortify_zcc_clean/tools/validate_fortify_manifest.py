#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

REQUIRED_TOP_LEVEL = [
    "kind",
    "status",
    "created_at",
    "target",
    "seed",
    "count",
    "git",
    "policy",
    "host",
    "reports",
    "artifact_hashes",
]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", default="artifacts/fortify-layout.manifest.json")
    args = parser.parse_args()

    path = Path(args.manifest)
    data = json.loads(path.read_text(encoding="utf-8"))

    errors = []

    for key in REQUIRED_TOP_LEVEL:
        if key not in data:
            errors.append(f"missing key: {key}")

    if data.get("kind") != "fortify-layout":
        errors.append("kind must be fortify-layout")

    if data.get("status") != "pass":
        errors.append("status must be pass")

    if not data.get("policy", {}).get("sha256"):
        errors.append("policy.sha256 is required")

    hashes = data.get("artifact_hashes")
    if not hashes or hashes.get("algorithm") != "sha256":
        errors.append("artifact_hashes.algorithm must be sha256")

    if errors:
        for error in errors:
            print(f"[FORTIFY-MANIFEST] {error}")
        raise SystemExit(1)

    print("[FORTIFY-MANIFEST] schema valid")

if __name__ == "__main__":
    main()