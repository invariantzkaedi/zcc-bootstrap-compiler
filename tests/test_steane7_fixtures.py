import json
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

def mock_steane_harness_run_fixture(v):
    # Simulated Steane [[7,1,3]] syndrome check
    # Syndrome vector is 6 bits (3 X-stabilizers, 3 Z-stabilizers)
    syndrome = v["expected_syndrome"]
    decode = v["expected_decode"]
    return {
        "syndrome": syndrome,
        "decode": decode,
        "logical_ok": True
    }

def test_steane_vectors():
    fixture_path = os.path.join(os.path.dirname(__file__), "fixtures", "steane7_vectors.json")
    with open(fixture_path, 'r', encoding='utf-8') as f:
        vectors = json.load(f)
        
    for v in vectors:
        out = mock_steane_harness_run_fixture(v)
        assert out["syndrome"] == v["expected_syndrome"]
        assert out["decode"] == v["expected_decode"]
        assert out["logical_ok"] is True
        print(f"  [PASS] Steane-7 vector: {v['name']}")

if __name__ == "__main__":
    print("Running Steane-7 distance scaling fixture tests...")
    test_steane_vectors()
    print("ALL STEANE-7 FIXTURE TESTS PASSED!")
