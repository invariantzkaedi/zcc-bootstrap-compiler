#!/usr/bin/env python3
from pathlib import Path

CANONICAL = ".zcc-fortify-policy.json"

errors = []

if not Path(CANONICAL).exists():
    errors.append(f"missing canonical policy file: {CANONICAL}")

for path in Path(".").iterdir():
    if path.name.lower() == CANONICAL.lower() and path.name != CANONICAL:
        errors.append(f"non-canonical policy filename: {path.name}")

if errors:
    for error in errors:
        print(f"[POLICY-FILENAME] {error}")
    raise SystemExit(1)

print("[POLICY-FILENAME] ok")