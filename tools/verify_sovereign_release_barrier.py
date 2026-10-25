#!/usr/bin/env python3
"""
🛡️ SOVEREIGN RELEASE ENFORCEMENT BARRIER (Rule ZCC-EF-0 & RFC 8032)
================================================================================
Prevents any producer-side build or repository commit from releasing software
without an authentic, cryptographically bound asymmetric attestation from the
independent external release authority (invariantzkaedi/zcc-release-authority).

Enforces asymmetric non-repudiation:
  - Cryptographically verifies the Ed25519 digital signature of the independent
    release authority using the pinned authority public key.
  - Strictly rejects unkeyed digests, modified verdicts, altered artifacts,
    unauthorized signers, and failed/rejected authority runs.
  - Binds the complete lineage tuple:
      producer_repo -> producer_commit -> producer_run_id -> artifact_id ->
      artifact_sha256 -> authority_repo -> authority_commit -> verifier_sha256 ->
      policy_sha256 -> pubkey_fingerprint -> verdict -> ed25519_signature
================================================================================
"""

import os
import sys
import json
import hashlib
import argparse
from pathlib import Path
from cryptography.hazmat.primitives.asymmetric import ed25519
from cryptography.hazmat.primitives import serialization
from cryptography.exceptions import InvalidSignature

EXIT_OK = 0
EXIT_RELEASE_BLOCKED = 1
EXIT_USAGE_ERROR = 2

# Canonical pinned authority public key fingerprint
CANONICAL_AUTHORITY_FINGERPRINT = "261d16d284351349d7ee2c27ec5ad9061917dace09d62a7f49e1f63f7e5527f5"

def compute_file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def load_authority_public_key(pubkey_path_str: str = None):
    # 1. Direct path argument
    if pubkey_path_str and Path(pubkey_path_str).is_file():
        pub_bytes = Path(pubkey_path_str).read_bytes()
        pubkey = serialization.load_pem_public_key(pub_bytes)
        raw_pub = pubkey.public_bytes(
            encoding=serialization.Encoding.Raw,
            format=serialization.PublicFormat.Raw
        )
        return pubkey, hashlib.sha256(raw_pub).hexdigest()

    # 2. Try default repository paths
    candidates = [
        Path(__file__).resolve().parent.parent / "keys" / "authority_ed25519_public.pem",
        Path(__file__).resolve().parent.parent / "contracts" / "authority_ed25519_public.pem",
        Path("/home/zkaedi/zcc-release-authority/keys/authority_ed25519_public.pem"),
        Path("/mnt/h/__DOWNLOADS/zcc_github_upload/contracts/authority_ed25519_public.pem"),
    ]
    for c in candidates:
        if c.is_file():
            pub_bytes = c.read_bytes()
            pubkey = serialization.load_pem_public_key(pub_bytes)
            raw_pub = pubkey.public_bytes(
                encoding=serialization.Encoding.Raw,
                format=serialization.PublicFormat.Raw
            )
            return pubkey, hashlib.sha256(raw_pub).hexdigest()

    raise ValueError("Authority Ed25519 public key not found!")

