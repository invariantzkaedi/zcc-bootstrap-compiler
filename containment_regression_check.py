from kill_switch import assert_not_globally_disabled
assert_not_globally_disabled()
import os
import sys
import json
import hashlib
import logging
from pathlib import Path

logging.basicConfig(level=logging.INFO, format='[REGRESSION] %(message)s')
logger = logging.getLogger("containment_regression")

def hash_file(filepath: str) -> str:
    path = Path(filepath)
    if not path.exists():
        return "MISSING"
    with open(path, "rb") as f:
        return f"sha256:{hashlib.sha256(f.read()).hexdigest()}"

def run_regression_check():
    logger.info("Initializing Containment Regression Check...")
    
    if not Path("policy_approvals.json").exists():
        logger.error("FATAL: policy_approvals.json is missing.")
        sys.exit(1)
        
    with open("policy_approvals.json", "r") as f:
        approvals = json.load(f)
        
    # 1. Hash Checks
    current_policy_hash = hash_file("fortify_policy.json")
    if current_policy_hash not in approvals["approved_policy_hashes"]:
        logger.error(f"FATAL: fortify_policy.json hash {current_policy_hash} is not approved.")
        sys.exit(1)
        
    current_docker_hash = hash_file("Dockerfile.sandbox")
    if current_docker_hash not in approvals["approved_dockerfile_hashes"]:
        logger.error(f"FATAL: Dockerfile.sandbox hash {current_docker_hash} is not approved.")
        sys.exit(1)
        
    # We load compiler hash from attestation report if available, else skip check for now (handled in gate)
    if Path("sandbox_attestation.json").exists():
        with open("sandbox_attestation.json", "r") as f:
            att = json.load(f)
            chash = att.get("compiler_provenance", {}).get("compiler_sha256")
            if chash and chash not in approvals["approved_compiler_hashes"]:
                logger.error(f"FATAL: Compiler hash {chash} is not approved.")
                sys.exit(1)
                
    # 2. Semantic Drift Checks
    if not Path("fortify_policy.json").exists():
        logger.error("FATAL: fortify_policy.json is missing.")
        sys.exit(1)
        
    with open("fortify_policy.json", "r") as f:
        current_policy = json.load(f)
        
    if current_policy.get("max_generations", 999) > approvals["max_generations"]:
        logger.error("FATAL: max_generations exceeds approved baseline.")
        sys.exit(1)
        
    if current_policy.get("max_stdout_bytes", 9999999) > approvals["max_stdout_bytes"]:
        logger.error("FATAL: max_stdout_bytes exceeds approved baseline.")
        sys.exit(1)
        
    if current_policy.get("max_stderr_bytes", 9999999) > approvals["max_stderr_bytes"]:
        logger.error("FATAL: max_stderr_bytes exceeds approved baseline.")
        sys.exit(1)
        
    if current_policy.get("max_preprocessed_bytes", 9999999) > approvals["max_preprocessed_bytes"]:
        logger.error("FATAL: max_preprocessed_bytes exceeds approved baseline.")
        sys.exit(1)
        
    # Check redteam baseline
    redteam_count = len(list(Path("tests/redteam").glob("*.c")))
    if redteam_count < approvals["minimum_redteam_fixtures"]:
        logger.error(f"FATAL: Red-team fixture count ({redteam_count}) dropped below approved baseline ({approvals['minimum_redteam_fixtures']}).")
        sys.exit(1)
        
    # 3. Baseline Snapshot & Capability Freeze
    if Path("baseline_snapshot.json").exists():
        with open("baseline_snapshot.json", "r") as f:
            baseline = json.load(f)
            
        if redteam_count < baseline.get("minimum_redteam_fixtures", 11):
            logger.error(f"FATAL: Red-team fixture count ({redteam_count}) dropped below baseline.")
            sys.exit(1)
            
        if os.environ.get("CHIMERA_CAPABILITY_FREEZE") == "1":
            logger.info("Capability Freeze is ACTIVE. Verifying strict baseline parity.")
            
            # Semantic Expansions
            if current_policy.get("max_generations") > baseline.get("max_generations"):
                logger.error("FREEZE VIOLATION: max_generations expanded.")
                sys.exit(1)
            if current_policy.get("max_stdout_bytes") > baseline.get("max_stdout_bytes"):
                logger.error("FREEZE VIOLATION: max_stdout_bytes expanded.")
                sys.exit(1)
            if current_policy.get("max_stderr_bytes") > baseline.get("max_stderr_bytes"):
                logger.error("FREEZE VIOLATION: max_stderr_bytes expanded.")
                sys.exit(1)
            if current_policy.get("max_preprocessed_bytes") > baseline.get("max_preprocessed_bytes"):
                logger.error("FREEZE VIOLATION: max_preprocessed_bytes expanded.")
                sys.exit(1)
            if current_policy.get("max_compile_seconds") > baseline.get("max_compile_seconds"):
                logger.error("FREEZE VIOLATION: max_compile_seconds expanded.")
                sys.exit(1)
            if current_policy.get("max_runtime_seconds") > baseline.get("max_runtime_seconds"):
                logger.error("FREEZE VIOLATION: max_runtime_seconds expanded.")
                sys.exit(1)
                
            # List check (Must not expand)
            if set(current_policy.get("allowed_includes", [])) - set(baseline.get("allowed_includes", [])):
                logger.error("FREEZE VIOLATION: allowed_includes expanded.")
                sys.exit(1)
                
            # List check (Must not shrink)
            if set(baseline.get("forbidden_tokens", [])) - set(current_policy.get("forbidden_tokens", [])):
                logger.error("FREEZE VIOLATION: forbidden_tokens shrank.")
                sys.exit(1)
                
            # Compiler check
            if Path("sandbox_attestation.json").exists():
                with open("sandbox_attestation.json", "r") as f:
                    att = json.load(f)
                    if att.get("compiler_provenance", {}).get("compiler_command") != "gcc":
                        logger.error("FREEZE VIOLATION: compiler command changed.")
                        sys.exit(1)
            
            # Kill switch check
            for ep in ["chimera_mutagen_daemon.py", "release_gate.py"]:
                text = Path(ep).read_text(encoding="utf-8")
                if "assert_not_globally_disabled" not in text:
                    logger.error(f"FREEZE VIOLATION: kill switch missing from {ep}")
                    sys.exit(1)
        
    logger.info("Containment Regression Check Passed. No unauthorized drift detected.")

if __name__ == "__main__":
    run_regression_check()
