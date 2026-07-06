#!/usr/bin/env python3
import json
import datetime as dt
from pathlib import Path

DASHBOARD_PATH = Path("artifacts/dashboard_data.json")
ANOMALY_PATH = Path("artifacts/anomaly_report.json")
FORECAST_PATH = Path("artifacts/forecast_report.json")
POLICY_PATH = Path("policies/qec_vop_policy.yaml")
OUT_PATH = Path("artifacts/remediation_plan.json")

def load_json_safe(path: Path):
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except Exception:
        return {}

def suggest_remediations():
    dash = load_json_safe(DASHBOARD_PATH)
    anom = load_json_safe(ANOMALY_PATH)
    fore = load_json_safe(FORECAST_PATH)
    
    recs = []
    
    # 1. Determinism Drift
    drifts = dash.get("windows", {}).get("days_7", {}).get("determinism_drifts", 0)
    if drifts > 0:
        recs.append({
            "id": "REC-001",
            "source_type": "alert",
            "source_code": "DETERMINISM_DRIFT",
            "severity": "critical",
            "priority_score": 95,
            "title": "Mitigate Determinism Drift in Fuzz Verification",
            "rationale": f"Observed {drifts} non-deterministic stabilizer outcomes in latest 7-day window.",
            "suggested_actions": [
                "Locate the failing seed and signature in artifacts/index.json.",
                "Execute 'bash artifacts/repro_<seed>.sh' to confirm local reproducibility.",
                "Compare the stabilizer frame propagation rules for CNOT/CZ gates against Tableau reference."
            ],
            "owner_hint": "codeowners:tests/test_stabilizer_fuzz.py",
            "links": ["docs/qec_vop_alerting.md#determinism-drift"]
        })
        
    # 2. Low Kill Rate
    kr = dash.get("windows", {}).get("days_14", {}).get("kill_rate_avg")
    if kr is not None and kr < 0.90:
        recs.append({
            "id": "REC-002",
            "source_type": "alert",
            "source_code": "LOW_KILL_RATE",
            "severity": "high",
            "priority_score": 85,
            "title": "Reinforce Mutation Assertions for Survivor Hotspots",
            "rationale": f"Average 14-day kill rate is {kr:.2f}, dropping below required 0.90 threshold.",
            "suggested_actions": [
                "Retrieve mutation_report.json to list survived mutants.",
                "Verify which mutation operators (gate deletion/operand swaps) are escaping tests.",
                "Add targeted assertion sweeps to tests/test_mutation_smoke.py."
            ],
            "owner_hint": "codeowners:tests/test_mutation_smoke.py",
            "links": ["docs/qec_vop_alerting.md#low-kill-rate"]
        })
        
    # 3. Anomaly Alerts
    anomalies = anom.get("anomalies", [])
    for a in anomalies:
        if a["metric"] == "signature_volume_spike":
            recs.append({
                "id": "REC-003",
                "source_type": "anomaly",
                "source_code": "SIGNATURE_SPIKE",
                "severity": a["severity"],
                "priority_score": 75 if a["severity"] == "critical" else 60,
                "title": "Analyze Unique Failure Signature Spike",
                "rationale": f"Signature count is {a['current']} (Z-Score: {a['zscore']:.2f}).",
                "suggested_actions": [
                    "Check recent commit histories for algebraic changes in compiler drivers.",
                    "Verify if new syndromes are introduced without golden updates."
                ],
                "owner_hint": "qa",
                "links": ["docs/qec_vop_alerting.md#signature-volume-spike"]
            })
            
    # 4. Forecast Alerts
    forecasts = fore.get("forecasts", [])
    for f in forecasts:
        prob = f.get("breach_probability")
        if prob is not None and prob >= 0.50:
            recs.append({
                "id": "REC-004",
                "source_type": "forecast",
                "source_code": "HIGH_BREACH_PROBABILITY",
                "severity": "high",
                "priority_score": int(prob * 100),
                "title": f"Preempt Breach of {f['metric']}",
                "rationale": f"Forecast projects 7-day breach probability of {prob*100:.1f}%.",
                "suggested_actions": [
                    "Increase execution frequency of required preflight checks.",
                    "Verify code coverage stats on targeted files."
                ],
                "owner_hint": "infra",
                "links": ["docs/qec_vop_alerting.md#breach-probability-interpretation"]
            })
            
    # Sort by priority
    recs.sort(key=lambda x: x["priority_score"], reverse=True)
    
    criticals = sum(1 for r in recs if r["severity"] == "critical")
    highs = sum(1 for r in recs if r["severity"] == "high")
    meds = sum(1 for r in recs if r["severity"] == "medium")
    
    plan = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "policy_version": "1.1.0",
        "advisory_mode": True,
        "recommendations": recs,
        "summary": {
            "critical_count": criticals,
            "high_count": highs,
            "medium_count": meds,
            "top_recommendation_id": recs[0]["id"] if recs else None
        }
    }
    
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps(plan, indent=2, sort_keys=True), encoding='utf-8')
    print(f"Generated remediation plan: {OUT_PATH}")

if __name__ == "__main__":
    suggest_remediations()
