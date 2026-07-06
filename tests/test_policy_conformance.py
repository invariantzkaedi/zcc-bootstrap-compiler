import json
import pytest
from pathlib import Path
from scripts.check_policy_conformance import run_conformance_checks, OUT_PATH, DASHBOARD_PATH

@pytest.fixture(autouse=True)
def setup_teardown():
    if OUT_PATH.exists():
        OUT_PATH.unlink()
    if DASHBOARD_PATH.exists():
        DASHBOARD_PATH.unlink()
    yield
    if OUT_PATH.exists():
        OUT_PATH.unlink()
    if DASHBOARD_PATH.exists():
        DASHBOARD_PATH.unlink()

def test_conformance_checks_pass():
    mock_dash = {
        "schema_version": "1.0.0",
        "alerts": []
    }
    DASHBOARD_PATH.write_text(json.dumps(mock_dash), encoding='utf-8')
    
    run_conformance_checks()
    assert OUT_PATH.exists()
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    assert data["status"] == "pass"

def test_conformance_checks_fail_threshold_mismatch():
    mock_dash = {
        "schema_version": "1.0.0",
        "alerts": [
            {
                "code": "LOW_KILL_RATE",
                "severity": "high",
                "value": 0.85,
                "threshold": 0.50 # Mismatch (policy specifies 0.90)
            }
        ]
    }
    DASHBOARD_PATH.write_text(json.dumps(mock_dash), encoding='utf-8')
    
    with pytest.raises(SystemExit) as excinfo:
        run_conformance_checks()
    assert excinfo.value.code == 1
    
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    assert data["status"] == "fail"
