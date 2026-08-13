#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import time
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description="Flipper deterministic ingestion daemon CLI")
    parser.add_argument("--session-dir", required=True, help="Session output directory")
    parser.add_argument("--steps", type=int, default=5, help="Number of ingestion steps")
    parser.add_argument("--seed", type=int, default=42, help="RNG seed")
    args = parser.parse_args()

    session_path = Path(args.session_dir)
    session_path.mkdir(parents=True, exist_ok=True)

    started_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

    raw_events = []
    signatures = []

    for step in range(args.steps):
        # Deterministic payload based on seed and step
        payload = f"step:{step}:seed:{args.seed}"
        event_hash = hashlib.sha256(payload.encode("utf-8")).hexdigest()
        event_data = {
            "step": step,
            "seed": args.seed,
            "hash": event_hash,
            "value": (args.seed * 31 + step * 17) % 1000
        }
        raw_events.append(event_data)

        sig_data = {
            "step": step,
            "signature": hashlib.sha256(f"sig:{event_hash}".encode("utf-8")).hexdigest()
        }
        signatures.append(sig_data)

    raw_bytes = ("\n".join(json.dumps(ev) for ev in raw_events) + "\n").encode("utf-8")
    sig_bytes = ("\n".join(json.dumps(sig) for sig in signatures) + "\n").encode("utf-8")

    raw_sha256 = hashlib.sha256(raw_bytes).hexdigest()
    sig_sha256 = hashlib.sha256(sig_bytes).hexdigest()
    checksum_str = hashlib.sha256(f"checksum:{args.seed}:{len(raw_events)}".encode("utf-8")).hexdigest()

    finished_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

    manifest = {
        "session_id": f"session-{args.seed}",
        "capture_started_at": started_at,
        "capture_finished_at": finished_at,
        "steps": args.steps,
        "seed": args.seed,
        "total_events": len(raw_events),
        "results": {
            "final_checksum": checksum_str,
            "raw_data_sha256": raw_sha256,
            "signature_data_sha256": sig_sha256
        }
    }

    with open(session_path / "manifest.json", "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    with open(session_path / "raw_events.jsonl", "wb") as f:
        f.write(raw_bytes)

    with open(session_path / "signatures.jsonl", "wb") as f:
        f.write(sig_bytes)

if __name__ == "__main__":
    main()
