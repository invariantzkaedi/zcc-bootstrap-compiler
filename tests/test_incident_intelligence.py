import json
import sqlite3
import pytest
from pathlib import Path
import scripts.incident_intelligence as intel

def test_incident_intelligence_computation(tmp_path, monkeypatch):
    # Setup temp DB
    db_file = tmp_path / "qec_warehouse.db"
    out_file = tmp_path / "incident_intelligence.json"
    
    # Patch paths
    monkeypatch.setattr(intel, "DB_PATH", db_file)
    monkeypatch.setattr(intel, "OUT_PATH", out_file)
    
    # Create schema and insert mock data
    conn = sqlite3.connect(db_file)
    cur = conn.cursor()
    cur.execute("""
    CREATE TABLE runs (
        run_id TEXT PRIMARY KEY,
        workflow_name TEXT NOT NULL,
        branch TEXT,
        commit_sha TEXT,
        status TEXT NOT NULL,
        started_at TEXT,
        ended_at TEXT,
        created_at TEXT NOT NULL DEFAULT (datetime('now'))
    )""")
    cur.execute("""
    CREATE TABLE failures (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        run_id TEXT NOT NULL,
        signature TEXT NOT NULL,
        seed TEXT,
        failure_type TEXT,
        n_qubits INTEGER,
        minimized_ratio REAL,
        artifact_path TEXT,
        created_at TEXT NOT NULL DEFAULT (datetime('now'))
    )""")
    cur.execute("""
    CREATE TABLE incidents (
        signature TEXT PRIMARY KEY,
        issue_url TEXT NOT NULL,
        issue_number INTEGER,
        status TEXT NOT NULL,
        title TEXT,
        owner TEXT,
        opened_at TEXT,
        closed_at TEXT,
        updated_at TEXT
    )""")
    
    # Insert a closed incident to verify MTTR (time_to_fix)
    cur.execute("""
        INSERT INTO incidents (signature, issue_url, issue_number, status, title, owner, opened_at, closed_at, updated_at)
        VALUES ('sig1', 'http://url1', 101, 'closed', 'Title 1', 'owner1', '2026-07-06 00:00:00', '2026-07-06 05:00:00', '2026-07-06 05:00:00')
    """)
    # Insert an open stale incident (> 7 days)
    cur.execute("""
        INSERT INTO incidents (signature, issue_url, issue_number, status, title, owner, opened_at, closed_at, updated_at)
        VALUES ('sig2', 'http://url2', 102, 'open', 'Title 2', 'owner2', '2026-06-20 00:00:00', NULL, '2026-06-20 00:00:00')
    """)
    
    # Insert corresponding runs and failures to verify detection time and subsystems
    cur.execute("""
        INSERT INTO runs (run_id, workflow_name, branch, commit_sha, status, created_at)
        VALUES ('run1', 'quantum-tests', 'main', 'sha1', 'completed', '2026-07-05 22:00:00')
    """)
    cur.execute("""
        INSERT INTO failures (run_id, signature, seed, failure_type, n_qubits, minimized_ratio, artifact_path, created_at)
        VALUES ('run1', 'sig1', '1337', 'math_mismatch', 5, 1.0, 'part0_pp.c', '2026-07-05 22:00:00')
    """)
    
    conn.commit()
    conn.close()
    
    # Run intelligence calculation
    intel.run_incident_intelligence()
    
    # Assert output exists and check structure
    assert out_file.exists()
    data = json.loads(out_file.read_text(encoding='utf-8'))
    
    # Verify KPIs
    assert data["kpis"]["time_to_fix_hours"] == 5.0
    assert data["kpis"]["time_to_detect_hours"] == 2.0
    assert data["kpis"]["stale_open_incidents_count"] == 1
    
    # Verify hot subsystems
    subs = {s["subsystem"]: s["severity_score"] for s in data["hot_subsystems"]}
    assert "preprocessor" in subs
    assert subs["preprocessor"] > 0
    
    # Verify pressure index
    assert data["pressure_index"] > 0
