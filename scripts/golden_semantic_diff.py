#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

def load(p):
    return json.loads(Path(p).read_text(encoding='utf-8'))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--expected", required=True)
    ap.add_argument("--actual", required=True)
    ap.add_argument("--out", default="artifacts/golden_semantic_diff.json")
    args = ap.parse_args()

    exp = load(args.expected)
    act = load(args.actual)

    diff = {
        "schema_version": "1.0.0",
        "step_count_expected": len(exp.get("steps", [])),
        "step_count_actual": len(act.get("steps", [])),
        "step_deltas": []
    }

    n = min(len(exp.get("steps", [])), len(act.get("steps", [])))
    for i in range(n):
        es = exp["steps"][i]
        as_ = act["steps"][i]
        delta = {"index": i, "gate_expected": es.get("gate"), "gate_actual": as_.get("gate")}
        if es.get("syndrome") != as_.get("syndrome"):
            delta["syndrome_changed"] = True
            delta["syndrome_expected"] = es.get("syndrome")
            delta["syndrome_actual"] = as_.get("syndrome")
        if es.get("correction") != as_.get("correction"):
            delta["correction_changed"] = True
            delta["correction_expected"] = es.get("correction")
            delta["correction_actual"] = as_.get("correction")
        if len(delta.keys()) > 3:
            diff["step_deltas"].append(delta)

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(diff, indent=2, sort_keys=True), encoding='utf-8')
    print(f"Wrote {out}")

if __name__ == "__main__":
    main()
