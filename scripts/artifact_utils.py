#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path

def canonical_json(obj) -> str:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"))

def failure_signature(failure_obj: dict) -> str:
    payload = {
        "failure_type": failure_obj.get("failure_type", "unknown"),
        "minimized_gate_sequence": failure_obj.get("minimized_gate_sequence", failure_obj.get("gate_sequence", [])),
        "input_pauli": failure_obj.get("input_pauli", {})
    }
    return hashlib.sha256(canonical_json(payload).encode()).hexdigest()

def write_index(artifacts_dir: str = "artifacts") -> Path:
    p = Path(artifacts_dir)
    p.mkdir(parents=True, exist_ok=True)
    failures = []
    for f in sorted(p.glob("failure_*.json")):
        try:
            obj = json.loads(f.read_text(encoding='utf-8'))
            sig = failure_signature(obj)
            failures.append({
                "file": str(f),
                "signature": sig,
                "seed": obj.get("seed"),
                "failure_type": obj.get("failure_type"),
                "n_qubits": obj.get("n_qubits"),
                "original_length": obj.get("original_length"),
                "minimized_length": obj.get("minimized_length"),
            })
        except Exception:
            pass
    idx = {
        "schema_version": "1.0.0",
        "count": len(failures),
        "unique_signatures": len(set(x["signature"] for x in failures)),
        "failures": failures
    }
    out = p / "index.json"
    out.write_text(json.dumps(idx, indent=2, sort_keys=True))
    return out

if __name__ == "__main__":
    out = write_index("artifacts")
    print(f"Wrote {out}")
