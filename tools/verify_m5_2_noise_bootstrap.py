#!/usr/bin/env python3
"""
ZCC OMEGA V4 — Independent Verifier for Gate M5-2 (Rule ZCC-EF-0)
=============================================================================
Independently validates the Gate M5-2 evidence bundle, receipt completeness,
full-matrix Control 0 parity bindings, statistical invariants, detached
SHA-256 digest, and artifacts.md manifest parity.

Includes --self-test with 8 adversarial negative controls.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

EXIT_OK = 0
EXIT_MISSING_ARTIFACT = 10
EXIT_SCHEMA_INCOMPLETE = 11
EXIT_CONTROL0_VIOLATION = 12
EXIT_STATISTICAL_VIOLATION = 13
EXIT_HASH_MISMATCH = 14
EXIT_SYMLINK_REJECTED = 15

REQUIRED_TOP_KEYS = [
    "metadata",
    "control_0_numerical_parity",
    "control_0b_native_multiseed_avx512",
    "experiment_1_multi_seed",
    "experiment_1_paired_t_tests_10seed",
    "experiment_2_paired_block_bootstrap",
    "experiment_3_regularization_sweep",
    "experiment_4_noise_robustness",
    "experiment_5_target_generalization",
    "experiment_6_repeated_measures_anova",
    "experiment_7_physical_mechanism_ablation",
]

REQUIRED_MODELS = [
    "B0_Linear_Delay",
    "V1_Volterra_Filter",
    "Q1_Quantum_LinearPhase",
    "Q2_Quantum_LocalIntensity",
    "Q4_Quantum_AllQuadratic",
    "Q5_Quantum_FullCandidate",
    "C1_Classical_ZeroBias_Linear",
    "C2_Classical_ZeroBias_PureQuad",
    "C4_Classical_Bias_Linear",
    "C5_Classical_Bias_Hybrid",
]


class VerificationFailure(Exception):
    def __init__(self, message: str, exit_code: int):
        super().__init__(message)
        self.exit_code = exit_code


def _sha256_file(p: Path) -> str:
    if os.path.islink(p):
        raise VerificationFailure(f"Symlink rejected: {p}", EXIT_SYMLINK_REJECTED)
    if not p.is_file():
        raise VerificationFailure(f"Missing required file: {p}", EXIT_MISSING_ARTIFACT)
    return hashlib.sha256(p.read_bytes()).hexdigest()


def verify_bundle(evidence_dir: Path, repo_root: Path) -> dict:
    if not evidence_dir.is_dir():
        raise VerificationFailure(f"Evidence directory not found: {evidence_dir}", EXIT_MISSING_ARTIFACT)

    receipt_path = evidence_dir / "m5_2_noise_bootstrap_receipt.json"
    sha_path = evidence_dir / "m5_2_noise_bootstrap_receipt.json.sha256"
    artifacts_md = evidence_dir / "artifacts.md"

    receipt_sha = _sha256_file(receipt_path)
    _sha256_file(sha_path)

    sha_text = sha_path.read_text(encoding="utf-8").strip()
    parts = sha_text.split()
    if len(parts) < 2 or parts[0].lower() != receipt_sha.lower():
        raise VerificationFailure(
            f"Detached .sha256 mismatch: file has '{sha_text}', recomputed '{receipt_sha}'",
            EXIT_HASH_MISMATCH,
        )

    try:
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    except Exception as exc:
        raise VerificationFailure(f"Malformed receipt JSON: {exc}", EXIT_SCHEMA_INCOMPLETE) from exc

    if not isinstance(receipt, dict) or not receipt:
        raise VerificationFailure("Empty or non-object receipt", EXIT_SCHEMA_INCOMPLETE)

    # 1. Mandatory top-level section completeness
    for key in REQUIRED_TOP_KEYS:
        if key not in receipt:
            raise VerificationFailure(f"Missing mandatory top-level key: '{key}'", EXIT_SCHEMA_INCOMPLETE)
        if not isinstance(receipt[key], dict) or len(receipt[key]) == 0:
            raise VerificationFailure(f"Mandatory section '{key}' is empty or invalid", EXIT_SCHEMA_INCOMPLETE)

    meta = receipt["metadata"]
    if meta.get("gate") != "GATE-M5-2":
        raise VerificationFailure(f"Unexpected gate identifier: {meta.get('gate')!r}", EXIT_SCHEMA_INCOMPLETE)
    if meta.get("verdict") != "PASS":
        raise VerificationFailure(f"Receipt metadata verdict is not PASS: {meta.get('verdict')!r}", EXIT_SCHEMA_INCOMPLETE)

    # 2. Control 0 full-matrix numerical parity & reference artifact binding
    ctrl0 = receipt["control_0_numerical_parity"]
    if ctrl0.get("status") != "PASS":
        raise VerificationFailure(f"Control 0 status is not PASS: {ctrl0.get('status')!r}", EXIT_CONTROL0_VIOLATION)

    for req_ctrl_key in (
        "max_abs_error",
        "max_abs_error_full_train",
        "max_abs_error_full_test",
        "rows_verified_train",
        "rows_verified_test",
        "ref_train_csv_sha256",
        "ref_test_csv_sha256",
        "zcc_m3_bin_sha256",
    ):
        if req_ctrl_key not in ctrl0:
            raise VerificationFailure(f"Control 0 missing mandatory field '{req_ctrl_key}'", EXIT_CONTROL0_VIOLATION)

    if ctrl0["rows_verified_train"] != 1000 or ctrl0["rows_verified_test"] != 400:
        raise VerificationFailure(
            f"Control 0 incomplete row coverage: train={ctrl0['rows_verified_train']}, test={ctrl0['rows_verified_test']}",
            EXIT_CONTROL0_VIOLATION,
        )

    if float(ctrl0["max_abs_error"]) > 1e-12:
        raise VerificationFailure(
            f"Control 0 Row 0 max_abs_error exceeds 1e-12: {ctrl0['max_abs_error']}",
            EXIT_CONTROL0_VIOLATION,
        )
    if float(ctrl0["max_abs_error_full_train"]) > 1e-11 or float(ctrl0["max_abs_error_full_test"]) > 1e-11:
        raise VerificationFailure(
            f"Control 0 full-matrix error exceeds 1e-11: train={ctrl0['max_abs_error_full_train']}, test={ctrl0['max_abs_error_full_test']}",
            EXIT_CONTROL0_VIOLATION,
        )

    m3_dir = repo_root / "experiments" / "zcc_eai_006_g3_m3_readout"
    ref_train_csv = m3_dir / "evidence" / "dataset_a" / "dataset_a_train_X.csv"
    ref_test_csv = m3_dir / "evidence" / "dataset_a" / "dataset_a_test_X.csv"
    zcc_m3_bin = m3_dir / "zcc_m3_bin"

    if (not ref_train_csv.is_file() or not ref_test_csv.is_file()) and zcc_m3_bin.is_file():
        import subprocess
        ref_train_csv.parent.mkdir(parents=True, exist_ok=True)
        os.chmod(zcc_m3_bin, 0o755)
        subprocess.run(
            [str(zcc_m3_bin), "--export-readout-prefix", str(ref_train_csv.parent / "dataset_a")],
            check=True,
            stdout=subprocess.DEVNULL,
        )

    actual_train_sha = _sha256_file(ref_train_csv)
    actual_test_sha = _sha256_file(ref_test_csv)
    actual_bin_sha = _sha256_file(zcc_m3_bin)

    if ctrl0["ref_train_csv_sha256"].lower() != actual_train_sha.lower():
        raise VerificationFailure("Control 0 ref_train_csv_sha256 mismatch vs on-disk CSV", EXIT_HASH_MISMATCH)
    if ctrl0["ref_test_csv_sha256"].lower() != actual_test_sha.lower():
        raise VerificationFailure("Control 0 ref_test_csv_sha256 mismatch vs on-disk CSV", EXIT_HASH_MISMATCH)
    if ctrl0["zcc_m3_bin_sha256"].lower() != actual_bin_sha.lower():
        raise VerificationFailure("Control 0 zcc_m3_bin_sha256 mismatch vs on-disk binary", EXIT_HASH_MISMATCH)

    if zcc_m3_bin.read_bytes()[:4] != b"\x7fELF":
        raise VerificationFailure("Control 0 zcc_m3_bin is not a valid ELF binary", EXIT_CONTROL0_VIOLATION)

    # 2b. Control 0b: Native 10-Seed & AVX-512 Symplectic Verlet verification
    ctrl0b = receipt["control_0b_native_multiseed_avx512"]
    if ctrl0b.get("status") != "PASS":
        raise VerificationFailure(f"Control 0b status is not PASS: {ctrl0b.get('status')!r}", EXIT_CONTROL0_VIOLATION)

    m5_dir = repo_root / "experiments" / "zcc_eai_006_g3_m5"
    zcc_nat_bin = m5_dir / "zcc_m5_2_native_bin"
    gcc_nat_bin = m5_dir / "gcc_m5_2_native_bin"
    zcc_nat_rec_path = evidence_dir / "m5_2_zcc_native_multiseed_receipt.json"
    gcc_nat_rec_path = evidence_dir / "m5_2_gcc_native_multiseed_receipt.json"

    actual_zcc_nat_bin_sha = _sha256_file(zcc_nat_bin)
    actual_gcc_nat_bin_sha = _sha256_file(gcc_nat_bin)
    actual_zcc_nat_rec_sha = _sha256_file(zcc_nat_rec_path)
    actual_gcc_nat_rec_sha = _sha256_file(gcc_nat_rec_path)

    if zcc_nat_bin.read_bytes()[:4] != b"\x7fELF" or gcc_nat_bin.read_bytes()[:4] != b"\x7fELF":
        raise VerificationFailure("Control 0b native binaries are not valid ELF executables", EXIT_CONTROL0_VIOLATION)

    if ctrl0b.get("zcc_native_bin_sha256", "").lower() != actual_zcc_nat_bin_sha.lower():
        raise VerificationFailure("Control 0b zcc_native_bin_sha256 mismatch vs on-disk ELF", EXIT_HASH_MISMATCH)
    if ctrl0b.get("gcc_native_bin_sha256", "").lower() != actual_gcc_nat_bin_sha.lower():
        raise VerificationFailure("Control 0b gcc_native_bin_sha256 mismatch vs on-disk ELF", EXIT_HASH_MISMATCH)
    if ctrl0b.get("zcc_native_receipt_sha256", "").lower() != actual_zcc_nat_rec_sha.lower():
        raise VerificationFailure("Control 0b zcc_native_receipt_sha256 mismatch vs on-disk receipt", EXIT_HASH_MISMATCH)
    if ctrl0b.get("gcc_native_receipt_sha256", "").lower() != actual_gcc_nat_rec_sha.lower():
        raise VerificationFailure("Control 0b gcc_native_receipt_sha256 mismatch vs on-disk receipt", EXIT_HASH_MISMATCH)

    zcc_nat = json.loads(zcc_nat_rec_path.read_text(encoding="utf-8"))
    gcc_nat = json.loads(gcc_nat_rec_path.read_text(encoding="utf-8"))

    for nat_obj, nat_lbl in ((zcc_nat, "ZCC"), (gcc_nat, "GCC")):
        eq = nat_obj.get("equivalence_verification", {})
        if eq.get("status") != "PASS":
            raise VerificationFailure(f"Control 0b {nat_lbl} equivalence status != PASS", EXIT_CONTROL0_VIOLATION)
        if int(eq.get("seeds_verified", 0)) != 10:
            raise VerificationFailure(f"Control 0b {nat_lbl} seeds_verified != 10", EXIT_CONTROL0_VIOLATION)
        if float(eq.get("scalar_vs_fused_max_abs_diff", 1.0)) != 0.0:
            raise VerificationFailure(f"Control 0b {nat_lbl} scalar_vs_fused_max_abs_diff != 0.0", EXIT_CONTROL0_VIOLATION)
        if float(eq.get("scalar_vs_avx512_max_abs_diff", 1.0)) != 0.0:
            raise VerificationFailure(f"Control 0b {nat_lbl} scalar_vs_avx512_max_abs_diff != 0.0", EXIT_CONTROL0_VIOLATION)
        if int(eq.get("scalar_vs_avx512_max_ulp_diff", 1)) != 0:
            raise VerificationFailure(f"Control 0b {nat_lbl} scalar_vs_avx512_max_ulp_diff != 0", EXIT_CONTROL0_VIOLATION)

    if float(zcc_nat.get("speedup_attribution", {}).get("avx512_vs_scalar_phase2_propagation_speedup", 0.0)) < 5.0:
        raise VerificationFailure("Control 0b ZCC AVX-512 Phase-2 speedup below 5.0x threshold", EXIT_CONTROL0_VIOLATION)
    if float(gcc_nat.get("speedup_attribution", {}).get("avx512_vs_scalar_phase2_propagation_speedup", 0.0)) < 1.5:
        raise VerificationFailure("Control 0b GCC AVX-512 Phase-2 speedup below 1.5x threshold", EXIT_CONTROL0_VIOLATION)

    # 3. Experiment 1..7 completeness & statistical predicates
    exp1 = receipt["experiment_1_multi_seed"]
    for nm in (
        "Q1_Quantum_LinearPhase",
        "Q2_Quantum_LocalIntensity",
        "Q4_Quantum_AllQuadratic",
        "Q5_Quantum_FullCandidate",
        "B0_Linear_Delay",
        "V1_Volterra_Filter",
    ):
        py_samples = exp1.get(nm, {}).get("samples", [])
        zcc_samples = zcc_nat.get("native_10seed_r2", {}).get(nm, [])
        gcc_samples = gcc_nat.get("native_10seed_r2", {}).get(nm, [])
        if len(py_samples) != 10 or len(zcc_samples) != 10 or len(gcc_samples) != 10:
            raise VerificationFailure(f"Control 0b 10-seed length mismatch on {nm}", EXIT_CONTROL0_VIOLATION)
        for idx in range(10):
            if abs(float(zcc_samples[idx]) - float(gcc_samples[idx])) > 1e-14:
                raise VerificationFailure(f"Control 0b ZCC vs GCC R^2 divergence on {nm} seed index {idx}", EXIT_CONTROL0_VIOLATION)
            if abs(float(py_samples[idx]) - float(zcc_samples[idx])) > 1e-9:
                raise VerificationFailure(f"Control 0b Python vs ZCC R^2 divergence on {nm} seed index {idx}", EXIT_CONTROL0_VIOLATION)
    for m in REQUIRED_MODELS:
        if m not in exp1:
            raise VerificationFailure(f"Experiment 1 missing model '{m}'", EXIT_SCHEMA_INCOMPLETE)
        m_entry = exp1[m]
        if len(m_entry.get("samples", [])) != 10:
            raise VerificationFailure(f"Experiment 1 model '{m}' does not have 10 seed samples", EXIT_STATISTICAL_VIOLATION)

    if float(exp1["Q1_Quantum_LinearPhase"]["mean"]) < 0.05:
        raise VerificationFailure("Experiment 1 Q1 mean R^2 below threshold 0.05", EXIT_STATISTICAL_VIOLATION)
    if float(exp1["C5_Classical_Bias_Hybrid"]["mean"]) < 0.15:
        raise VerificationFailure("Experiment 1 C5 mean R^2 below threshold 0.15", EXIT_STATISTICAL_VIOLATION)
    if float(exp1["C1_Classical_ZeroBias_Linear"]["mean"]) > 0.0:
        raise VerificationFailure("Experiment 1 C1 mean R^2 expected negative", EXIT_STATISTICAL_VIOLATION)

    exp1_t = receipt["experiment_1_paired_t_tests_10seed"]
    if float(exp1_t.get("Q1_minus_Q2", {}).get("p_val", 1.0)) >= 1e-5:
        raise VerificationFailure("Experiment 1 Q1_minus_Q2 paired t-test p_val >= 1e-5", EXIT_STATISTICAL_VIOLATION)
    if float(exp1_t.get("C5_minus_Q1", {}).get("p_val", 1.0)) >= 0.01:
        raise VerificationFailure("Experiment 1 C5_minus_Q1 paired t-test p_val >= 0.01", EXIT_STATISTICAL_VIOLATION)

    exp2 = receipt["experiment_2_paired_block_bootstrap"]
    for req_pair in ("C5_minus_Q1", "Q1_minus_Q5", "Q1_minus_Q2"):
        if req_pair not in exp2 or len(exp2[req_pair].get("ci_95", [])) != 2:
            raise VerificationFailure(f"Experiment 2 missing bootstrap pair '{req_pair}'", EXIT_STATISTICAL_VIOLATION)

    exp7 = receipt["experiment_7_physical_mechanism_ablation"]
    if float(exp7.get("decoupled_lattice_J0_V100", {}).get("test_r2", 1.0)) >= 0.01:
        raise VerificationFailure("Experiment 7 J=0 ablation did not collapse as required", EXIT_STATISTICAL_VIOLATION)
    if float(exp7.get("free_wave_J25_V0", {}).get("test_r2", 1.0)) >= 0.0:
        raise VerificationFailure("Experiment 7 V=0 ablation did not collapse as required", EXIT_STATISTICAL_VIOLATION)

    # 4. Verify artifacts.md manifest table against actual on-disk bytes & SHA-256
    _sha256_file(artifacts_md)
    art_lines = artifacts_md.read_text(encoding="utf-8").splitlines()
    row_re = re.compile(r"^\|\s*`([^`]+)`\s*\|\s*([0-9,]+)\s*\|\s*`([0-9a-fA-F]{64})`\s*\|")
    checked_rows = 0
    for line in art_lines:
        m = row_re.match(line.strip())
        if not m:
            continue
        rel_str, size_str, expected_sha = m.group(1), m.group(2).replace(",", ""), m.group(3)
        expected_size = int(size_str)
        # Resolve relative to evidence_dir first, then repo_root
        cand = evidence_dir / Path(rel_str).name if not rel_str.startswith("experiments/") else repo_root / rel_str
        if not cand.is_file():
            cand = repo_root / rel_str
        actual_sha = _sha256_file(cand)
        actual_size = cand.stat().st_size
        if actual_size != expected_size:
            raise VerificationFailure(
                f"Manifest size mismatch for {rel_str}: manifest={expected_size}, disk={actual_size}",
                EXIT_HASH_MISMATCH,
            )
        if actual_sha.lower() != expected_sha.lower():
            raise VerificationFailure(
                f"Manifest SHA-256 mismatch for {rel_str}: manifest={expected_sha}, disk={actual_sha}",
                EXIT_HASH_MISMATCH,
            )
        checked_rows += 1

    if checked_rows < 6:
        raise VerificationFailure(
            f"artifacts.md contains only {checked_rows} verified rows (expected >= 6)",
            EXIT_SCHEMA_INCOMPLETE,
        )

    # 5. Cross-document consistency: M5_2_NOISE_BOOTSTRAP_REPORT.md must embed exact receipt SHA-256
    report_md = evidence_dir / "M5_2_NOISE_BOOTSTRAP_REPORT.md"
    _sha256_file(report_md)
    report_text = report_md.read_text(encoding="utf-8")
    if receipt_sha.lower() not in report_text.lower():
        raise VerificationFailure(
            f"Cross-document drift: M5_2_NOISE_BOOTSTRAP_REPORT.md does not contain current receipt SHA-256 {receipt_sha}",
            EXIT_HASH_MISMATCH,
        )

    return {
        "verdict": "VERIFIED",
        "receipt_sha256": receipt_sha,
        "manifest_rows_verified": checked_rows,
        "train_rows_verified": ctrl0["rows_verified_train"],
        "test_rows_verified": ctrl0["rows_verified_test"],
    }


def _rewrite_sha_and_manifest(ev_dir: Path) -> None:
    receipt_path = ev_dir / "m5_2_noise_bootstrap_receipt.json"
    sha_path = ev_dir / "m5_2_noise_bootstrap_receipt.json.sha256"
    r_sha = hashlib.sha256(receipt_path.read_bytes()).hexdigest()
    sha_path.write_text(f"{r_sha}  m5_2_noise_bootstrap_receipt.json\n", encoding="utf-8")


def run_self_test(evidence_dir: Path, repo_root: Path) -> int:
    print("=== [M5-2 INDEPENDENT VERIFIER] RUNNING 12 ADVERSARIAL NEGATIVE CONTROLS ===")
    # Positive control first
    res = verify_bundle(evidence_dir, repo_root)
    print(f"[PASS] Positive Control: {res}")

    controls = []

    def expect_reject(name: str, mutator, expected_code: int):
        with tempfile.TemporaryDirectory() as td:
            sub = Path(td) / "ev"
            shutil.copytree(evidence_dir, sub)
            mutator(sub)
            try:
                verify_bundle(sub, repo_root)
                print(f"[FAIL] {name}: verifier unexpectedly PASSED!", file=sys.stderr)
                controls.append(False)
            except VerificationFailure as vf:
                if vf.exit_code == expected_code:
                    print(f"[PASS] {name}: rejected as expected (rc={vf.exit_code}: {vf})")
                    controls.append(True)
                else:
                    print(
                        f"[FAIL] {name}: wrong exit code (got {vf.exit_code}, expected {expected_code}: {vf})",
                        file=sys.stderr,
                    )
                    controls.append(False)

    # NC-1: Missing mandatory section
    def mut_missing_sec(sub: Path):
        p = sub / "m5_2_noise_bootstrap_receipt.json"
        d = json.loads(p.read_text())
        del d["experiment_2_paired_block_bootstrap"]
        p.write_text(json.dumps(d, indent=2))
        _rewrite_sha_and_manifest(sub)

    expect_reject("NC-1 (Remove mandatory section)", mut_missing_sec, EXIT_SCHEMA_INCOMPLETE)

    # NC-2: Empty Control 0 dict (simulating pre-patch missing CSV fail-open)
    def mut_empty_ctrl0(sub: Path):
        p = sub / "m5_2_noise_bootstrap_receipt.json"
        d = json.loads(p.read_text())
        d["control_0_numerical_parity"] = {}
        p.write_text(json.dumps(d, indent=2))
        _rewrite_sha_and_manifest(sub)

    expect_reject("NC-2 (Empty Control 0 fail-open bypass)", mut_empty_ctrl0, EXIT_SCHEMA_INCOMPLETE)

    # NC-3: Replace Control 0 status PASS with FAIL
    def mut_ctrl0_fail(sub: Path):
        p = sub / "m5_2_noise_bootstrap_receipt.json"
        d = json.loads(p.read_text())
        d["control_0_numerical_parity"]["status"] = "FAIL"
        p.write_text(json.dumps(d, indent=2))
        _rewrite_sha_and_manifest(sub)

    expect_reject("NC-3 (Control 0 status=FAIL)", mut_ctrl0_fail, EXIT_CONTROL0_VIOLATION)

    # NC-4: Corrupt ref_train_csv_sha256 in receipt
    def mut_bad_csv_sha(sub: Path):
        p = sub / "m5_2_noise_bootstrap_receipt.json"
        d = json.loads(p.read_text())
        d["control_0_numerical_parity"]["ref_train_csv_sha256"] = "0" * 64
        p.write_text(json.dumps(d, indent=2))
        _rewrite_sha_and_manifest(sub)

    expect_reject("NC-4 (Forged ref_train_csv_sha256)", mut_bad_csv_sha, EXIT_HASH_MISMATCH)

    # NC-5: Perturb Control 0 max_abs_error above threshold
    def mut_high_err(sub: Path):
        p = sub / "m5_2_noise_bootstrap_receipt.json"
        d = json.loads(p.read_text())
        d["control_0_numerical_parity"]["max_abs_error"] = 1e-6
        p.write_text(json.dumps(d, indent=2))
        _rewrite_sha_and_manifest(sub)

    expect_reject("NC-5 (Control 0 error > 1e-12)", mut_high_err, EXIT_CONTROL0_VIOLATION)

    # NC-6: Modify an artifact after manifest is generated
    def mut_tamper_report(sub: Path):
        p = sub / "M5_2_NOISE_BOOTSTRAP_REPORT.md"
        p.write_text(p.read_text() + "\n<!-- tampered -->\n")

    expect_reject("NC-6 (Post-receipt report tampering vs artifacts.md)", mut_tamper_report, EXIT_HASH_MISMATCH)

    # NC-7: Corrupt detached .sha256 file
    def mut_bad_detached_sha(sub: Path):
        p = sub / "m5_2_noise_bootstrap_receipt.json.sha256"
        p.write_text("f" * 64 + "  m5_2_noise_bootstrap_receipt.json\n")

    expect_reject("NC-7 (Corrupted detached .sha256)", mut_bad_detached_sha, EXIT_HASH_MISMATCH)

    # NC-8: Symlink attack on receipt
    def mut_symlink(sub: Path):
        p = sub / "m5_2_noise_bootstrap_receipt.json"
        bak = sub / "receipt.real"
        p.rename(bak)
        os.symlink(bak, p)

    expect_reject("NC-8 (Symlinked receipt file)", mut_symlink, EXIT_SYMLINK_REJECTED)

    # NC-9: Stale receipt SHA-256 in M5_2_NOISE_BOOTSTRAP_REPORT.md (even if artifacts.md is updated to match report)
    def mut_stale_report_sha(sub: Path):
        rep = sub / "M5_2_NOISE_BOOTSTRAP_REPORT.md"
        r_sha = hashlib.sha256((sub / "m5_2_noise_bootstrap_receipt.json").read_bytes()).hexdigest()
        old_bytes = rep.read_bytes()
        old_sha = hashlib.sha256(old_bytes).hexdigest()
        new_text = rep.read_text().replace(r_sha, "d" * 64)
        new_bytes = new_text.encode("utf-8")
        rep.write_bytes(new_bytes)
        new_sha = hashlib.sha256(new_bytes).hexdigest()
        art = sub / "artifacts.md"
        art.write_text(
            art.read_text().replace(old_sha, new_sha).replace(f"{len(old_bytes):,}", f"{len(new_bytes):,}"),
            encoding="utf-8",
        )

    expect_reject("NC-9 (Stale receipt SHA-256 in report)", mut_stale_report_sha, EXIT_HASH_MISMATCH)

    # NC-10: Statistical threshold violation in Experiment 1
    def mut_stat_fail(sub: Path):
        p = sub / "m5_2_noise_bootstrap_receipt.json"
        d = json.loads(p.read_text())
        d["experiment_1_multi_seed"]["Q1_Quantum_LinearPhase"]["mean"] = 0.01
        p.write_text(json.dumps(d, indent=2))
        _rewrite_sha_and_manifest(sub)

    expect_reject("NC-10 (Experiment 1 Q1 mean R^2 statistical collapse)", mut_stat_fail, EXIT_STATISTICAL_VIOLATION)

    # NC-11: Native 10-seed R^2 divergence in m5_2_zcc_native_multiseed_receipt.json
    def mut_native_r2_tamper(sub: Path):
        nat_p = sub / "m5_2_zcc_native_multiseed_receipt.json"
        nd = json.loads(nat_p.read_text())
        nd["native_10seed_r2"]["Q1_Quantum_LinearPhase"][3] += 0.05
        nat_p.write_text(json.dumps(nd, indent=2))
        nat_sha = hashlib.sha256(nat_p.read_bytes()).hexdigest()
        p = sub / "m5_2_noise_bootstrap_receipt.json"
        d = json.loads(p.read_text())
        d["control_0b_native_multiseed_avx512"]["zcc_native_receipt_sha256"] = nat_sha
        p.write_text(json.dumps(d, indent=2))
        _rewrite_sha_and_manifest(sub)

    expect_reject("NC-11 (Native 10-seed ZCC R^2 tamper on Seed 45)", mut_native_r2_tamper, EXIT_CONTROL0_VIOLATION)

    # NC-12: AVX-512 vs Scalar non-zero ULP drift in m5_2_zcc_native_multiseed_receipt.json
    def mut_avx512_ulp_drift(sub: Path):
        nat_p = sub / "m5_2_zcc_native_multiseed_receipt.json"
        nd = json.loads(nat_p.read_text())
        nd["equivalence_verification"]["scalar_vs_avx512_max_ulp_diff"] = 4
        nat_p.write_text(json.dumps(nd, indent=2))
        nat_sha = hashlib.sha256(nat_p.read_bytes()).hexdigest()
        p = sub / "m5_2_noise_bootstrap_receipt.json"
        d = json.loads(p.read_text())
        d["control_0b_native_multiseed_avx512"]["zcc_native_receipt_sha256"] = nat_sha
        p.write_text(json.dumps(d, indent=2))
        _rewrite_sha_and_manifest(sub)

    expect_reject("NC-12 (AVX-512 vs Scalar non-zero ULP drift)", mut_avx512_ulp_drift, EXIT_CONTROL0_VIOLATION)

    if all(controls):
        print(f"=== ALL {len(controls)}/12 NEGATIVE CONTROLS + 1 POSITIVE CONTROL PASSED ===")
        return EXIT_OK
    return 1


def main() -> None:
    parser = argparse.ArgumentParser(description="Independent Verifier for Gate M5-2")
    parser.add_argument(
        "--evidence-dir",
        default="docs/evidence/2026-10-28/m5_2_noise_bootstrap",
        help="Path to Gate M5-2 evidence directory",
    )
    parser.add_argument("--repo-root", default=".", help="Path to repository root")
    parser.add_argument("--self-test", action="store_true", help="Run adversarial negative control suite")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    evidence_dir = (repo_root / args.evidence_dir).resolve()

    if args.self_test:
        sys.exit(run_self_test(evidence_dir, repo_root))

    try:
        res = verify_bundle(evidence_dir, repo_root)
        print(f"M5-2 INDEPENDENT VERIFIER PASS: {json.dumps(res)}")
        sys.exit(EXIT_OK)
    except VerificationFailure as vf:
        print(f"M5-2 INDEPENDENT VERIFIER REJECT (rc={vf.exit_code}): {vf}", file=sys.stderr)
        sys.exit(vf.exit_code)


if __name__ == "__main__":
    main()

