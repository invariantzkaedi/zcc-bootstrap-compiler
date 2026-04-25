#!/usr/bin/env python3
"""Write the immutable fortify subject manifest.

The subject manifest is the document that gets hashed and signed.
After it is written, it must not be mutated (signing happens on the
file as-written; signing-status lives in a separate sibling file
and is referenced by the bundle, never inlined into the subject).
"""
import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def git_commit() -> str:
    """Resolve the git commit. Prefer GITHUB_SHA, then `git rev-parse HEAD`."""
    val = os.environ.get("GITHUB_SHA")
    if val:
        return val
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        )
        return out.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def load_optional_json(path: str):
    p = Path(path)
    if not p.exists():
        return None
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True,
                        help="ABI triple (e.g. x86_64-linux-gnu)")
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--policy", default=".zcc-fortify-policy.json")
    parser.add_argument("--hashes", required=True,
                        help="Path to fortify-artifact-hashes.json")
    parser.add_argument("--no-placeholders-report",
                        default="artifacts/no-placeholders.json")
    parser.add_argument("--core-freeze-report",
                        default="artifacts/layout-core-freeze.json")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    policy_path = Path(args.policy)
    if not policy_path.exists():
        print(f"[FORTIFY-MANIFEST] policy missing: {policy_path}", file=sys.stderr)
        return 1
    policy_sha = sha256_file(policy_path)

    hashes = load_optional_json(args.hashes)
    if hashes is None:
        print(f"[FORTIFY-MANIFEST] artifact-hashes missing: {args.hashes}",
              file=sys.stderr)
        return 1

    file_count = hashes.get("count", len(hashes.get("files", [])))

    manifest = {
        "kind": "fortify-layout",
        "status": "pass",
        "created_at": datetime.now(timezone.utc).isoformat(),
        "target": args.target,
        "seed": args.seed,
        "count": file_count,
        "git": {"commit": git_commit()},
        "policy": {
            "path": str(policy_path),
            "sha256": policy_sha,
        },
        "host": {
            "system": platform.system(),
            "machine": platform.machine(),
            "platform": platform.platform(),
        },
        "reports": {
            "no_placeholders": args.no_placeholders_report,
            "layout_core_freeze": args.core_freeze_report,
        },
        "artifact_hashes": hashes,
    }

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(f"[FORTIFY-MANIFEST] wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
