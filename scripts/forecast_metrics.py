#!/usr/bin/env python3
import json
import sqlite3
import math
from pathlib import Path
import datetime as dt

DB_PATH = Path("artifacts/qec_warehouse.db")
POLICY_PATH = Path("policies/qec_vop_policy.yaml")
OUT_PATH = Path("artifacts/forecast_report.json")

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

def compute_ewma_and_linear(values, alpha=0.3):
    if not values:
        return [0.0] * 7, "fallback", 0.0
    n = len(values)
    if n < 3:
        # Simple fallback to average
        avg = sum(values) / n
        return [avg] * 7, "fallback", 0.5
        
    # EWMA
    ewma = values[0]
    for val in values[1:]:
        ewma = alpha * val + (1 - alpha) * ewma
        
    # Linear Trend
    # x: [0, 1, ..., n-1]
    # y: values
    x_mean = (n - 1) / 2.0
    y_mean = sum(values) / n
    num = sum((i - x_mean) * (values[i] - y_mean) for i in range(n))
    den = sum((i - x_mean) ** 2 for i in range(n))
    slope = (num / den) if den != 0 else 0.0
    
    # Project 7 days
    forecast_values = []
    current_val = ewma
    for step in range(1, 8):
        projected = current_val + slope * step
        forecast_values.append(projected)
        
    return forecast_values, "ewma+linear", 0.85

def calculate_breach_probability(forecast_values, threshold, historical_values, lower_is_breach=True):
    if not historical_values or len(historical_values) < 3:
        return 0.0
        
    n = len(historical_values)
    mean = sum(historical_values) / n
    variance = sum((x - mean) ** 2 for x in historical_values) / (n - 1)
    std_dev = math.sqrt(variance)
    if std_dev == 0.0:
        std_dev = 1e-5
        
    # Estimate the maximum probability of breach over the 7 days horizon
    max_prob = 0.0
    for val in forecast_values:
        # z-score to threshold boundary
        z = (val - threshold) / std_dev
        # Simple normal CDF approximation or distance-based probability mapping
        if lower_is_breach:
            # Breach if value <= threshold, i.e., val is low
            # If val is far above threshold, z is positive. Probability of breach is low.
            # If val is at threshold, z = 0, probability = 0.5
            p = 0.5 * (1.0 - math.erf(z / math.sqrt(2.0)))
        else:
            # Breach if value >= threshold, i.e., val is high
            p = 0.5 * (1.0 + math.erf(z / math.sqrt(2.0)))
        max_prob = max(max_prob, p)
        
    return max(0.0, min(1.0, max_prob))

def generate_forecasts():
    policy = load_policy_yaml(POLICY_PATH)
    mut_threshold = policy.get("mutation_kill_rate_min", 0.90)
    
    # Initialize default lists
    pass_rates = []
    kill_rates = []
    incident_counts = []
    sig_volumes = []
    
    if DB_PATH.exists():
        conn = sqlite3.connect(DB_PATH)
        cur = conn.cursor()
        
        # 1. Pass Rate history
        cur.execute("""
        SELECT count(*), sum(case when status='success' then 1 else 0 end), date(created_at)
        FROM runs
        GROUP BY date(created_at)
        ORDER BY date(created_at) DESC
        LIMIT 30
        """)
        pass_rates = [(r[1]/r[0]) if r[0] > 0 else 1.0 for r in cur.fetchall()]
        pass_rates.reverse()
        
        # 2. Mutation Kill Rate history
        cur.execute("SELECT kill_rate FROM mutation ORDER BY id DESC LIMIT 30")
        kill_rates = [r[0] for r in cur.fetchall() if r[0] is not None]
        kill_rates.reverse()
        
        # 3. Incident Backlog history
        cur.execute("""
        SELECT count(*), date(opened_at)
        FROM incidents
        WHERE status='open'
        GROUP BY date(opened_at)
        ORDER BY date(opened_at) DESC
        LIMIT 30
        """)
        incident_counts = [r[0] for r in cur.fetchall()]
        incident_counts.reverse()
        
        # 4. Signature Volume history
        cur.execute("""
        SELECT count(distinct signature), date(created_at)
        FROM failures
        GROUP BY date(created_at)
        ORDER BY date(created_at) DESC
        LIMIT 30
        """)
        sig_volumes = [r[0] for r in cur.fetchall()]
        sig_volumes.reverse()
        conn.close()
        
    # Execute forecasts
    fc_pass, model_pass, conf_pass = compute_ewma_and_linear(pass_rates)
    fc_kill, model_kill, conf_kill = compute_ewma_and_linear(kill_rates)
    fc_inc, model_inc, conf_inc = compute_ewma_and_linear(incident_counts)
    fc_sig, model_sig, conf_sig = compute_ewma_and_linear(sig_volumes)
    
    # Calculate probabilities
    prob_pass = calculate_breach_probability(fc_pass, 0.95, pass_rates, lower_is_breach=True)
    prob_kill = calculate_breach_probability(fc_kill, mut_threshold, kill_rates, lower_is_breach=True)
    
    # Setup results
    forecasts = [
        {
            "metric": "pass_rate",
            "model_used": model_pass,
            "recent_window_days": len(pass_rates),
            "forecast_values": [float(round(x, 4)) for x in fc_pass],
            "confidence": float(conf_pass),
            "breach_threshold": 0.95,
            "breach_probability": float(round(prob_pass, 4)),
            "breach_condition": "pass_rate < 0.95",
            "notes": "Forecast based on historical post-merge and nightly CI runs."
        },
        {
            "metric": "mutation_kill_rate",
            "model_used": model_kill,
            "recent_window_days": len(kill_rates),
            "forecast_values": [float(round(x, 4)) for x in fc_kill],
            "confidence": float(conf_kill),
            "breach_threshold": float(mut_threshold),
            "breach_probability": float(round(prob_kill, 4)),
            "breach_condition": f"kill_rate < {mut_threshold}",
            "notes": "Evaluates probability of mutation survivors slipping past gates."
        },
        {
            "metric": "open_incident_backlog",
            "model_used": model_inc,
            "recent_window_days": len(incident_counts),
            "forecast_values": [float(round(x, 2)) for x in fc_inc],
            "confidence": float(conf_inc),
            "breach_threshold": None,
            "breach_probability": None,
            "breach_condition": None,
            "notes": "Tracks growth/stabilization of incident tracking backlog."
        },
        {
            "metric": "signature_volume",
            "model_used": model_sig,
            "recent_window_days": len(sig_volumes),
            "forecast_values": [float(round(x, 2)) for x in fc_sig],
            "confidence": float(conf_sig),
            "breach_threshold": None,
            "breach_probability": None,
            "breach_condition": None,
            "notes": "Forecasts number of distinct failure signatures observed."
        }
    ]
    
    # Summarize highest risk
    highest_prob = 0.0
    highest_metric = "none"
    med_or_higher = 0
    
    for f in forecasts:
        p = f.get("breach_probability")
        if p is not None:
            if p >= 0.20:
                med_or_higher += 1
            if p > highest_prob:
                highest_prob = p
                highest_metric = f["metric"]
                
    report = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "policy_version": "1.1.0",
        "horizon_days": 7,
        "forecasts": forecasts,
        "summary": {
            "highest_risk_metric": highest_metric,
            "highest_breach_probability": float(round(highest_prob, 4)),
            "medium_or_higher_risks_count": med_or_higher
        }
    }
    
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps(report, indent=2, sort_keys=True), encoding='utf-8')
    print(f"Generated forecast report: {OUT_PATH}")

if __name__ == "__main__":
    generate_forecasts()
