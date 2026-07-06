#!/usr/bin/env python3
import json
import glob
from pathlib import Path
import jsonschema

def load(p):
    return json.loads(Path(p).read_text(encoding='utf-8'))

def main():
    art = Path("artifacts")
    art.mkdir(parents=True, exist_ok=True)

    failure_schema = load("schemas/qec_failure_schema.json")
    trace_schema = load("schemas/qec_trace_schema.json")

    checked = 0
    for fp in glob.glob("artifacts/failure_*.json"):
        obj = load(fp)
        jsonschema.validate(obj, failure_schema)
        checked += 1

    for fp in glob.glob("artifacts/trace_*.json"):
        obj = load(fp)
        jsonschema.validate(obj, trace_schema)
        checked += 1

    print(f"Validated {checked} artifact files")

if __name__ == "__main__":
    main()
