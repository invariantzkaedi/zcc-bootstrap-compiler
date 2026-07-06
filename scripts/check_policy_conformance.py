#!/usr/bin/env python3
import json
import sqlite3
import sys
from pathlib import Path
import datetime as dt

DB_PATH = Path("artifacts/qec_warehouse.db")
POLICY_PATH = Path("policies/qec_vop_policy.yaml")
DASHBOARD_PATH = Path("artifacts/dashboard_data.json")
OUT_PATH = Path("artifacts/policy_check_report.json")

def load_policy_yaml(path: Path):
    if not path.exists():
        return {}
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
            if v.startswith("[") and v.endswith("]"):
                v = [int(x.strip()) for x in v[1:-1].split(",") if x.strip()]
            elif v.replace(".", "", 1).isdigit():
                v = float(v) if "." in v else int(v)
            elif v.lower() == "true":
                v = True
            elif v.lower() == "false":
                v = False
            
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

def run_conformance_checks():
    policy = load_policy_yaml(POLICY_PATH)
    dash = {}
    if DASHBOARD_PATH.exists():
        try:
            dash = json.loads(DASHBOARD_PATH.read_text(encoding='utf-8'))
        except Exception:
            pass
            
    checks = []
    blocking_failures = 0
    warnings = 0
    
    # Check 1: Policy structure
    has_policy = bool(policy)
    checks.append({
        "check_id": "POLICY_EXISTS",
        "status": "PASS" if has_policy else "FAIL",
        "message": "Policy file load status",
        "evidence": f"Path: {POLICY_PATH}"
    })
    if not has_policy:
        blocking_failures += 1
        
    # Check 2: Threshold alignment in alerts
    if has_policy and dash:
        policy_kill_rate = policy.get("mutation_kill_rate_min", 0.90)
        # Find any LOW_KILL_RATE alert in dashboard alerts
        alerts = dash.get("alerts", [])
        mut_alert = [a for a in alerts if a["code"] == "LOW_KILL_RATE"]
        if mut_alert:
            alert_threshold = mut_alert[0].get("threshold")
            aligned = (alert_threshold == policy_kill_rate)
            checks.append({
                "check_id": "THRESHOLD_ALIGNMENT_KILL_RATE",
                "status": "PASS" if aligned else "FAIL",
                "message": "Mutation kill rate threshold alignment",
                "evidence": f"Policy: {policy_kill_rate}, Dashboard Alert: {alert_threshold}"
            })
            if not aligned:
                blocking_failures += 1
        else:
            checks.append({
                "check_id": "THRESHOLD_ALIGNMENT_KILL_RATE",
                "status": "PASS",
                "message": "Mutation kill rate threshold alignment (No alert active)",
                "evidence": f"Policy: {policy_kill_rate}"
            })
            
    # Check 3: Schema version conformance
    schema_ver = dash.get("schema_version") if dash else None
    if schema_ver:
        checks.append({
            "check_id": "SCHEMA_VERSION_CHECK",
            "status": "PASS" if schema_ver == "1.0.0" else "FAIL",
            "message": "Verify dashboard schema version",
            "evidence": f"Found: {schema_ver}"
        })
        if schema_ver != "1.0.0":
            blocking_failures += 1
            
    status = "pass" if blocking_failures == 0 else "fail"
    
    report = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "policy_version": "1.1.0",
        "status": status,
        "checks": checks,
        "blocking_failures_count": blocking_failures,
        "warnings_count": warnings
    }
    
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps(report, indent=2, sort_keys=True), encoding='utf-8')
    print(f"Generated policy conformance report: {OUT_PATH} (Status: {status})")
    
    if status == "fail":
        print("Conformance check FAILED!")
        sys.exit(1)
        
if __name__ == "__main__":
    run_conformance_checks()
