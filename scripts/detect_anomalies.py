#!/usr/bin/env python3
import json
import sqlite3
import math
from pathlib import Path
import datetime as dt

DB_PATH = Path("artifacts/qec_warehouse.db")
POLICY_PATH = Path("policies/qec_vop_policy.yaml")
OUT_PATH = Path("artifacts/anomaly_report.json")

def load_policy_yaml(path: Path):
    if not path.exists():
        raise FileNotFoundError(f"Policy file not found: {path}")
    policy = {}
    current_section = None
    
    for line in path.read_text(encoding='utf-8').splitlines():
        raw_line = line
        line = line.strip()
        if not line or line.startswith("#"):
            continue
            
        if ":" in line:
            parts = line.split(":", 1)
            k = parts[0].strip()
            v = parts[1].strip()
            
            # Simple list parser
            if v.startswith("[") and v.endswith("]"):
                v = [int(x.strip()) for x in v[1:-1].split(",") if x.strip()]
            elif v.replace(".", "", 1).isdigit():
                v = float(v) if "." in v else int(v)
            elif v.lower() == "true":
                v = True
            elif v.lower() == "false":
                v = False
            
            # Check indentation to determine section nesting
            is_indented = raw_line.startswith("  ") or raw_line.startswith("\t")
            if is_indented and current_section:
                policy[current_section][k] = v
            else:
                if not v and v != 0:
                    current_section = k
                    policy[k] = {}
                else:
                    current_section = None
                    policy[k] = v
    return policy

def calculate_zscore(values, current):
    if not values or len(values) < 3:
        return 0.0, 1.0 # default low deviation/confidence
    n = len(values)
    mean = sum(values) / n
    variance = sum((x - mean) ** 2 for x in values) / (n - 1)
    std_dev = math.sqrt(variance)
    
    # Handle flat baseline histories
    if std_dev == 0.0:
        std_dev = 1e-5
        
    z = (current - mean) / std_dev
    return z, mean

def detect_anomalies():
    policy = load_policy_yaml(POLICY_PATH)
    anomaly_cfg = policy.get("anomaly", {})
    z_warn = anomaly_cfg.get("zscore_warn", 2.0)
    z_crit = anomaly_cfg.get("zscore_critical", 3.0)
    min_pts = anomaly_cfg.get("min_points", 7)
    
    if not DB_PATH.exists():
        print("Database not found. Initializing empty report.")
        report = {
            "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "policy_version": "1.1.0",
            "anomalies": [],
            "summary": {"critical": 0, "warning": 0}
        }
        OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
        OUT_PATH.write_text(json.dumps(report, indent=2, sort_keys=True), encoding='utf-8')
        return
        
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    
    anomalies = []
    
    # 1. Determinism Drifts
    cur.execute("SELECT unique_hashes FROM determinism ORDER BY id DESC LIMIT 30")
    drift_values = [r[0] for r in cur.fetchall()]
    if len(drift_values) >= min_pts:
        current = drift_values[0]
        history = drift_values[1:]
        z, mean = calculate_zscore(history, current)
        if abs(z) >= z_warn:
            severity = "critical" if abs(z) >= z_crit else "warning"
            anomalies.append({
                "metric": "determinism_drifts",
                "window_days": 30,
                "baseline": mean,
                "current": float(current),
                "deviation": float(current - mean),
                "zscore": float(z),
                "severity": severity,
                "confidence": 0.90,
                "suggested_action": "Freeze merge pipeline and inspect latest execution trace mismatch."
            })
            
    # 2. Mutation Kill Rate
    cur.execute("SELECT kill_rate FROM mutation ORDER BY id DESC LIMIT 30")
    kill_rates = [r[0] for r in cur.fetchall() if r[0] is not None]
    if len(kill_rates) >= min_pts:
        current = kill_rates[0]
        history = kill_rates[1:]
        z, mean = calculate_zscore(history, current)
        # For kill rate, a drop is bad (negative z-score)
        if z <= -z_warn:
            severity = "critical" if abs(z) >= z_crit else "warning"
            anomalies.append({
                "metric": "mutation_kill_rate",
                "window_days": 30,
                "baseline": mean,
                "current": float(current),
                "deviation": float(current - mean),
                "zscore": float(z),
                "severity": severity,
                "confidence": 0.85,
                "suggested_action": "Audit survived mutants in mutation_report.json and reinforce assertions."
            })
            
    # 3. Signature Volume Spike
    cur.execute("""
    SELECT count(distinct signature), date(created_at)
    FROM failures
    GROUP BY date(created_at)
    ORDER BY date(created_at) DESC
    LIMIT 30
    """)
    sig_counts = [r[0] for r in cur.fetchall()]
    if len(sig_counts) >= min_pts:
        current = sig_counts[0]
        history = sig_counts[1:]
        z, mean = calculate_zscore(history, current)
        if z >= z_warn:
            severity = "critical" if z >= z_crit else "warning"
            anomalies.append({
                "metric": "signature_volume_spike",
                "window_days": 30,
                "baseline": mean,
                "current": float(current),
                "deviation": float(current - mean),
                "zscore": float(z),
                "severity": severity,
                "confidence": 0.80,
                "suggested_action": "Verify if a recent PR broke parsing rules/convention alignment across stabilizers."
            })
            
    # Summary counts
    crit_count = sum(1 for a in anomalies if a["severity"] == "critical")
    warn_count = sum(1 for a in anomalies if a["severity"] == "warning")
    
    report = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "policy_version": "1.1.0",
        "anomalies": anomalies,
        "summary": {
            "critical": crit_count,
            "warning": warn_count
        }
    }
    
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps(report, indent=2, sort_keys=True), encoding='utf-8')
    print(f"Generated anomaly detection report: {OUT_PATH}")
    conn.close()

if __name__ == "__main__":
    detect_anomalies()
