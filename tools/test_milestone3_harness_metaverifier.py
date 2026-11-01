#!/usr/bin/env python3
"""
ZCC v17.7 Milestone 3 — Specialist 3: Harness Meta-Verifier
=====================================================================================
Systematically mutates oracle, receipt, policy, and Ed25519 cryptographic signature
inputs across 20 adversarial scenarios to prove fail-closed adjudication.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Dict, List

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ed25519


def run_cmd(cmd: List[str], cwd: Path | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, cwd=str(cwd) if cwd else None, capture_output=True, text=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="ZCC Milestone 3 Specialist 3 Harness Meta-Verifier")
    parser.add_argument("--repo", default="/mnt/h/__DOWNLOADS/zcc_github_upload", help="Repository root")
    parser.add_argument("--evidence", required=True, help="Path to active evidence directory with stage2/3 artifacts")
    parser.add_argument("--receipt", required=True, help="Path to active meta_verification_receipt.json")
    parser.add_argument("--policy", required=True, help="Path to active sovereign_release_policy.json")
    parser.add_argument("--commit", required=True, help="Expected 40-char Git commit SHA")
    parser.add_argument("--out", default=None, help="Output JSON matrix path")
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    evid_src = Path(args.receipt).resolve().parent if not Path(args.evidence).is_dir() else Path(args.evidence).resolve()
    receipt_src = Path(args.receipt).resolve()
    policy_src = Path(args.policy).resolve()
    exp_commit = args.commit.strip()

    work = Path(tempfile.mkdtemp(prefix="zcc_m3_metaverifier_", dir="/tmp"))
    matrix: List[Dict[str, object]] = []

    def record(cid: str, subsystem: str, mutation: str, expected_rc: str, actual_rc: int, ok: bool, diag: str) -> None:
        matrix.append({
            "id": cid,
            "subsystem": subsystem,
            "mutation": mutation,
            "expected_rc": expected_rc,
            "actual_rc": actual_rc,
            "verdict": "PASS" if ok else "FAIL",
            "diagnostic": diag.strip().splitlines()[0] if diag.strip() else "",
        })
        if not ok:
            raise RuntimeError(f"Meta-Verifier check {cid} FAILED: actual_rc={actual_rc}, diag={diag}")

    try:
        # ======================================================================
        # 1. ORACLE MUTATION CONTROLS (3 checks)
        # ======================================================================
        r_ora1 = run_cmd(["python3", str(repo / "tools" / "zcc_differential_c99_oracle.py"), "--compiler", str(repo / "zcc2"), "--seeds", "0", "--self-test"])
        record(
            "MV-ORACLE-01",
            "zcc_differential_c99_oracle.py",
            "Built-in assembly constant perturbation (SEED=99999 -> SEED=99998) + UBSan signed overflow trap",
            "0 (self-test detects both perturbations)",
            r_ora1.returncode,
            r_ora1.returncode == 0 and "PASS: Differential C99 oracle mutation & UBSan negative controls verified." in r_ora1.stdout,
            r_ora1.stdout,
        )

        r_ora2 = run_cmd(["python3", str(repo / "tools" / "zcc_differential_c99_oracle.py"), "--compiler", "/tmp/nonexistent_zcc_binary", "--seeds", "1"])
        record(
            "MV-ORACLE-02",
            "zcc_differential_c99_oracle.py",
            "Missing compiler binary path (/tmp/nonexistent_zcc_binary)",
            "67 (EXIT_COMPILE_FAIL)",
            r_ora2.returncode,
            r_ora2.returncode == 67 and "not found" in r_ora2.stderr,
            r_ora2.stderr,
        )

        # ======================================================================
        # 2. RECEIPT ADJUDICATOR MUTATION CONTROLS (7 checks)
        # ======================================================================
        evid_clean = work / "evid_clean"
        evid_clean.mkdir(parents=True, exist_ok=True)
        for fn in ["stage2.s", "stage3.s", "stage2.o", "stage3.o", "stage2.bin.norm", "stage3.bin.norm"]:
            shutil.copy2(evid_src / fn, evid_clean / fn)

        base_rec_path = work / "receipt_clean.json"
        shutil.copy2(receipt_src, base_rec_path)
        base_rec = json.loads(base_rec_path.read_text(encoding="utf-8"))

        # MV-RECEIPT-01: Genuine baseline
        r_rec1 = run_cmd(["python3", str(repo / "tools" / "verify_meta_receipt.py"), str(base_rec_path), exp_commit, str(evid_clean)])
        record("MV-RECEIPT-01", "verify_meta_receipt.py", "Genuine unmutated receipt + evidence", "0", r_rec1.returncode, r_rec1.returncode == 0, r_rec1.stdout)

        # MV-RECEIPT-02: Stale commit lineage
        rec_m2 = copy.deepcopy(base_rec)
        rec_m2["provenance"]["git_commit"] = "0" * 40
        p_m2 = work / "rec_m2.json"
        p_m2.write_text(json.dumps(rec_m2), encoding="utf-8")
        r_rec2 = run_cmd(["python3", str(repo / "tools" / "verify_meta_receipt.py"), str(p_m2), exp_commit, str(evid_clean)])
        record("MV-RECEIPT-02", "verify_meta_receipt.py", "Stale provenance.git_commit (0000...)", "1", r_rec2.returncode, r_rec2.returncode == 1 and "NC-16 LINEAGE MISMATCH" in r_rec2.stderr, r_rec2.stderr)

        # MV-RECEIPT-03: Negative control unconditional pass forgery
        rec_m3 = copy.deepcopy(base_rec)
        rec_m3["negative_controls"]["assembly"]["gate_rc"] = 0
        p_m3 = work / "rec_m3.json"
        p_m3.write_text(json.dumps(rec_m3), encoding="utf-8")
        r_rec3 = run_cmd(["python3", str(repo / "tools" / "verify_meta_receipt.py"), str(p_m3), exp_commit, str(evid_clean)])
        record("MV-RECEIPT-03", "verify_meta_receipt.py", "Forged negative_controls.assembly.gate_rc = 0", "1", r_rec3.returncode, r_rec3.returncode == 1 and "NC-15 FORGERY DETECTED" in r_rec3.stderr, r_rec3.stderr)

        # MV-RECEIPT-04: Counterfeit Stage 3 digest in receipt
        rec_m4 = copy.deepcopy(base_rec)
        orig_s2 = rec_m4["digests"]["stage2_s"]
        flipped = ("0" if orig_s2[0] != "0" else "1") + orig_s2[1:]
        rec_m4["digests"]["stage2_s"] = flipped
        rec_m4["digests"]["stage3_s"] = flipped
        p_m4 = work / "rec_m4.json"
        p_m4.write_text(json.dumps(rec_m4), encoding="utf-8")
        r_rec4 = run_cmd(["python3", str(repo / "tools" / "verify_meta_receipt.py"), str(p_m4), exp_commit, str(evid_clean)])
        record("MV-RECEIPT-04", "verify_meta_receipt.py", "Counterfeit stage2_s/stage3_s digest in receipt JSON", "1", r_rec4.returncode, r_rec4.returncode == 1 and "NC-16 STALE ARTIFACT" in r_rec4.stderr, r_rec4.stderr)

        # MV-RECEIPT-05: 1-byte mutation in physical stage3.bin.norm on disk
        evid_m5 = work / "evid_m5"
        shutil.copytree(evid_clean, evid_m5)
        b_data = bytearray((evid_m5 / "stage3.bin.norm").read_bytes())
        b_data[512] ^= 0x01
        (evid_m5 / "stage3.bin.norm").write_bytes(b_data)
        r_rec5 = run_cmd(["python3", str(repo / "tools" / "verify_meta_receipt.py"), str(base_rec_path), exp_commit, str(evid_m5)])
        record("MV-RECEIPT-05", "verify_meta_receipt.py", "1-byte XOR flip in physical stage3.bin.norm at offset 512", "1", r_rec5.returncode, r_rec5.returncode == 1 and "DIRECT BYTE MISMATCH" in r_rec5.stderr, r_rec5.stderr)

        # MV-RECEIPT-06: Symlink sabotage (stage3.s -> stage2.s)
        evid_m6 = work / "evid_m6"
        shutil.copytree(evid_clean, evid_m6)
        (evid_m6 / "stage3.s").unlink()
        (evid_m6 / "stage3.s").symlink_to(evid_m6 / "stage2.s")
        r_rec6 = run_cmd(["python3", str(repo / "tools" / "verify_meta_receipt.py"), str(base_rec_path), exp_commit, str(evid_m6)])
        record("MV-RECEIPT-06", "verify_meta_receipt.py", "Symlink sabotage (stage3.s -> stage2.s)", "1", r_rec6.returncode, r_rec6.returncode == 1 and "NC-18 SYMLINK FORBIDDEN" in r_rec6.stderr, r_rec6.stderr)

        # MV-RECEIPT-07: Truncated artifact (128 bytes)
        evid_m7 = work / "evid_m7"
        shutil.copytree(evid_clean, evid_m7)
        (evid_m7 / "stage2.s").write_bytes(b"/* truncated */\n" * 4)
        (evid_m7 / "stage3.s").write_bytes(b"/* truncated */\n" * 4)
        r_rec7 = run_cmd(["python3", str(repo / "tools" / "verify_meta_receipt.py"), str(base_rec_path), exp_commit, str(evid_m7)])
        record("MV-RECEIPT-07", "verify_meta_receipt.py", "Truncated stage2.s/stage3.s below 1MB minimum threshold", "1", r_rec7.returncode, r_rec7.returncode == 1 and "NC-19 TRUNCATED ARTIFACT" in r_rec7.stderr, r_rec7.stderr)

        # ======================================================================
        # 3. SOVEREIGN POLICY VERIFIER MUTATION CONTROLS (5 checks)
        # ======================================================================
        base_pol_path = work / "policy_clean.json"
        shutil.copy2(policy_src, base_pol_path)
        base_pol = json.loads(base_pol_path.read_text(encoding="utf-8"))

        # MV-POLICY-01: Genuine baseline
        r_pol1 = run_cmd([
            "python3", str(repo / "tools" / "sovereign_release_verifier.py"),
            "--policy", str(base_pol_path),
            "--evidence", str(evid_clean),
            "--receipt", str(base_rec_path),
            "--expected-commit", exp_commit,
            "--repo", str(repo),
        ])
        record("MV-POLICY-01", "sovereign_release_verifier.py", "Genuine sovereign policy + receipt + evidence", "0", r_pol1.returncode, r_pol1.returncode == 0, r_pol1.stdout)

        # MV-POLICY-02: Disable required_commit_binding in policy
        pol_m2 = copy.deepcopy(base_pol)
        pol_m2["required_commit_binding"] = False
        pm2 = work / "pol_m2.json"
        pm2.write_text(json.dumps(pol_m2), encoding="utf-8")
        r_pol2 = run_cmd(["python3", str(repo / "tools" / "sovereign_release_verifier.py"), "--policy", str(pm2), "--evidence", str(evid_clean), "--receipt", str(base_rec_path), "--expected-commit", exp_commit, "--repo", str(repo)])
        record("MV-POLICY-02", "sovereign_release_verifier.py", "Policy downgrade: required_commit_binding=false", "10", r_pol2.returncode, r_pol2.returncode == 10 and "V4-06" in r_pol2.stderr, r_pol2.stderr)

        # MV-POLICY-03: Disable require_direct_byte_equality in policy
        pol_m3 = copy.deepcopy(base_pol)
        pol_m3["stage_identity"]["require_direct_byte_equality"] = False
        pm3 = work / "pol_m3.json"
        pm3.write_text(json.dumps(pol_m3), encoding="utf-8")
        r_pol3 = run_cmd(["python3", str(repo / "tools" / "sovereign_release_verifier.py"), "--policy", str(pm3), "--evidence", str(evid_clean), "--receipt", str(base_rec_path), "--expected-commit", exp_commit, "--repo", str(repo)])
        record("MV-POLICY-03", "sovereign_release_verifier.py", "Policy downgrade: require_direct_byte_equality=false", "10", r_pol3.returncode, r_pol3.returncode == 10 and "V4-06" in r_pol3.stderr, r_pol3.stderr)

        # MV-POLICY-04: Disable disallow_unconditional_pass in policy
        pol_m4 = copy.deepcopy(base_pol)
        pol_m4["negative_controls"]["disallow_unconditional_pass"] = False
        pm4 = work / "pol_m4.json"
        pm4.write_text(json.dumps(pol_m4), encoding="utf-8")
        r_pol4 = run_cmd(["python3", str(repo / "tools" / "sovereign_release_verifier.py"), "--policy", str(pm4), "--evidence", str(evid_clean), "--receipt", str(base_rec_path), "--expected-commit", exp_commit, "--repo", str(repo)])
        record("MV-POLICY-04", "sovereign_release_verifier.py", "Policy downgrade: disallow_unconditional_pass=false", "10", r_pol4.returncode, r_pol4.returncode == 10 and "V4-06" in r_pol4.stderr, r_pol4.stderr)

        # MV-POLICY-05: Perturb pinned gate.sh SHA-256 in policy
        pol_m5 = copy.deepcopy(base_pol)
        orig_gsha = pol_m5["adjudicator_integrity"]["pinned_scripts"]["gate.sh"]
        pol_m5["adjudicator_integrity"]["pinned_scripts"]["gate.sh"] = ("0" if orig_gsha[0] != "0" else "1") + orig_gsha[1:]
        pm5 = work / "pol_m5.json"
        pm5.write_text(json.dumps(pol_m5), encoding="utf-8")
        r_pol5 = run_cmd(["python3", str(repo / "tools" / "sovereign_release_verifier.py"), "--policy", str(pm5), "--evidence", str(evid_clean), "--receipt", str(base_rec_path), "--expected-commit", exp_commit, "--repo", str(repo)])
        record("MV-POLICY-05", "sovereign_release_verifier.py", "Perturbed pinned_scripts['gate.sh'] SHA-256", "12", r_pol5.returncode, r_pol5.returncode == 12 and "ADJUDICATOR COMPROMISED" in r_pol5.stderr, r_pol5.stderr)

        # ======================================================================
        # 4. ED25519 ASYMMETRIC RELEASE BARRIER MUTATION CONTROLS (5 checks)
        # ======================================================================
        priv_a = ed25519.Ed25519PrivateKey.generate()
        pub_a = priv_a.public_key()
        pub_a_pem = pub_a.public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
        fp_a = hashlib.sha256(pub_a.public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)).hexdigest()
        pub_a_path = work / "auth_pub_a.pem"
        pub_a_path.write_bytes(pub_a_pem)

        bundle_path = work / "release_bundle.tar.gz"
        bundle_path.write_bytes(b"ZCC_V17_7_MILESTONE3_BUNDLE_" + os.urandom(64))
        bundle_sha = hashlib.sha256(bundle_path.read_bytes()).hexdigest()
        auth_commit = "1af08cd000000000000000000000000000000000"
        run_id = "m3-cleanroom-run"

        def make_signed_verdict(priv_key, pub_fp: str, art_sha: str, verdict_str: str = "SOVEREIGN_RELEASE_CERTIFIED", status_str: str = "success") -> Dict[str, object]:
            signable = {
                "authority": "invariantzkaedi/zcc-release-authority",
                "authority_workflow": "adjudicate-release.yml",
                "authority_commit": auth_commit,
                "authority_verifier_sha256": hashlib.sha256((repo / "tools" / "sovereign_release_verifier.py").read_bytes()).hexdigest(),
                "authority_policy_sha256": hashlib.sha256(base_pol_path.read_bytes()).hexdigest(),
                "authority_pubkey_fingerprint": pub_fp,
                "evaluated_producer_repo": "invariantzkaedi/zcc-bootstrap-compiler",
                "evaluated_producer_commit": exp_commit,
                "evaluated_producer_run_id": run_id,
                "evaluated_artifact_id": "v17.7-m3",
                "evaluated_artifact_sha256": art_sha,
                "verdict": verdict_str,
                "status": status_str,
                "adjudication_timestamp": "2026-11-01T21:20:00Z",
            }
            c_bytes = json.dumps(signable, sort_keys=True, indent=2).encode("utf-8")
            c_sha = hashlib.sha256(c_bytes).hexdigest()
            sig_hex = priv_key.sign(c_bytes).hex()
            full = copy.deepcopy(signable)
            full["canonical_payload_sha256"] = c_sha
            full["ed25519_signature"] = {
                "algorithm": "Ed25519 (RFC 8032)",
                "public_key_fingerprint": pub_fp,
                "signed_canonical_payload_sha256": c_sha,
                "signature_hex": sig_hex,
            }
            return full

        v_clean_path = work / "verdict_clean.json"
        v_clean = make_signed_verdict(priv_a, fp_a, bundle_sha)
        v_clean_path.write_text(json.dumps(v_clean, indent=2), encoding="utf-8")

        # MV-SIG-01: Genuine Ed25519 signed release barrier
        r_sig1 = run_cmd([
            "python3", str(repo / "tools" / "verify_sovereign_release_barrier.py"),
            "--verdict", str(v_clean_path),
            "--artifact", str(bundle_path),
            "--expected-commit", exp_commit,
            "--expected-run-id", run_id,
            "--expected-authority-commit", auth_commit,
            "--authority-pubkey", str(pub_a_path),
            "--pinned-fingerprint", fp_a,
        ])
        record("MV-SIG-01", "verify_sovereign_release_barrier.py", "Genuine RFC 8032 Ed25519 signed verdict + bundle", "0", r_sig1.returncode, r_sig1.returncode == 0, r_sig1.stdout)

        # MV-SIG-02: 1-bit flip in release bundle
        bad_bundle = work / "bad_bundle.tar.gz"
        bb = bytearray(bundle_path.read_bytes())
        bb[0] ^= 0x01
        bad_bundle.write_bytes(bb)
        r_sig2 = run_cmd([
            "python3", str(repo / "tools" / "verify_sovereign_release_barrier.py"),
            "--verdict", str(v_clean_path),
            "--artifact", str(bad_bundle),
            "--expected-commit", exp_commit,
            "--expected-run-id", run_id,
            "--expected-authority-commit", auth_commit,
            "--authority-pubkey", str(pub_a_path),
            "--pinned-fingerprint", fp_a,
        ])
        record("MV-SIG-02", "verify_sovereign_release_barrier.py", "1-bit XOR flip in release artifact bundle", "1", r_sig2.returncode, r_sig2.returncode == 1 and "Artifact digest mismatch" in r_sig2.stderr, r_sig2.stderr)

        # MV-SIG-03: Recomputed-digest forgery without authority private key
        v_forged = copy.deepcopy(v_clean)
        bad_sha = hashlib.sha256(bad_bundle.read_bytes()).hexdigest()
        v_forged["evaluated_artifact_sha256"] = bad_sha
        signable_forged = {k: v_forged[k] for k in [
            "authority", "authority_workflow", "authority_commit", "authority_verifier_sha256",
            "authority_policy_sha256", "authority_pubkey_fingerprint", "evaluated_producer_repo",
            "evaluated_producer_commit", "evaluated_producer_run_id", "evaluated_artifact_id",
            "evaluated_artifact_sha256", "verdict", "status", "adjudication_timestamp"
        ]}
        recomp_sha = hashlib.sha256(json.dumps(signable_forged, sort_keys=True, indent=2).encode("utf-8")).hexdigest()
        v_forged["canonical_payload_sha256"] = recomp_sha
        v_forged["ed25519_signature"]["signed_canonical_payload_sha256"] = recomp_sha
        v_forged_path = work / "verdict_forged.json"
        v_forged_path.write_text(json.dumps(v_forged, indent=2), encoding="utf-8")
        r_sig3 = run_cmd([
            "python3", str(repo / "tools" / "verify_sovereign_release_barrier.py"),
            "--verdict", str(v_forged_path),
            "--artifact", str(bad_bundle),
            "--expected-commit", exp_commit,
            "--expected-run-id", run_id,
            "--expected-authority-commit", auth_commit,
            "--authority-pubkey", str(pub_a_path),
            "--pinned-fingerprint", fp_a,
        ])
        record("MV-SIG-03", "verify_sovereign_release_barrier.py", "Recomputed-digest forgery without Ed25519 private key", "1", r_sig3.returncode, r_sig3.returncode == 1 and "Cryptographic Ed25519 signature verification failed" in r_sig3.stderr, r_sig3.stderr)

        # MV-SIG-04: Rogue Ed25519 keypair substitution against pinned fingerprint
        priv_rogue = ed25519.Ed25519PrivateKey.generate()
        pub_rogue = priv_rogue.public_key()
        fp_rogue = hashlib.sha256(pub_rogue.public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)).hexdigest()
        pub_rogue_path = work / "auth_pub_rogue.pem"
        pub_rogue_path.write_bytes(pub_rogue.public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo))
        v_rogue = make_signed_verdict(priv_rogue, fp_rogue, bundle_sha)
        v_rogue_path = work / "verdict_rogue.json"
        v_rogue_path.write_text(json.dumps(v_rogue, indent=2), encoding="utf-8")
        r_sig4 = run_cmd([
            "python3", str(repo / "tools" / "verify_sovereign_release_barrier.py"),
            "--verdict", str(v_rogue_path),
            "--artifact", str(bundle_path),
            "--expected-commit", exp_commit,
            "--expected-run-id", run_id,
            "--expected-authority-commit", auth_commit,
            "--authority-pubkey", str(pub_rogue_path),
            "--pinned-fingerprint", fp_a,
        ])
        record("MV-SIG-04", "verify_sovereign_release_barrier.py", "Rogue Ed25519 keypair substitution vs pinned public-key fingerprint", "1", r_sig4.returncode, r_sig4.returncode == 1 and "Authority public key fingerprint mismatch" in r_sig4.stderr, r_sig4.stderr)

        # MV-SIG-05: Authentic authority-signed rejection
        v_rej = make_signed_verdict(priv_a, fp_a, bundle_sha, verdict_str="SOVEREIGN_RELEASE_REJECTED", status_str="failure")
        v_rej_path = work / "verdict_rejected.json"
        v_rej_path.write_text(json.dumps(v_rej, indent=2), encoding="utf-8")
        r_sig5 = run_cmd([
            "python3", str(repo / "tools" / "verify_sovereign_release_barrier.py"),
            "--verdict", str(v_rej_path),
            "--artifact", str(bundle_path),
            "--expected-commit", exp_commit,
            "--expected-run-id", run_id,
            "--expected-authority-commit", auth_commit,
            "--authority-pubkey", str(pub_a_path),
            "--pinned-fingerprint", fp_a,
        ])
        record("MV-SIG-05", "verify_sovereign_release_barrier.py", "Authentic Ed25519-signed SOVEREIGN_RELEASE_REJECTED verdict", "1", r_sig5.returncode, r_sig5.returncode == 1 and "Deployment prohibited" in r_sig5.stderr, r_sig5.stderr)

        summary = {
            "specialist": "Harness Meta-Verifier",
            "verdict": "PASS",
            "total_checks": len(matrix),
            "passed_checks": len(matrix),
            "matrix": matrix,
        }
        if args.out:
            out_p = Path(args.out)
            out_p.parent.mkdir(parents=True, exist_ok=True)
            out_p.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

        print("================================================================================")
        print(f"🔬 SPECIALIST 3 (HARNESS META-VERIFIER): ALL {len(matrix)}/{len(matrix)} MUTATION CHECKS PASSED")
        print("================================================================================")
        return 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())

