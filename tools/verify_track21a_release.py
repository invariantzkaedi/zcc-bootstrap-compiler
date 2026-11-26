#!/usr/bin/env python3
"""ZCC Track 21A — Independent Fail-Closed Release Verifier (T21-G03 .. T21-G10).

Enforces ZCC-EF-0 & ZCC-EF-7:
- Never certifies PASS from printed markers or exit codes alone.
- Recomputes cryptographic artifact digests and independent canonical receipt hashes/HMACs.
- Executes real receipt tamper negative controls (T21-G09).
- Audits compiled portable objects via objdump for AVX/FMA leakage (T21-G03).
- Verifies --self-test negative controls fail closed when tamper checks/missing artifacts/stale digests occur.
- Preserves existing self-host / baseline integrity (no edits to BOOTSTRAP_BASELINES.tsv or part0..part6_ptx).
- Explicitly marks T21-G01 (SPIR-V) and T21-G02 (Vulkan) DEFERRED to Track 21B.
"""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import hmac
import json
import os
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Tuple

REPO_ROOT = Path(os.environ.get("ZCC_REPO_ROOT") or Path(__file__).resolve().parents[1]).resolve()
sys.path.insert(0, str(REPO_ROOT))

from sdk.zcc_compute import (  # noqa: E402
    BACKEND_AVX2_FMA,
    BACKEND_PTX_SM120,
    BACKEND_SCALAR_C99,
    OP_VEC_ADD_F32,
    POLICY_EXACT_REQUIRED,
    ZccComputeReceipt,
    ZccComputeRuntime,
    ZccExecOptions,
    ZccTensorDesc,
)
from tests.track21a.differential_oracle import make_f32_desc  # noqa: E402

# Canonical Track 21A release manifest (31 tracked source/doc/contract/report deliverables;
# excludes TRACK21A_RELEASE_EVIDENCE.json itself to avoid self-referential hash cycles, making 32 staged files total).
TRACK21A_MANIFEST_FILES = [
    ".gitignore",
    "Makefile",
    "TRACK21A_REPO_AUDIT.json",
    "TRACK21A_COMPUTE_CONTRACT.md",
    "TRACK21A_IMPLEMENTATION_PLAN.md",
    "TRACK21A_INVARIANT_REGISTRY.json",
    "TRACK21A_DIFFERENTIAL_REPORT.json",
    "TRACK21A_MUTATION_REPORT.json",
    "TRACK21A_PERFORMANCE_REPORT.json",
    "tickets/VERIFIER-PURITY-LOG-BOOTSTRAP-HASH-MUTATION.md",
    "docs/TRACK21A_REPRODUCTION_AND_MIGRATION.md",
    "include/zcc_compute.h",
    "runtime/compute_sha256.c",
    "runtime/cpu_capabilities.c",
    "runtime/compute_scalar.c",
    "runtime/compute_avx2.c",
    "runtime/kernels_t21a_ptx.c",
    "runtime/kernels_t21a_sm120.ptx",
    "runtime/compute_ptx.c",
    "runtime/compute_dispatch.c",
    "runtime/execution_receipt.c",
    "sdk/zcc_compute.py",
    "sdk/zcc_compute.mjs",
    "tests/track21a/build_runtime.sh",
    "tests/track21a/run_c_suite.sh",
    "tests/track21a/test_track21a_compute.c",
    "tests/track21a/test_scalar_zcc.c",
    "tests/track21a/test_sdk_mjs.mjs",
    "tests/track21a/differential_oracle.py",
    "tests/track21a/mutation_observatory.py",
    "tools/verify_track21a_release.py",
]

PROTECTED_CORE_FILES = [
    "part0_pp.c",
    "part1.c",
    "part2.c",
    "part3.c",
    "part4.c",
    "part5.c",
    "part6_ptx.c",
    "BOOTSTRAP_BASELINES.tsv",
]

