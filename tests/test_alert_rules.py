import json
import sqlite3
import pytest
from scripts.generate_dashboard_data import generate_report, OUT_PATH, DB_PATH
from scripts.ingest_artifacts import init_db

@pytest.fixture(autouse=True)
def setup_teardown():
    if DB_PATH.exists():
        DB_PATH.unlink()
    if OUT_PATH.exists():
        OUT_PATH.unlink()
    yield
    if DB_PATH.exists():
        DB_PATH.unlink()
    if OUT_PATH.exists():
        OUT_PATH.unlink()

def test_alert_rules_determinism_drift():
    init_db()
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    
    # Insert run
    cur.execute("""
    INSERT INTO runs (run_id, workflow_name, branch, commit_sha, status)
    VALUES ('run_det_drift', 'verify', 'main', 'sha123', 'success')
    """)
    # Insert determinism fail (drift)
    cur.execute("""
    INSERT INTO determinism (run_id, unique_hashes, hash_set, pass_fail)
    VALUES ('run_det_drift', 2, '["hash1", "hash2"]', 'FAIL')
    """)
    
    conn.commit()
    conn.close()
    
    generate_report()
    
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    alerts = data.get("alerts", [])
    
    # Assert determinism drift alert is active
    drift_alert = [a for a in alerts if a["code"] == "DETERMINISM_DRIFT"]
    assert len(drift_alert) == 1
    assert drift_alert[0]["severity"] == "critical"
    assert drift_alert[0]["value"] == 1.0

def test_alert_rules_low_kill_rate():
    init_db()
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    
    cur.execute("""
    INSERT INTO runs (run_id, workflow_name, branch, commit_sha, status)
    VALUES ('run_low_mut', 'verify', 'main', 'sha123', 'success')
    """)
    # Insert mutation with low kill_rate (0.85 < 0.90)
    cur.execute("""
    INSERT INTO mutation (run_id, total_mutants, killed, survived, kill_rate, pass_fail)
    VALUES ('run_low_mut', 100, 85, 15, 0.85, 'FAIL')
    """)
    
    conn.commit()
    conn.close()
    
    generate_report()
    
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    alerts = data.get("alerts", [])
    
    # Assert low kill rate alert is active
    mut_alert = [a for a in alerts if a["code"] == "LOW_KILL_RATE"]
    assert len(mut_alert) == 1
    assert mut_alert[0]["severity"] == "high"
    assert mut_alert[0]["value"] == 0.85
