#!/usr/bin/env python3

import argparse
import hashlib
import json
from pathlib import Path

DEFAULT_EXCLUDES = {
    # Hashes itself
    "fortify-artifact-hashes.json",
    # The subject manifest is the document that gets signed; it's not evidence.
    "fortify-layout.subject.json",
    # The bundle wraps subject+hashes+signing; not evidence either.
    "fortify-attestation.bundle.json",
    # Signing status is mutated by sign-fortify-manifest.sh after hashing.
    "fortify-signing-status.json",
    # Legacy name kept for compatibility with older trees.
    "fortify-layout.manifest.json",
    "fortify-layout.manifest.json.sig",
}

# Suffixes for signature/certificate artifacts that get produced
# AFTER hashing and therefore must not be in the hashed evidence set.
DEFAULT_EXCLUDE_SUFFIXES = {".sig", ".asc", ".minisig", ".cert"}

def sha256_file(path):
    h = hashlib.sha256()

    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)

    return h.hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default="artifacts")
    parser.add_argument("--out", default="artifacts/fortify-artifact-hashes.json")
    args = parser.parse_args()

    root = Path(args.root)
    out = Path(args.out)

    files = []

    if root.exists():
        for path in sorted(root.rglob("*")):
            if not path.is_file():
                continue

            rel = path.relative_to(root).as_posix()

            if path.resolve() == out.resolve():
                continue

            if Path(rel).name in DEFAULT_EXCLUDES:
                continue

            if path.suffix in DEFAULT_EXCLUDE_SUFFIXES:
                continue

            files.append({
                "path": rel,
                "sha256": sha256_file(path),
                "bytes": path.stat().st_size,
            })

    result = {
        "kind": "fortify-artifact-hashes",
        "algorithm": "sha256",
        "root": str(root),
        "count": len(files),
        "files": files,
    }

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"[FORTIFY-HASHES] wrote {out}")

if __name__ == "__main__":
    main()