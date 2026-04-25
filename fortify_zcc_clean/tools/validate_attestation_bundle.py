#!/usr/bin/env python3
"""Structural validator for fortify attestation bundles.

Checks the bundle's shape, required keys, and signing-status invariants.
Does NOT re-hash artifacts (that's verify_attestation_bundle.py).
"""
import argparse
import json
import sys
from pathlib import Path

REQUIRED_TOP = [
    "kind",
    "status",
    "created_at",
    "subject",
    "artifact_hashes",
    "signing",
    "signing_status_file",
    "host",
]
REQUIRED_FILE_ENTRY = ["path", "sha256", "bytes"]
REQUIRED_SIGNING = ["kind", "status", "signer", "subject", "required"]
VALID_SIGNING_STATUSES = {"signed", "skipped"}
VALID_SIGNERS = {"", "minisign", "gpg", "cosign"}


def check_required(obj: dict, keys: list, label: str, errors: list) -> None:
    if not isinstance(obj, dict):
        errors.append(f"{label} must be an object, got {type(obj).__name__}")
        return
    for key in keys:
        if key not in obj:
            errors.append(f"{label} missing key: {key}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle", required=True)
    args = parser.parse_args()

    path = Path(args.bundle)
    if not path.exists():
        print(f"[ATTESTATION-VALIDATE] bundle missing: {path}", file=sys.stderr)
        return 1

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        print(f"[ATTESTATION-VALIDATE] bundle is not valid JSON: {exc}", file=sys.stderr)
        return 1

    errors = []

    # Top level
    check_required(data, REQUIRED_TOP, "bundle", errors)
    if data.get("kind") != "fortify-attestation-bundle":
        errors.append(
            f"kind must be 'fortify-attestation-bundle', got {data.get('kind')!r}"
        )
    if data.get("status") != "pass":
        errors.append(f"status must be 'pass', got {data.get('status')!r}")

    # Subject is a file_entry
    subject = data.get("subject")
    if subject is None:
        errors.append("subject is required")
    else:
        check_required(subject, REQUIRED_FILE_ENTRY, "subject", errors)

    # Signing
    signing = data.get("signing")
    if signing is None:
        errors.append("signing is required")
    else:
        check_required(signing, REQUIRED_SIGNING, "signing", errors)
        sig_status = signing.get("status")
        if sig_status not in VALID_SIGNING_STATUSES:
            errors.append(
                f"signing.status must be one of {sorted(VALID_SIGNING_STATUSES)}, "
                f"got {sig_status!r}"
            )
        signer = signing.get("signer")
        if signer not in VALID_SIGNERS:
            errors.append(
                f"signing.signer must be one of {sorted(VALID_SIGNERS)}, got {signer!r}"
            )
        if not isinstance(signing.get("required"), bool):
            errors.append("signing.required must be a boolean")

        # Cross-field invariants
        if sig_status == "signed":
            sig_entry = data.get("signature")
            if not sig_entry:
                errors.append("signature entry is required when signing.status='signed'")
            else:
                check_required(sig_entry, REQUIRED_FILE_ENTRY, "signature", errors)
            if signer == "cosign":
                cert_entry = data.get("certificate")
                if not cert_entry:
                    errors.append("certificate entry is required for cosign signing")
                else:
                    check_required(cert_entry, REQUIRED_FILE_ENTRY, "certificate", errors)

    # artifact_hashes
    hashes = data.get("artifact_hashes")
    if hashes is None:
        errors.append("artifact_hashes is required")
    elif not isinstance(hashes, dict):
        errors.append("artifact_hashes must be an object")
    else:
        if hashes.get("algorithm") not in (None, "sha256"):
            errors.append(
                f"artifact_hashes.algorithm must be 'sha256', got {hashes.get('algorithm')!r}"
            )
        files = hashes.get("files")
        if not isinstance(files, list):
            errors.append("artifact_hashes.files must be a list")
        else:
            for i, entry in enumerate(files):
                check_required(entry, REQUIRED_FILE_ENTRY, f"artifact_hashes.files[{i}]", errors)

    if errors:
        for e in errors:
            print(f"[ATTESTATION-VALIDATE] {e}", file=sys.stderr)
        return 1

    print(f"[ATTESTATION-VALIDATE] bundle structurally valid: {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
