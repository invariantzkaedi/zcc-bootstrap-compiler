import json
import sqlite3
import pytest
from pathlib import Path
from scripts.forecast_metrics import generate_forecasts, DB_PATH, OUT_PATH, POLICY_PATH

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

def test_forecast_no_data():
    generate_forecasts()
    assert OUT_PATH.exists()
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    # Under empty data, breach probability should be 0.0 or low/null
    assert len(data["forecasts"]) == 4

def test_forecast_stable_series():
    from scripts.ingest_artifacts import init_db
    init_db()
    
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    # Insert 10 runs with 1.0 (100%) mutation kill rates
    for i in range(15):
        run_id = f"run_{i}"
        cur.execute("INSERT INTO runs (run_id, workflow_name, status) VALUES (?, 'verify', 'success')", (run_id,))
        cur.execute("INSERT INTO mutation (run_id, total_mutants, killed, survived, kill_rate, pass_fail) VALUES (?, 100, 100, 0, 1.0, 'PASS')", (run_id,))
    conn.commit()
    conn.close()
    
    generate_forecasts()
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    kill_forecast = [f for f in data["forecasts"] if f["metric"] == "mutation_kill_rate"][0]
    assert kill_forecast["breach_probability"] < 0.20

def test_forecast_degrading_series():
    from scripts.ingest_artifacts import init_db
    init_db()
    
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    # Degrade kill rate from 1.0 down to 0.88 sequentially
    rates = [1.0, 1.0, 0.98, 0.96, 0.95, 0.94, 0.93, 0.92, 0.91, 0.90, 0.89, 0.88]
    for i, rate in enumerate(rates):
        run_id = f"run_{i}"
        killed_val = int(rate * 100)
        cur.execute("INSERT INTO runs (run_id, workflow_name, status) VALUES (?, 'verify', 'success')", (run_id,))
        cur.execute("INSERT INTO mutation (run_id, total_mutants, killed, survived, kill_rate, pass_fail) VALUES (?, 100, ?, 10, ?, 'FAIL')", (run_id, killed_val, rate))
    conn.commit()
    conn.close()
    
    generate_forecasts()
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    kill_forecast = [f for f in data["forecasts"] if f["metric"] == "mutation_kill_rate"][0]
    # Under degrading trend reaching 0.88 (below 0.90 threshold), breach probability must be elevated
    assert kill_forecast["breach_probability"] > 0.50
