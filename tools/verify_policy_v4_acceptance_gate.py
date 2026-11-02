#!/usr/bin/env python3
"""
ZCC v17.7 — Release Policy v4.0.0-OMEGA Final Acceptance Gate (6/6 Checklist Verifier)
======================================================================================
Executes the 6 Independent Acceptance Checks required by the Verification Architect's
Track 2 Release Certification Adjudication:

  [1/6] Recompute gate.sh and verifier SHA-256; compare with policy pins & git HEAD
  [2/6] Authenticate policy file against signed authority revision & FAIL_CLOSED enforcement
  [3/6] Reconcile 14/14 bundle inventory cardinality (0 excluded members)
  [4/6] Independently recompute physical bundle digest and all 14 member digests
  [5/6] Verify Ed25519 signature using pinned public key and canonical payload
  [6/6] Confirm durability and signing-custody claims are scoped to threat-model limitations
"""

import argparse
import hashlib
import json
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path
from cryptography.hazmat.primitives.asymmetric import ed25519


EXPECTED_REQUIRED_14 = sorted([
    "zcc2.s",
    "zcc3.s",
    "zcc2",
    "zcc3",
    "stage2.o",
    "stage3.o",
    "stage2.bin.norm",
    "stage3.bin.norm",
    "release_manifest.json",
    "provenance.jsonl",
    "toolchain.json",
    "differential_c99_receipt.json",
    "meta_verification_receipt.json",
    "ACTIVE_GENERATION.json",
])

SIGNABLE_VERDICT_KEYS = [
    "authority",
    "authority_workflow",
    "authority_commit",
    "authority_verifier_sha256",
    "authority_policy_sha256",
    "authority_pubkey_fingerprint",
    "evaluated_producer_repo",
    "evaluated_producer_commit",
    "evaluated_producer_run_id",
    "evaluated_artifact_id",
    "evaluated_artifact_name",
    "evaluated_artifact_sha256",
    "signing_environment_provenance",
    "signed_fields_schema",
    "signed_fields",
    "verdict",
    "status",
    "adjudication_timestamp",
]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def git_cmd(repo_dir: Path, *args: str) -> str:
    res = subprocess.run(
        ["git", *args],
        cwd=str(repo_dir),
        capture_output=True,
        text=True,
        check=True,
    )
    return res.stdout


