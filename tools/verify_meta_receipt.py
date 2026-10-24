#!/usr/bin/env python3
"""
ZCC-EF-0 Independent Meta-Verification Receipt Adjudicator
Validates meta_verification_receipt.json against strict anti-vacuity, anti-forgery,
and lineage constraints (NC-14, NC-15, NC-16, EF-01 through EF-06).
"""
import json
import os
import sys
from pathlib import Path

EXPECTED_CODES = {
    "assembly": 20,
    "object": 21,
    "binary": 22,
}

def reject(message):
    print(f"ADJUDICATION FAIL: {message}", file=sys.stderr)
    sys.exit(1)

def verify(receipt_path, expected_commit=None, artifacts_dir=None):
    p = Path(receipt_path)
    if not p.is_file():
        reject(f"Receipt file does not exist: {receipt_path}")
    if p.stat().st_size == 0:
        reject(f"Receipt file is empty (0 bytes): {receipt_path}")
    
    try:
        receipt = json.loads(p.read_text())
    except Exception as e:
        reject(f"Malformed JSON in receipt: {e}")

    # 1. Basic Verdict & Exit Code
    verdict = receipt.get("verdict")
    exit_code = receipt.get("exit_code")
    if verdict != "PASS":
        reject(f"Receipt verdict is '{verdict}', expected 'PASS'")
    if exit_code != 0:
        reject(f"Receipt reports nonzero exit code: {exit_code}")

    # 2. Positive Controls Status
    positives = receipt.get("positive_controls", {})
    if positives.get("status") != "PASS":
        reject(f"Positive controls status is '{positives.get('status')}', expected 'PASS'")
    for key in ("cmp_assembly", "cmp_object", "cmp_binary_norm"):
        if positives.get(key) != 0:
            reject(f"Positive control '{key}' exit code is {positives.get(key)}, expected 0")

    # 3. Stage Digests & Pairwise Equality
    digests = receipt.get("digests", {})
    for suffix in ("s", "o", "bin_norm"):
        s2 = digests.get(f"stage2_{suffix}")
        s3 = digests.get(f"stage3_{suffix}")
        if not s2 or not s3:
            reject(f"Missing digest for stage2_{suffix} or stage3_{suffix}")
        if s2 == "UNCOMPUTED" or s3 == "UNCOMPUTED":
            reject(f"Uncomputed digest for stage2_{suffix} or stage3_{suffix}")
        if s2 != s3:
            reject(f"Stage identity mismatch for '{suffix}': {s2} != {s3}")

    # 4. Anti-Vacuity (NC-14): Evaluated Tests Count & Completeness
    negative_controls = receipt.get("negative_controls", {})
    if not negative_controls or len(negative_controls) == 0:
        reject("NC-14 VIOLATION: Empty / vacuous negative controls collection (evaluated_tests == 0)")
    
    evaluated_tests = 0
    for name, expected_code in EXPECTED_CODES.items():
        test_info = negative_controls.get(name)
        if not test_info:
            reject(f"NC-14 VIOLATION: Missing required negative control: '{name}'")
        
        # 5. Anti-Forgery (NC-15): Internal Consistency & Contradiction Detection
        status = test_info.get("status")
        observed_rc = test_info.get("gate_rc")
        if status != "PASS":
            reject(f"NC-15 FORGERY DETECTED: Control '{name}' marked '{status}' despite top-level PASS verdict")
        if observed_rc != expected_code:
            reject(f"NC-15 FORGERY DETECTED: Control '{name}' returned gate_rc={observed_rc}, expected {expected_code}")
        
        diag = test_info.get("diagnostic", "")
        if not diag:
            reject(f"NC-15 FORGERY DETECTED: Missing diagnostic error message for '{name}'")
        
        evaluated_tests += 1

    if evaluated_tests != len(EXPECTED_CODES):
        reject(f"NC-14 VIOLATION: Expected {len(EXPECTED_CODES)} evaluated tests, got {evaluated_tests}")

    # 6. Cleanup Verification (EF-03)
    cleanup = receipt.get("cleanup", {})
    if not cleanup.get("harness_workdir_cleaned"):
        reject("Workdir cleanup was not verified as cleaned (EF-03 violation)")

    # 7. Lineage & Anti-Stale Artifact Substitution (NC-16)
    provenance = receipt.get("provenance", {})
    recorded_commit = provenance.get("git_commit")
    if expected_commit and recorded_commit != expected_commit:
        reject(f"NC-16 LINEAGE MISMATCH: Receipt commit {recorded_commit} does not match expected {expected_commit}")

    if artifacts_dir:
        art_path = Path(artifacts_dir)
        if not art_path.is_dir():
            reject(f"Artifact directory does not exist: {artifacts_dir}")
        import hashlib
        for suffix, fname in [("s", "stage2.s"), ("o", "stage2.o"), ("bin_norm", "stage2.bin.norm")]:
            f = art_path / fname
            if not f.is_file():
                reject(f"NC-16 STALE ARTIFACT: File {fname} missing from artifact dir")
            calc_sha = hashlib.sha256(f.read_bytes()).hexdigest()
            recorded_sha = digests.get(f"stage2_{suffix}")
            if calc_sha != recorded_sha:
                reject(f"NC-16 STALE ARTIFACT: File {fname} sha256 {calc_sha} differs from receipt {recorded_sha}")

    print("ADJUDICATION PASS: All invariants, positive controls, negative controls (20/21/22), anti-vacuity, anti-forgery, and cleanup verified.")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <receipt_path> [expected_commit] [artifacts_dir]", file=sys.stderr)
        sys.exit(2)
    receipt_arg = sys.argv[1]
    expected_commit_arg = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] != "" else None
    artifacts_dir_arg = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] != "" else None
    verify(receipt_arg, expected_commit_arg, artifacts_dir_arg)
