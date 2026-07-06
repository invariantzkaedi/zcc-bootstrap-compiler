import os
import shutil
from pathlib import Path
import pytest
from scripts.ingest_artifacts import ingest_directory, QUARANTINE_DIR, DB_PATH

@pytest.fixture(autouse=True)
def setup_teardown():
    if DB_PATH.exists():
        DB_PATH.unlink()
    test_dir = Path("temp_test_artifacts_quarantine")
    if test_dir.exists():
        shutil.rmtree(test_dir)
    test_dir.mkdir(parents=True, exist_ok=True)
    if QUARANTINE_DIR.exists():
        shutil.rmtree(QUARANTINE_DIR)
        
    yield test_dir
    
    if DB_PATH.exists():
        DB_PATH.unlink()
    if test_dir.exists():
        shutil.rmtree(test_dir)
    if QUARANTINE_DIR.exists():
        shutil.rmtree(QUARANTINE_DIR)

def test_ingest_quarantine_invalid_json(setup_teardown):
    test_dir = setup_teardown
    
    # Create invalid JSON failure file
    fail_file = test_dir / "failure_99.json"
    fail_file.write_text("this is not json { [", encoding='utf-8')
    
    ingest_directory(str(test_dir))
    
    # Assert file is in quarantine
    quarantined = QUARANTINE_DIR / "failure_99.json"
    assert quarantined.exists()
    
    reason_file = QUARANTINE_DIR / "failure_99.json.reason.txt"
    assert reason_file.exists()
    assert "Expecting value" in reason_file.read_text(encoding='utf-8')

def test_ingest_quarantine_oversized(setup_teardown):
    test_dir = setup_teardown
    
    # Create oversized failure file
    fail_file = test_dir / "failure_large.json"
    # Write >10MB of data
    fail_file.write_bytes(b"a" * (11 * 1024 * 1024))
    
    ingest_directory(str(test_dir))
    
    quarantined = QUARANTINE_DIR / "failure_large.json"
    assert quarantined.exists()
    
    reason_file = QUARANTINE_DIR / "failure_large.json.reason.txt"
    assert reason_file.exists()
    assert "exceeds max size" in reason_file.read_text(encoding='utf-8')
