import os
import shutil
import sqlite3
import json
from pathlib import Path
import pytest
from scripts.ingest_artifacts import ingest_directory, DB_PATH

@pytest.fixture(autouse=True)
def setup_teardown():
    # Clean up DB before and after
    if DB_PATH.exists():
        DB_PATH.unlink()
    # Create temp artifacts dir
    test_dir = Path("temp_test_artifacts")
    if test_dir.exists():
        shutil.rmtree(test_dir)
    test_dir.mkdir(parents=True, exist_ok=True)
    
    yield test_dir
    
    if DB_PATH.exists():
        DB_PATH.unlink()
    if test_dir.exists():
        shutil.rmtree(test_dir)

def test_ingest_idempotency(setup_teardown):
    test_dir = setup_teardown
    
    # Create a failure file
    fail_data = {
        "seed": 42,
        "signature": "sig_abc123",
        "failure_type": "math_rule_mismatch",
        "n_qubits": 7,
        "original_length": 100,
        "minimized_length": 50
    }
    fail_file = test_dir / "failure_42.json"
    fail_file.write_text(json.dumps(fail_data), encoding='utf-8')
    
    # Set env mock
    os.environ["GITHUB_RUN_ID"] = "test_run_123"
    os.environ["GITHUB_WORKFLOW"] = "test_workflow"
    
    # Ingest once
    ingest_directory(str(test_dir))
    
    # Ingest twice
    ingest_directory(str(test_dir))
    
    # Verify count
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    
    cur.execute("SELECT count(*) FROM runs WHERE run_id='test_run_123'")
    assert cur.fetchone()[0] == 1
    
    cur.execute("SELECT count(*) FROM failures WHERE run_id='test_run_123'")
    assert cur.fetchone()[0] == 1
    
    conn.close()
