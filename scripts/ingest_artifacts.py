#!/usr/bin/env python3
import json
import sqlite3
import hashlib
import os
import shutil
from pathlib import Path
import datetime as dt

DB_PATH = Path("artifacts/qec_warehouse.db")
QUARANTINE_DIR = Path("artifacts/quarantine")
MAX_FILE_SIZE = 10 * 1024 * 1024 # 10MB

def load_json_strict(path: Path, max_bytes: int = MAX_FILE_SIZE):
    if not path.exists():
        return None
    raw = path.read_bytes()
    if len(raw) > max_bytes:
        raise ValueError(f"{path} exceeds max size {max_bytes} bytes")
    # Strict UTF-8 decoding
    text = raw.decode("utf-8")
    return json.loads(text)

def compute_sha256(path: Path):
    if not path.exists():
        return ""
    h = hashlib.sha256()
    h.update(path.read_bytes())
    return h.hexdigest()

def quarantine_file(path: Path, reason: str):
    QUARANTINE_DIR.mkdir(parents=True, exist_ok=True)
    dest = QUARANTINE_DIR / path.name
    shutil.copy2(path, dest)
    log_path = QUARANTINE_DIR / f"{path.name}.reason.txt"
    log_path.write_text(reason, encoding='utf-8')
    print(f"Quarantined: {path} -> {dest} (Reason: {reason})")

def init_db():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    
    # 1. Meta table
    cur.execute("""
    CREATE TABLE IF NOT EXISTS meta (
        key TEXT PRIMARY KEY,
        value TEXT NOT NULL
    )
    """)
    
    # Set schema version
    cur.execute("INSERT OR REPLACE INTO meta (key, value) VALUES ('schema_version', '1.0.0')")
    
    # 2. Runs table
    cur.execute("""
    CREATE TABLE IF NOT EXISTS runs (
        run_id TEXT PRIMARY KEY,
        workflow_name TEXT NOT NULL,
        branch TEXT,
        commit_sha TEXT,
        status TEXT NOT NULL,
        started_at TEXT,
        ended_at TEXT,
        created_at TEXT NOT NULL DEFAULT (datetime('now'))
    )
    """)
    
    # 3. Failures table
    cur.execute("""
    CREATE TABLE IF NOT EXISTS failures (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        run_id TEXT NOT NULL,
        signature TEXT NOT NULL,
        seed TEXT,
        failure_type TEXT,
        n_qubits INTEGER,
        minimized_ratio REAL,
        artifact_path TEXT,
        created_at TEXT NOT NULL DEFAULT (datetime('now')),
        UNIQUE(run_id, signature, seed),
        FOREIGN KEY(run_id) REFERENCES runs(run_id) ON DELETE CASCADE
    )
    """)
    
    # 4. Determinism table
    cur.execute("""
    CREATE TABLE IF NOT EXISTS determinism (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        run_id TEXT NOT NULL,
        unique_hashes INTEGER NOT NULL,
        hash_set TEXT NOT NULL,
        pass_fail TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT (datetime('now')),
        UNIQUE(run_id),
        FOREIGN KEY(run_id) REFERENCES runs(run_id) ON DELETE CASCADE
    )
    """)
    
    # 5. Mutation table
    cur.execute("""
    CREATE TABLE IF NOT EXISTS mutation (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        run_id TEXT NOT NULL,
        total_mutants INTEGER,
        killed INTEGER,
        survived INTEGER,
        kill_rate REAL,
        pass_fail TEXT,
        created_at TEXT NOT NULL DEFAULT (datetime('now')),
        UNIQUE(run_id),
        FOREIGN KEY(run_id) REFERENCES runs(run_id) ON DELETE CASCADE
    )
    """)
    
    # 6. Incidents table
    cur.execute("""
    CREATE TABLE IF NOT EXISTS incidents (
        signature TEXT PRIMARY KEY,
        issue_url TEXT NOT NULL,
        issue_number INTEGER,
        status TEXT NOT NULL,
        title TEXT,
        owner TEXT,
        opened_at TEXT,
        closed_at TEXT,
        updated_at TEXT
    )
    """)
    
    # 7. Risk scores table
    cur.execute("""
    CREATE TABLE IF NOT EXISTS risk_scores (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        pr_number INTEGER NOT NULL,
        commit_sha TEXT,
        score INTEGER NOT NULL,
        level TEXT NOT NULL,
        reasons_json TEXT NOT NULL,
        suggested_reviewers_json TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT (datetime('now')),
        UNIQUE(pr_number, commit_sha)
    )
    """)
    
    # Indexes
    cur.execute("CREATE INDEX IF NOT EXISTS idx_failures_sig ON failures(signature)")
    cur.execute("CREATE INDEX IF NOT EXISTS idx_failures_run ON failures(run_id)")
    cur.execute("CREATE INDEX IF NOT EXISTS idx_failures_date ON failures(created_at)")
    cur.execute("CREATE INDEX IF NOT EXISTS idx_mutation_run ON mutation(run_id)")
    cur.execute("CREATE INDEX IF NOT EXISTS idx_det_run ON determinism(run_id)")
    cur.execute("CREATE INDEX IF NOT EXISTS idx_incidents_status ON incidents(status)")
    cur.execute("CREATE INDEX IF NOT EXISTS idx_incidents_updated ON incidents(updated_at)")
    
    conn.commit()
    conn.close()

