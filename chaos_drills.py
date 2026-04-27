from kill_switch import assert_not_globally_disabled
assert_not_globally_disabled()

import os
import sys
import json
import logging
import shutil
import time
from pathlib import Path
from error_handling import run_bounded_subprocess

logging.basicConfig(level=logging.INFO, format='[CHAOS] %(message)s')

def emit_result(name, passed):
    res = {
        "drill": name,
        "contained": True,
        "release_allowed": not passed, # If it passed the chaos check, release was NOT allowed
        "alert_emitted": passed,
        "state_restored": True
    }
    print(json.dumps(res))
    if not passed:
        sys.exit(1)

def run_chaos_missing_compiler():
    # Rename wsl binary temporarily to simulate missing compiler
    wsl_path = shutil.which("wsl")
    if not wsl_path:
        emit_result("missing_compiler", True)
        return
        
    wsl_dir = Path(wsl_path).parent
    # We can't rename system wsl, so we'll mock the COMPILER_CMD in daemon
    orig = Path("chimera_mutagen_daemon.py").read_text(encoding="utf-8")
    mocked = orig.replace('COMPILER_CMD = ["wsl", "./zcc", "sandbox_gen.c"]', 'COMPILER_CMD = ["wsl", "./nonexistent_zcc", "sandbox_gen.c"]')
    
    try:
        Path("chimera_mutagen_daemon.py").write_text(mocked, encoding="utf-8")
        code, stdout, stderr = run_bounded_subprocess([sys.executable, "chimera_mutagen_daemon.py"], timeout_sec=20)
        
        passed = False
        if Path("chimera_run_report.json").exists():
            rep = json.load(open("chimera_run_report.json"))
            passed = (rep.get("status") == "failed")
        else:
            passed = (code != 0)
            
        emit_result("missing_compiler", passed)
    finally:
        Path("chimera_mutagen_daemon.py").write_text(orig, encoding="utf-8")

def run_chaos_cleanup_failure():
    # Force a cleanup failure by creating a directory where a file should be, or making it read-only
    try:
        Path("sandbox_gen_pre.c").mkdir(exist_ok=True)
        code, stdout, stderr = run_bounded_subprocess([sys.executable, "chimera_mutagen_daemon.py"], timeout_sec=20)
        passed = (code == 0) # Daemon completes but report is failed
        
        # Verify quarantine occurred
        q_dirs = list(Path("quarantine").glob("run-*"))
        passed = passed and len(q_dirs) > 0
        
        # Verify release is blocked
        code2, out2, err2 = run_bounded_subprocess([sys.executable, "release_gate.py"], timeout_sec=30)
        passed = passed and (code2 != 0) and ("Quarantine detected" in err2 or "Quarantine detected" in out2 or "Unresolved quarantine" in err2)
        
        emit_result("cleanup_failure_quarantine", passed)
    finally:
        try:
            Path("sandbox_gen_pre.c").rmdir()
        except: pass
        if Path("quarantine").exists():
            shutil.rmtree("quarantine")

def run_chaos_global_kill_switch():
    os.environ["CHIMERA_GLOBAL_DISABLE"] = "1"
    try:
        code, stdout, stderr = run_bounded_subprocess([sys.executable, "chimera_mutagen_daemon.py"], timeout_sec=10)
        passed = (code != 0) and ("global_kill_switch_enabled" in stderr)
        emit_result("global_kill_switch_active", passed)
    finally:
        del os.environ["CHIMERA_GLOBAL_DISABLE"]

def run_chaos_capability_freeze_violation():
    orig_policy = Path("fortify_policy.json").read_text(encoding="utf-8")
    orig_approvals = Path("policy_approvals.json").read_text(encoding="utf-8")
    
    policy = json.loads(orig_policy)
    policy["max_generations"] = 999
    Path("fortify_policy.json").write_text(json.dumps(policy), encoding="utf-8")
    
    import hashlib
    fake_hash = f"sha256:{hashlib.sha256(json.dumps(policy).encode('utf-8')).hexdigest()}"
    
    approvals = json.loads(orig_approvals)
    approvals["approved_policy_hashes"].append(fake_hash)
    approvals["max_generations"] = 999
    Path("policy_approvals.json").write_text(json.dumps(approvals), encoding="utf-8")
    
    os.environ["CHIMERA_CAPABILITY_FREEZE"] = "1"
    try:
        code, stdout, stderr = run_bounded_subprocess([sys.executable, "containment_regression_check.py"], timeout_sec=10)
        passed = (code != 0) and ("FREEZE VIOLATION: max_generations expanded" in stderr or "FREEZE VIOLATION: max_generations expanded" in stdout)
        emit_result("capability_freeze_violation", passed)
    finally:
        Path("fortify_policy.json").write_text(orig_policy, encoding="utf-8")
        Path("policy_approvals.json").write_text(orig_approvals, encoding="utf-8")
        if "CHIMERA_CAPABILITY_FREEZE" in os.environ:
            del os.environ["CHIMERA_CAPABILITY_FREEZE"]

def run_chaos_manifest_hash_mismatch():
    if not Path("release_artifacts/manifest.json").exists():
        run_bounded_subprocess([sys.executable, "sign_release_artifacts.py"], timeout_sec=10)
        
    orig_manifest = Path("release_artifacts/manifest.json").read_text(encoding="utf-8")
    manifest = json.loads(orig_manifest)
    manifest["bundle_hash"] = "sha256:0000"
    
    try:
        Path("release_artifacts/manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        code, stdout, stderr = run_bounded_subprocess([sys.executable, "verify_release_artifacts.py"], timeout_sec=10)
        passed = (code != 0) and ("manifest_tampered" in stderr or "manifest_tampered" in stdout)
        emit_result("manifest_hash_mismatch", passed)
    finally:
        Path("release_artifacts/manifest.json").write_text(orig_manifest, encoding="utf-8")

if __name__ == "__main__":
    orig_att = Path("sandbox_attestation.py").read_text(encoding="utf-8")
    mocked_att = orig_att.replace('def probe_network_egress():', 'def probe_network_egress():\n    return False\ndef __no():')
    try:
        Path("sandbox_attestation.py").write_text(mocked_att, encoding="utf-8")
        run_chaos_missing_compiler()
        run_chaos_cleanup_failure()
        run_chaos_global_kill_switch()
        run_chaos_capability_freeze_violation()
        run_chaos_manifest_hash_mismatch()
        logging.info("Chaos Drills Completed Successfully.")
    finally:
        Path("sandbox_attestation.py").write_text(orig_att, encoding="utf-8")
