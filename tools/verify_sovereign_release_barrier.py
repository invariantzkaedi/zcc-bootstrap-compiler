#!/usr/bin/env python3
"""
🛡️ SOVEREIGN RELEASE ENFORCEMENT BARRIER (Rule ZCC-EF-0)
================================================================================
Prevents any producer-side build or repository commit from releasing software
without an authentic, cryptographically bound attestation from the independent
external release authority (invariantzkaedi/zcc-release-authority).

Verifies the full lineage tuple:
  producer_repo -> producer_commit -> producer_run_id -> artifact_id ->
  artifact_sha256 -> authority_commit -> verifier_sha256 -> policy_sha256 ->
  verdict -> signature
================================================================================
"""

import sys
import json
import hashlib
import argparse
from pathlib import Path

EXIT_OK = 0
EXIT_RELEASE_BLOCKED = 1
EXIT_USAGE_ERROR = 2

def compute_file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def main():
    parser = argparse.ArgumentParser(
        description="Verify independent release authority attestation and enforce release barrier."
    )
    parser.add_argument("--verdict", required=True, help="Path to authority release_verdict.json")
    parser.add_argument("--artifact", required=True, help="Path to producer artifact bundle (.zip or .tar.gz)")
    parser.add_argument("--expected-commit", required=True, help="Expected producer Git commit SHA")
    parser.add_argument("--expected-run-id", required=False, help="Expected producer GitHub Actions run ID")
    parser.add_argument("--expected-authority", default="invariantzkaedi/zcc-release-authority", help="Expected authority repo")

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

    # 1. Authority Provenance Check
    authority = verdict.get("authority")
    if authority != args.expected_authority:
        print(f"RELEASE BLOCKED: Untrusted release authority '{authority}' != expected '{args.expected_authority}'", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # 2. Producer Commit Binding Check
    prod_commit = str(verdict.get("evaluated_producer_commit", "")).strip().lower()
    exp_commit = str(args.expected_commit).strip().lower()
    if prod_commit != exp_commit:
        print(f"RELEASE BLOCKED: Lineage commit mismatch! Verdict certified commit '{prod_commit}', but releasing '{exp_commit}'", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # 3. Producer Run ID Binding Check
    if args.expected_run_id:
        prod_run = str(verdict.get("evaluated_producer_run_id", "")).strip()
        exp_run = str(args.expected_run_id).strip()
        if prod_run != exp_run:
            print(f"RELEASE BLOCKED: Workflow run ID mismatch! Verdict certified run '{prod_run}', but releasing '{exp_run}'", file=sys.stderr)
            sys.exit(EXIT_RELEASE_BLOCKED)

    # 4. Artifact SHA-256 Digest Verification
    actual_art_sha = compute_file_sha256(artifact_path)
    cert_art_sha = str(verdict.get("evaluated_artifact_sha256", "")).strip().lower()
    if actual_art_sha != cert_art_sha:
        print(f"RELEASE BLOCKED: Artifact digest mismatch! Physical artifact SHA-256 '{actual_art_sha}' != certified '{cert_art_sha}'", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # 5. Cryptographic Binding Signature Integrity
    producer_repo = verdict.get("evaluated_producer_repo", "")
    producer_run_id = verdict.get("evaluated_producer_run_id", "")
    artifact_id = verdict.get("evaluated_artifact_id", "")
    authority_commit = verdict.get("authority_commit", "")
    verifier_sha = verdict.get("authority_verifier_sha256", "")
    policy_sha = verdict.get("authority_policy_sha256", "")
    verdict_str = verdict.get("verdict", "")
    timestamp = verdict.get("adjudication_timestamp", "")
    claimed_sig = verdict.get("cryptographic_binding_signature", "")

    reconstructed_payload = f"{producer_repo}:{prod_commit}:{producer_run_id}:{artifact_id}:{cert_art_sha}:{authority_commit}:{verifier_sha}:{policy_sha}:{verdict_str}:{timestamp}"
    expected_sig = hashlib.sha256(reconstructed_payload.encode("utf-8")).hexdigest()

    if claimed_sig != expected_sig:
        print("RELEASE BLOCKED: Cryptographic binding signature invalid or forged!", file=sys.stderr)
        print(f"  Claimed:  {claimed_sig}", file=sys.stderr)
        print(f"  Expected: {expected_sig}", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    # 6. Sovereign Release Verdict Check
    if verdict_str != "SOVEREIGN_RELEASE_CERTIFIED" or verdict.get("status") != "success":
        print(f"RELEASE BLOCKED: Sovereign authority verdict is '{verdict_str}' (status: '{verdict.get('status')}'). Deployment prohibited.", file=sys.stderr)
        sys.exit(EXIT_RELEASE_BLOCKED)

    print("================================================================================")
    print("✨ SOVEREIGN RELEASE BARRIER: ACCESS GRANTED")
    print(f"   Authority:         {authority} ({authority_commit[:10]})")
    print(f"   Producer Commit:   {prod_commit}")
    print(f"   Producer Run ID:   {producer_run_id}")
    print(f"   Artifact SHA-256:  {actual_art_sha}")
    print(f"   Binding Signature: {claimed_sig}")
    print("   Verdict:           SOVEREIGN_RELEASE_CERTIFIED (PASS)")
    print("================================================================================")
    sys.exit(EXIT_OK)

if __name__ == "__main__":
    main()
