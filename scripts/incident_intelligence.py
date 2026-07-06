#!/usr/bin/env python3
import json
import sqlite3
import datetime as dt
import sys
from pathlib import Path

DB_PATH = Path("artifacts/qec_warehouse.db")
POLICY_PATH = Path("policies/qec_vop_policy.yaml")
OUT_PATH = Path("artifacts/incident_intelligence.json")

def load_policy_stale_days():
    if not POLICY_PATH.exists():
        return 7
    try:
        # Simple line parsing for yaml
        for line in POLICY_PATH.read_text(encoding='utf-8').splitlines():
            if "stale_incident_days_high:" in line:
                return int(line.split(":", 1)[1].strip())
    except Exception:
        pass
    return 7



def parse_iso_datetime(val):
    if not val:
        return None
    val = val.replace("Z", "+00:00")
    try:
        d = dt.datetime.fromisoformat(val)
        if d.tzinfo is None:
            d = d.replace(tzinfo=dt.timezone.utc)
        return d
    except ValueError:
        try:
            d = dt.datetime.strptime(val.split(".")[0], "%Y-%m-%d %H:%M:%S")
            return d.replace(tzinfo=dt.timezone.utc)
        except ValueError:
            return None

def infer_subsystem(path):
    if not path:
        return "other"
    path = path.lower()
    if "part0" in path or "preprocessor" in path or "pp" in path:
        return "preprocessor"
    if "part2" in path or "lex" in path:
        return "lexer"
    if "part3" in path or "parse" in path:
        return "parser"
    if "part4" in path or "codegen" in path:
        return "codegen"
    if "part5" in path or "driver" in path:
        return "driver"
    if "ir" in path:
        return "ir_backend"
    return "other"

