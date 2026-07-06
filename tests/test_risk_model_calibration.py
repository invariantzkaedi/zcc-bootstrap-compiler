import json
import sqlite3
import yaml
import pytest
from pathlib import Path
import scripts.calibrate_risk_model as calibrate

def test_risk_model_calibration(tmp_path, monkeypatch):
    db_file = tmp_path / "qec_warehouse.db"
    out_file = tmp_path / "risk_model_calibration.json"
    weights_file = tmp_path / "risk_model_weights.yaml"
    
    # Patch paths
    monkeypatch.setattr(calibrate, "DB_PATH", db_file)
    monkeypatch.setattr(calibrate, "OUT_PATH", out_file)
    monkeypatch.setattr(calibrate, "WEIGHTS_PATH", weights_file)
    
    # Create test database
    conn = sqlite3.connect(db_file)
    cur = conn.cursor()
    cur.execute("""
    CREATE TABLE risk_scores (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        pr_number INTEGER NOT NULL,
        commit_sha TEXT,
        score INTEGER NOT NULL,
        level TEXT NOT NULL,
        reasons_json TEXT NOT NULL,
        suggested_reviewers_json TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT (datetime('now')),
        UNIQUE(pr_number, commit_sha)
    )""")
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
    
    # Insert a few PRs: some with failures (risky) and some without
    # PR 1: Score 80, commit sha1, has failures (True Positive)
    cur.execute("""
        INSERT INTO risk_scores (pr_number, commit_sha, score, level, reasons_json, suggested_reviewers_json)
        VALUES (1, 'sha1', 80, 'high', '[]', '[]')
    """)
    cur.execute("""
        INSERT INTO runs (run_id, workflow_name, branch, commit_sha, status)
        VALUES ('run1', 'quantum-tests', 'pr-1', 'sha1', 'failed')
    """)
    cur.execute("""
        INSERT INTO failures (run_id, signature, seed, failure_type, artifact_path)
        VALUES ('run1', 'sig1', '1', 'type1', 'part0.c')
    """)
    
    # PR 2: Score 30, commit sha2, has failures (False Negative)
    cur.execute("""
        INSERT INTO risk_scores (pr_number, commit_sha, score, level, reasons_json, suggested_reviewers_json)
        VALUES (2, 'sha2', 30, 'low', '[]', '[]')
    """)
    cur.execute("""
        INSERT INTO runs (run_id, workflow_name, branch, commit_sha, status)
        VALUES ('run2', 'quantum-tests', 'pr-2', 'sha2', 'failed')
    """)
    cur.execute("""
        INSERT INTO failures (run_id, signature, seed, failure_type, artifact_path)
        VALUES ('run2', 'sig2', '2', 'type2', 'part3.c')
    """)
    
    conn.commit()
    conn.close()
    
    # Run calibration
    calibrate.calibrate()
    
    # Assert output files exist
    assert out_file.exists()
    assert Path("policies/proposed_risk_model_weights.yaml").exists()
    
    # Verify bounds of proposed weights
    proposed = yaml.safe_load(Path("policies/proposed_risk_model_weights.yaml").read_text(encoding='utf-8'))
    original = calibrate.DEFAULT_WEIGHTS
    
    for k, val in proposed.items():
        orig_val = original[k]
        # Assert within ±20%
        assert orig_val * 0.8 <= val <= orig_val * 1.2
        
    # Verify json report contents
    report = json.loads(out_file.read_text(encoding='utf-8'))
    assert "metrics" in report
    assert report["metrics"]["total_evaluated_prs"] == 2
    assert report["metrics"]["true_positives"] == 1
    assert report["metrics"]["false_negatives"] == 1
    assert report["metrics"]["precision"] == 1.0
