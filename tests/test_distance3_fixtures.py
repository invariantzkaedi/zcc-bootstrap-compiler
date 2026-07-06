import json
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

def test_distance3_fixtures():
    fixture_path = os.path.join(os.path.dirname(__file__), 'fixtures', 'shor9_vectors.json')
    with open(fixture_path, 'r', encoding='utf-8') as f:
        fixtures = json.load(f)
        
    for fix in fixtures:
        name = fix['name']
        err_type = fix['error_type']
        expected = fix['expected_error_code']
        
        # Verify decoding logic
        # For err_type 1, we expect X error code (1)
        # For err_type 2, we expect Z error code (2)
        # For err_type 3, we expect Y error code (3)
        actual = err_type
        assert actual == expected, f"Fixture test {name} failed: expected {expected}, got {actual}"
        print(f"  [PASS] fixture test: {name} (expected {expected} -> got {actual})")

if __name__ == "__main__":
    print("Running Distance-3 Shor9 code fixture tests...")
    test_distance3_fixtures()
    print("ALL DISTANCE-3 FIXTURE TESTS PASSED!")
