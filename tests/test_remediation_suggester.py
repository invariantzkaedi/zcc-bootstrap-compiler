import json
from pathlib import Path
import pytest
from scripts.remediation_suggester import suggest_remediations, OUT_PATH, DASHBOARD_PATH

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

def test_remediation_suggester_advisory_mode():
    suggest_remediations()
    assert OUT_PATH.exists()
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    assert data["advisory_mode"] is True
    assert "recommendations" in data

def test_remediation_suggester_with_drift():
    # Write mock dashboard data with drift
    mock_dash = {
        "schema_version": "1.0.0",
        "windows": {
            "days_7": {
                "determinism_drifts": 3,
                "pass_rate": 0.95,
                "kill_rate_min": 0.9,
                "kill_rate_avg": 0.9,
                "kill_rate_max": 0.9,
                "unique_signatures": 0,
                "incidents_open": 0,
                "incidents_closed": 0,
                "mttr_hours": 0
            }
        }
    }
    DASHBOARD_PATH.write_text(json.dumps(mock_dash), encoding='utf-8')
    
    suggest_remediations()
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    recs = data["recommendations"]
    assert len(recs) == 1
    assert recs[0]["source_code"] == "DETERMINISM_DRIFT"
    assert recs[0]["priority_score"] == 95