def ingest_directory(dir_path="artifacts"):
    p = Path(dir_path)
    if not p.exists():
        return
        
    init_db()
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    
    # Fetch run info from env
    run_id = os.environ.get("GITHUB_RUN_ID", "run_" + dt.datetime.now().strftime("%Y%m%d%H%M%S"))
    workflow = os.environ.get("GITHUB_WORKFLOW", "local_verify")
    branch = os.environ.get("GITHUB_REF_NAME", "main")
    commit_sha = os.environ.get("GITHUB_SHA", "dev")
    
    # Insert or update run
    cur.execute("""
    INSERT INTO runs (run_id, workflow_name, branch, commit_sha, status, started_at, ended_at)
    VALUES (?, ?, ?, ?, ?, ?, ?)
    ON CONFLICT(run_id) DO UPDATE SET
      workflow_name=excluded.workflow_name,
      branch=excluded.branch,
      commit_sha=excluded.commit_sha,
      status=excluded.status,
      started_at=excluded.started_at,
      ended_at=excluded.ended_at
    """, (
        run_id, workflow, branch, commit_sha, "running",
        dt.datetime.now(dt.timezone.utc).isoformat(),
        dt.datetime.now(dt.timezone.utc).isoformat()
    ))
    
    # Ingest failures
    for f in p.glob("failure_*.json"):
        try:
            data = load_json_strict(f)
            if not data:
                continue
            
            # Compute sha256 and store file info in meta
            sha = compute_sha256(f)
            cur.execute("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)", (f"file_hash_{f.name}", sha))
            
            # Calculate minimized ratio
            orig = data.get("original_length", 0)
            mini = data.get("minimized_length", 0)
            ratio = (mini / orig) if orig > 0 else 1.0
            
            cur.execute("""
            INSERT INTO failures (run_id, signature, seed, failure_type, n_qubits, minimized_ratio, artifact_path)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(run_id, signature, seed) DO UPDATE SET
              failure_type=excluded.failure_type,
              n_qubits=excluded.n_qubits,
              minimized_ratio=excluded.minimized_ratio,
              artifact_path=excluded.artifact_path
            """, (
                run_id,
                data.get("signature", "unknown"),
                str(data.get("seed", 0)),
                data.get("failure_type", "unknown"),
                data.get("n_qubits", 0),
                ratio,
                str(f)
            ))
        except Exception as e:
            quarantine_file(f, str(e))
            
    # Ingest determinism
    det_file = p / "determinism_hashes.json"
    if det_file.exists():
        try:
            data = load_json_strict(det_file)
            if data:
                sha = compute_sha256(det_file)
                cur.execute("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)", (f"file_hash_{det_file.name}", sha))
                
                hashes = data.get("hashes", [])
                uniq = len(set(hashes))
                status = "PASS" if uniq == 1 else "FAIL"
                
                cur.execute("""
                INSERT INTO determinism (run_id, unique_hashes, hash_set, pass_fail)
                VALUES (?, ?, ?, ?)
                ON CONFLICT(run_id) DO UPDATE SET
                  unique_hashes=excluded.unique_hashes,
                  hash_set=excluded.hash_set,
                  pass_fail=excluded.pass_fail
                """, (run_id, uniq, json.dumps(hashes), status))
        except Exception as e:
            quarantine_file(det_file, str(e))
            
    # Ingest mutation
    mut_file = p / "mutation_report.json"
    if mut_file.exists():
        try:
            data = load_json_strict(mut_file)
            if data:
                sha = compute_sha256(mut_file)
                cur.execute("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)", (f"file_hash_{mut_file.name}", sha))
                
                total = data.get("total_mutants", 0)
                killed = data.get("killed", 0)
                survived = data.get("survived", 0)
                kr = data.get("kill_rate", 0.0)
                status = "PASS" if kr >= 0.90 else "FAIL"
                
                cur.execute("""
                INSERT INTO mutation (run_id, total_mutants, killed, survived, kill_rate, pass_fail)
                VALUES (?, ?, ?, ?, ?, ?)
                ON CONFLICT(run_id) DO UPDATE SET
                  total_mutants=excluded.total_mutants,
                  killed=excluded.killed,
                  survived=excluded.survived,
                  kill_rate=excluded.kill_rate,
                  pass_fail=excluded.pass_fail
                """, (run_id, total, killed, survived, kr, status))
        except Exception as e:
            quarantine_file(mut_file, str(e))
            
    # Update run status to success on clean completion
    cur.execute("UPDATE runs SET status='success' WHERE run_id=?", (run_id,))
    
    conn.commit()
    conn.close()

if __name__ == "__main__":
    init_db()
    ingest_directory("artifacts")
    print(f"Ingested artifacts into {DB_PATH}")
