import json
import sqlite3
import pytest
from pathlib import Path
from scripts.detect_anomalies import load_policy_yaml, detect_anomalies, DB_PATH, OUT_PATH, POLICY_PATH

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

def test_load_policy_yaml():
    policy = load_policy_yaml(POLICY_PATH)
    assert policy["determinism_drift_critical"] == 0
    assert policy["mutation_kill_rate_min"] == 0.90
    assert policy["anomaly"]["zscore_warn"] == 2.0
    assert policy["anomaly"]["min_points"] == 7

def test_detect_anomalies_no_data():
    detect_anomalies()
    assert OUT_PATH.exists()
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    assert len(data["anomalies"]) == 0
    assert data["summary"]["critical"] == 0

def test_detect_anomalies_normal_behavior():
    from scripts.ingest_artifacts import init_db
    init_db()
    
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    # Insert 10 identical runs with 1 unique hash (pass)
    for i in range(15):
        run_id = f"run_{i}"
        cur.execute("INSERT INTO runs (run_id, workflow_name, status) VALUES (?, 'verify', 'success')", (run_id,))
        cur.execute("INSERT INTO determinism (run_id, unique_hashes, hash_set, pass_fail) VALUES (?, 1, '[\"h\"]', 'PASS')", (run_id,))
    conn.commit()
    conn.close()
    
    detect_anomalies()
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    assert len(data["anomalies"]) == 0

def test_detect_anomalies_drift_spike():
    from scripts.ingest_artifacts import init_db
    init_db()
    
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    # Insert 10 normal runs (unique_hashes = 1)
    for i in range(10):
        run_id = f"run_{i}"
        cur.execute("INSERT INTO runs (run_id, workflow_name, status) VALUES (?, 'verify', 'success')", (run_id,))
        cur.execute("INSERT INTO determinism (run_id, unique_hashes, hash_set, pass_fail) VALUES (?, 1, '[\"h\"]', 'PASS')", (run_id,))
    # Insert spike (unique_hashes = 5)
    cur.execute("INSERT INTO runs (run_id, workflow_name, status) VALUES ('run_spike', 'verify', 'success')")
    cur.execute("INSERT INTO determinism (run_id, unique_hashes, hash_set, pass_fail) VALUES ('run_spike', 5, '[\"h\"]', 'FAIL')")
    conn.commit()
    conn.close()
    
    detect_anomalies()
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    assert len(data["anomalies"]) == 1
    assert data["anomalies"][0]["metric"] == "determinism_drifts"
    assert data["anomalies"][0]["severity"] == "critical"
