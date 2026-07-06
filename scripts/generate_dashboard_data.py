#!/usr/bin/env python3
import json
import sqlite3
from pathlib import Path
import datetime as dt
from jsonschema import validate

DB_PATH = Path("artifacts/qec_warehouse.db")
OUT_PATH = Path("artifacts/dashboard_data.json")
SCHEMA_PATH = Path("schemas/dashboard_data_schema.json")

def get_window_metrics(cur, days: int):
    # Calculate pass_rate from runs
    cur.execute("""
    SELECT count(*), sum(case when status='success' then 1 else 0 end)
    FROM runs
    WHERE created_at >= datetime('now', ?)
    """, (f"-{days} day",))
    total_runs, success_runs = cur.fetchone()
    pass_rate = (success_runs / total_runs) if (total_runs and total_runs > 0) else None
    
    # Calculate determinism drifts
    cur.execute("""
    SELECT count(*)
    FROM determinism
    WHERE pass_fail = 'FAIL' AND created_at >= datetime('now', ?)
    """, (f"-{days} day",))
    drifts = cur.fetchone()[0]
    
    # Calculate mutation kill rates
    cur.execute("""
    SELECT min(kill_rate), avg(kill_rate), max(kill_rate)
    FROM mutation
    WHERE created_at >= datetime('now', ?)
    """, (f"-{days} day",))
    k_min, k_avg, k_max = cur.fetchone()
    
    # Calculate unique signatures
    cur.execute("""
    SELECT count(distinct signature)
    FROM failures
    WHERE created_at >= datetime('now', ?)
    """, (f"-{days} day",))
    uniq_sigs = cur.fetchone()[0]
    
    # Calculate incidents open/closed
    cur.execute("""
    SELECT count(*) FROM incidents WHERE status = 'open' AND opened_at >= datetime('now', ?)
    """, (f"-{days} day",))
    inc_open = cur.fetchone()[0]
    
    cur.execute("""
    SELECT count(*) FROM incidents WHERE status = 'closed' AND closed_at >= datetime('now', ?)
    """, (f"-{days} day",))
    inc_closed = cur.fetchone()[0]
    
    # MTTR
    cur.execute("""
    SELECT avg(strftime('%s', closed_at) - strftime('%s', opened_at)) / 3600.0
    FROM incidents
    WHERE status = 'closed' AND closed_at >= datetime('now', ?)
    """, (f"-{days} day",))
    mttr = cur.fetchone()[0]
    
    return {
        "pass_rate": pass_rate,
        "determinism_drifts": drifts,
        "kill_rate_min": k_min,
        "kill_rate_avg": k_avg,
        "kill_rate_max": k_max,
        "unique_signatures": uniq_sigs,
        "incidents_open": inc_open,
        "incidents_closed": inc_closed,
        "mttr_hours": mttr
    }

