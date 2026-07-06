#!/usr/bin/env python3
import argparse, hashlib, json
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument("--files", nargs="+", required=True)
ap.add_argument("--out", default="artifacts/determinism_hashes.json")
args = ap.parse_args()

hashes = []
for f in args.files:
    p = Path(f)
    if not p.exists():
        continue
    hashes.append(hashlib.sha256(p.read_bytes()).hexdigest())

out = Path(args.out)
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({"hashes": hashes}, indent=2, sort_keys=True), encoding='utf-8')
print(out)

if len(hashes) >= 2 and len(set(hashes)) != 1:
    raise SystemExit("Determinism hash drift detected")
