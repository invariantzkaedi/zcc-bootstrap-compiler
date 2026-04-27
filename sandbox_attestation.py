from kill_switch import assert_not_globally_disabled
assert_not_globally_disabled()
import os
import sys
import json
import socket
import logging
import datetime
from pathlib import Path
from error_handling import run_bounded_subprocess
from production_guard import assert_sandbox_mode
import hashlib

def get_policy_hash():
    with open("fortify_policy.json", "r", encoding="utf-8") as f:
        return hashlib.sha256(f.read().encode()).hexdigest()

logging.basicConfig(level=logging.INFO, format='[ATTESTATION] %(message)s')
logger = logging.getLogger("sandbox_attestation")

RISK_PREFIXES = [
    "TOKEN", "SECRET", "PASSWORD", "KEY", "AWS_", "AZURE_", 
    "GCP_", "GH_", "GITHUB_TOKEN", "NPM_TOKEN", "PYPI_TOKEN", 
    "SSH", "PRIVATE", "CREDENTIAL"
]

def check_secrets():
    detected = []
    for k in os.environ.keys():
        k_upper = k.upper()
        if any(prefix in k_upper for prefix in RISK_PREFIXES):
            detected.append(k)
    return detected

def probe_network_egress():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2.0)
        # Attempt to reach example.com (93.184.215.14) or google DNS (8.8.8.8)
        sock.connect(("8.8.8.8", 53))
        sock.close()
        return True
    except (socket.timeout, OSError):
        return False

def get_compiler_provenance():
    prov = {
        "compiler_command": "wsl gcc",
        "compiler_path": "unknown",
        "compiler_version": "unknown",
        "compiler_sha256": "unknown"
    }
    
    code, stdout, _ = run_bounded_subprocess(["wsl", "which", "gcc"], timeout_sec=2)
    if code == 0 and stdout.strip():
        gcc_path = stdout.strip()
        prov["compiler_path"] = gcc_path
        
        code, stdout, _ = run_bounded_subprocess(["wsl", "sha256sum", gcc_path], timeout_sec=5)
        if code == 0 and stdout.strip():
            prov["compiler_sha256"] = f"sha256:{stdout.split()[0]}"
            
    code, stdout, _ = run_bounded_subprocess(["wsl", "gcc", "--version"], timeout_sec=2)
    if code == 0:
        prov["compiler_version"] = stdout.splitlines()[0]
        
    return prov

def run_attestation():
    logger.info("Running Sandbox Environment Attestation...")
    
    try:
        assert_sandbox_mode()
    except RuntimeError as e:
        logger.error(str(e))
        sys.exit(1)
        
    secrets = check_secrets()
    if secrets:
        logger.error(f"FATAL: Sensitive environment variables detected: {', '.join(secrets)}")
        sys.exit(1)
        
    network_reachable = probe_network_egress()
    local_override = os.environ.get("CHIMERA_ALLOW_LOCAL_NETWORK_DIAGNOSTIC") == "1"
    
    status = "ok"
    if network_reachable:
        if local_override:
            status = "local_override"
            logger.warning("Network egress is REACHABLE, but local diagnostic override is enabled.")
        else:
            status = "failed"
            logger.error("FATAL: Network egress is REACHABLE in strict mode.")
            
    prov = get_compiler_provenance()
    
    report = {
        "status": status,
        "sandbox_mode": True,
        "network_probe_performed": True,
        "network_reachable": network_reachable,
        "network_policy": "egress_must_be_blocked",
        "local_network_override_used": local_override,
        "secrets_detected": len(secrets) > 0,
        "compiler_provenance": prov,
        "policy_hash": f"sha256:{get_policy_hash()}",
        "working_directory": os.getcwd(),
        "ephemeral_workspace": True,
        "python_version": sys.version.split(" ")[0],
        "timestamp_utc": datetime.datetime.utcnow().isoformat() + "Z"
    }
    
    with open("sandbox_attestation.json", "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
        
    logger.info(f"Attestation Complete. Status: {status}")
    if status == "failed":
        sys.exit(1)

if __name__ == "__main__":
    run_attestation()
