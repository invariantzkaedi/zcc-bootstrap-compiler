#!/usr/bin/env python3
"""Verify a fortify attestation bundle end-to-end.

Always checks:
  1. Bundle structure (top-level required keys, kind, status).
  2. Subject hash matches what's recorded in the bundle.
  3. Every artifact_hashes entry exists on disk and re-hashes to the recorded value.
  4. If signing.status == "signed", signature/certificate file hashes match recorded.

Optional gates:
  --require-signature              fail unless signing.status == "signed"
  --verify-policy <path>           cross-check signer/expected fields against policy
  --expected-git-commit <sha>      fail unless subject.git.commit matches
  --expected-target <triple>       fail unless subject.target matches
  --expected-policy-sha <sha256>   fail unless subject.policy.sha256 matches
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def check_required_keys(obj: dict, required: list, label: str, errors: list) -> None:
    for key in required:
        if key not in obj:
            errors.append(f"{label} missing key: {key}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle", required=True)
    parser.add_argument("--artifact-root", required=True)
    parser.add_argument("--verify-policy", default=None)
    parser.add_argument("--require-signature", action="store_true")
    parser.add_argument("--expected-git-commit", default=None)
    parser.add_argument("--expected-target", default=None)
    parser.add_argument("--expected-policy-sha", default=None)
    args = parser.parse_args()

    bundle_path = Path(args.bundle)
    if not bundle_path.exists():
        print(f"[FORTIFY-VERIFY] bundle missing: {bundle_path}", file=sys.stderr)
        return 1

    try:
        bundle = load_json(bundle_path)
    except json.JSONDecodeError as exc:
        print(f"[FORTIFY-VERIFY] bundle is not valid JSON: {exc}", file=sys.stderr)
        return 1

    errors = []

    # --- 1. structure ---
    check_required_keys(
        bundle,
        ["kind", "status", "created_at", "subject", "artifact_hashes",
         "signing", "signing_status_file", "host"],
        "bundle",
        errors,
    )
    if bundle.get("kind") != "fortify-attestation-bundle":
        errors.append(
            f"bundle.kind must be 'fortify-attestation-bundle', got {bundle.get('kind')!r}"
        )
    if bundle.get("status") != "pass":
        errors.append(f"bundle.status must be 'pass', got {bundle.get('status')!r}")

    # --- 2. subject hash re-verification ---
    subject_entry = bundle.get("subject") or {}
    subject_path_str = subject_entry.get("path", "")
    subject_path = Path(subject_path_str) if subject_path_str else None
    subject_data = None

    if not subject_path or not subject_path.exists():
        errors.append(f"subject artifact missing on disk: {subject_path_str}")
    else:
        actual = sha256_file(subject_path)
        expected = subject_entry.get("sha256")
        if expected is None:
            errors.append("bundle.subject.sha256 missing")
        elif actual != expected:
            errors.append(
                f"subject hash mismatch: expected={expected} actual={actual} path={subject_path}"
            )
        try:
            subject_data = load_json(subject_path)
        except json.JSONDecodeError as exc:
            errors.append(f"subject is not valid JSON: {exc}")

    # --- 3. artifact_hashes re-verification ---
    artifact_root = Path(args.artifact_root)
    hashes = bundle.get("artifact_hashes") or {}
    if hashes.get("algorithm") not in (None, "sha256"):
        errors.append(f"unsupported artifact_hashes.algorithm: {hashes.get('algorithm')!r}")

    for entry in hashes.get("files", []):
        rel = entry.get("path")
        if not rel:
            errors.append("artifact_hashes entry missing 'path'")
            continue
        target = artifact_root / rel
        if not target.exists():
            errors.append(f"artifact missing on disk: {target}")
            continue
        actual = sha256_file(target)
        expected = entry.get("sha256")
        if actual != expected:
            errors.append(
                f"artifact hash mismatch: {target}: expected={expected} actual={actual}"
            )

    # --- 4. signing status & signature artifact integrity ---
    signing = bundle.get("signing") or {}
    sig_status = signing.get("status")

    if args.require_signature and sig_status != "signed":
        errors.append(
            f"signature required: --require-signature given but signing.status is {sig_status!r}"
        )

    if sig_status == "signed":
        sig_entry = bundle.get("signature")
        if not sig_entry:
            errors.append("signing.status='signed' but bundle.signature is null")
        else:
            sig_path = Path(sig_entry.get("path", ""))
            if not sig_path.exists():
                errors.append(f"signature file missing: {sig_path}")
            else:
                actual = sha256_file(sig_path)
                if actual != sig_entry.get("sha256"):
                    errors.append(
                        f"signature hash mismatch: {sig_path}: "
                        f"expected={sig_entry.get('sha256')} actual={actual}"
                    )

        if signing.get("signer") == "cosign":
            cert_entry = bundle.get("certificate")
            if not cert_entry:
                errors.append("cosign signing requires bundle.certificate")
            else:
                cert_path = Path(cert_entry.get("path", ""))
                if not cert_path.exists():
                    errors.append(f"certificate file missing: {cert_path}")
                else:
                    actual = sha256_file(cert_path)
                    if actual != cert_entry.get("sha256"):
                        errors.append(
                            f"certificate hash mismatch: {cert_path}: "
                            f"expected={cert_entry.get('sha256')} actual={actual}"
                        )

    # --- 5. expected fields against subject manifest ---
    if subject_data is not None:
        if args.expected_git_commit is not None:
            actual = (subject_data.get("git") or {}).get("commit")
            if actual != args.expected_git_commit:
                errors.append(
                    f"subject.git.commit mismatch: expected={args.expected_git_commit} actual={actual}"
                )
        if args.expected_target is not None:
            actual = subject_data.get("target")
            if actual != args.expected_target:
                errors.append(
                    f"subject.target mismatch: expected={args.expected_target} actual={actual}"
                )
        if args.expected_policy_sha is not None:
            actual = (subject_data.get("policy") or {}).get("sha256")
            if actual != args.expected_policy_sha:
                errors.append(
                    f"subject.policy.sha256 mismatch: expected={args.expected_policy_sha} actual={actual}"
                )

    # --- 6. verify-policy gate ---
    if args.verify_policy:
        policy_path = Path(args.verify_policy)
        if not policy_path.exists():
            errors.append(f"verify policy missing: {policy_path}")
        else:
            try:
                policy = load_json(policy_path)
            except json.JSONDecodeError as exc:
                errors.append(f"verify policy not valid JSON: {exc}")
                policy = {}

            if policy.get("require_signature") and sig_status != "signed":
                errors.append(
                    "verify-policy require_signature=true but bundle signing.status != 'signed'"
                )

            if sig_status == "signed":
                signer = signing.get("signer", "")
                allowed = policy.get("allowed_signers") or {}
                if signer and signer not in allowed:
                    errors.append(
                        f"signer {signer!r} not in verify-policy allowed_signers "
                        f"(allowed={sorted(allowed)})"
                    )

            expected = policy.get("expected") or {}
            if subject_data is not None:
                exp_commit = expected.get("git_commit")
                if exp_commit:
                    actual = (subject_data.get("git") or {}).get("commit")
                    if actual != exp_commit:
                        errors.append(
                            f"verify-policy expected.git_commit mismatch: "
                            f"expected={exp_commit} actual={actual}"
                        )
                exp_target = expected.get("target")
                if exp_target:
                    actual = subject_data.get("target")
                    if actual != exp_target:
                        errors.append(
                            f"verify-policy expected.target mismatch: "
                            f"expected={exp_target} actual={actual}"
                        )
                exp_policy_sha = expected.get("policy_sha256")
                if exp_policy_sha:
                    actual = (subject_data.get("policy") or {}).get("sha256")
                    if actual != exp_policy_sha:
                        errors.append(
                            f"verify-policy expected.policy_sha256 mismatch: "
                            f"expected={exp_policy_sha} actual={actual}"
                        )

    if errors:
        for e in errors:
            print(f"[FORTIFY-VERIFY] {e}", file=sys.stderr)
        return 1

    print(f"[FORTIFY-VERIFY] bundle verified: {bundle_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
