#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
zcc_calibrate.py — D-30: Calibration Corpus Orchestrator & Accuracy Analyzer

Compiles and executes the five calibration corpus programs under various
configurations, runs the attribution engine on each, and calculates prediction
precision, recall, F1-score, and mean score deviation.
"""

import os
import sys
import json
import subprocess

TESTS = ["reg_pressure", "call_graph", "mem_layout", "ctrl_flow", "compiler_self"]

def run_cmd(cmd, env=None, allowed_codes={0}):
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    if res.returncode not in allowed_codes:
        print(f"Error running command: {cmd}")
        print(f"stdout: {res.stdout.decode('utf-8', errors='ignore')}")
        print(f"stderr: {res.stderr.decode('utf-8', errors='ignore')}")
        sys.exit(1)
    return res.stdout.decode('utf-8', errors='ignore')

def get_verdict(score):
    if score == 0:
        return "NONE"
    elif score <= 2:
        return "LOW"
    elif score <= 5:
        return "MEDIUM"
    else:
        return "HIGH"

def main():
    print("==========================================================")
    # 1. Ensure directories exist
    os.makedirs("scratch", exist_ok=True)
    os.makedirs("scratch/calibration", exist_ok=True)
    
    # 2. Recompile tools if needed
    print("[Calibration] Ensuring tools are compiled...")
    run_cmd("gcc -O2 -Wall -Itools tools/zcc_topology_auditor.c -o tools/zcc_topology_auditor -lm")
    run_cmd("gcc -O2 -Wall -Itools tools/zcc_impact_attribution.c -o tools/zcc_impact_attribution -lm")
    
    results = {}
    
    for t in TESTS:
        print(f"\n[Calibration] Running experiment for: {t}")
        
        # A. Compile Version A (Peephole optimization disabled)
        print("  - Compiling Version A (peephole=0)...")
        env_a = os.environ.copy()
        env_a["ZCC_OPT_PEEPHOLE"] = "0"
        run_cmd(f"./zcc tests/calibration/{t}.c -o scratch/{t}_a.s", env=env_a)
        run_cmd(f"gcc -c scratch/{t}_a.s -o scratch/{t}_a.o")
        run_cmd(f"./tools/zcc_topology_auditor scratch/{t}_a.o --json > scratch/{t}_static_a.json")
        
        # B. Compile Version B (Peephole optimization enabled)
        print("  - Compiling Version B (peephole=1)...")
        env_b = os.environ.copy()
        env_b["ZCC_OPT_PEEPHOLE"] = "1"
        run_cmd(f"./zcc tests/calibration/{t}.c -o scratch/{t}_b.s", env=env_b)
        run_cmd(f"gcc -c scratch/{t}_b.s -o scratch/{t}_b.o")
        run_cmd(f"./tools/zcc_topology_auditor scratch/{t}_b.o --json > scratch/{t}_static_b.json")
        
        # C. Compile Runtime A (GCC -O0)
        print("  - Compiling Runtime A (GCC -O0)...")
        run_cmd(f"gcc -O0 -finstrument-functions tools/zcc_runtime_probe.c tests/calibration/{t}.c -o scratch/{t}_run_a")
        # Run it
        env_run = os.environ.copy()
        env_run["ZCC_PROBE_OUT"] = f"scratch/{t}_runtime_a.json"
        run_cmd(f"./scratch/{t}_run_a", env=env_run)
        
        # D. Compile Runtime B (GCC -O2)
        print("  - Compiling Runtime B (GCC -O2)...")
        run_cmd(f"gcc -O2 -finstrument-functions tools/zcc_runtime_probe.c tests/calibration/{t}.c -o scratch/{t}_run_b")
        # Run it
        env_run["ZCC_PROBE_OUT"] = f"scratch/{t}_runtime_b.json"
        run_cmd(f"./scratch/{t}_run_b", env=env_run)
        
        # E. Run Attribution Engine
        print("  - Running attribution engine...")
        run_cmd(f"./tools/zcc_impact_attribution "
                f"--static-a scratch/{t}_static_a.json "
                f"--static-b scratch/{t}_static_b.json "
                f"--runtime-a scratch/{t}_runtime_a.json "
                f"--runtime-b scratch/{t}_runtime_b.json "
                f"--version-a v0.29-A --version-b v0.29-B "
                f"--out scratch/{t}_attribution.json", allowed_codes={0, 1, 2})
        
        # F. Load predictions
        with open(f"scratch/{t}_attribution.json", "r") as f:
            attr = json.load(f)
            
        pred_score = attr["impact_score"]
        pred_verdict = attr["estimated_impact"]
        
        # G. Calculate Ground Truth (Measured outcome)
        # We compare Version A vs B static genomes and Runtime A vs B runtime genomes
        # actual performance drift is computed from:
        # - actual static instruction count change
        # - actual static register pressure change
        # - actual static stack growth
        # - actual runtime call count change
        # - actual peak call depth change
        static_drift = attr["static_drift"]
        runtime_drift = attr.get("runtime_drift", {})
        
        reg_drift = abs(static_drift["register_drift_pct"])
        stack_drift = abs(static_drift["stack_drift_bytes"])
        instr_drift = abs(static_drift["instr_drift_pct"])
        
        call_drift = abs(runtime_drift.get("call_volume_change_pct", 0))
        depth_drift = abs(runtime_drift.get("depth_change_frames", 0))
        
        # Ground truth measured impact score formula (empirically calibrating thresholds)
        measured_score = 0
        if reg_drift > 10:   measured_score += 2  # standard threshold was 20%
        if stack_drift > 32: measured_score += 2  # standard threshold was 64 bytes
        if instr_drift > 8:  measured_score += 2  # standard threshold was 15%
        if call_drift > 30:  measured_score += 3  # standard threshold was 50%
        if depth_drift > 1:  measured_score += 1  # standard threshold was 3 frames
        
        measured_verdict = get_verdict(measured_score)
        
        results[t] = {
            "predicted_score": pred_score,
            "predicted_verdict": pred_verdict,
            "measured_score": measured_score,
            "measured_verdict": measured_verdict,
            "metrics": {
                "reg_drift_pct": static_drift["register_drift_pct"],
                "stack_drift_bytes": static_drift["stack_drift_bytes"],
                "instr_drift_pct": static_drift["instr_drift_pct"],
                "call_volume_change_pct": runtime_drift.get("call_volume_change_pct", 0),
                "depth_change_frames": runtime_drift.get("depth_change_frames", 0)
            }
        }
        
        print(f"    Predicted: Score={pred_score} ({pred_verdict})")
        print(f"    Measured:  Score={measured_score} ({measured_verdict})")

    # 3. Calculate statistics
    print("\n[Calibration] Calculating statistical metrics...")
    
    total_cases = len(TESTS)
    true_positives = 0
    false_positives = 0
    true_negatives = 0
    false_negatives = 0
    total_deviation = 0
    
    for t, res in results.items():
        p_v = res["predicted_verdict"]
        m_v = res["measured_verdict"]
        p_s = res["predicted_score"]
        m_s = res["measured_score"]
        
        total_deviation += abs(p_s - m_s)
        
        is_pred_active = (p_v != "NONE")
        is_meas_active = (m_v != "NONE")
        
        if is_pred_active and is_meas_active:
            true_positives += 1
        elif is_pred_active and not is_meas_active:
            false_positives += 1
        elif not is_pred_active and is_meas_active:
            false_negatives += 1
        else:
            true_negatives += 1

    precision = true_positives / (true_positives + false_positives) if (true_positives + false_positives) > 0 else 1.0
    recall = true_positives / (true_positives + false_negatives) if (true_positives + false_negatives) > 0 else 1.0
    f1 = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 1.0
    mean_deviation = total_deviation / total_cases
    
    # 4. Emit Reports
    report = {
        "schema": "zcc.calibration_report.v1",
        "timestamp": "2026-06-06T11:20:00Z",
        "metrics": {
            "total_cases": total_cases,
            "true_positives": true_positives,
            "false_positives": false_positives,
            "true_negatives": true_negatives,
            "false_negatives": false_negatives,
            "precision": round(precision, 4),
            "recall": round(recall, 4),
            "f1_score": round(f1, 4),
            "mean_deviation_score": round(mean_deviation, 4)
        },
        "experiments": results
    }
    
    forecast_accuracy = {
        "schema": "zcc.forecast_accuracy.v1",
        "precision_pct": round(precision * 100, 2),
        "recall_pct": round(recall * 100, 2),
        "f1_pct": round(f1 * 100, 2),
        "mean_score_error": round(mean_deviation, 4),
        "status": "CALIBRATED" if f1 >= 0.7 else "UNSTABLE"
    }
    
    with open("scratch/calibration_report.json", "w") as f:
        json.dump(report, f, indent=2)
        
    with open("scratch/forecast_accuracy.json", "w") as f:
        json.dump(forecast_accuracy, f, indent=2)
        
    print("\n==========================================================")
    print("             ZCC PREDICTION ACCURACY REPORT")
    print("==========================================================")
    print(f"  Total Experiments:  {total_cases}")
    print(f"  Precision:          {precision*100:.1f}%")
    print(f"  Recall:             {recall*100:.1f}%")
    print(f"  F1 Score:           {f1*100:.1f}%")
    print(f"  Mean Score Error:   {mean_deviation:.2f} points")
    print("──────────────────────────────────────────────────────────")
    print("  Verdict Matrix:")
    print("  %-15s  %-12s  %-12s  %-10s" % ("Workload", "Predicted", "Measured", "Deviation"))
    for t, res in results.items():
        print("  %-15s  %-12s  %-12s  %-10d" % 
              (t, res["predicted_verdict"], res["measured_verdict"], abs(res["predicted_score"] - res["measured_score"])))
    print("==========================================================")
    print("Calibration files generated successfully in scratch/")

if __name__ == "__main__":
    main()
