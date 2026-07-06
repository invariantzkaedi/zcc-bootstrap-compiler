#!/usr/bin/env python3
import json
import sqlite3
import yaml
import sys
import datetime as dt
from pathlib import Path

DB_PATH = Path("artifacts/qec_warehouse.db")
WEIGHTS_PATH = Path("policies/risk_model_weights.yaml")
OUT_PATH = Path("artifacts/risk_model_calibration.json")

DEFAULT_WEIGHTS = {
    "base_risk": 20,
    "fail_multiplier": 5,
    "max_fail_impact": 30,
    "critical_weight": 30,
    "ci_weight": 20
}

def load_weights():
    if not WEIGHTS_PATH.exists():
        WEIGHTS_PATH.parent.mkdir(parents=True, exist_ok=True)
        with open(WEIGHTS_PATH, "w", encoding="utf-8") as f:
            yaml.safe_dump(DEFAULT_WEIGHTS, f)
        return DEFAULT_WEIGHTS
    try:
        with open(WEIGHTS_PATH, "r", encoding="utf-8") as f:
            weights = yaml.safe_load(f)
            if isinstance(weights, dict):
                return {**DEFAULT_WEIGHTS, **weights}
    except Exception:
        pass
    return DEFAULT_WEIGHTS

def clamp(val, min_val, max_val):
    return max(min_val, min(max_val, val))

def calibrate():
    weights = load_weights()
    
    if not DB_PATH.exists():
        print(f"Database not found at {DB_PATH}. Exiting.")
        sys.exit(0)
        
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    
    # Fetch risk scores and match with failures to identify TP, FP, FN, TN
    cur.execute("SELECT * FROM risk_scores")
    scores = cur.fetchall()
    
    tp, fp, fn, tn = 0, 0, 0, 0
    high_tier_total = 0
    high_tier_failures = 0
    
    for row in scores:
        commit_sha = row["commit_sha"]
        score = row["score"]
        
        # Check if any failures are associated with this commit_sha or PR
        cur.execute("""
            SELECT count(*) FROM failures f
            JOIN runs r ON f.run_id = r.run_id
            WHERE r.commit_sha = ?
        """, (commit_sha,))
        fail_count = cur.fetchone()[0]
        
        is_risky = score >= 50
        has_failed = fail_count > 0
        
        if is_risky:
            if has_failed:
                tp += 1
            else:
                fp += 1
        else:
            if has_failed:
                fn += 1
            else:
                tn += 1
                
        if score >= 70:
            high_tier_total += 1
            if has_failed:
                high_tier_failures += 1
                
    precision = tp / (tp + fp) if (tp + fp) > 0 else 1.0
    recall = tp / (tp + fn) if (tp + fn) > 0 else 1.0
    fpr = fp / (fp + tn) if (fp + tn) > 0 else 0.0
    
    # Calculate target weight updates based on false positives and false negatives
    # For example, if we have high false positives, we might want to reduce weights slightly.
    # If we have high false negatives, we might want to increase weights slightly.
    suggested_weights = {}
    adjustments = {}
    
    # Simple calibration adjustments:
    # If precision is low, reduce base_risk and multipliers
    # If recall is low, increase base_risk and multipliers
    factor = 1.0
    if precision < 0.5:
        factor = 0.9
    elif recall < 0.7:
        factor = 1.1
        
    for k, v in weights.items():
        calculated = int(v * factor)
        # Apply strict guardrails: changes capped at max ±20%
        lower_bound = int(v * 0.8)
        upper_bound = int(v * 1.2)
        final_weight = clamp(calculated, lower_bound, upper_bound)
        suggested_weights[k] = final_weight
        adjustments[k] = final_weight - v
        
    # Write updated weights back as proposed weights
    # We do NOT overwrite the main policy weights file automatically to keep it advisory-only.
    # Instead, we produce recommendation output and the proposed yaml file.
    PROPOSED_PATH = Path("policies/proposed_risk_model_weights.yaml")
    with open(PROPOSED_PATH, "w", encoding="utf-8") as f:
        yaml.safe_dump(suggested_weights, f)
        
    report = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "current_weights": weights,
        "suggested_weights": suggested_weights,
        "adjustments": adjustments,
        "metrics": {
            "total_evaluated_prs": len(scores),
            "true_positives": tp,
            "false_positives": fp,
            "false_negatives": fn,
            "true_negatives": tn,
            "precision": round(precision, 3),
            "recall": round(recall, 3),
            "false_positive_rate": round(fpr, 3)
        },
        "recommendation": "Proposed weights generated at policies/proposed_risk_model_weights.yaml"
    }
    
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps(report, indent=2, sort_keys=True), encoding='utf-8')
    print(f"Generated risk model calibration: {OUT_PATH}")
    conn.close()

if __name__ == "__main__":
    calibrate()
