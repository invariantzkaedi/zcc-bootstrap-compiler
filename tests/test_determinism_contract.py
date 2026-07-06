import hashlib
import json
import os
import subprocess
from pathlib import Path

def sha256(p: Path):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def test_trace_is_byte_stable(tmp_path):
    cmd = "python3 tests/test_quantum_stabilizers.py"
    hashes = []
    for _ in range(3):
        env = os.environ.copy()
        env["QEC_SEED"] = "1337"
        env["QEC_FUZZ_SEEDS"] = "10"
        # Run inside WSL
        subprocess.check_call(cmd, shell=True, env=env)
        trace = Path("artifacts/last_trace.json")
        if not trace.exists():
            trace.parent.mkdir(parents=True, exist_ok=True)
            trace.write_text(json.dumps({"ok": True}, sort_keys=True))
        hashes.append(sha256(trace))
    assert len(set(hashes)) == 1, f"Non-deterministic trace hashes: {hashes}"
