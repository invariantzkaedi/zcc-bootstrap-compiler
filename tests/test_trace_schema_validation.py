import json
from pathlib import Path
import jsonschema

def load_json(p):
    return json.loads(Path(p).read_text(encoding='utf-8'))

def test_failure_schema_examples():
    schema = load_json("schemas/qec_failure_schema.json")
    sample = {
        "schema_version": "1.0.0",
        "timestamp_utc": "2026-07-06T12:00:00Z",
        "test_name": "test_stabilizer_fuzz",
        "failure_type": "math_rule_mismatch",
        "seed": 7,
        "n_qubits": 2,
        "gate_sequence": [{"gate": "CNOT", "qubits": [0,1]}],
        "input_pauli": {"x_bits":[1,0], "z_bits":[0,0]},
        "expected": {"x_bits":[1,1], "z_bits":[0,0]},
        "actual": {"x_bits":[1,0], "z_bits":[0,0]},
        "repro": {"command":"QEC_SEED=7 pytest -q tests/test_stabilizer_fuzz.py", "env":{"QEC_SEED":"7"}},
        "tool_versions": {"python":"3.12.3", "pytest":"pytest 9.0.2"}
    }
    jsonschema.validate(sample, schema)

def test_trace_schema_examples():
    schema = load_json("schemas/qec_trace_schema.json")
    sample = {
        "schema_version": "1.0.0",
        "trace_id": "abc123",
        "seed": 7,
        "n_qubits": 2,
        "steps": [{
            "index": 0,
            "gate": "CNOT",
            "qubits": [0,1],
            "frame_before": {"x_bits":[1,0], "z_bits":[0,0]},
            "frame_after": {"x_bits":[1,1], "z_bits":[0,0]},
            "syndrome": [1,0,0,0,0,0],
            "correction": "X@1"
        }]
    }
    jsonschema.validate(sample, schema)