def main():
    parser = argparse.ArgumentParser(
        description="Verify independent release authority attestation and enforce sovereign release barrier."
    )
    parser.add_argument("--verdict", required=True, help="Path to authority release_verdict.json")
    parser.add_argument("--artifact", required=True, help="Path to producer artifact bundle (.zip or .tar.gz)")
    parser.add_argument("--expected-commit", required=True, help="Expected producer Git commit SHA")
    parser.add_argument("--expected-run-id", required=True, help="Expected producer GitHub Actions run ID (V4-M5-17)")
    parser.add_argument("--expected-producer-repo", default="invariantzkaedi/zcc-bootstrap-compiler", help="Expected producer repo (V4-M5-18)")
    parser.add_argument("--expected-authority", default="invariantzkaedi/zcc-release-authority", help="Expected authority repo (V4-M5-18)")
    parser.add_argument("--expected-authority-commit", required=False, help="Expected authority commit SHA (V4-M5-18)")
    parser.add_argument("--authority-pubkey", required=False, help="Path to authority Ed25519 public key")
    parser.add_argument("--pinned-fingerprint", default=CANONICAL_AUTHORITY_FINGERPRINT, help="Expected public key fingerprint")

    args = parser.parse_args()

    verdict_path = Path(args.verdict)
    artifact_path = Path(args.artifact)

    if not verdict_path.is_file():
        print(f"RELEASE BLOCKED: Missing authority verdict file: {verdict_path}", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    if not artifact_path.is_file():
        print(f"RELEASE BLOCKED: Missing artifact bundle file: {artifact_path}", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    try:
        verdict = json.loads(verdict_path.read_text(encoding="utf-8"))
    except Exception as e:
        print(f"RELEASE BLOCKED: Malformed authority verdict JSON: {e}", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # 1. Authority Provenance Check (V4-M5-18)
    authority = str(verdict.get("authority", "")).strip()
    if authority != args.expected_authority:
        print(f"RELEASE BLOCKED: Untrusted release authority '{authority}' != expected '{args.expected_authority}'", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # 2. Producer Repository Binding Check (V4-M5-18)
    prod_repo = str(verdict.get("evaluated_producer_repo", "")).strip()
    if prod_repo != args.expected_producer_repo:
        print(f"RELEASE BLOCKED: Producer repo mismatch! Certified repo '{prod_repo}' != expected '{args.expected_producer_repo}'", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # 3. Producer Commit Binding Check
    prod_commit = str(verdict.get("evaluated_producer_commit", "")).strip().lower()
    exp_commit = str(args.expected_commit).strip().lower()
    if prod_commit != exp_commit:
        print(f"RELEASE BLOCKED: Lineage commit mismatch! Verdict certified commit '{prod_commit}', but releasing '{exp_commit}'", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # 4. Producer Run ID Binding Check (V4-M5-17)
    prod_run = str(verdict.get("evaluated_producer_run_id", "")).strip()
    exp_run = str(args.expected_run_id).strip()
    if prod_run != exp_run:
        print(f"RELEASE BLOCKED: Workflow run ID mismatch! Verdict certified run '{prod_run}', but releasing '{exp_run}'", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # 5. Authority Revision Check (if specified) (V4-M5-18)
    if args.expected_authority_commit:
        auth_commit = str(verdict.get("authority_commit", "")).strip().lower()
        exp_auth = str(args.expected_authority_commit).strip().lower()
        if auth_commit != exp_auth:
            print(f"RELEASE BLOCKED: Authority revision mismatch! Certified revision '{auth_commit}' != expected '{exp_auth}'", file=sys.stderr)
            sys.exit(EXIT_RELEASE_BLOCKED)

    # 6. Artifact SHA-256 Digest Verification
    actual_art_sha = compute_file_sha256(artifact_path)
    cert_art_sha = str(verdict.get("evaluated_artifact_sha256", "")).strip().lower()
    if actual_art_sha != cert_art_sha:
        print(f"RELEASE BLOCKED: Artifact digest mismatch! Physical artifact SHA-256 '{actual_art_sha}' != certified '{cert_art_sha}'", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # 7. Asymmetric Ed25519 Cryptographic Signature Verification (V4-M5-15)
    sig_obj = verdict.get("ed25519_signature")
    if not sig_obj:
        print("RELEASE BLOCKED: No cryptographic Ed25519 signature found in verdict! Unkeyed or unsigned verdicts are strictly rejected.", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    try:
        pubkey, actual_fingerprint = load_authority_public_key(args.authority_pubkey)
    except Exception as e:
        print(f"RELEASE BLOCKED: Unable to load trusted authority Ed25519 public key: {e}", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    if args.pinned_fingerprint and actual_fingerprint != args.pinned_fingerprint:
        print(f"RELEASE BLOCKED: Authority public key fingerprint mismatch! Found '{actual_fingerprint}' != pinned '{args.pinned_fingerprint}'", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    claimed_sig_hex = sig_obj.get("signature_hex", "")
    try:
        signature_bytes = bytes.fromhex(claimed_sig_hex)
    except Exception:
        print(f"RELEASE BLOCKED: Malformed hex in Ed25519 signature: {claimed_sig_hex}", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # Reconstruct canonical signable payload binding the full lineage tuple
    signable_payload = {
        "authority": authority,
        "authority_workflow": str(verdict.get("authority_workflow", "")).strip(),
        "authority_commit": str(verdict.get("authority_commit", "")).strip(),
        "authority_verifier_sha256": str(verdict.get("authority_verifier_sha256", "")).strip(),
        "authority_policy_sha256": str(verdict.get("authority_policy_sha256", "")).strip(),
        "authority_pubkey_fingerprint": actual_fingerprint,
        "evaluated_producer_repo": prod_repo,
        "evaluated_producer_commit": prod_commit,
        "evaluated_producer_run_id": prod_run,
        "evaluated_artifact_id": str(verdict.get("evaluated_artifact_id", "")).strip(),
        "evaluated_artifact_sha256": cert_art_sha,
        "verdict": str(verdict.get("verdict", "")).strip(),
        "status": str(verdict.get("status", "")).strip(),
        "adjudication_timestamp": str(verdict.get("adjudication_timestamp", "")).strip(),
    }
    canonical_bytes = json.dumps(signable_payload, sort_keys=True, indent=2).encode("utf-8")
    expected_payload_sha = hashlib.sha256(canonical_bytes).hexdigest()

    signed_payload_sha = sig_obj.get("signed_canonical_payload_sha256", "")
    if expected_payload_sha != signed_payload_sha:
        print("RELEASE BLOCKED: Canonical payload digest mismatch! Tampering of lineage tuple detected.", file=sys.stderr)
        print(f"  Reconstructed Payload SHA: {expected_payload_sha}", file=sys.stderr)
        print(f"  Signed Payload SHA:        {signed_payload_sha}", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    try:
        pubkey.verify(signature_bytes, canonical_bytes)
    except InvalidSignature:
        print("RELEASE BLOCKED: Cryptographic Ed25519 signature verification failed! (Invalid or forged signature)", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)
    except Exception as e:
        print(f"RELEASE BLOCKED: Cryptographic verification error: {e}", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # 8. Sovereign Release Verdict Check
    verdict_str = signable_payload["verdict"]
    status_str = signable_payload["status"]
    if verdict_str != "SOVEREIGN_RELEASE_CERTIFIED" or status_str != "success":
        print(f"RELEASE BLOCKED: Sovereign authority verdict is '{verdict_str}' (status: '{status_str}'). Deployment prohibited.", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    print("================================================================================")
    print("✨ SOVEREIGN RELEASE BARRIER: ACCESS GRANTED")
    print(f"   Authority:               {authority} ({signable_payload['authority_commit'][:10]})")
    print(f"   Authority Pubkey Hash:   {actual_fingerprint}")
    print(f"   Producer Repo:           {prod_repo}")
    print(f"   Producer Commit:         {prod_commit}")
    print(f"   Producer Run ID:         {prod_run}")
    print(f"   Artifact SHA-256:        {actual_art_sha}")
    print(f"   Ed25519 Digital Sig:     {claimed_sig_hex[:32]}... [RFC 8032 VERIFIED]")
    print(f"   Verdict:                 SOVEREIGN_RELEASE_CERTIFIED (PASS)")
    print("================================================================================")
    sys.exit(EXIT_OK)

if __name__ == "__main__":
    main()
