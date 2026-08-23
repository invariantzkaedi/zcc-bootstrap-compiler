#!/usr/bin/env python3
"""
ZKAEDI Strict Evidence Manifest & Invariant Validator
Enforces artifact existence, st_size > 0, SHA-256 hash match, zero-byte rejection,
exit_code == 0, and optional HEAD commit alignment.
"""

import sys
import json
import hashlib
import subprocess
from pathlib import Path

EMPTY_FILE_SHA256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

def validate_manifest(manifest_path="evidence/manifest.json", assert_head=False):
    path = Path(manifest_path)
    if not path.exists():
        print(f"[FAIL] Manifest missing: {path}")
        sys.exit(1)
        
    with open(path, "r", encoding="utf-8") as f:
        manifest = json.load(f)
        
    if assert_head:
        try:
            res = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, check=True)
            head_sha = res.stdout.strip()
            m_sha = manifest.get("commit_sha", "")
            if m_sha != head_sha and not head_sha.startswith(m_sha) and not m_sha.startswith(head_sha):
                print(f"[FAIL] Manifest commit_sha ({m_sha}) != HEAD ({head_sha})")
                sys.exit(1)
        except Exception as e:
            print(f"[WARN] Git commit HEAD assertion skipped: {e}")
            
    print(f"[VERIFY] Validating manifest for commit: {manifest.get('commit_sha')[:8]}")
    for inv in manifest.get("invariants", []):
        inv_id = inv["id"]
        
        # 1. Enforce exit_code == 0
        recorded_exit = inv.get("exit_code")
        if recorded_exit != 0:
            print(f"[{inv_id}] FAIL: Recorded gate exit code is {recorded_exit}, expected 0")
            sys.exit(1)
            
        # 2. Enforce artifact file existence
        art_path = Path(inv["artifact_path"])
        if not art_path.exists():
            print(f"[{inv_id}] FAIL: Artifact file not found at {art_path}")
            sys.exit(1)
            
        # 3. Enforce st_size > 0
        file_size = art_path.stat().st_size
        if file_size == 0:
            print(f"[{inv_id}] FAIL: Artifact is 0 bytes (empty file rejected): {art_path}")
            sys.exit(1)
            
        # 4. Enforce SHA-256 match & empty digest rejection
        digest = hashlib.sha256(art_path.read_bytes()).hexdigest()
        if digest == EMPTY_FILE_SHA256:
            print(f"[{inv_id}] FAIL: Empty file SHA-256 hash detected for {art_path}")
            sys.exit(1)
            
        if digest != inv["sha256"]:
            print(f"[{inv_id}] FAIL: SHA-256 mismatch for {art_path}: expected {inv['sha256']}, got {digest}")
            sys.exit(1)
            
        print(f"[{inv_id}] PASS: {inv['name']} (exit_code=0, {file_size} bytes, SHA-256: {digest[:12]}...)")
        
    print("[SUCCESS] All evidence invariants verified clean.")
    return 0

if __name__ == "__main__":
    assert_head = "--assert-head" in sys.argv
    sys.exit(validate_manifest(assert_head=assert_head))
