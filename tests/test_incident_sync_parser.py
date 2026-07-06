import sqlite3
import json
from pathlib import Path
import pytest
import scripts.sync_incidents
from scripts.sync_incidents import extract_signature, sync_incidents, DB_PATH
from scripts.ingest_artifacts import init_db

@pytest.fixture(autouse=True)
def setup_teardown():
    if DB_PATH.exists():
        DB_PATH.unlink()
    yield
    if DB_PATH.exists():
        DB_PATH.unlink()

def test_extract_signature():
    title = "[QEC Incident] a1b2c3d4e5f67890abcdef1234567890abcdef12 error"
    body = "Signature: a1b2c3d4e5f67890abcdef1234567890abcdef12"
    
    # Extract from title
    sig = extract_signature(title, body)
    assert sig == "a1b2c3d4e5f67890abcdef1234567890abcdef12"
    
    # Extract from body only
    sig = extract_signature("Unmatching title", body)
    assert sig == "a1b2c3d4e5f67890abcdef1234567890abcdef12"

def test_sync_incidents_dry_run(monkeypatch):
    init_db()
    
    # Mock fetch_issues to return simulated issue
    mock_issue = {
        "number": 101,
        "title": "[QEC Incident] a1b2c3d4e5f67890abcdef1234567890abcdef12 math mismatch",
        "html_url": "https://github.com/izkaedi-ui/ZCC/issues/101",
        "state": "open",
        "assignee": {"login": "izkaedi-ui"},
        "created_at": "2026-07-06T00:00:00Z",
        "closed_at": None,
        "updated_at": "2026-07-06T01:00:00Z",
        "body": "Failure signature: a1b2c3d4e5f67890abcdef1234567890abcdef12"
    }
    monkeypatch.setattr(scripts.sync_incidents, "fetch_issues", lambda token=None: [mock_issue])
    
    # Ingest in dry run (should write nothing to DB)
    sync_incidents(dry_run=True)
    
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("SELECT count(*) FROM incidents")
    assert cur.fetchone()[0] == 0
    conn.close()

def test_sync_incidents_live(monkeypatch):
    init_db()
    
    mock_issue = {
        "number": 101,
        "title": "[QEC Incident] a1b2c3d4e5f67890abcdef1234567890abcdef12 math mismatch",
        "html_url": "https://github.com/izkaedi-ui/ZCC/issues/101",
        "state": "open",
        "assignee": {"login": "izkaedi-ui"},
        "created_at": "2026-07-06T00:00:00Z",
        "closed_at": None,
        "updated_at": "2026-07-06T01:00:00Z",
        "body": "Failure signature: a1b2c3d4e5f67890abcdef1234567890abcdef12"
    }
    monkeypatch.setattr(scripts.sync_incidents, "fetch_issues", lambda token=None: [mock_issue])
    
    sync_incidents(dry_run=False)
    
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("SELECT count(*) FROM incidents")
    assert cur.fetchone()[0] == 1
    
    cur.execute("SELECT signature, status, issue_number FROM incidents")
    row = cur.fetchone()
    assert row[0] == "a1b2c3d4e5f67890abcdef1234567890abcdef12"
    assert row[1] == "open"
    assert row[2] == 101
    
    conn.close()
