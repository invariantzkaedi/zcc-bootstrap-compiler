from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def run_ingest(session_dir: str, steps: int = 5, seed: int = 42) -> subprocess.CompletedProcess:
    """Helper to run the ingestion daemon CLI."""
    # Resolve script path relative to this test file
    script_path = Path(__file__).parent.parent / "scripts" / "flipper_ingest.py"
    
    cmd = [
        sys.executable, 
        str(script_path), 
        "--session-dir", session_dir, 
        "--steps", str(steps), 
        "--seed", str(seed)
    ]
    return subprocess.run(cmd, capture_output=True, text=True, check=True)


def test_session_dir_reproducibility() -> None:
    """
    Prove that independent ingestion runs with identical seeds produce 
    bit-identical archival bundles (raw events, signatures, and hashes).
    """
    with tempfile.TemporaryDirectory() as tmp_base:
        dir1 = os.path.join(tmp_base, "session_1")
        dir2 = os.path.join(tmp_base, "session_2")
        
        # Run 1
        run_ingest(dir1, steps=10, seed=123)
        # Run 2 (identical parameters)
        run_ingest(dir2, steps=10, seed=123)
        
        # 1. Verify Manifest Identity (excluding capture_started_at)
        with open(os.path.join(dir1, "manifest.json")) as f: m1 = json.load(f)
        with open(os.path.join(dir2, "manifest.json")) as f: m2 = json.load(f)
        
        # Remove nondeterministic and session-specific fields for comparison
        for field in ["capture_started_at", "capture_finished_at", "session_id"]:
            m1.pop(field, None)
            m2.pop(field, None)
        
        assert m1 == m2
        
        # 2. Verify Raw Data Identity (Byte-level)
        raw1 = Path(dir1) / "raw_events.jsonl"
        raw2 = Path(dir2) / "raw_events.jsonl"
        assert raw1.read_bytes() == raw2.read_bytes()
        
        # 3. Verify Signature Data Identity (Byte-level)
        sig1 = Path(dir1) / "signatures.jsonl"
        sig2 = Path(dir2) / "signatures.jsonl"
        assert sig1.read_bytes() == sig2.read_bytes()
        
        # 4. Verify cross-run divergence (different seeds)
        dir3 = os.path.join(tmp_base, "session_3")
        run_ingest(dir3, steps=10, seed=456)
        with open(os.path.join(dir3, "manifest.json")) as f: m3 = json.load(f)
        
        assert m3["results"]["final_checksum"] != m1["results"]["final_checksum"]
        assert m3["results"]["raw_data_sha256"] != m1["results"]["raw_data_sha256"]
