#!/usr/bin/env python3
"""
ZCC OMEGA V4 — Sovereign Release Verifier (Rule ZCC-EF-0+)
=============================================================================
Independent release authority that enforces externally maintained trust policies
against raw stage artifacts and verification receipts. Does NOT trust producer
telemetry or self-reported exit codes.
"""
import argparse
import hashlib
import json
import os
import sys
from pathlib import Path

EXIT_OK = 0
EXIT_POLICY_ERROR = 10
EXIT_BYTE_MISMATCH = 11
EXIT_FORGERY_DETECTED = 12
EXIT_MISSING_ARTIFACT = 13

def reject(msg, code):
    print(f"SOVEREIGN VERIFIER REJECT: {msg}", file=sys.stderr)
    sys.exit(code)

def main():
    parser = argparse.ArgumentParser(description="ZCC OMEGA V4 Sovereign Release Verifier")
    parser.add_argument("--policy", required=True, help="Path to externally maintained sovereign trust policy JSON")
    parser.add_argument("--evidence", required=True, help="Path to evidence directory containing raw artifacts")
    parser.add_argument("--receipt", default=None, help="Optional explicit path to receipt JSON")
    parser.add_argument("--expected-commit", default=None, help="Optional expected commit SHA to bind evidence")
    args = parser.parse_args()

    policy_path = Path(args.policy).resolve()
    if not policy_path.is_file():
        reject(f"Policy file not found: {policy_path}", EXIT_POLICY_ERROR)

    try:
        policy = json.loads(policy_path.read_text())
    except Exception as e:
        reject(f"Malformed policy JSON: {e}", EXIT_POLICY_ERROR)

    evidence_dir = Path(args.evidence).resolve()
    if not evidence_dir.is_dir():
        reject(f"Evidence directory not found: {evidence_dir}", EXIT_MISSING_ARTIFACT)

    if args.receipt:
        receipt_file = Path(args.receipt).resolve()
    elif (evidence_dir / "meta_verification_receipt.json").is_file():
        receipt_file = evidence_dir / "meta_verification_receipt.json"
    elif (evidence_dir.parent / "meta_verification_receipt.json").is_file():
        receipt_file = evidence_dir.parent / "meta_verification_receipt.json"
    elif Path("meta_verification_receipt.json").is_file():
        receipt_file = Path("meta_verification_receipt.json").resolve()
    else:
        reject(f"Receipt file missing in evidence dir or repo root", EXIT_MISSING_ARTIFACT)

    if not receipt_file.is_file():
        reject(f"Receipt file missing: {receipt_file}", EXIT_MISSING_ARTIFACT)
    if os.path.islink(receipt_file):
        reject(f"Receipt file must not be a symbolic link: {receipt_file}", EXIT_POLICY_ERROR)

    try:
        receipt = json.loads(receipt_file.read_text())
    except Exception as e:
        reject(f"Malformed receipt JSON: {e}", EXIT_POLICY_ERROR)

    # 1. Negative Control Integrity & Anti-Forgery (NC-21 Detection)
    neg_policy = policy.get("negative_controls", {})
    if neg_policy.get("disallow_unconditional_pass", True):
        forbidden_rcs = set(neg_policy.get("forbidden_gate_rc_values", [0]))
        required_codes = neg_policy.get("required_rejection_codes", {})
        neg_receipt = receipt.get("negative_controls", {})

        if not neg_receipt or len(neg_receipt) == 0:
            reject("NC-14 VIOLATION: Empty negative controls collection", EXIT_FORGERY_DETECTED)

        for name, expected_rc in required_codes.items():
            ctrl = neg_receipt.get(name)
            if not ctrl:
                reject(f"Missing required negative control '{name}' in receipt", EXIT_FORGERY_DETECTED)

            observed_rc = ctrl.get("gate_rc")
            if observed_rc in forbidden_rcs:
                reject(
                    f"CRITICAL FORGERY DETECTED (NC-21): negative control '{name}' reported gate_rc={observed_rc} "
                    f"(forbidden unconditional PASS)",
                    EXIT_FORGERY_DETECTED,
                )

            if observed_rc != expected_rc:
                reject(
                    f"CRITICAL FORGERY DETECTED: negative control '{name}' reported gate_rc={observed_rc}, expected {expected_rc}",
                    EXIT_FORGERY_DETECTED,
                )

    # 2. Stage Identity & Raw Disk Byte Equality
    stage_policy = policy.get("stage_identity", {})
    if stage_policy.get("require_direct_byte_equality", True):
        stage_pairs = stage_policy.get("required_stage_pairs", [])
        digests = receipt.get("digests", {})

        for pair in stage_pairs:
            fn2 = pair.get("stage2")
            fn3 = pair.get("stage3")
            p2 = evidence_dir / fn2
            p3 = evidence_dir / fn3

            if not p2.is_file():
                reject(f"Required artifact '{fn2}' missing from evidence directory", EXIT_MISSING_ARTIFACT)
            if not p3.is_file():
                reject(f"Required artifact '{fn3}' missing from evidence directory", EXIT_MISSING_ARTIFACT)

            if os.path.islink(p2) or os.path.islink(p3):
                reject(f"Artifacts must not be symbolic links: {fn2} or {fn3}", EXIT_POLICY_ERROR)

            b2 = p2.read_bytes()
            b3 = p3.read_bytes()

            if b2 != b3:
                reject(f"RAW BYTE INEQUALITY: {fn2} and {fn3} bytes do not match", EXIT_BYTE_MISMATCH)

            sha2 = hashlib.sha256(b2).hexdigest()
            sha3 = hashlib.sha256(b3).hexdigest()

            if sha2 != sha3:
                reject(f"DIGEST MISMATCH: {fn2} ({sha2}) != {fn3} ({sha3})", EXIT_BYTE_MISMATCH)

            # Cross-check against receipt digests if available
            rec2 = digests.get(f"stage2_{pair.get('name')}")
            rec3 = digests.get(f"stage3_{pair.get('name')}")
            if rec2 and rec2 != sha2:
                reject(f"STALE DIGEST: Recomputed {fn2} ({sha2}) != receipt ({rec2})", EXIT_BYTE_MISMATCH)
            if rec3 and rec3 != sha3:
                reject(f"COUNTERFEIT STAGE 3 DIGEST: Recomputed {fn3} ({sha3}) != receipt ({rec3})", EXIT_BYTE_MISMATCH)

    # 3. Cleanliness Enforcement
    clean_policy = policy.get("cleanliness", {})
    if clean_policy.get("require_workdir_cleaned", True):
        cleanup = receipt.get("cleanup", {})
        if not cleanup.get("harness_workdir_cleaned"):
            reject("Workdir cleanup was not verified (EF-03 violation)", EXIT_POLICY_ERROR)

    # 4. Lineage Binding
    if args.expected_commit:
        prov = receipt.get("provenance", {})
        rec_commit = prov.get("git_commit")
        if rec_commit != args.expected_commit:
            reject(f"LINEAGE MISMATCH: Receipt commit {rec_commit} != expected {args.expected_commit}", EXIT_POLICY_ERROR)

    print("SOVEREIGN RELEASE POLICY VERIFIED: PASS", file=sys.stdout)
    sys.exit(EXIT_OK)

if __name__ == "__main__":
    main()
