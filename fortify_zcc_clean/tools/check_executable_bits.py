#!/usr/bin/env python3
import os
from pathlib import Path

errors = []

for path in Path("ci").glob("*.sh"):
    if not os.access(path, os.X_OK):
        errors.append(str(path))

if errors:
    for path in errors:
        print(f"[EXECUTABLE-BIT] missing executable bit: {path}")
    raise SystemExit(1)

print("[EXECUTABLE-BIT] ok")