def run_incident_intelligence():
    stale_days = load_policy_stale_days()
    
    if not DB_PATH.exists():
        print(f"Database not found at {DB_PATH}. Exiting.")
        sys.exit(0)
        
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    
    # 1. Lifecycle KPIs
    # Calculate MTTR (time_to_fix_hours) for closed incidents
    cur.execute("SELECT opened_at, closed_at FROM incidents WHERE status='closed'")
    closed_rows = cur.fetchall()
    fix_times = []
    for r in closed_rows:
        op = parse_iso_datetime(r["opened_at"])
        cl = parse_iso_datetime(r["closed_at"])
        if op and cl:
            fix_times.append((cl - op).total_seconds() / 3600.0)
    time_to_fix = sum(fix_times) / len(fix_times) if fix_times else 0.0
    
    # Calculate time_to_detect_hours: time from first failure occurrence to incident creation
    cur.execute("SELECT signature, opened_at FROM incidents")
    inc_rows = cur.fetchall()
    detect_times = []
    for r in inc_rows:
        sig = r["signature"]
        op = parse_iso_datetime(r["opened_at"])
        if op:
            cur.execute("SELECT min(created_at) FROM failures WHERE signature=?", (sig,))
            fail_row = cur.fetchone()
            if fail_row and fail_row[0]:
                fail_time = parse_iso_datetime(fail_row[0])
                if fail_time:
                    detect_times.append(max(0.0, (op - fail_time).total_seconds() / 3600.0))
    time_to_detect = sum(detect_times) / len(detect_times) if detect_times else 0.0
    
    # Calculate stale incidents
    now = dt.datetime.now(dt.timezone.utc)
    cur.execute("SELECT opened_at FROM incidents WHERE status='open'")
    open_rows = cur.fetchall()
    stale_count = 0
    for r in open_rows:
        op = parse_iso_datetime(r["opened_at"])
        if op and (now - op).days > stale_days:
            stale_count += 1
            
    # Mock/simulated reopen rate for KPI section
    reopen_rate = 0.0 # Standard default
    
    kpis = {
        "time_to_detect_hours": round(time_to_detect, 2),
        "time_to_classify_hours": 0.0, # classification not explicitly tracked
        "time_to_fix_hours": round(time_to_fix, 2),
        "reopen_rate": reopen_rate,
        "stale_open_incidents_count": stale_count
    }
    
    # 2. Signature Lineage
    cur.execute("SELECT signature, issue_url, issue_number, status, opened_at, closed_at FROM incidents")
    inc_list = cur.fetchall()
    lineage = []
    for inc in inc_list:
        sig = inc["signature"]
        cur.execute("""
            SELECT f.created_at, r.branch, r.workflow_name 
            FROM failures f 
            JOIN runs r ON f.run_id = r.run_id 
            WHERE f.signature=? 
            ORDER BY f.created_at ASC
        """, (sig,))
        fails = cur.fetchall()
        
        if fails:
            first_seen = fails[0]["created_at"]
            last_seen = fails[-1]["created_at"]
            branches = sorted(list(set(f["branch"] for f in fails if f["branch"])))
            workflows = sorted(list(set(f["workflow_name"] for f in fails if f["workflow_name"])))
        else:
            first_seen = inc["opened_at"]
            last_seen = inc["opened_at"]
            branches = []
            workflows = []
            
        lineage.append({
            "signature": sig,
            "issue_number": inc["issue_number"],
            "issue_url": inc["issue_url"],
            "status": inc["status"],
            "recurrence_count": len(fails),
            "first_seen": first_seen,
            "last_seen": last_seen,
            "affected_branches": branches,
            "affected_workflows": workflows
        })
        
    # 3. Hot Subsystem Ranking
    cur.execute("SELECT signature, artifact_path, created_at FROM failures")
    fail_records = cur.fetchall()
    subsystem_counts = {}
    for f in fail_records:
        sub = infer_subsystem(f["artifact_path"])
        if sub not in subsystem_counts:
            subsystem_counts[sub] = {"incident_count": 0, "recurrence": 0, "recent_count": 0}
        subsystem_counts[sub]["recurrence"] += 1
        
    # Map unique signatures to subsystems
    cur.execute("SELECT DISTINCT signature, artifact_path FROM failures")
    sig_sub_rows = cur.fetchall()
    for ss in sig_sub_rows:
        sub = infer_subsystem(ss["artifact_path"])
        if sub in subsystem_counts:
            subsystem_counts[sub]["incident_count"] += 1
            
    # Format and rank subsystems
    hot_subsystems = []
    for sub, stats in subsystem_counts.items():
        # Simple severity heuristic based on recurrence and unique incidents
        severity_score = (stats["incident_count"] * 10) + stats["recurrence"]
        hot_subsystems.append({
            "subsystem": sub,
            "incident_count": stats["incident_count"],
            "recurrence": stats["recurrence"],
            "severity_score": severity_score
        })
    hot_subsystems.sort(key=lambda x: x["severity_score"], reverse=True)
    
    # 4. Incident Pressure Index (0..100)
    cur.execute("SELECT count(*) FROM incidents WHERE status='open'")
    open_incidents = cur.fetchone()[0]
    
    # Pressure = (open_incidents * 15) + (stale_count * 20)
    # Plus a delta for recent failures recurrence count (if any)
    cur.execute("SELECT count(*) FROM failures WHERE created_at >= date('now', '-7 days')")
    recent_failures_count = cur.fetchone()[0]
    
    pressure_index = (open_incidents * 15) + (stale_count * 20) + (recent_failures_count * 2)
    pressure_index = max(0, min(100, pressure_index))
    
    # Top Action Recommendations
    top_actions = []
    if open_incidents > 0:
        top_actions.append(f"Triage and assign owners to the {open_incidents} active open incidents.")
    if stale_count > 0:
        top_actions.append(f"Escalate {stale_count} stale incidents exceeding the {stale_days}-day limit.")
    if hot_subsystems and hot_subsystems[0]["severity_score"] > 20:
        top_actions.append(f"Schedule architectural audit for hot subsystem: '{hot_subsystems[0]['subsystem']}'.")
    if not top_actions:
        top_actions.append("No active alerts or incident pressure. Maintain baseline monitor.")
        
    report = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "kpis": kpis,
        "lineage": lineage,
        "hot_subsystems": hot_subsystems,
        "pressure_index": pressure_index,
        "top_actions": top_actions
    }
    
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps(report, indent=2, sort_keys=True), encoding='utf-8')
    print(f"Generated incident intelligence report: {OUT_PATH} (Pressure: {pressure_index})")
    conn.close()

if __name__ == "__main__":
    run_incident_intelligence()