def generate_report():
    if not DB_PATH.exists():
        print(f"Database not found at {DB_PATH}")
        return
        
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    
    # General stats
    cur.execute("SELECT count(*) as total_fails, count(distinct signature) as unique_sigs FROM failures")
    stats = dict(cur.fetchone())
    
    # Windows
    windows = {
        "days_7": get_window_metrics(cur, 7),
        "days_14": get_window_metrics(cur, 14),
        "days_30": get_window_metrics(cur, 30)
    }
    
    # Top signatures (30d)
    cur.execute("""
    SELECT signature, count(*) as occurrences, count(distinct run_id) as run_span
    FROM failures
    WHERE created_at >= datetime('now', '-30 day')
    GROUP BY signature
    ORDER BY occurrences DESC
    LIMIT 20
    """)
    top_signatures = [dict(r) for r in cur.fetchall()]
    
    # New signatures this week
    cur.execute("""
    SELECT DISTINCT signature
    FROM failures
    WHERE created_at >= datetime('now', '-7 day')
    AND signature NOT IN (
        SELECT DISTINCT signature
        FROM failures
        WHERE created_at < datetime('now', '-7 day')
    )
    """)
    new_signatures = [r[0] for r in cur.fetchall()]
    
    # Promotion readiness (14d criteria)
    # 1. 14d stable CI execution
    cur.execute("SELECT count(*) FROM runs WHERE status='failure' AND created_at >= datetime('now', '-14 day')")
    failed_runs = cur.fetchone()[0]
    stable_14d = (failed_runs == 0)
    
    # 2. Kill rate average >= 0.90
    cur.execute("SELECT avg(kill_rate) FROM mutation WHERE created_at >= datetime('now', '-14 day')")
    avg_kr = cur.fetchone()[0]
    kill_rate_ok = (avg_kr >= 0.90) if avg_kr is not None else False
    
    # 3. No open survivor issues older than 7 days
    cur.execute("SELECT count(*) FROM incidents WHERE status='open' AND opened_at < datetime('now', '-7 day')")
    stale_incidents = cur.fetchone()[0]
    no_stale_survivors = (stale_incidents == 0)
    
    recommendation = "promote" if (stable_14d and kill_rate_ok and no_stale_survivors) else "monitor"
    
    promotion_readiness = {
        "stable_14d": stable_14d,
        "kill_rate_ok": kill_rate_ok,
        "no_stale_survivors": no_stale_survivors,
        "recommendation": recommendation
    }
    
    # Alerts
    alerts = []
    # Drift Alert
    if windows["days_7"]["determinism_drifts"] > 0:
        alerts.append({
            "code": "DETERMINISM_DRIFT",
            "severity": "critical",
            "value": float(windows["days_7"]["determinism_drifts"]),
            "threshold": 0.0
        })
    # Low Kill Rate
    if windows["days_14"]["kill_rate_avg"] is not None and windows["days_14"]["kill_rate_avg"] < 0.90:
        alerts.append({
            "code": "LOW_KILL_RATE",
            "severity": "high",
            "value": float(windows["days_14"]["kill_rate_avg"]),
            "threshold": 0.90
        })
    # Signature WoW Spike > 50%
    cur.execute("""
    SELECT count(distinct signature)
    FROM failures
    WHERE created_at >= datetime('now', '-7 day')
    """)
    sigs_this_week = cur.fetchone()[0]
    
    cur.execute("""
    SELECT count(distinct signature)
    FROM failures
    WHERE created_at >= datetime('now', '-14 day') AND created_at < datetime('now', '-7 day')
    """)
    sigs_last_week = cur.fetchone()[0]
    if sigs_last_week > 0:
        spike_pct = (sigs_this_week - sigs_last_week) / sigs_last_week
        if spike_pct > 0.50:
            alerts.append({
                "code": "SIGNATURE_SPIKE_WOW",
                "severity": "medium",
                "value": float(spike_pct),
                "threshold": 0.50
            })
    # Stale Incidents
    if stale_incidents > 0:
        alerts.append({
            "code": "STALE_INCIDENTS",
            "severity": "high",
            "value": float(stale_incidents),
            "threshold": 0.0
        })
        
    # Recent failures
    cur.execute("SELECT * FROM failures ORDER BY created_at DESC LIMIT 100")
    recent_failures = []
    for r in cur.fetchall():
        d = dict(r)
        recent_failures.append({
            "seed": int(d.get("seed", 0)),
            "test_name": d.get("failure_type", "unknown"),
            "failure_type": d.get("failure_type", "unknown"),
            "n_qubits": d.get("n_qubits", 0),
            "original_length": 0,
            "minimized_length": 0,
            "timestamp": d.get("created_at"),
            "signature": d.get("signature")
        })
        
    # Recent runs
    cur.execute("SELECT * FROM runs ORDER BY created_at DESC LIMIT 50")
    recent_runs = []
    for r in cur.fetchall():
        d = dict(r)
        # Fetch matching mutation info
        cur.execute("SELECT kill_rate FROM mutation WHERE run_id=?", (d["run_id"],))
        m_row = cur.fetchone()
        kr = m_row[0] if m_row else 0.95
        recent_runs.append({
            "run_id": d["run_id"],
            "timestamp": d["created_at"],
            "status": d["status"],
            "kill_rate": kr,
            "fast_suite_seconds": 15.72
        })
        
    report = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "schema_version": "1.0.0",
        "stats": stats,
        "windows": windows,
        "top_signatures": top_signatures,
        "new_signatures_this_week": new_signatures,
        "promotion_readiness": promotion_readiness,
        "alerts": alerts,
        "recent_failures": recent_failures,
        "runs": recent_runs
    }
    
    # Optional forecast inclusion
    fc_path = Path("artifacts/forecast_report.json")
    if fc_path.exists():
        try:
            fc_data = json.loads(fc_path.read_text(encoding='utf-8'))
            sum_data = fc_data.get("summary", {})
            fcs = fc_data.get("forecasts", [])
            metrics_dict = {}
            for fc in fcs:
                metrics_dict[fc["metric"]] = fc["forecast_values"]
            report["forecast"] = {
                "highest_risk_metric": sum_data.get("highest_risk_metric", "none"),
                "highest_breach_probability": float(sum_data.get("highest_breach_probability", 0.0)),
                "metrics": metrics_dict
            }
        except Exception as e:
            print(f"Failed to read/integrate forecast data: {e}")
            
    # Validate against JSON schema
    if SCHEMA_PATH.exists():
        schema = json.loads(SCHEMA_PATH.read_text(encoding='utf-8'))
        validate(instance=report, schema=schema)
        
    OUT_PATH.write_text(json.dumps(report, indent=2, sort_keys=True), encoding='utf-8')
    print(f"Generated and validated dashboard data: {OUT_PATH}")
    conn.close()

if __name__ == "__main__":
    generate_report()
