import json
import csv
import pytest
from pathlib import Path
import scripts.export_executive_scorecard as scorecard

def test_executive_scorecard_compilation(tmp_path, monkeypatch):
    out_json = tmp_path / "executive_scorecard.json"
    out_md = tmp_path / "executive_scorecard.md"
    out_csv = tmp_path / "executive_scorecard.csv"
    
    # Patch paths
    monkeypatch.setattr(scorecard, "OUT_JSON", out_json)
    monkeypatch.setattr(scorecard, "OUT_MD", out_md)
    monkeypatch.setattr(scorecard, "OUT_CSV", out_csv)
    
    # Create fake inputs
    db_file = tmp_path / "dashboard_data.json"
    db_file.write_text(json.dumps({
        "runs": [{"status": "completed", "kill_rate": 0.95}],
        "stats": {"total_fails": 10, "unique_sigs": 3}
    }), encoding="utf-8")
    
    pol_file = tmp_path / "policy_check_report.json"
    pol_file.write_text(json.dumps({
        "status": "pass",
        "blocking_failures_count": 0,
        "warnings_count": 1
    }), encoding="utf-8")
    
    intel_file = tmp_path / "incident_intelligence.json"
    intel_file.write_text(json.dumps({
        "kpis": {"time_to_fix_hours": 1.5, "stale_open_incidents_count": 0},
        "pressure_index": 20,
        "top_actions": ["Fix staging preprocessor"]
    }), encoding="utf-8")
    
    monkeypatch.setattr(scorecard, "DASHBOARD_PATH", db_file)
    monkeypatch.setattr(scorecard, "POLICY_CHECK_PATH", pol_file)
    monkeypatch.setattr(scorecard, "INTEL_PATH", intel_file)
    
    # Compile
    scorecard.compile_scorecard()
    
    # Verify outputs exist
    assert out_json.exists()
    assert out_md.exists()
    assert out_csv.exists()
    
    # Validate JSON
    data = json.loads(out_json.read_text(encoding="utf-8"))
    assert data["reliability_posture"]["pass_rate_trend"] == "100.0%"
    assert data["policy_compliance"]["status"] == "PASS"
    assert "Fix staging preprocessor" in data["priorities"]
    
    # Validate CSV
    rows = []
    with open(out_csv, "r", newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        rows = list(reader)
    
    assert len(rows) > 1
    assert rows[0] == ["Category", "Metric", "Value", "Status"]
    assert any(row[1] == "Pass Rate Trend" and row[2] == "100.0%" for row in rows)
    
    # Validate MD
    md_text = out_md.read_text(encoding="utf-8")
    assert "# QEC-VOP Executive Reliability Scorecard" in md_text
    assert "Pass Rate Trend" in md_text