def git_blob_sha256(repo_dir: Path, rel_path: str) -> str:
    res = subprocess.run(
        ["git", "show", f"HEAD:{rel_path}"],
        cwd=str(repo_dir),
        capture_output=True,
        check=True,
    )
    return sha256_bytes(res.stdout)


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify all 6 Policy v4.0.0-OMEGA Final Acceptance Checks.")
    parser.add_argument("--repo-root", required=True, help="Producer repository root")
    parser.add_argument("--cleanroom-dir", required=True, help="Clean-room repository root (/tmp/zcc_cleanroom_repro)")
    parser.add_argument("--evidence-dir", required=True, help="Track 2 certification evidence directory")
    parser.add_argument("--out", required=True, help="Output JSON report path")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    clean_dir = Path(args.cleanroom_dir).resolve()
    evid_dir = Path(args.evidence_dir).resolve()

    policy_path = repo_root / "contracts" / "sovereign_release_policy.json"
    verdict_path = evid_dir / "release_verdict.json"
    crash_report_path = evid_dir / "publication_crash_matrix_report.json"
    deep_bundle_report_path = evid_dir / "bundle_deep_authentication_report.json"

    policy = json.loads(policy_path.read_text(encoding="utf-8"))
    verdict_doc = json.loads(verdict_path.read_text(encoding="utf-8"))

    checks = []

    # =========================================================================
    # CHECK 1: Recompute gate.sh and verifier SHA-256; compare with policy pins
    # =========================================================================
    adj_int = policy.get("adjudicator_integrity", {})
    pinned_scripts = adj_int.get("pinned_scripts", {})
    pin_gate_sh = pinned_scripts.get("gate.sh", "")
    pin_meta_py = pinned_scripts.get("tools/verify_meta_receipt.py", "")

    work_gate_sh = sha256_file(clean_dir / "gate.sh")
    work_meta_py = sha256_file(clean_dir / "tools" / "verify_meta_receipt.py")
    work_policy = sha256_file(clean_dir / "contracts" / "sovereign_release_policy.json")

    head_gate_sh = git_blob_sha256(clean_dir, "gate.sh")
    head_meta_py = git_blob_sha256(clean_dir, "tools/verify_meta_receipt.py")
    head_policy = git_blob_sha256(clean_dir, "contracts/sovereign_release_policy.json")

    clean_porcelain = git_cmd(clean_dir, "status", "--porcelain=v1").strip()
    verifier_porcelain = git_cmd(repo_root, "status", "--porcelain=v1").strip()
    clean_head = git_cmd(clean_dir, "rev-parse", "HEAD").strip()
    verifier_head = git_cmd(repo_root, "rev-parse", "HEAD").strip()
    signed_prod_commit = verdict_doc.get("evaluated_producer_commit", "")

    ver_gate_sh = sha256_file(repo_root / "gate.sh")
    ver_meta_py = sha256_file(repo_root / "tools" / "verify_meta_receipt.py")
    ver_policy = sha256_file(repo_root / "contracts" / "sovereign_release_policy.json")

    check1_ok = (
        pin_gate_sh == work_gate_sh == head_gate_sh == ver_gate_sh
        and pin_meta_py == work_meta_py == head_meta_py == ver_meta_py
        and work_policy == head_policy == ver_policy
        and clean_porcelain == ""
        and verifier_porcelain == ""
        and clean_head == signed_prod_commit
    )
    if not check1_ok:
        raise RuntimeError(
            f"Check 1 failed: pin_gate={pin_gate_sh}, work_gate={work_gate_sh}, head_gate={head_gate_sh}, "
            f"clean_porcelain={clean_porcelain!r}, ver_porcelain={verifier_porcelain!r}, "
            f"clean_head={clean_head}, ver_head={verifier_head}, signed={signed_prod_commit}"
        )

    checks.append({
        "check_id": "ACCEPTANCE-01",
        "requirement": "Recompute gate.sh and verifier SHA-256; compare with policy pins and committed HEAD",
        "pinned_gate_sh_sha256": pin_gate_sh,
        "worktree_gate_sh_sha256": work_gate_sh,
        "git_head_gate_sh_sha256": head_gate_sh,
        "pinned_verify_meta_receipt_sha256": pin_meta_py,
        "worktree_verify_meta_receipt_sha256": work_meta_py,
        "git_head_verify_meta_receipt_sha256": head_meta_py,
        "worktree_policy_sha256": work_policy,
        "git_head_policy_sha256": head_policy,
        "producer_cleanroom_git_status_porcelain": clean_porcelain,
        "verifier_cleanroom_git_status_porcelain": verifier_porcelain,
        "producer_cleanroom_git_head": clean_head,
        "verifier_cleanroom_git_head": verifier_head,
        "verdict": "PASS",
    })

    # =========================================================================
    # CHECK 2: Authenticate policy file against signed authority revision
    # =========================================================================
    policy_sha = sha256_file(policy_path)
    signed_policy_sha = verdict_doc.get("authority_policy_sha256", "")
    enforcement_mode = policy.get("enforcement_mode", "")
    require_pin = adj_int.get("require_adjudicator_hash_pinned", False)

    # Run negative control: verify sovereign_release_verifier.py rejects PERMISSIVE policy
    with tempfile.TemporaryDirectory(prefix="zcc_acc02_") as td:
        bad_pol_path = Path(td) / "bad_policy.json"
        bad_pol = json.loads(json.dumps(policy))
        bad_pol["enforcement_mode"] = "PERMISSIVE"
        bad_pol_path.write_text(json.dumps(bad_pol, indent=2), encoding="utf-8")
        neg_res = subprocess.run(
            [
                sys.executable,
                str(repo_root / "tools" / "sovereign_release_verifier.py"),
                "--evidence", str(evid_dir / "cleanroom_evidence_artifacts"),
                "--receipt", str(evid_dir / "meta_verification_receipt.json"),
                "--policy", str(bad_pol_path),
                "--expected-commit", signed_prod_commit,
                "--repo", str(repo_root),
            ],
            capture_output=True,
            text=True,
        )
        permissive_rejected = (
            neg_res.returncode == 10
            and "enforcement_mode='FAIL_CLOSED'" in (neg_res.stdout + neg_res.stderr)
        )

    check2_ok = (
        policy_sha == signed_policy_sha
        and enforcement_mode == "FAIL_CLOSED"
        and require_pin is True
        and permissive_rejected
    )
    if not check2_ok:
        raise RuntimeError(
            f"Check 2 failed: policy_sha={policy_sha}, signed={signed_policy_sha}, "
            f"mode={enforcement_mode}, neg={permissive_rejected}"
        )

    checks.append({
        "check_id": "ACCEPTANCE-02",
        "requirement": "Authenticate policy file against signed authority revision and enforce FAIL_CLOSED",
        "policy_sha256": policy_sha,
        "signed_authority_policy_sha256": signed_policy_sha,
        "authority_repo": verdict_doc.get("authority"),
        "authority_commit": verdict_doc.get("authority_commit"),
        "authority_verifier_sha256": verdict_doc.get("authority_verifier_sha256"),
        "enforcement_mode": enforcement_mode,
        "require_adjudicator_hash_pinned": require_pin,
        "permissive_mode_negative_control_exit_10": permissive_rejected,
        "verdict": "PASS",
    })

    # =========================================================================
    # CHECK 3: Reconcile 14/14 bundle inventory cardinality
    # =========================================================================
    bundle_name = verdict_doc.get("evaluated_artifact_name", "")
    bundle_path = evid_dir / bundle_name
    if not bundle_path.is_file():
        raise RuntimeError(f"Missing release bundle at {bundle_path}")

    with tarfile.open(bundle_path, "r:gz") as tf:
        file_members = [m for m in tf.getmembers() if m.isfile()]
        member_names = sorted([Path(m.name).name for m in file_members])

    deep_report = json.loads(deep_bundle_report_path.read_text(encoding="utf-8"))
    primary_res = deep_report.get("authenticated_bundle", deep_report)
    card = primary_res.get("cardinality_accounting", {})

    check3_ok = (
        len(file_members) == 14
        and member_names == EXPECTED_REQUIRED_14
        and card.get("physical_archive_members") == 14
        and card.get("expected_manifest_members") == 14
        and card.get("independently_authenticated_members") == 14
        and card.get("excluded_count") == 0
        and card.get("excluded_or_unauthenticated_members") == []
        and "ACTIVE_GENERATION.json" in primary_res.get("member_inventory", {})
    )
    if not check3_ok:
        raise RuntimeError(f"Check 3 failed: len={len(file_members)}, card={card}")

    checks.append({
        "check_id": "ACCEPTANCE-03",
        "requirement": "Reconcile 14/14 bundle inventory cardinality with 0 excluded members",
        "physical_archive_members": 14,
        "expected_manifest_members": 14,
        "independently_authenticated_members": 14,
        "excluded_or_unauthenticated_members": 0,
        "fourteenth_member_reconciled": "ACTIVE_GENERATION.json (promoted to REQUIRED_BUNDLE_MEMBERS and cross-verified against release_manifest.json)",
        "members": member_names,
        "verdict": "PASS",
    })

    # =========================================================================
    # CHECK 4: Independently recompute physical bundle digest and member digests
    # =========================================================================
    recomputed_bundle_sha = sha256_file(bundle_path)
    signed_bundle_sha = verdict_doc.get("evaluated_artifact_sha256", "")

    extracted_digests = {}
    extracted_bytes = {}
    with tarfile.open(bundle_path, "r:gz") as tf:
        for m in tf.getmembers():
            if m.isfile():
                bname = Path(m.name).name
                fobj = tf.extractfile(m)
                assert fobj is not None
                raw_b = fobj.read()
                extracted_bytes[bname] = raw_b
                extracted_digests[bname] = sha256_bytes(raw_b)

    manifest_inside = json.loads(extracted_bytes["release_manifest.json"].decode("utf-8"))
    active_inside = json.loads(extracted_bytes["ACTIVE_GENERATION.json"].decode("utf-8"))
    receipt_inside = json.loads(extracted_bytes["meta_verification_receipt.json"].decode("utf-8"))

    check4_ok = (
        recomputed_bundle_sha == signed_bundle_sha
        and len(extracted_digests) == 14
        and extracted_digests["release_manifest.json"] == active_inside["manifest_sha256"]
        and extracted_digests["zcc2"] == extracted_digests["zcc3"] == manifest_inside["artifacts"]["zcc2"]
        and extracted_digests["zcc2.s"] == extracted_digests["zcc3.s"] == manifest_inside["artifacts"]["zcc2.s"]
        and receipt_inside["provenance"]["git_commit"] == signed_prod_commit
    )
    if not check4_ok:
        raise RuntimeError("Check 4 failed: bundle/member digest mismatch")

    checks.append({
        "check_id": "ACCEPTANCE-04",
        "requirement": "Independently recompute physical bundle digest and all 14 member digests",
        "bundle_filename": bundle_path.name,
        "recomputed_bundle_sha256": recomputed_bundle_sha,
        "signed_bundle_sha256": signed_bundle_sha,
        "member_digests": extracted_digests,
        "verdict": "PASS",
    })

    # =========================================================================
    # CHECK 5: Verify Ed25519 signature using pinned public key & canonical payload
    # =========================================================================
    sig_obj = verdict_doc["ed25519_signature"]
    raw_pub = bytes.fromhex(sig_obj["public_key_raw_hex"])
    pub_fp = sha256_bytes(raw_pub)
    declared_signed_fields = verdict_doc.get("signed_fields", SIGNABLE_VERDICT_KEYS)
    unsigned_top_fields = set(verdict_doc.keys()) - {"canonical_payload_sha256", "ed25519_signature"} - set(declared_signed_fields)
    signable_payload = {k: verdict_doc[k] for k in declared_signed_fields}
    canonical_bytes = json.dumps(signable_payload, sort_keys=True, indent=2).encode("utf-8")
    canonical_sha = sha256_bytes(canonical_bytes)
    sig_bytes = bytes.fromhex(sig_obj["signature_hex"])
    pub_key = ed25519.Ed25519PublicKey.from_public_bytes(raw_pub)
    pub_key.verify(sig_bytes, canonical_bytes)

    custody_prov = verdict_doc.get("signing_environment_provenance", {})
    prod_priv_absent = not (repo_root / "contracts" / "authority_ed25519_private.pem").exists()

    check5_ok = (
        pub_fp == verdict_doc.get("authority_pubkey_fingerprint")
        and canonical_sha == verdict_doc.get("canonical_payload_sha256") == sig_obj.get("signed_canonical_payload_sha256")
        and len(sig_bytes) == 64
        and prod_priv_absent is True
        and custody_prov.get("block_device") == "/dev/sde"
        and len(unsigned_top_fields) == 0
        and sorted(declared_signed_fields) == sorted(SIGNABLE_VERDICT_KEYS)
    )
    if not check5_ok:
        raise RuntimeError(f"Check 5 failed: Ed25519 or custody check mismatch (unsigned={unsigned_top_fields})")

    checks.append({
        "check_id": "ACCEPTANCE-05",
        "requirement": "Verify Ed25519 signature using pinned public key and canonical payload (100% top-level field coverage)",
        "public_key_fingerprint_sha256": pub_fp,
        "canonical_payload_sha256": canonical_sha,
        "signed_fields_schema": verdict_doc.get("signed_fields_schema"),
        "signed_fields_count": len(declared_signed_fields),
        "unsigned_top_level_fields_count": len(unsigned_top_fields),
        "producer_commit_signed": signed_prod_commit,
        "signature_bytes_len": len(sig_bytes),
        "producer_private_key_absent_on_dev_sdd": prod_priv_absent,
        "authority_block_device": custody_prov.get("block_device"),
        "verdict": "PASS",
    })

    # =========================================================================
    # CHECK 6: Confirm durability & signing-custody claims scoped to threat model
    # =========================================================================
    crash_doc = json.loads(crash_report_path.read_text(encoding="utf-8"))
    cal_stmt = crash_doc.get("calibrated_assurance_statement", "")
    dur_bound = crash_doc.get("durability_vs_recovery_boundary", {})
    storage_lim = dur_bound.get("storage_stack_limitation", "")
    shared_host_disc = custody_prov.get("shared_host_boundary_disclosure", "")

    check6_ok = (
        cal_stmt == "Crash-recovery and simulated-storage-corruption controls passed the reported 14-scenario matrix."
        and "does NOT certify physical host power-loss durability" in storage_lim
        and "same Windows hypervisor" in shared_host_disc
    )
    if not check6_ok:
        raise RuntimeError(f"Check 6 failed: cal_stmt={cal_stmt!r}")

    checks.append({
        "check_id": "ACCEPTANCE-06",
        "requirement": "Confirm durability and signing-custody claims are scoped to actual threat-model limitations",
        "calibrated_durability_statement": cal_stmt,
        "storage_stack_limitation": storage_lim,
        "signing_custody_boundary": shared_host_disc,
        "verdict": "PASS",
    })

    report = {
        "schema": "zcc-policy-v4-final-acceptance-gate-v1",
        "policy_version": policy.get("policy_version"),
        "producer_commit": signed_prod_commit,
        "authority_commit": verdict_doc.get("authority_commit"),
        "checks_evaluated": len(checks),
        "checks_passed": len(checks),
        "verdict": "PASS",
        "checks": checks,
    }

    out_p = Path(args.out)
    out_p.parent.mkdir(parents=True, exist_ok=True)
    out_p.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print("================================================================================")
    print("🏛️  POLICY v4.0.0-OMEGA FINAL ACCEPTANCE GATE: ALL 6/6 CHECKS PASSED")
    print(f"   [1/6] Pinned Script & HEAD Reconciliation: PASS (gate.sh={pin_gate_sh[:16]}..., porcelain='')")
    print(f"   [2/6] Policy Auth & FAIL_CLOSED Control:   PASS (policy={policy_sha[:16]}..., PERMISSIVE->exit 10)")
    print("   [3/6] Bundle Inventory Cardinality:        PASS (physical=14, expected=14, authenticated=14/14, excluded=0)")
    print(f"   [4/6] Bundle & 14-Member SHA-256 Recomputation: PASS (bundle={recomputed_bundle_sha[:16]}...)")
    print(f"   [5/6] RFC 8032 Ed25519 Signature & Custody: PASS (payload={canonical_sha[:16]}..., pub={pub_fp[:16]}...)")
    print(f"   [6/6] Threat-Model Claim Calibration:      PASS ('{cal_stmt}')")
    print("================================================================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
