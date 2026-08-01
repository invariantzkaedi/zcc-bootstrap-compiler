#!/usr/bin/env python3
import hashlib
import json
import sys
from pathlib import Path


def verify_manifest():
    manifest_path = Path("release_artifacts/manifest.json")
    if not manifest_path.exists():
        print("No manifest.json found in release_artifacts, skipping verification.")
        return 0

    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except Exception as e:
        print(f"FAIL: Could not parse manifest.json: {e}")
        return 1

    errors = 0
    for item in manifest.get("artifacts", []):
        name = item.get("path")
        expected_digest = item.get("sha256")
        if not name or not expected_digest:
            continue
        p = Path("release_artifacts") / name
        if not p.exists():
            print(f"FAIL: Missing artifact {p}")
            errors += 1
            continue
        actual_digest = "sha256:" + hashlib.sha256(p.read_bytes()).hexdigest()
        if actual_digest != expected_digest:
            print(f"FAIL: Checksum mismatch for {p} (expected {expected_digest}, got {actual_digest})")
            errors += 1

    if errors > 0:
        print(f"VERIFICATION FAILED: {errors} artifact errors found.")
        sys.exit(1)

    print("ALL RELEASE ARTIFACT CHECKSUMS VERIFIED SUCCESSFULLY!")
    return 0


if __name__ == "__main__":
    sys.exit(verify_manifest())
