import json
import subprocess
from pathlib import Path

def emit_failure(seed, n_qubits, gates, input_pauli, expected, actual, test_name):
    art = Path("artifacts")
    art.mkdir(parents=True, exist_ok=True)

    tmp = {
        "gates.json": gates,
        "input_pauli.json": input_pauli,
        "expected.json": expected,
        "actual.json": actual,
    }
    for k, v in tmp.items():
        (art / k).write_text(json.dumps(v, indent=2, sort_keys=True))

    out = art / f"failure_{seed}.json"
    cmd = [
        "python3", "scripts/emit_failure_artifact.py",
        "--out", str(out),
        "--test-name", test_name,
        "--failure-type", "math_rule_mismatch",
        "--seed", str(seed),
        "--n-qubits", str(n_qubits),
        "--gate-sequence-json", str(art / "gates.json"),
        "--input-pauli-json", str(art / "input_pauli.json"),
        "--expected-json", str(art / "expected.json"),
        "--actual-json", str(art / "actual.json"),
        "--repro-command", f"QEC_SEED={seed} pytest -q {test_name}",
    ]
    subprocess.run(cmd, check=True)
    return out
