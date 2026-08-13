#!/usr/bin/env python3
"""
ZKAEDI Persisted-Byte Evidence Manifest Generator
Executes gate commands directly, ensures non-empty output for silent success commands,
writes raw output to disk, computes SHA-256 hashes FROM PERSISTED DISK BYTES,
records dynamic exit codes, and returns status codes cleanly.
"""

import sys
import json
import hashlib
import platform
import subprocess
from pathlib import Path

def get_git_sha():
    try:
        res = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, check=True)
        return res.stdout.strip()
    except Exception:
        return "0000000000000000000000000000000000000000"

# Target gates (native Linux / WSL compatible)
GATES = [
    (
        "GATE-001",
        "bootstrap-fixed-point",
        ["bash", "-c", "cmp zcc2.s zcc3.s && echo 'SELF-HOST VERIFIED (assembly identical)'"],
        "cmp_selfhost.log"
    ),
    (
        "GATE-002",
        "rust-frontend-smoke",
        ["make", "rust-front-smoke"],
        "rust_smoke.log"
    ),
]

def generate_manifest(custom_gates=None, output_manifest="evidence/manifest.json", evidence_dir="evidence/latest"):
    # Ensure self-host artifacts exist before running gates
    if not Path("zcc2.s").exists() or not Path("zcc3.s").exists():
        print("[PRE-FLIGHT] Assembly artifacts zcc2.s/zcc3.s missing. Executing make selfhost-raw...")
        subprocess.run(["make", "selfhost-raw"], check=True)

    gates_to_run = custom_gates if custom_gates is not None else GATES
    commit_sha = get_git_sha()
    target_evidence_dir = Path(evidence_dir)
    target_evidence_dir.mkdir(parents=True, exist_ok=True)
    
    invariants = []
    has_failure = False
    
    for gate_id, name, cmd, log_name in gates_to_run:
        log_path = target_evidence_dir / log_name
        
        # 1. Execute command and capture raw output & exit code
        res = subprocess.run(cmd, capture_output=True, text=True)
        actual_exit_code = res.returncode
        raw_output = res.stdout + res.stderr
        
        # 2. Ensure non-empty evidence output for silent success commands
        if not raw_output.strip() and actual_exit_code == 0:
            raw_output = f"[{gate_id}] VERIFIED PASS (Exit Code: 0)\n"
            
        # 3. Persist output to disk FIRST
        log_path.write_text(raw_output, encoding="utf-8")
        
        # 4. Hash the ACTUAL PERSISTED DISK BYTES
        persisted_bytes = log_path.read_bytes()
        file_hash = hashlib.sha256(persisted_bytes).hexdigest()
        
        invariants.append({
            "id": gate_id,
            "name": name,
            "command": " ".join(cmd),
            "exit_code": actual_exit_code,
            "artifact_path": str(log_path).replace("\\", "/"),
            "sha256": file_hash
        })
        
        if actual_exit_code != 0:
            print(f"[{gate_id}] FAIL: Command '{' '.join(cmd)}' failed with exit code {actual_exit_code}")
            has_failure = True
        else:
            print(f"[{gate_id}] PASS: Executed clean with exit code 0 ({len(persisted_bytes)} bytes)")

    manifest = {
        "version": "1.0.0",
        "repository": "invariantzkaedi/ZCC",
        "commit_sha": commit_sha,
        "environment": {
            "os": platform.platform(),
            "python": platform.python_version()
        },
        "invariants": invariants
    }

    manifest_path = Path(output_manifest)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        
    if has_failure:
        print(f"[ERROR] Manifest {manifest_path} generated with gate failures!")
        return 1
        
    print(f"[SUCCESS] Updated {manifest_path} for commit {commit_sha[:8]} with {len(invariants)} verified invariants.")
    return 0

if __name__ == "__main__":
    sys.exit(generate_manifest())
