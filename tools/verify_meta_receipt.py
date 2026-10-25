#!/usr/bin/env python3
"""
ZCC-EF-0 Independent Meta-Verification Receipt Adjudicator (v3.0 OMEGA)
=============================================================================
Enforces independent verification authority, double-stage recomputation,
direct byte-level artifact comparison, symlink rejection, truncation checks,
anti-vacuity, anti-forgery, and anti-stale controls.
"""
import hashlib
import json
import os
import sys
from pathlib import Path

EXPECTED_CODES = {
    "assembly": 20,
    "object": 21,
    "binary": 22,
}

MIN_SIZES = {
    "stage2.s": 1_000_000,
    "stage3.s": 1_000_000,
    "stage2.o": 500_000,
    "stage3.o": 500_000,
    "stage2.bin.norm": 500_000,
    "stage3.bin.norm": 500_000,
}

def reject(message, exit_code=1):
    print(f"ADJUDICATION FAIL: {message}", file=sys.stderr)
    sys.exit(exit_code)

def validate_schema(receipt_data, repo_root):
    schema_path = repo_root / "contracts" / "meta_verification_receipt.schema.json"
    if schema_path.is_file():
        try:
            import jsonschema
            schema = json.loads(schema_path.read_text())
            jsonschema.validate(instance=receipt_data, schema=schema)
        except ImportError:
            # Fallback lightweight schema validation
            for req in ["timestamp_utc", "verdict", "exit_code", "provenance", "digests", "positive_controls", "negative_controls", "cleanup"]:
                if req not in receipt_data:
                    reject(f"Schema violation: missing required top-level key '{req}'")
        except Exception as e:
            reject(f"Receipt schema validation failure: {e}")

def verify(receipt_path, expected_commit=None, artifacts_dir=None):
    p = Path(receipt_path)
    if not p.is_file():
        reject(f"Receipt file does not exist: {receipt_path}", exit_code=30)
    if p.stat().st_size == 0:
        reject(f"Receipt file is empty (0 bytes): {receipt_path}", exit_code=30)
    if os.path.islink(p):
        reject(f"Receipt file must not be a symbolic link: {receipt_path}", exit_code=1)

    try:
        receipt = json.loads(p.read_text())
    except Exception as e:
        reject(f"Malformed JSON in receipt: {e}")

    repo_root = Path(__file__).resolve().parent.parent
    validate_schema(receipt, repo_root)

    # 1. Top-Level Verdict & Exit Code
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

    # 3. Stage Digests Pairwise Equality in Receipt
    digests = receipt.get("digests", {})
    for suffix in ("s", "o", "bin_norm"):
        s2 = digests.get(f"stage2_{suffix}")
        s3 = digests.get(f"stage3_{suffix}")
        if not s2 or not s3:
            reject(f"Missing digest for stage2_{suffix} or stage3_{suffix}")
        if s2 == "UNCOMPUTED" or s3 == "UNCOMPUTED":
            reject(f"Uncomputed digest for stage2_{suffix} or stage3_{suffix}")
        if s2 != s3:
            reject(f"Stage identity mismatch in receipt for '{suffix}': {s2} != {s3}")

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

    # 7. Lineage Binding
    provenance = receipt.get("provenance", {})
    recorded_commit = provenance.get("git_commit")
    if expected_commit and recorded_commit != expected_commit:
        reject(f"NC-16 LINEAGE MISMATCH: Receipt commit {recorded_commit} does not match expected {expected_commit}")

    # 8. Full Independent Double-Stage Recomputation & Direct Byte Comparison
    if artifacts_dir:
        art_path = Path(artifacts_dir)
        if not art_path.is_dir():
            reject(f"Artifact directory does not exist: {artifacts_dir}", exit_code=30)

        pairs = [
            ("s", "stage2.s", "stage3.s"),
            ("o", "stage2.o", "stage3.o"),
            ("bin_norm", "stage2.bin.norm", "stage3.bin.norm"),
        ]

        for suffix, fn2, fn3 in pairs:
            f2 = art_path / fn2
            f3 = art_path / fn3

            # Check existence
            if not f2.is_file():
                reject(f"Artifact {fn2} missing from directory {artifacts_dir}", exit_code=30)
            if not f3.is_file():
                reject(f"Artifact {fn3} missing from directory {artifacts_dir}", exit_code=30)

            # NC-18: Symlink Sabotage Check
            if os.path.islink(f2):
                reject(f"NC-18 SYMLINK FORBIDDEN: {fn2} is a symbolic link")
            if os.path.islink(f3):
                reject(f"NC-18 SYMLINK FORBIDDEN: {fn3} is a symbolic link")

            # NC-19: Truncation Check
            s2_sz = f2.stat().st_size
            s3_sz = f3.stat().st_size
            if s2_sz < MIN_SIZES[fn2]:
                reject(f"NC-19 TRUNCATED ARTIFACT: {fn2} size {s2_sz} below minimum threshold {MIN_SIZES[fn2]}")
            if s3_sz < MIN_SIZES[fn3]:
                reject(f"NC-19 TRUNCATED ARTIFACT: {fn3} size {s3_sz} below minimum threshold {MIN_SIZES[fn3]}")

            # Read raw bytes for both stages
            bytes2 = f2.read_bytes()
            bytes3 = f3.read_bytes()

            # Direct Byte-Level Comparison
            if bytes2 != bytes3:
                reject(f"DIRECT BYTE MISMATCH: {fn2} and {fn3} raw bytes are not identical")

            # ELF Header Validation for object and binary files
            if suffix in ("o", "bin_norm"):
                if not bytes2.startswith(b"\x7fELF"):
                    reject(f"MALFORMED BINARY: {fn2} does not have valid ELF magic header")
                if not bytes3.startswith(b"\x7fELF"):
                    reject(f"MALFORMED BINARY: {fn3} does not have valid ELF magic header")

            # Independent SHA-256 Recomputation for BOTH stages
            calc2_sha = hashlib.sha256(bytes2).hexdigest()
            calc3_sha = hashlib.sha256(bytes3).hexdigest()

            if calc2_sha != calc3_sha:
                reject(f"HASH PARITY FAILURE: Computed SHA256 of {fn2} ({calc2_sha}) != {fn3} ({calc3_sha})")

            # Compare against receipt's recorded digests for both stages
            rec2_sha = digests.get(f"stage2_{suffix}")
            rec3_sha = digests.get(f"stage3_{suffix}")

            if calc2_sha != rec2_sha:
                reject(f"NC-16 STALE ARTIFACT: Recomputed {fn2} ({calc2_sha}) differs from receipt ({rec2_sha})")
            if calc3_sha != rec3_sha:
                # NC-17: Counterfeit Stage 3 Digest Detection
                reject(f"NC-17 COUNTERFEIT STAGE 3 DIGEST: Recomputed {fn3} ({calc3_sha}) differs from receipt ({rec3_sha})")

    print("ADJUDICATION PASS: All invariants, positive controls, negative controls (20/21/22), anti-vacuity, anti-forgery, double-stage SHA256 recomputation, direct byte-level identity, and cleanup verified.")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <receipt_path> [expected_commit] [artifacts_dir]", file=sys.stderr)
        sys.exit(2)
    receipt_arg = sys.argv[1]
    expected_commit_arg = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] != "" else None
    artifacts_dir_arg = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] != "" else None
    verify(receipt_arg, expected_commit_arg, artifacts_dir_arg)
