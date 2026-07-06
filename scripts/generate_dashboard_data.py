#!/usr/bin/env python3
import json
import sqlite3
from pathlib import Path

DB_PATH = Path("artifacts/qec_warehouse.db")
OUT_PATH = Path("artifacts/dashboard_data.json")

def generate_report():
    if not DB_PATH.exists():
        return
        
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    
    # Fetch failures
    cur.execute("SELECT * FROM failures ORDER BY timestamp DESC LIMIT 100")
    failures = [dict(r) for r in cur.fetchall()]
    
    # Fetch runs
    cur.execute("SELECT * FROM runs ORDER BY timestamp DESC LIMIT 50")
    runs = [dict(r) for r in cur.fetchall()]
    
    # Stats
    cur.execute("SELECT count(*) as total_fails, count(distinct signature) as unique_sigs FROM failures")
    stats = dict(cur.fetchone())
    
    report = {
        "schema_version": "1.0.0",
        "stats": stats,
        "recent_failures": failures,
        "runs": runs
    }
    
    OUT_PATH.write_text(json.dumps(report, indent=2, sort_keys=True), encoding='utf-8')
    print(f"Generated dashboard data: {OUT_PATH}")
    conn.close()

if __name__ == "__main__":
    generate_report()
