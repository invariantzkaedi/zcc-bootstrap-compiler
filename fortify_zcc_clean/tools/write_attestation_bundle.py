#!/usr/bin/env python3
import argparse
import hashlib
import json
import platform
from pathlib import Path
from datetime import datetime, timezone

def sha256_file(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def load_json(path):
    p = Path(path)
    if not p.exists():
        return None
    return json.loads(p.read_text(encoding="utf-8"))

def file_entry(path):
    if not path:
        return None

    p = Path(path)
    if not p.exists():
        return None

    return {
        "path": str(p),
        "sha256": sha256_file(p),
        "bytes": p.stat().st_size,
    }

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--subject", required=True)
    parser.add_argument("--hashes", required=True)
    parser.add_argument("--signing-status", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    signing = load_json(args.signing_status)
    signature_path = signing.get("signature") if signing else None
    cert_path = signing.get("certificate") if signing else None

    bundle = {
        "kind": "fortify-attestation-bundle",
        "status": "pass",
        "created_at": datetime.now(timezone.utc).isoformat(),
        "subject": file_entry(args.subject),
        "artifact_hashes": load_json(args.hashes),
        "signing": signing,
        "signing_status_file": file_entry(args.signing_status),
        "signature": file_entry(signature_path),
        "certificate": file_entry(cert_path),
        "host": {
            "system": platform.system(),
            "machine": platform.machine(),
            "platform": platform.platform(),
        },
    }

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(bundle, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"[ATTESTATION-BUNDLE] wrote {out}")

if __name__ == "__main__":
    main()