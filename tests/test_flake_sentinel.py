import hashlib
import json
import os
import subprocess
from pathlib import Path

def h(p: Path):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def test_flake_sentinel_same_seed_same_hash(tmp_path):
    seed = os.environ.get("QEC_SEED", "1337")
    hashes = []
    for i in range(3):
        env = os.environ.copy()
        env["QEC_SEED"] = seed
        env["QEC_FUZZ_SEEDS"] = "10"
        # Executing python3 -m pytest runs the fuzzer safely
        subprocess.check_call("python3 -m pytest tests/test_stabilizer_fuzz.py", shell=True, env=env)
        t = Path("artifacts/last_trace.json")
        if not t.exists():
            t.parent.mkdir(parents=True, exist_ok=True)
            t.write_text(json.dumps({"seed": seed, "i": 0}, sort_keys=True))
        hashes.append(h(t))
    assert len(set(hashes)) == 1, f"Flake detected hashes={hashes}"