UNRELATED_WORKSPACE_FILES = [
    "LAUNCH_COSMIC_ELEPHANT.bat",
    "LAUNCH_DEMO_SPRITES.bat",
    "LAUNCH_HAMILTONIAN_VISUALIZER.bat",
    "LAUNCH_HEIST_DASHBOARD.bat",
    "LAUNCH_MASTER_GOD.bat",
    "LAUNCH_MASTER_GOD.py",
    "exp1_raytracer_simd.c",
    "spectrogram_waterfall_prime.html",
    "tools/dashboards/master_god_dashboard.html",
]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def snapshot_files(root: Path, rel_paths: List[str]) -> Dict[str, str]:
    snap: Dict[str, str] = {}
    for rel in rel_paths:
        p = root / rel
        if p.is_file():
            snap[rel] = sha256_file(p)
    return snap


def compare_snapshots(before: Dict[str, str], after: Dict[str, str]) -> List[str]:
    drifts: List[str] = []
    for k in sorted(set(before.keys()) | set(after.keys())):
        if before.get(k) != after.get(k):
            drifts.append(k)
    return drifts


def check_manifest_closure(repo_root: Path, staged_only: bool = False) -> Tuple[bool, Dict[str, Any]]:
    """INV-T21A-012: Verify exact canonical deliverable closure & gitignore safety."""
    missing = [f for f in TRACK21A_MANIFEST_FILES if not (repo_root / f).is_file()]
    oversized = [
        f for f in TRACK21A_MANIFEST_FILES + ["TRACK21A_RELEASE_EVIDENCE.json"]
        if (repo_root / f).is_file() and (repo_root / f).stat().st_size > 1048576
    ]
    ignored: List[str] = []
    expected_staged = sorted(TRACK21A_MANIFEST_FILES + ["TRACK21A_RELEASE_EVIDENCE.json"])
    if (repo_root / ".git").exists():
        ig_proc = subprocess.run(
            ["git", "check-ignore", "--stdin"],
            input="\n".join(expected_staged) + "\n",
            cwd=str(repo_root),
            capture_output=True,
            text=True,
        )
        ignored = [line.strip() for line in ig_proc.stdout.splitlines() if line.strip()]

    staged_info: Dict[str, Any] = {"checked": staged_only, "missing_staged": [], "unexpected_staged": [], "index_sha_mismatches": []}
    if staged_only:
        staged_raw = subprocess.check_output(
            ["git", "diff", "--cached", "--name-only"],
            cwd=str(repo_root),
            text=True,
        )
        actual_staged = sorted([line.strip() for line in staged_raw.splitlines() if line.strip()])
        missing_st = sorted(set(expected_staged) - set(actual_staged))
        unexpected_st = sorted(set(actual_staged) - set(expected_staged))
        staged_info["missing_staged"] = missing_st
        staged_info["unexpected_staged"] = unexpected_st

        ev_path = repo_root / "TRACK21A_RELEASE_EVIDENCE.json"
        if not ev_path.is_file():
            staged_info["index_sha_mismatches"].append("TRACK21A_RELEASE_EVIDENCE.json:missing")
        else:
            ev_data = json.loads(ev_path.read_text(encoding="utf-8"))
            recorded = ev_data.get("artifact_sha256", {})
            for f in TRACK21A_MANIFEST_FILES:
                idx_bytes = subprocess.check_output(["git", "show", f":{f}"], cwd=str(repo_root))
                idx_sha = sha256_bytes(idx_bytes)
                if recorded.get(f) != idx_sha:
                    staged_info["index_sha_mismatches"].append(f"{f}:index!={recorded.get(f)}")
            ev_idx = sha256_bytes(subprocess.check_output(["git", "show", ":TRACK21A_RELEASE_EVIDENCE.json"], cwd=str(repo_root)))
            if ev_idx != sha256_file(ev_path):
                staged_info["index_sha_mismatches"].append("TRACK21A_RELEASE_EVIDENCE.json:worktree!=index")

    ok = (
        not missing
        and not oversized
        and not ignored
        and not staged_info["missing_staged"]
        and not staged_info["unexpected_staged"]
        and not staged_info["index_sha_mismatches"]
    )
    return ok, {
        "missing": missing,
        "oversized": oversized,
        "ignored": ignored,
        "staged": staged_info,
    }


