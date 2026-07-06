import json
import sqlite3
from pathlib import Path
import pytest
import jsonschema
from scripts.generate_dashboard_data import generate_report, OUT_PATH, SCHEMA_PATH, DB_PATH

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

def test_dashboard_data_schema_valid():
    # Setup a mock DB with minimal data
    from scripts.ingest_artifacts import init_db
    init_db()
    
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    # Insert mock run
    cur.execute("""
    INSERT INTO runs (run_id, workflow_name, branch, commit_sha, status)
    VALUES ('run_test_1', 'verify', 'main', 'sha123', 'success')
    """)
    # Insert mock failure
    cur.execute("""
    INSERT INTO failures (run_id, signature, seed, failure_type, n_qubits, minimized_ratio, artifact_path)
    VALUES ('run_test_1', 'sig_123', '42', 'math_rule_mismatch', 7, 0.5, 'path/to/art')
    """)
    conn.commit()
    conn.close()
    
    # Generate report
    generate_report()
    
    assert OUT_PATH.exists()
    
    # Load and validate
    data = json.loads(OUT_PATH.read_text(encoding='utf-8'))
    schema = json.loads(SCHEMA_PATH.read_text(encoding='utf-8'))
    
    # Validate
    jsonschema.validate(instance=data, schema=schema)
