#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

VALID_SIGNERS = {"minisign", "gpg", "cosign"}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--policy", default="fortify-verify-policy.json")
    args = parser.parse_args()

    path = Path(args.policy)
    data = json.loads(path.read_text(encoding="utf-8"))

    errors = []

    if not isinstance(data.get("require_signature"), bool):
        errors.append("require_signature must be boolean")

    allowed = data.get("allowed_signers", {})
    if not isinstance(allowed, dict):
        errors.append("allowed_signers must be object")
        allowed = {}

    unknown = set(allowed) - VALID_SIGNERS
    for signer in sorted(unknown):
        errors.append(f"unknown signer policy: {signer}")

    if data.get("require_signature") and not allowed:
        errors.append("require_signature=true requires at least one allowed signer")

    minisign = allowed.get("minisign")
    if minisign is not None:
        if not minisign.get("public_key") and not minisign.get("public_key_file"):
            errors.append("minisign requires public_key or public_key_file")

    gpg = allowed.get("gpg")
    if gpg is not None:
        fps = gpg.get("fingerprints", [])
        if not fps:
            errors.append("gpg requires at least one allowed fingerprint")
        for fp in fps:
            if len(fp) < 16:
                errors.append(f"gpg fingerprint too short: {fp}")

    cosign = allowed.get("cosign")
    if cosign is not None:
        if not cosign.get("certificate_identity"):
            errors.append("cosign requires certificate_identity")
        if not cosign.get("certificate_oidc_issuer"):
            errors.append("cosign requires certificate_oidc_issuer")

    expected = data.get("expected", {})
    if expected is not None and not isinstance(expected, dict):
        errors.append("expected must be object")

    if errors:
        for error in errors:
            print(f"[VERIFY-POLICY] {error}")
        raise SystemExit(1)

    print("[VERIFY-POLICY] valid")

if __name__ == "__main__":
    main()