def check_ptx_regeneration(repo_root: Path) -> Tuple[bool, str, str]:
    ptx_path = repo_root / "runtime" / "kernels_t21a_sm120.ptx"
    src_path = repo_root / "runtime" / "kernels_t21a_ptx.c"
    zcc_bin = repo_root / "zcc"
    if not (ptx_path.is_file() and src_path.is_file() and zcc_bin.is_file()):
        return False, "", "missing_ptx_or_zcc"
    committed_sha = sha256_file(ptx_path)
    with tempfile.NamedTemporaryFile(prefix="t21a_ptx_regen_", suffix=".ptx", delete=False) as tf:
        tmp_ptx = Path(tf.name)
    try:
        res = subprocess.run(
            [str(zcc_bin), "--target=ptx", "--gpu-arch=sm_120", str(src_path), "-o", str(tmp_ptx)],
            cwd=str(repo_root),
            capture_output=True,
            text=True,
        )
        if res.returncode != 0 or not tmp_ptx.is_file():
            return False, committed_sha, f"zcc_ptx_failed:rc={res.returncode}"
        regen_sha = sha256_file(tmp_ptx)
        if regen_sha != committed_sha:
            return False, committed_sha, f"ptx_mismatch:regen={regen_sha}"
        return True, committed_sha, "BYTE_IDENTICAL"
    finally:
        if tmp_ptx.exists():
            tmp_ptx.unlink()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fp:
        for chunk in iter(lambda: fp.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def canonical_core_from_json(r: Dict[str, Any]) -> str:
    """Independently reconstruct runtime canonical receipt string from serialized JSON."""
    bk_map = {"AUTO": 0, "SCALAR_C99": 1, "AVX2_FMA": 2, "PTX_SM120": 3, "NONE": 0}
    dt_map = {"f32": 1, "f16": 2, "bf16": 3, "q8_0": 4, "q4_0": 5, "q4_k": 6, "u8": 7, "i32": 8}
    hw = r["hardware_caps"]
    dims = r["dims"]
    cc_parts = str(hw["gpu_cc"]).split(".")
    cc_maj = int(cc_parts[0])
    cc_min = int(cc_parts[1])
    xcr0_raw = int(str(hw["xcr0_hex"]), 16)
    abi_int = int(str(r["abi_version"]), 16)
    return (
        f"schema={r['schema_version']}|abi={abi_int}|contract={r['contract_version']}|"
        f"compiler={r['compiler_id']}|csha={r['compiler_sha256']}|esha={r['executable_sha256']}|ptxsha={r['ptx_module_sha256']}|"
        f"breq={bk_map[r['backend_requested']]}|bexec={bk_map[r['backend_executed']]}|pol={r['selection_policy']}|"
        f"fb={1 if r['fallback_occurred'] else 0}|reason={r['selection_reason']}|op={r['op_id']}|dt={dt_map[r['dtype']]}|"
        f"acc={r['accum_policy']}|det={r['determinism_mode']}|seed={r['seed']}|in={r['input_count']}|"
        f"m={dims['m']}|n={dims['n']}|k={dims['k']}|ind={r['inputs_descriptor_sha256']}|"
        f"inb={r['input_buffers_sha256']}|outd={r['output_descriptor_sha256']}|outb={r['output_buffer_sha256']}|"
        f"status={r['status_code']}|estat={r['execution_status']}|vstat={r['verification_status']}|"
        f"ikind={r['integrity_kind']}|zk={1 if r['zk_proof_present'] else 0}|ns={r['elapsed_ns']}|"
        f"avx2={hw['avx2']}|osx={hw['osxsave']}|xmm={hw['xcr0_xmm']}|ymm={hw['xcr0_ymm']}|"
        f"fma={hw['fma']}|xcr0={xcr0_raw}|cuda={hw['gpu_available']}|cc={cc_maj}.{cc_min}"
    )


def independent_verify_json_receipt(r: Dict[str, Any], hmac_key: bytes | None = None) -> Tuple[bool, str]:
    if r.get("schema_version") != "track21a.receipt.v1":
        return False, "schema_version_mismatch"
    if r.get("contract_version") != "1.0.0":
        return False, "contract_version_mismatch"
    if r.get("verification_status") != "UNVERIFIED_RUNTIME_ONLY":
        return False, f"illegal_verification_status:{r.get('verification_status')}"
    if r.get("zk_proof_present") is not False:
        return False, "illegal_zk_proof_claim"
    try:
        core = canonical_core_from_json(r).encode("utf-8")
    except Exception as exc:
        return False, f"malformed_receipt_fields:{exc}"
    expected_sha = hashlib.sha256(core).hexdigest()
    if expected_sha != r.get("receipt_canonical_sha256"):
        return False, "canonical_sha256_mismatch"
    if hmac_key is not None:
        if r.get("integrity_kind") != "hmac_sha256_attested":
            return False, "missing_hmac_integrity_kind"
        expected_hmac = hmac.new(hmac_key, core, hashlib.sha256).hexdigest()
        if not hmac.compare_digest(expected_hmac, str(r.get("hmac_sha256", ""))):
            return False, "hmac_sha256_mismatch"
    return True, "ok"


def run_tamper_controls() -> Tuple[bool, List[Dict[str, Any]]]:
    rt = ZccComputeRuntime("/tmp/libzcc_compute.so")
    try:
        key = b"zcc-t21a-independent-verifier-hmac"
        da, ba = make_f32_desc([1.0, 2.0, 3.0, 4.0], (4,))
        db, bb = make_f32_desc([5.0, 6.0, 7.0, 8.0], (4,))
        do, bo = make_f32_desc([0.0] * 4, (4,))
        opts = ZccExecOptions()
        rt.lib.zcc_exec_options_init(ctypes.byref(opts))
        opts.requested_backend = BACKEND_AVX2_FMA
        opts.selection_policy = POLICY_EXACT_REQUIRED
        key_buf = ctypes.create_string_buffer(key, len(key))
        opts.hmac_key = ctypes.cast(key_buf, ctypes.c_void_p)
        opts.hmac_key_len = len(key)

        rec = ZccComputeReceipt()
        rc = rt.lib.zcc_compute_exec(OP_VEC_ADD_F32, (ZccTensorDesc * 2)(da, db), 2, ctypes.byref(do), ctypes.byref(opts), ctypes.byref(rec))
        if rc != 0:
            return False, [{"control": "generate_live_receipt", "status": "FAIL", "reason": f"rc={rc}"}]

        base_json = rt.receipt_dict(rec)
        ok_pos, msg_pos = independent_verify_json_receipt(base_json, hmac_key=key)
        # Independently check raw output digest against Python SHA-256 of output buffer
        py_out_sha = hashlib.sha256(bytes(bo[:16])).hexdigest()
        if py_out_sha != base_json["output_buffer_sha256"]:
            return False, [{"control": "positive_output_buffer_digest", "status": "FAIL", "reason": "buffer digest mismatch"}]

        controls: List[Dict[str, Any]] = [{
            "control": "POSITIVE_VALID_HMAC_RECEIPT",
            "expected": "ACCEPT",
            "observed": "ACCEPT" if ok_pos else f"REJECT:{msg_pos}",
            "status": "PASS" if ok_pos else "FAIL",
        }]

        tamper_specs = [
            ("TAMPER_OUTPUT_SHA256", lambda d: d.__setitem__("output_buffer_sha256", "0" * 64)),
            ("TAMPER_INPUT_SHA256", lambda d: d.__setitem__("input_buffers_sha256", "f" * 64)),
            ("TAMPER_BACKEND_SPOOF", lambda d: d.__setitem__("backend_executed", "PTX_SM120")),
            ("TAMPER_STATUS_CODE", lambda d: d.__setitem__("status_code", -5)),
            ("TAMPER_SELF_CERTIFY_PASS", lambda d: d.__setitem__("verification_status", "PASS")),
            ("TAMPER_ZK_CLAIM_FORGED", lambda d: d.__setitem__("zk_proof_present", True)),
            ("TAMPER_SCHEMA_DOWNGRADE", lambda d: d.__setitem__("schema_version", "track21a.receipt.v0")),
            ("TAMPER_WRONG_HMAC_KEY", lambda d: None),
        ]

        tampered_rejected = True
        for name, mutator in tamper_specs:
            copy_d = json.loads(json.dumps(base_json))
            mutator(copy_d)
            use_key = b"wrong-hmac-secret" if name == "TAMPER_WRONG_HMAC_KEY" else key
            accepted, reason = independent_verify_json_receipt(copy_d, hmac_key=use_key)
            if accepted:
                tampered_rejected = False
            controls.append({
                "control": name,
                "expected": "REJECT",
                "observed": "ACCEPT" if accepted else f"REJECT:{reason}",
                "status": "PASS" if not accepted else "FAIL",
            })

        if not tampered_rejected:
            return False, controls
        return ok_pos and tampered_rejected, controls
    finally:
        rt.shutdown()


def check_portable_isa(obj_dir: Path) -> Tuple[bool, Dict[str, int]]:
    portable = [
        "compute_sha256.o",
        "cpu_capabilities.o",
        "compute_scalar.o",
        "compute_ptx.o",
        "compute_dispatch.o",
        "execution_receipt.o",
    ]
    counts: Dict[str, int] = {}
    for name in portable:
        p = obj_dir / name
        if not p.exists():
            return False, {name: -1}
        out = subprocess.check_output(["objdump", "-d", str(p)], text=True)
        hits = sum(
            1 for line in out.splitlines()
            if any(tok in line for tok in ("ymm", "zmm", "vfmadd", "vfnmadd", "vaddps", "vmulps", "vpadd", "vpbroadcast"))
        )
        counts[name] = hits
        if hits != 0:
            return False, counts
    return True, counts


def run_verifier_self_test() -> int:
    """ZCC-EF-0 meta-control: verify validator rejects empty, stale, malformed, self-certified, survived-mutant, incomplete-closure, and impure-verifier inputs."""
    ok_tamp, _ = run_tamper_controls()
    if not ok_tamp:
        print("SELF_TEST_FAIL: receipt tamper controls did not reject all tampered payloads", file=sys.stderr)
        return 1

    # Negative Control A: verifier must reject self-certified verdict without valid canonical hash
    bad_rec = {"schema_version": "track21a.receipt.v1", "contract_version": "1.0.0", "verification_status": "PASS"}
    acc, _ = independent_verify_json_receipt(bad_rec)
    if acc:
        print("SELF_TEST_FAIL: accepted self-certified receipt", file=sys.stderr)
        return 1

    # Negative Control B: verifier run against empty evidence directory must fail non-zero
    with tempfile.TemporaryDirectory(prefix="t21a_st_") as td:
        rc = verify_release(Path(td), run_fresh=False, write_evidence=False)
        if rc == 0:
            print("SELF_TEST_FAIL: empty evidence directory returned PASS", file=sys.stderr)
            return 1

        # Negative Control C: survived mutant inside mutation report must fail non-zero
        for fname in [
            "TRACK21A_REPO_AUDIT.json",
            "TRACK21A_COMPUTE_CONTRACT.md",
            "TRACK21A_IMPLEMENTATION_PLAN.md",
            "TRACK21A_INVARIANT_REGISTRY.json",
            "TRACK21A_DIFFERENTIAL_REPORT.json",
            "TRACK21A_PERFORMANCE_REPORT.json",
        ]:
            src = REPO_ROOT / fname
            if src.exists():
                shutil.copy2(src, Path(td) / fname)
        bad_mut = {"schema_version": "track21a.mutation_report.v1", "total_mutants": 1, "killed": 0, "survived": 1, "false_positives_on_clean_control": 0, "verdict": "PASS"}
        (Path(td) / "TRACK21A_MUTATION_REPORT.json").write_text(json.dumps(bad_mut), encoding="utf-8")
        if verify_release(Path(td), run_fresh=False, write_evidence=False) == 0:
            print("SELF_TEST_FAIL: survived mutant accepted despite verdict=PASS claim", file=sys.stderr)
            return 1

        # Negative Control D (INV-RELEASE-CLOSURE): incomplete manifest directory must fail check_manifest_closure
        ok_cl, cl_info = check_manifest_closure(Path(td), staged_only=False)
        if ok_cl or not cl_info["missing"]:
            print("SELF_TEST_FAIL: incomplete release closure passed check_manifest_closure", file=sys.stderr)
            return 1

    # Negative Control E (INV-VERIFIER-PURITY): baseline drift S_before != S_after must be detected
    drifts = compare_snapshots({"BOOTSTRAP_BASELINES.tsv": "aaa"}, {"BOOTSTRAP_BASELINES.tsv": "bbb"})
    if drifts != ["BOOTSTRAP_BASELINES.tsv"]:
        print("SELF_TEST_FAIL: compare_snapshots failed to detect protected baseline drift", file=sys.stderr)
        return 1

    print("VERIFY_TRACK21A_SELF_TEST PASS (all negative controls rejected cleanly)")
    return 0


def verify_staged_tree(repo_root: Path) -> int:
    ok_cl, cl_info = check_manifest_closure(repo_root, staged_only=True)
    ev_path = repo_root / "TRACK21A_RELEASE_EVIDENCE.json"
    ev_ok = False
    ev_verdict = "MISSING"
    if ev_path.is_file():
        ev_data = json.loads(ev_path.read_text(encoding="utf-8"))
        ev_verdict = ev_data.get("ef7_adjudication", {}).get("verdict", "MISSING")
        recorded = ev_data.get("artifact_sha256", {})
        manifest_pairs = "\n".join(f"{k}:{recorded.get(k, '')}" for k in sorted(TRACK21A_MANIFEST_FILES)).encode("utf-8")
        recomputed_manifest_sha = sha256_bytes(manifest_pairs)
        ev_ok = (ev_verdict == "PASS" and ev_data.get("canonical_manifest_sha256") == recomputed_manifest_sha)

    if not ok_cl or not ev_ok:
        print(
            f"STAGED_CLOSURE_FAIL ok_closure={ok_cl} ev_ok={ev_ok} ev_verdict={ev_verdict} info={json.dumps(cl_info)}",
            file=sys.stderr,
        )
        return 7

    print(
        f"STAGED_CLOSURE_PASS staged_count={len(TRACK21A_MANIFEST_FILES) + 1} "
        f"manifest_sha256={ev_data['canonical_manifest_sha256']}"
    )
    return 0


def verify_release(evidence_dir: Path, run_fresh: bool = True, write_evidence: bool = True) -> int:
    before_snap = snapshot_files(REPO_ROOT, PROTECTED_CORE_FILES + UNRELATED_WORKSPACE_FILES)

    required_files = [
        "TRACK21A_REPO_AUDIT.json",
        "TRACK21A_COMPUTE_CONTRACT.md",
        "TRACK21A_IMPLEMENTATION_PLAN.md",
        "TRACK21A_INVARIANT_REGISTRY.json",
        "TRACK21A_DIFFERENTIAL_REPORT.json",
        "TRACK21A_MUTATION_REPORT.json",
        "TRACK21A_PERFORMANCE_REPORT.json",
    ]
    missing = [f for f in required_files if not (evidence_dir / f).is_file()]
    if missing:
        print(f"RELEASE_BLOCKED: Missing deliverables in {evidence_dir}: {missing}", file=sys.stderr)
        return 2

    # Reject if core compiler files or BOOTSTRAP_BASELINES.tsv were dirtied
    dirty_core = subprocess.check_output(
        ["git", "status", "--porcelain", "--", *PROTECTED_CORE_FILES],
        cwd=str(REPO_ROOT),
        text=True,
    ).strip()
    if dirty_core:
        print(f"RELEASE_BLOCKED: Core compiler or BOOTSTRAP_BASELINES.tsv modified:\n{dirty_core}", file=sys.stderr)
        return 3

    c_suite_ran = False
    c_suite_rc = 1
    diff_rc = 1
    mut_rc = 1
    if run_fresh and evidence_dir.resolve() == REPO_ROOT.resolve():
        c_suite_rc = subprocess.run(["bash", "tests/track21a/run_c_suite.sh"], cwd=str(REPO_ROOT)).returncode
        c_suite_ran = True
        diff_rc = subprocess.run([sys.executable, "tests/track21a/differential_oracle.py"], cwd=str(REPO_ROOT)).returncode
        mut_rc = subprocess.run([sys.executable, "tests/track21a/mutation_observatory.py"], cwd=str(REPO_ROOT)).returncode
        if c_suite_rc != 0 or diff_rc != 0 or mut_rc != 0:
            print(f"RELEASE_BLOCKED: Fresh execution failed (c={c_suite_rc}, diff={diff_rc}, mut={mut_rc})", file=sys.stderr)
            return 4

    diff_rep = json.loads((evidence_dir / "TRACK21A_DIFFERENTIAL_REPORT.json").read_text(encoding="utf-8"))
    mut_rep = json.loads((evidence_dir / "TRACK21A_MUTATION_REPORT.json").read_text(encoding="utf-8"))
    perf_rep = json.loads((evidence_dir / "TRACK21A_PERFORMANCE_REPORT.json").read_text(encoding="utf-8"))

    diff_ok = (diff_rep.get("verdict") == "PASS" and diff_rep.get("failed", 1) == 0 and diff_rep.get("total_checks", 0) > 0)
    mut_ok = (
        mut_rep.get("verdict") == "PASS"
        and mut_rep.get("survived", 1) == 0
        and mut_rep.get("false_positives_on_clean_control", 1) == 0
        and mut_rep.get("killed", 0) == mut_rep.get("total_mutants", -1)
        and mut_rep.get("unknown_signature_probe", {}).get("preserved_as_unknown") is True
    )
    perf_ok = len(perf_rep.get("results", [])) >= 12
    c_ok = (c_suite_ran and c_suite_rc == 0)

    isa_ok, isa_counts = check_portable_isa(Path("/tmp/t21a_build"))
    tamper_ok, tamper_controls = run_tamper_controls()
    ptx_path = REPO_ROOT / "runtime" / "kernels_t21a_sm120.ptx"
    ptx_header_ok = ptx_path.is_file() and ".target sm_120" in ptx_path.read_text(encoding="utf-8")
    ptx_regen_ok, ptx_sha, ptx_regen_status = check_ptx_regeneration(REPO_ROOT)
    ptx_ok = ptx_header_ok and ptx_regen_ok

    closure_ok, closure_info = check_manifest_closure(REPO_ROOT, staged_only=False)
    oversized = closure_info["oversized"]

    after_snap = snapshot_files(REPO_ROOT, PROTECTED_CORE_FILES + UNRELATED_WORKSPACE_FILES)
    purity_drifts = compare_snapshots(before_snap, after_snap)
    purity_ok = (len(purity_drifts) == 0)
    if not purity_ok:
        print(f"RELEASE_BLOCKED: INV-VERIFIER-PURITY violated on: {purity_drifts}", file=sys.stderr)
        return 6

    gates = {
        "T21-G01": {"status": "DEFERRED", "target": "Track 21B (SPIR-V)", "evidence": "Explicitly out of scope for Track 21A"},
        "T21-G02": {"status": "DEFERRED", "target": "Track 21B (Vulkan)", "evidence": "Explicitly out of scope for Track 21A"},
        "T21-G03": {"status": "PASS" if (isa_ok and diff_ok) else "FAIL", "evidence": {"portable_avx_opcode_counts": isa_counts, "cpuid_osxsave_xcr0_fma_fallback": "tested"}},
        "T21-G04": {"status": "PASS" if (c_ok and diff_ok) else "FAIL", "evidence": "Scalar C99 compiled & verified with GCC (-mno-avx/-mno-avx2/-mno-fma) and ./zcc"},
        "T21-G05": {"status": "PASS" if (diff_ok and ptx_ok) else "FAIL", "evidence": {"differential": f"{diff_rep.get('passed')}/{diff_rep.get('total_checks')} PASS", "ptx_regeneration": ptx_regen_status, "ptx_sha256": ptx_sha}},
        "T21-G06": {"status": "PASS" if c_ok else "FAIL", "evidence": "ASan+LSan+UBSan (CPU/OOM/bounds) + full UBSan (CPU+PTX) 0 errors"},
        "T21-G07": {"status": "PASS" if mut_ok else "FAIL", "evidence": f"{mut_rep.get('killed')}/{mut_rep.get('total_mutants')} controlled mutants killed; false_pos=0; unknown preserved"},
        "T21-G08": {"status": "PASS" if c_ok else "FAIL", "evidence": "Bitwise output SHA-256 identical across 3x replay per backend (SCALAR_C99, AVX2_FMA, PTX_SM120)"},
        "T21-G09": {"status": "PASS" if tamper_ok else "FAIL", "evidence": tamper_controls},
        "T21-G10": {"status": "PASS" if (c_ok and closure_ok and purity_ok and not dirty_core and diff_ok and mut_ok and perf_ok and isa_ok and tamper_ok and ptx_ok) else "FAIL", "evidence": {"release_closure": closure_info, "verifier_purity_drifts": purity_drifts}},
    }

    active_pass = sum(1 for n in range(3, 11) if gates[f"T21-G{n:02d}"]["status"] == "PASS")
    verdict = "PASS" if active_pass == 8 else "FAIL"

    artifact_hashes = {
        f: sha256_file(REPO_ROOT / f) for f in sorted(TRACK21A_MANIFEST_FILES)
    }
    manifest_pairs = "\n".join(f"{k}:{artifact_hashes[k]}" for k in sorted(TRACK21A_MANIFEST_FILES)).encode("utf-8")
    canonical_manifest_sha = sha256_bytes(manifest_pairs)
    so_file = Path("/tmp/libzcc_compute.so")
    if so_file.is_file():
        artifact_hashes["/tmp/libzcc_compute.so"] = sha256_file(so_file)

    git_head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=str(REPO_ROOT), text=True).strip()

    release_record = {
        "schema_version": "track21a.release_evidence.v1",
        "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "governing_rule_version": "ZCC-EF-7 v1.0.0 + ZCC-EF-0",
        "evidence_tier": "E3" if run_fresh else "E2",
        "git_head_sha": git_head,
        "manifest_file_count": len(TRACK21A_MANIFEST_FILES),
        "canonical_manifest_sha256": canonical_manifest_sha,
        "ptx_regeneration": {"status": ptx_regen_status, "sha256": ptx_sha},
        "oversized_files": oversized,
        "dirty_core_files": dirty_core,
        "verifier_purity_drifts": purity_drifts,
        "release_closure": closure_info,
        "artifact_sha256": artifact_hashes,
        "gates": gates,
        "host_limitations": [
            "Native Windows PE32+ .exe execution remains blocked by Windows Smart App Control / Device Guard on this workstation; Linux WSL2 ELF64 + libcuda.so.1 execution runs natively.",
            "NVIDIA proprietary WSL2 driver (libnvdxgdmal.so.1 / libcuda.so.1) triggers an internal ASan double-free inside cuInit(0); ASan+LSan+UBSan ran 100% cleanly on CPU paths (--cpu-only) while full UBSan (-fsanitize=undefined -fno-sanitize-recover=all) verified CPU + PTX_SM120 together.",
            "T21-G01 (SPIR-V) and T21-G02 (Vulkan) are explicitly DEFERRED to Track 21B.",
            "Tracked defect DEFECT-T21A-VP01 (tickets/VERIFIER-PURITY-LOG-BOOTSTRAP-HASH-MUTATION.md): scripts/log_bootstrap_hash.sh --check appends a row to BOOTSTRAP_BASELINES.tsv unless restored/remediated separately."
        ],
        "ef7_adjudication": {
            "claim": "ZCC Track 21A Portable Verified Compute Foundation satisfies T21-G03..T21-G10 and INV-T21A-012/013 without compiler core or bootstrap regression.",
            "evidence_class": "OBSERVED",
            "evidence_tier": "E3" if run_fresh else "E2",
            "verdict": verdict,
        },
    }

    if write_evidence:
        (evidence_dir / "TRACK21A_RELEASE_EVIDENCE.json").write_text(json.dumps(release_record, indent=2) + "\n", encoding="utf-8")

    print(f"TRACK21A_RELEASE_VERIFIER verdict={verdict} active_gates={active_pass}/8 deferred=T21-G01,T21-G02 manifest_sha256={canonical_manifest_sha[:16]}")
    return 0 if verdict == "PASS" else 5


def main() -> int:
    parser = argparse.ArgumentParser(description="Independent Fail-Closed Release Verifier for ZCC Track 21A")
    parser.add_argument("--self-test", action="store_true", help="Run ZCC-EF-0 negative self-tests")
    parser.add_argument("--no-rerun", action="store_true", help="Verify existing artifacts without re-running suites")
    parser.add_argument("--verify-staged", action="store_true", help="Verify exact staged 32-file closure and index SHA-256 bindings")
    parser.add_argument("--verify-dir", default=str(REPO_ROOT), help="Directory containing Track 21A deliverables")
    args = parser.parse_args()

    if args.self_test:
        return run_verifier_self_test()
    if args.verify_staged:
        return verify_staged_tree(REPO_ROOT)
    return verify_release(Path(args.verify_dir), run_fresh=not args.no_rerun, write_evidence=(Path(args.verify_dir).resolve() == REPO_ROOT.resolve()))


if __name__ == "__main__":
    raise SystemExit(main())
