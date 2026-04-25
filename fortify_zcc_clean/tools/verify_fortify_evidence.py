#!/usr/bin/env python3

import argparse
import hashlib
import json
from pathlib import Path

def sha256_file(path):
    h = hashlib.sha256()

    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)

    return h.hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", default="artifacts/fortify-layout.manifest.json")
    parser.add_argument("--artifact-root", default="artifacts")
    args = parser.parse_args()

    manifest_path = Path(args.manifest)
    artifact_root = Path(args.artifact_root)

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    hashes = manifest.get("artifact_hashes")

    if not hashes:
        raise SystemExit("manifest missing artifact_hashes")

    failures = []

    for entry in hashes.get("files", []):
        path = artifact_root / entry["path"]

        if not path.exists():
            failures.append(f"missing artifact: {path}")
            continue

        actual = sha256_file(path)
        expected = entry["sha256"]

        if actual != expected:
            failures.append(
                f"hash mismatch: {path}: expected={expected} actual={actual}"
            )

    if failures:
        for failure in failures:
            print(f"[FORTIFY-VERIFY] {failure}")
        raise SystemExit(1)

    print("[FORTIFY-VERIFY] artifact hashes verified")

if __name__ == "__main__":
    main()