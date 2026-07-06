import json
import sqlite3
from pathlib import Path
import pytest
from scripts.score_pr_risk import compute_risk, get_reviewers, DB_PATH
from scripts.ingest_artifacts import init_db

@pytest.fixture(autouse=True)
def setup_teardown():
    if DB_PATH.exists():
        DB_PATH.unlink()
    yield
    if DB_PATH.exists():
        DB_PATH.unlink()

def test_compute_risk_baseline():
    score, level, reasons = compute_risk(["docs/README.md"])
    assert score == 20
    assert level == "low"
    assert "baseline risk" in reasons[0]

def test_compute_risk_critical():
    score, level, reasons = compute_risk(["src/zqec.h"])
    assert score == 50 # 20 base + 30 critical
    assert level == "medium"
    assert "critical QEC" in reasons[0]

def test_compute_risk_ci_workflow():
    score, level, reasons = compute_risk([".github/workflows/quantum-tests.yml"])
    assert score == 40 # 20 base + 20 workflow
    assert level == "medium"
    assert "CI workflow" in reasons[0]

def test_get_reviewers():
    codeowners = [
        ("schemas/*", ["@izkaedi-ui"]),
        ("tests/*", ["@tester-owner"]),
    ]
    
    revs = get_reviewers(["schemas/qec_failure_schema.json"], codeowners)
    assert "@izkaedi-ui" in revs
    assert len(revs) == 1
    
    revs = get_reviewers(["tests/test_fuzz.py", "docs/README.md"], codeowners)
    assert "@tester-owner" in revs
    assert len(revs) == 1
