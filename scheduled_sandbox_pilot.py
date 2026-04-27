from kill_switch import assert_not_globally_disabled
assert_not_globally_disabled()

import os
import sys
import json
import time
import argparse
import logging
from pathlib import Path
import datetime
from error_handling import run_bounded_subprocess
from alerting import emit_alert

logging.basicConfig(level=logging.INFO, format='[PILOT] %(message)s')
logger = logging.getLogger("sandbox_pilot")

def run_pilot_cycle():
    logger.info("Initiating Scheduled Sandbox Pilot Cycle...")
    start_time = time.time()
    
    # 1. Canary
    code, stdout, stderr = run_bounded_subprocess([sys.executable, "sandbox_canary.py"], timeout_sec=60)
    if code != 0:
        emit_alert("critical", "pilot_canary_failed", "sandbox_pilot", "pilot_aborted")
        sys.exit(1)
        
    # 2. Incident Drills
    code, stdout, stderr = run_bounded_subprocess([sys.executable, "incident_drills.py"], timeout_sec=120)
    if code != 0:
        emit_alert("critical", "pilot_incident_drills_failed", "sandbox_pilot", "pilot_aborted")
        sys.exit(1)
        
    # 3. Chaos Drills
    code, stdout, stderr = run_bounded_subprocess([sys.executable, "chaos_drills.py"], timeout_sec=120)
    if code != 0:
        emit_alert("critical", "pilot_chaos_drills_failed", "sandbox_pilot", "pilot_aborted")
        sys.exit(1)
        
    # 4. Release Gate
    code, stdout, stderr = run_bounded_subprocess([sys.executable, "release_gate.py"], timeout_sec=300)
    if code != 0:
        emit_alert("critical", "pilot_release_gate_failed", "sandbox_pilot", "pilot_aborted")
        sys.exit(1)
        
    # 5. Manifest Generation and Verification
    code, stdout, stderr = run_bounded_subprocess([sys.executable, "sign_release_artifacts.py"], timeout_sec=20)
    code2, stdout2, stderr2 = run_bounded_subprocess([sys.executable, "verify_release_artifacts.py"], timeout_sec=20)
    if code != 0 or code2 != 0:
        emit_alert("critical", "pilot_manifest_verification_failed", "sandbox_pilot", "pilot_aborted")
        sys.exit(1)
        
    # 6. Update Ops Summary
    try:
        ops_path = Path("release_artifacts/ops_summary.json")
        if ops_path.exists():
            with open(ops_path, "r") as f:
                ops = json.load(f)
        else:
            ops = {"consecutive_successes": 0, "consecutive_failures": 0}
            
        with open("release_artifacts/metrics.json", "r") as f:
            metrics = json.load(f)
            
        ops["last_successful_run"] = datetime.datetime.utcnow().isoformat() + "Z"
        ops["consecutive_successes"] += 1
        ops["consecutive_failures"] = 0
        ops["latest_policy_hash"] = metrics["policy_hash"]
        ops["latest_compiler_hash"] = metrics["compiler_hash"]
        ops["latest_redteam_count"] = metrics["total_fixtures_tested"]
        ops["latest_fixtures_contained"] = metrics["redteam_fixtures_contained"]
        ops["latest_average_generation_duration_ms"] = metrics["average_generation_duration_ms"]
        ops["latest_output_limit_kills"] = metrics["output_limit_kills"]
        
        with open(ops_path, "w") as f:
            json.dump(ops, f, indent=2)
            
    except Exception as e:
        emit_alert("critical", "pilot_ops_summary_failed", "sandbox_pilot", "pilot_aborted")
        sys.exit(1)
        
    # Emit Pilot Report
    report = {
      "status": "ok",
      "run_id": datetime.datetime.utcnow().isoformat() + "Z",
      "sandbox_image_hash": json.load(open("policy_approvals.json"))["approved_dockerfile_hashes"][0],
      "policy_hash": ops["latest_policy_hash"],
      "compiler_hash": ops["latest_compiler_hash"],
      "canary_passed": True,
      "release_gate_passed": True,
      "incident_drills_passed": True,
      "network_reachable": False,
      "secrets_detected": False,
      "duration_ms": int((time.time() - start_time) * 1000)
    }
    
    with open("release_artifacts/pilot_run_report.json", "w") as f:
        json.dump(report, f, indent=2)
        
    logger.info("Scheduled Sandbox Pilot Cycle Completed Successfully.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Scheduled Sandbox Pilot Runner")
    parser.add_argument("--once", action="store_true", help="Run the pilot exactly once and exit")
    parser.add_argument("--interval-seconds", type=int, default=3600, help="Interval between runs in continuous mode")
    args = parser.parse_args()
    
    if args.once:
        run_pilot_cycle()
    else:
        logger.info(f"Starting continuous pilot runner every {args.interval_seconds} seconds.")
        while True:
            try:
                run_pilot_cycle()
            except Exception as e:
                logger.error(f"Pilot cycle crashed: {e}")
                emit_alert("critical", "pilot_crash", "sandbox_pilot", "continuous_loop_failed")
            time.sleep(args.interval_seconds)
