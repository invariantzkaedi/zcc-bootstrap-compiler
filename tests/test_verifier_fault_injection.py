#!/usr/bin/env python3
"""
ZKAEDI Hermetically Isolated Verifier Fault Injection Test
Runs generate_evidence_manifest.py and verify_evidence.py inside a isolated TemporaryDirectory, asserting:
1. Generator returns status code 1 (gen_status == 1)
2. Recorded exit_code == 17 in manifest.json
3. Persisted failure log artifact exists, is non-empty, and SHA-256 hashes match recomputed disk bytes
4. verify_evidence.validate_manifest() exits with code 1
"""

import sys
import json
import hashlib
import tempfile
from pathlib import Path

# Add scripts directory to path to import verifier modules directly
sys.path.insert(0, str(Path(__file__).parent.parent / "scripts"))
import generate_evidence_manifest
import verify_evidence

def test_pipeline_fault_injection():
    print("=== [HERMETIC SELF-TEST] Testing Complete Verifier Falsification Path ===")
    
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_path = Path(tmp_dir)
        test_manifest = tmp_path / "test_failing_manifest.json"
        test_evidence_dir = tmp_path / "evidence_latest"
        
        # 1. Inject artificial failing gate (exit status 17) into production generator
        failing_gates = [
            (
                "FAULT-017",
                "simulated-failure-gate",
                [sys.executable, "-c", "import sys; print('SIMULATED INVARIANT FAILURE'); sys.exit(17)"],
                "fault_injection_17.log"
            )
        ]
        
        # 2. Run generator in HERMETIC mode inside TemporaryDirectory
        gen_status = generate_evidence_manifest.generate_manifest(
            custom_gates=failing_gates,
            output_manifest=str(test_manifest),
            evidence_dir=str(test_evidence_dir)
        )
        assert gen_status == 1, f"Expected gen_status == 1, got {gen_status}"
        
        # 3. Assert manifest was written and recorded exit_code == 17
        assert test_manifest.exists(), "Manifest file was not written."
        with open(test_manifest, "r", encoding="utf-8") as f:
            data = json.load(f)
            
        recorded_exit = data["invariants"][0]["exit_code"]
        assert recorded_exit == 17, f"Expected recorded exit_code 17, got {recorded_exit}"
        
        # 4. Assert persisted failure log artifact exists and is non-empty
        art_path = Path(data["invariants"][0]["artifact_path"])
        assert art_path.exists(), "Persisted failure log artifact missing."
        assert art_path.stat().st_size > 0, "Failure log artifact is 0 bytes."
        
        # 5. Explicitly recompute and assert SHA-256 hash identity against disk bytes
        persisted_bytes = art_path.read_bytes()
        persisted_hash = hashlib.sha256(persisted_bytes).hexdigest()
        recorded_hash = data["invariants"][0]["sha256"]
        assert persisted_hash == recorded_hash, f"Digest mismatch: expected {recorded_hash}, got {persisted_hash}"
        
        # 6. Assert verify_evidence.validate_manifest() catches recorded_exit == 17 and exits with code 1
        try:
            verify_evidence.validate_manifest(manifest_path=str(test_manifest), assert_head=False)
            assert False, "verify_evidence.py failed to reject manifest with non-zero exit code!"
        except SystemExit as e:
            assert e.code == 1, f"verify_evidence.py exited with code {e.code}, expected 1"
            
        print("[HERMETIC SELF-TEST] SUCCESS: Complete verifier falsification path verified clean in isolated tempdir.")

if __name__ == "__main__":
    test_pipeline_fault_injection()
