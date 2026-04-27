from kill_switch import assert_not_globally_disabled
assert_not_globally_disabled()
import os
import sys
import json
import time
import logging
from pathlib import Path
from error_handling import run_bounded_subprocess

logging.basicConfig(level=logging.INFO, format='[CANARY] %(message)s')
logger = logging.getLogger("sandbox_canary")

def run_canary():
    logger.info("Initializing Sandbox Canary Runner...")
    start_time = time.time()
    
    if os.environ.get("CHIMERA_SANDBOX_ONLY") != "1":
        logger.error("FATAL: CHIMERA_SANDBOX_ONLY=1 is not set.")
        sys.exit(1)
        
    if os.environ.get("CHIMERA_ALLOW_LOCAL_NETWORK_DIAGNOSTIC") == "1":
        logger.error("FATAL: CHIMERA_ALLOW_LOCAL_NETWORK_DIAGNOSTIC=1 is set. Canary requires real sandbox posture.")
        sys.exit(1)
        
    # 1. Attestation (Network + Secrets)
    os.environ["CHIMERA_SEED"] = "canary-seed-test"
    code, stdout, stderr = run_bounded_subprocess([sys.executable, "sandbox_attestation.py"], timeout_sec=10)
    if code != 0:
        logger.error(f"Attestation FAILED (Exit {code}).")
        sys.exit(1)
        
    with open("sandbox_attestation.json", "r") as f:
        att = json.load(f)
        if att.get("network_reachable"):
            logger.error("FATAL: Network is reachable.")
            sys.exit(1)
        if att.get("secrets_detected"):
            logger.error("FATAL: Secrets detected in environment.")
            sys.exit(1)
            
    # 2. Daemon Run
    os.environ["CHIMERA_REPORT_PATH"] = "chimera_run_report.canary.json"
    code, stdout, stderr = run_bounded_subprocess([sys.executable, "chimera_mutagen_daemon.py"], timeout_sec=20)
    if code != 0:
        logger.error(f"Daemon FAILED (Exit {code}).")
        sys.exit(1)
        
    with open("chimera_run_report.canary.json", "r") as f:
        rep = json.load(f)
        if rep.get("generations_completed") != 3:
            logger.error("FATAL: Daemon did not complete exactly 3 generations.")
            sys.exit(1)
        if not rep.get("artifacts_cleaned"):
            logger.error("FATAL: Daemon did not clean up artifacts.")
            sys.exit(1)
            
    # 3. Red-Team Smoke Test
    # Test one fixture to ensure fortify gate is active
    code, stdout, stderr = run_bounded_subprocess([sys.executable, "-c", "from fuzz_fortify_gate import analyze_containment; from pathlib import Path; print(analyze_containment(Path('tests/redteam/stdout_flood.c')))"], timeout_sec=10)
    if code != 0 or "NOT_CONTAINED" in stdout:
        logger.error("FATAL: Red-team smoke fixture was NOT contained.")
        sys.exit(1)
        
    # Emit Report
    report = {
      "status": "ok",
      "policy_hash": rep.get("policy_hash"),
      "compiler_hash": att.get("compiler_provenance", {}).get("compiler_sha256"),
      "generations_completed": 3,
      "network_reachable": False,
      "secrets_detected": False,
      "redteam_smoke_passed": True,
      "duration_ms": int((time.time() - start_time) * 1000)
    }
    
    with open("sandbox_canary_report.json", "w") as f:
        json.dump(report, f, indent=2)
        
    logger.info("Canary Run Complete. Sandbox containment holds.")
    
if __name__ == "__main__":
    run_canary()
