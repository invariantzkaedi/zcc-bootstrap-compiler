#!/usr/bin/env python3
"""ZCC Track 21B — Independent Fail-Closed Release Verifier (ZCC-EF-0, ZCC-EF-7).

Enforces:
- BLOCKED (exit 3) if independent Khronos `spirv-val`, `glslangValidator`,
  `libvulkan.so.1`, or `VK_LAYER_KHRONOS_validation` is absent. Never falls back to PASS.
- Validates deterministic SPIR-V 1.0 emission + independent `spirv-val --target-env vulkan1.0`
  on all 13 kernels + ISA isolation across all 9 portable objects.
- Enforces honest physical device identity (CPU llvmpipe cannot masquerade as DISCRETE_GPU).
- Executes live track21b.receipt.v1 canonical SHA-256 + HMAC-SHA256 tamper negative controls.
- Preserves Track 21A release freeze (`TRACK21A_*`, `include/zcc_compute.h`, Track 21A runtimes),
  compiler core `PARTS`, and `BOOTSTRAP_BASELINES.tsv` (`S_before == S_after`).
- Preserves `T21B-GPU-NV` and `T21B-GPU-AMD` explicitly as `DEFERRED`.
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

from sdk.zcc_vulkan_compute import (  # noqa: E402
    BACKEND_VULKAN_GPU,
    BACKEND_VULKAN_SPV,
    OP_VEC_ADD_F32,
    POLICY_EXACT_REQUIRED,
    ZccComputeV2Receipt,
    ZccTensorDesc,
    ZccVulkanComputeRuntime,
    ZccVulkanExecOptions,
)
from tests.track21a.differential_oracle import make_f32_desc  # noqa: E402
from tests.track21b.verify_hw_vulkan_dzn import validate_existing_report  # noqa: E402
from tools.track21b_transitive_verifier import (  # noqa: E402
    INSUFFICIENT_EVIDENCE,
    UPSTREAM_DEPENDENCY_BLOCKED,
    VERIFIED_FAIL,
    VERIFIED_PASS,
    evaluate_dependency_aware_verdicts,
    verify_verdict_artifact,
)
from tools.verify_track21a_release import canonical_core_from_json  # noqa: E402

TRACK21B_MANIFEST_FILES = [
    ".github/workflows/track21b-ci.yml",
    ".gitignore",
    "Makefile",
    "TRACK21B_REPO_AUDIT.json",
    "TRACK21B_COMPUTE_CONTRACT.md",
    "TRACK21B_IMPLEMENTATION_PLAN.md",
    "TRACK21B_INVARIANT_REGISTRY.json",
    "TRACK21B_DIFFERENTIAL_REPORT.json",
    "TRACK21B_MUTATION_REPORT.json",
    "TRACK21B_TRANSITIVE_MUTATION_REPORT.json",
    "TRACK21B_PERFORMANCE_REPORT.json",
    "TRACK21B_HW_VULKAN_REPORT.json",
    "patches/mesa-25.2.8-u_cpu_detect-l3-teardown.patch",
    "docs/TRACK21B_REPRODUCTION_AND_MIGRATION.md",
    "docs/track21b/DISPATCH_INIT_OPTIMIZATION.md",
    "docs/track21b/MESA_UPSTREAM_ISSUE_REPORT.md",
    "docs/track21b/forensics/FORENSIC_MANIFEST.json",
    "docs/track21b/forensics/MESA_LVP_128B_FORENSICS.md",
    "docs/track21b/forensics/SHA256SUMS",
    "include/zcc_spirv.h",
    "include/zcc_vulkan_compute.h",
    "src/part6_spirv.c",
    "tools/zcc_spirv_cli.c",
    "tools/track21b_transitive_verifier.py",
    "runtime/compute_vulkan.c",
    "runtime/compute_dispatch_v2.c",
    "sdk/zcc_vulkan_compute.py",
    "sdk/zcc_vulkan_compute.mjs",
    "tests/track21b/build_runtime.sh",
    "tests/track21b/build_clean_mesa_lvp.sh",
    "tests/track21b/run_vulkan_suite.sh",
    "tests/track21b/run_ci_lane.sh",
    "tests/track21b/test_track21b_vulkan.c",
    "tests/track21b/test_sdk_v2_mjs.mjs",
    "tests/track21b/differential_oracle_21b.py",
    "tests/track21b/mutation_observatory_21b.py",
    "tests/track21b/transitive_alloc_auditor.c",
    "tests/track21b/transitive_mutation_controls.py",
    "tests/track21b/forensics/zero_zcc_vulkan_repro.c",
    "tests/track21b/verify_hw_vulkan_dzn.py",
    "tools/verify_track21b_release.py",
]

PROTECTED_CORE_AND_T21A_FILES = [
    "part0_pp.c",
    "part1.c",
    "part2.c",
    "part3.c",
    "part4.c",
    "part5.c",
    "part6_ptx.c",
    "BOOTSTRAP_BASELINES.tsv",
    "include/zcc_compute.h",
    "runtime/compute_sha256.c",
    "runtime/cpu_capabilities.c",
    "runtime/compute_scalar.c",
    "runtime/compute_avx2.c",
    "runtime/compute_ptx.c",
    "runtime/compute_dispatch.c",
    "runtime/execution_receipt.c",
    "TRACK21A_REPO_AUDIT.json",
    "TRACK21A_COMPUTE_CONTRACT.md",
    "TRACK21A_IMPLEMENTATION_PLAN.md",
    "TRACK21A_INVARIANT_REGISTRY.json",
    "TRACK21A_DIFFERENTIAL_REPORT.json",
    "TRACK21A_MUTATION_REPORT.json",
    "TRACK21A_PERFORMANCE_REPORT.json",
    "TRACK21A_RELEASE_EVIDENCE.json",
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

V2_BACKEND_MAP = {
    "AUTO": 0,
    "SCALAR_C99": 1,
    "AVX2_FMA": 2,
    "PTX_SM120": 3,
    "VULKAN_SPV": 4,
    "VULKAN_GPU": 5,
    "NONE": 0,
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fp:
        for chunk in iter(lambda: fp.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def snapshot_files(root: Path, rel_paths: List[str]) -> Dict[str, str]:
    snap: Dict[str, str] = {}
    for rel in rel_paths:
        p = root / rel
        if p.is_file():
            snap[rel] = sha256_file(p)
    return snap


def compare_snapshots(before: Dict[str, str], after: Dict[str, str]) -> List[str]:
    return [k for k in sorted(set(before.keys()) | set(after.keys())) if before.get(k) != after.get(k)]


def vk_identity_consistent(vk_dict: Dict[str, Any], backend_executed: str = "") -> Tuple[bool, str]:
    """Fail closed if a software CPU Vulkan ICD claims GPU identity or Vulkan reports NONE when active."""
    dev_name = str(vk_dict.get("device_name", "")).lower()
    dev_type = str(vk_dict.get("device_type", "")).upper()
    vendor = str(vk_dict.get("vendor_id", "")).lower()
    vk_avail = int(vk_dict.get("vulkan_available", 0) or 0)
    is_sw_name = any(tok in dev_name for tok in ("llvmpipe", "lavapipe", "swiftshader", "softpipe"))
    if (is_sw_name or vendor == "0x10005") and dev_type != "CPU":
        return False, f"fabricated_gpu_identity_for_sw_icd:{dev_type}:{vk_dict.get('device_name')}"
    if dev_type == "NONE":
        if backend_executed in ("VULKAN_SPV", "VULKAN_GPU") or vk_avail != 0 or is_sw_name or vendor not in ("", "0x0000", "0x0", "0"):
            return False, f"invalid_none_vk_device_type:{backend_executed}:avail={vk_avail}"
        return True, "ok"
    if dev_type not in ("CPU", "DISCRETE_GPU", "INTEGRATED_GPU", "VIRTUAL_GPU", "OTHER"):
        return False, f"invalid_vk_device_type:{dev_type}"
    return True, "ok"


def base_core_for_v2_json(v2r: Dict[str, Any]) -> str:
    base = v2r["base_receipt"]
    core = canonical_core_from_json(
        {
            **base,
            "backend_requested": base["backend_requested"]
            if base["backend_requested"] in ("AUTO", "SCALAR_C99", "AVX2_FMA", "PTX_SM120", "NONE")
            else "AUTO",
            "backend_executed": base["backend_executed"]
            if base["backend_executed"] in ("AUTO", "SCALAR_C99", "AVX2_FMA", "PTX_SM120", "NONE")
            else "AUTO",
        }
    )
    breq = V2_BACKEND_MAP[v2r["backend_requested"]]
    bexec = V2_BACKEND_MAP[v2r["backend_executed"]]
    core = core.replace(
        f"breq={V2_BACKEND_MAP.get(base['backend_requested'], 0)}|bexec={V2_BACKEND_MAP.get(base['backend_executed'], 0)}|",
        f"breq={breq}|bexec={bexec}|",
        1,
    )
    return core


def canonical_v2_core_from_json(r: Dict[str, Any], base_canon_sha: str) -> str:
    vk = r["vulkan_caps"]
    breq = V2_BACKEND_MAP[r["backend_requested"]]
    bexec = V2_BACKEND_MAP[r["backend_executed"]]
    abi_int = int(str(r["v2_abi_version"]), 16)
    vendor_int = int(str(vk["vendor_id"]), 16)
    return (
        f"v2schema={r['v2_schema_version']}|v2abi={abi_int}|v2contract={r['v2_contract_version']}|"
        f"basecanon={base_canon_sha}|breq={breq}|bexec={bexec}|"
        f"spvsha={r['spirv_module_sha256']}|spvwords={r['spirv_word_count']}|"
        f"vkavail={vk['vulkan_available']}|vktype={vk['device_type']}|vkname={vk['device_name']}|"
        f"vkvendor=0x{vendor_int:04x}|vkapi={vk['api_version']}|vkval={vk['validation_layer_enabled']}|"
        f"bar={r['sync_barrier_count']}|fl={r['mem_flush_count']}|inv={r['mem_invalidate_count']}|"
        f"valerr={r['validation_error_count']}|status={r['status_code']}|outb={r['output_buffer_sha256']}|"
        f"v2stat={r['v2_verification_status']}|v2ikind={r['v2_integrity_kind']}|"
        f"v2zk={1 if r['v2_zk_proof_present'] else 0}"
    )


def independent_verify_v2_json_receipt(r: Dict[str, Any], hmac_key: bytes | None = None) -> Tuple[bool, str]:
    if r.get("v2_schema_version") != "track21b.receipt.v1":
        return False, "v2_schema_mismatch"
    if r.get("v2_contract_version") != "1.1.0":
        return False, "v2_contract_mismatch"
    if r.get("v2_verification_status") != "UNVERIFIED_RUNTIME_ONLY":
        return False, f"illegal_v2_verification_status:{r.get('v2_verification_status')}"
    if r.get("v2_zk_proof_present") is not False:
        return False, "illegal_v2_zk_proof_claim"

    base = r.get("base_receipt")
    if not isinstance(base, dict):
        return False, "missing_base_receipt"
    if base.get("verification_status") != "UNVERIFIED_RUNTIME_ONLY" or base.get("zk_proof_present") is not False:
        return False, "illegal_base_verification_status"

    try:
        base_core = base_core_for_v2_json(r).encode("utf-8")
        base_sha = hashlib.sha256(base_core).hexdigest()
        if base_sha != base.get("receipt_canonical_sha256"):
            return False, "base_canonical_sha256_mismatch"

        id_ok, id_msg = vk_identity_consistent(r.get("vulkan_caps", {}), str(r.get("backend_executed", "")))
        if not id_ok:
            return False, id_msg

        if r.get("backend_executed") in ("VULKAN_SPV", "VULKAN_GPU"):
            if int(r.get("sync_barrier_count", 0)) < 2:
                return False, "missing_vulkan_sync_barriers"
            if int(r.get("validation_error_count", 1)) != 0:
                return False, "nonzero_vulkan_validation_errors"
            if r.get("backend_executed") == "VULKAN_GPU" and r["vulkan_caps"].get("device_type") == "CPU":
                return False, "cpu_device_executed_vulkan_gpu"

        v2_core = canonical_v2_core_from_json(r, base_sha).encode("utf-8")
    except Exception as exc:
        return False, f"malformed_v2_fields:{exc}"

    exp_v2_sha = hashlib.sha256(v2_core).hexdigest()
    if exp_v2_sha != r.get("v2_canonical_sha256"):
        return False, "v2_canonical_sha256_mismatch"

    if hmac_key is not None:
        if r.get("v2_integrity_kind") != "hmac_sha256_attested":
            return False, "missing_v2_hmac_integrity_kind"
        exp_hmac = hmac.new(hmac_key, v2_core, hashlib.sha256).hexdigest()
        if not hmac.compare_digest(exp_hmac, str(r.get("v2_hmac_sha256", ""))):
            return False, "v2_hmac_sha256_mismatch"
        exp_base_hmac = hmac.new(hmac_key, base_core, hashlib.sha256).hexdigest()
        if not hmac.compare_digest(exp_base_hmac, str(base.get("hmac_sha256", ""))):
            return False, "base_hmac_sha256_mismatch"

    return True, "ok"


def check_manifest_closure(repo_root: Path, staged_only: bool = False) -> Tuple[bool, Dict[str, Any]]:
    missing = [f for f in TRACK21B_MANIFEST_FILES if not (repo_root / f).is_file()]
    oversized = [
        f
        for f in TRACK21B_MANIFEST_FILES + ["TRACK21B_RELEASE_EVIDENCE.json"]
        if (repo_root / f).is_file() and (repo_root / f).stat().st_size > 1048576
    ]
    ignored: List[str] = []
    expected_staged = sorted(TRACK21B_MANIFEST_FILES + ["TRACK21B_RELEASE_EVIDENCE.json"])
    if (repo_root / ".git").exists() and shutil.which("git"):
        ig_proc = subprocess.run(
            ["git", "check-ignore", "--stdin"],
            input="\n".join(expected_staged) + "\n",
            cwd=str(repo_root),
            capture_output=True,
            text=True,
        )
        ignored = [line.strip() for line in ig_proc.stdout.splitlines() if line.strip()]

    staged_info: Dict[str, Any] = {
        "checked": staged_only,
        "missing_staged": [],
        "unexpected_staged": [],
        "index_sha_mismatches": [],
    }
    if staged_only:
        staged_raw = subprocess.check_output(["git", "diff", "--cached", "--name-only"], cwd=str(repo_root), text=True)
        actual_staged = sorted([line.strip() for line in staged_raw.splitlines() if line.strip()])
        tracked_in_index_raw = subprocess.check_output(["git", "ls-files", "--cached", "--", *expected_staged], cwd=str(repo_root), text=True)
        tracked_in_index = sorted([line.strip() for line in tracked_in_index_raw.splitlines() if line.strip()])
        staged_info["missing_staged"] = sorted(set(expected_staged) - set(tracked_in_index))
        staged_info["unexpected_staged"] = sorted(set(actual_staged) - set(expected_staged))

        ev_path = repo_root / "TRACK21B_RELEASE_EVIDENCE.json"
        if not ev_path.is_file():
            staged_info["index_sha_mismatches"].append("TRACK21B_RELEASE_EVIDENCE.json:missing")
        else:
            ev_data = json.loads(ev_path.read_text(encoding="utf-8"))
            recorded = ev_data.get("artifact_sha256", {})
            for f in TRACK21B_MANIFEST_FILES:
                idx_bytes = subprocess.check_output(["git", "show", f":{f}"], cwd=str(repo_root))
                if recorded.get(f) != sha256_bytes(idx_bytes):
                    staged_info["index_sha_mismatches"].append(f"{f}:index_sha_mismatch")
            ev_idx = sha256_bytes(subprocess.check_output(["git", "show", ":TRACK21B_RELEASE_EVIDENCE.json"], cwd=str(repo_root)))
            if ev_idx != sha256_file(ev_path):
                staged_info["index_sha_mismatches"].append("TRACK21B_RELEASE_EVIDENCE.json:worktree!=index")

    ok = (
        not missing
        and not oversized
        and not ignored
        and not staged_info["missing_staged"]
        and not staged_info["unexpected_staged"]
        and not staged_info["index_sha_mismatches"]
    )
    return ok, {"missing": missing, "oversized": oversized, "ignored": ignored, "staged": staged_info}


def check_portable_isa_v2(obj_dir: Path) -> Tuple[bool, Dict[str, int]]:
    portable = [
        "compute_sha256.o",
        "cpu_capabilities.o",
        "compute_scalar.o",
        "compute_ptx.o",
        "compute_dispatch.o",
        "execution_receipt.o",
        "part6_spirv.o",
        "compute_vulkan.o",
        "compute_dispatch_v2.o",
    ]
    counts: Dict[str, int] = {}
    for name in portable:
        p = obj_dir / name
        if not p.exists():
            return False, {name: -1}
        out = subprocess.check_output(["objdump", "-d", str(p)], text=True)
        hits = sum(
            1
            for line in out.splitlines()
            if any(tok in line for tok in ("ymm", "zmm", "vfmadd", "vfnmadd", "vaddps", "vmulps", "vpadd", "vpbroadcast"))
        )
        counts[name] = hits
        if hits != 0:
            return False, counts
    return True, counts


def check_independent_spirv_val(cli_path: str = "/tmp/zcc-spirv") -> Tuple[bool, str, Dict[str, str]]:
    spirv_val = shutil.which("spirv-val")
    glslang = shutil.which("glslangValidator")
    if not spirv_val or not glslang:
        return False, "BLOCKED_MISSING_SPIRV_TOOLS", {}
    if not Path(cli_path).exists():
        return False, "MISSING_ZCC_SPIRV_CLI", {}

    shas1: Dict[str, str] = {}
    with tempfile.TemporaryDirectory(prefix="t21b_rel_spv1_") as d1, tempfile.TemporaryDirectory(
        prefix="t21b_rel_spv2_"
    ) as d2:
        for d in (d1, d2):
            p = subprocess.run([cli_path, "--emit-all", d], capture_output=True, text=True)
            if p.returncode != 0:
                return False, f"CLI_FAILED:{p.stderr.strip()}", {}
        files1 = sorted(Path(d1).glob("*.spv"))
        files2 = sorted(Path(d2).glob("*.spv"))
        if len(files1) != 13 or len(files2) != 13:
            return False, f"SPV_COUNT_MISMATCH:{len(files1)}", {}
        for f1, f2 in zip(files1, files2):
            s1, s2 = sha256_file(f1), sha256_file(f2)
            if s1 != s2:
                return False, f"NON_DETERMINISTIC:{f1.name}", {}
            vr = subprocess.run([spirv_val, "--target-env", "vulkan1.0", str(f1)], capture_output=True, text=True)
            if vr.returncode != 0:
                return False, f"SPIRV_VAL_FAILED:{f1.name}:{vr.stderr.strip()}", {}
            shas1[f1.name] = s1
    return True, "OK", shas1


def run_v2_tamper_controls() -> Tuple[bool, List[Dict[str, Any]]]:
    rt = ZccVulkanComputeRuntime("/tmp/libzcc_compute_v2.so")
    try:
        key = b"zcc-t21b-independent-verifier-hmac"
        da, _ba = make_f32_desc([1.0, 2.0, 3.0, 4.0], (4,))
        db, _bb = make_f32_desc([5.0, 6.0, 7.0, 8.0], (4,))
        do, bo = make_f32_desc([0.0] * 4, (4,))
        rc, base_json, _ = rt.exec_v2(
            OP_VEC_ADD_F32, [da, db], do, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED, hmac_key=key
        )
        if rc != 0:
            return False, [{"control": "generate_live_v2_receipt", "status": "FAIL", "reason": f"rc={rc}"}]

        py_out_sha = hashlib.sha256(bytes(bo[:16])).hexdigest()
        if py_out_sha != base_json["output_buffer_sha256"]:
            return False, [{"control": "positive_v2_output_sha256", "status": "FAIL", "reason": "buffer digest mismatch"}]

        ok_pos, msg_pos = independent_verify_v2_json_receipt(base_json, hmac_key=key)
        controls: List[Dict[str, Any]] = [
            {
                "control": "POSITIVE_VALID_V2_HMAC_RECEIPT",
                "expected": "ACCEPT",
                "observed": "ACCEPT" if ok_pos else f"REJECT:{msg_pos}",
                "status": "PASS" if ok_pos else "FAIL",
            }
        ]

        tamper_specs = [
            ("TAMPER_V2_SPV_SHA256", lambda d: d.__setitem__("spirv_module_sha256", "0" * 64)),
            ("TAMPER_V2_SPOOF_DISCRETE_GPU", lambda d: d["vulkan_caps"].__setitem__("device_type", "DISCRETE_GPU")),
            ("TAMPER_V2_BARRIER_COUNT_ZERO", lambda d: d.__setitem__("sync_barrier_count", 0)),
            ("TAMPER_V2_VALIDATION_ERR_NONZERO", lambda d: d.__setitem__("validation_error_count", 1)),
            ("TAMPER_V2_SELF_CERTIFY_PASS", lambda d: d.__setitem__("v2_verification_status", "PASS")),
            ("TAMPER_V2_ZK_CLAIM_FORGED", lambda d: d.__setitem__("v2_zk_proof_present", True)),
            ("TAMPER_V2_OUTPUT_SHA256", lambda d: d.__setitem__("output_buffer_sha256", "f" * 64)),
            ("TAMPER_V2_BASE_OUTPUT_SHA256", lambda d: d["base_receipt"].__setitem__("output_buffer_sha256", "e" * 64)),
            ("TAMPER_V2_WRONG_HMAC_KEY", lambda d: None),
        ]

        all_rejected = True
        for name, mut in tamper_specs:
            cp = json.loads(json.dumps(base_json))
            mut(cp)
            use_key = b"wrong-v2-hmac-secret" if name == "TAMPER_V2_WRONG_HMAC_KEY" else key
            acc, msg = independent_verify_v2_json_receipt(cp, hmac_key=use_key)
            if acc:
                all_rejected = False
            controls.append(
                {
                    "control": name,
                    "expected": "REJECT",
                    "observed": "ACCEPT" if acc else f"REJECT:{msg}",
                    "status": "PASS" if not acc else "FAIL",
                }
            )
        return ok_pos and all_rejected, controls
    finally:
        rt.shutdown()


def run_verifier_self_test() -> int:
    ok_tamp, _ = run_v2_tamper_controls()
    if not ok_tamp:
        print("SELF_TEST_FAIL: v2 receipt tamper controls failed", file=sys.stderr)
        return 1

    # Negative Control A: self-certified receipt without canonical SHA rejected
    bad_rec = {"v2_schema_version": "track21b.receipt.v1", "v2_contract_version": "1.1.0", "v2_verification_status": "PASS"}
    acc, _ = independent_verify_v2_json_receipt(bad_rec)
    if acc:
        print("SELF_TEST_FAIL: accepted self-certified v2 receipt", file=sys.stderr)
        return 1

    # Negative Control B: spoofed DISCRETE_GPU on llvmpipe or invalid NONE rejected in identity helper
    bad_id, _ = vk_identity_consistent({"device_name": "llvmpipe (LLVM 21.1.8, 256 bits)", "device_type": "DISCRETE_GPU", "vendor_id": "0x10005"})
    if bad_id:
        print("SELF_TEST_FAIL: accepted fabricated DISCRETE_GPU for llvmpipe", file=sys.stderr)
        return 1
    bad_none1, _ = vk_identity_consistent({"device_name": "", "device_type": "NONE", "vendor_id": "0x0000", "vulkan_available": 1}, "VULKAN_SPV")
    bad_none2, _ = vk_identity_consistent({"device_name": "", "device_type": "NONE", "vendor_id": "0x0000", "vulkan_available": 0}, "VULKAN_SPV")
    good_none, _ = vk_identity_consistent({"device_name": "", "device_type": "NONE", "vendor_id": "0x0000", "vulkan_available": 0}, "SCALAR_C99")
    if bad_none1 or bad_none2 or not good_none:
        print("SELF_TEST_FAIL: invalid NONE vk identity handling", file=sys.stderr)
        return 1

    # Negative Control C: missing spirv-val on PATH MUST return BLOCKED (rc=3), NEVER PASS (rc=0)
    with tempfile.TemporaryDirectory(prefix="t21b_nospv_") as empty_bin:
        env_no_spv = {**os.environ, "PATH": empty_bin, "ZCC_REPO_ROOT": str(REPO_ROOT)}
        p_blk = subprocess.run(
            [sys.executable, str(Path(__file__).resolve()), "--no-rerun"],
            cwd=str(REPO_ROOT),
            env=env_no_spv,
            capture_output=True,
            text=True,
        )
        if p_blk.returncode != 3 or "RELEASE_BLOCKED" not in p_blk.stderr:
            print(f"SELF_TEST_FAIL: missing spirv-val returned rc={p_blk.returncode} instead of BLOCKED(3)", file=sys.stderr)
            return 1

    # Negative Control D: empty evidence dir + survived mutant rejected
    with tempfile.TemporaryDirectory(prefix="t21b_st_") as td:
        if verify_release(Path(td), run_fresh=False, write_evidence=False) == 0:
            print("SELF_TEST_FAIL: empty evidence directory returned PASS", file=sys.stderr)
            return 1
        for fname in [
            "TRACK21B_REPO_AUDIT.json",
            "TRACK21B_COMPUTE_CONTRACT.md",
            "TRACK21B_IMPLEMENTATION_PLAN.md",
            "TRACK21B_INVARIANT_REGISTRY.json",
        ]:
            src = REPO_ROOT / fname
            if src.exists():
                shutil.copy2(src, Path(td) / fname)
        (Path(td) / "TRACK21B_DIFFERENTIAL_REPORT.json").write_text(
            json.dumps({"verdict": "PASS", "failed": 0, "total_checks": 1, "replay_13_ops_count": 13, "hardware": {"vulkan_device_type": "CPU"}}),
            encoding="utf-8",
        )
        (Path(td) / "TRACK21B_PERFORMANCE_REPORT.json").write_text(
            json.dumps({"results": [{"k": i} for i in range(12)]}),
            encoding="utf-8",
        )
        (Path(td) / "TRACK21B_TRANSITIVE_MUTATION_REPORT.json").write_text(
            json.dumps({"schema_version": "track21b.transitive_mutation_report.v1", "total_controls": 9, "killed": 9, "survived": 0, "false_positives_on_clean_control": 0, "verdict": "PASS"}),
            encoding="utf-8",
        )
        bad_mut = {"schema_version": "track21b.mutation_report.v1", "total_mutants": 1, "killed": 0, "survived": 1, "false_positives_on_clean_control": 0, "verdict": "PASS"}
        (Path(td) / "TRACK21B_MUTATION_REPORT.json").write_text(json.dumps(bad_mut), encoding="utf-8")
        if verify_release(Path(td), run_fresh=False, write_evidence=False) != 5:
            print("SELF_TEST_FAIL: survived mutant did not fail with rc=5", file=sys.stderr)
            return 1
        ok_cl, cl_info = check_manifest_closure(Path(td), staged_only=False)
        if ok_cl or not cl_info["missing"]:
            print("SELF_TEST_FAIL: incomplete release closure passed", file=sys.stderr)
            return 1

    # Negative Control E: baseline drift S_before != S_after detected
    if compare_snapshots({"BOOTSTRAP_BASELINES.tsv": "aaa"}, {"BOOTSTRAP_BASELINES.tsv": "bbb"}) != ["BOOTSTRAP_BASELINES.tsv"]:
        print("SELF_TEST_FAIL: compare_snapshots failed to detect protected baseline drift", file=sys.stderr)
        return 1

    # Negative Control F: Dependency-aware verdict engine never promotes UPSTREAM_DEPENDENCY_BLOCKED -> VERIFIED_PASS
    ev_up = {
        "lane": "stock-ubuntu2404-compat",
        "cpu_is_zen": True,
        "mandatory_gates": {"spirv_val": "PASS", "ubsan": "PASS", "o2": "PASS", "differential_oracle": "PASS", "mutation_observatory": "PASS"},
        "zcc_c_runtime": {"pass": 262, "fail": 0, "live_total": 0, "seq_violations": 0, "faults_injected": 21, "teardown_mask": "0x7ffe", "vk_identity_ok": True},
        "lsan_strict": {"detect_leaks": True, "suppression_used": False, "exit_code": 1, "leaked_bytes": 1280, "leaked_allocs": 10},
        "alloc_audit": {
            "zcc_owned_live_bytes": 0, "vulkan_api_live_bytes": 0,
            "orphaned_on_dlclose_bytes": 1280, "orphaned_on_dlclose_count": 10,
            "target_dso_live_bytes": 1280, "final_target_mapped": 0, "unmapped_target_events": 10,
            "dso_build_ids": [{"module": "libvulkan_lvp.so", "build_id": "8494c48f5f2c2e0ff01872de64b5f9b40867e7a0"}],
            "buckets": [{"module": "libvulkan_lvp.so", "build_id": "8494c48f5f2c2e0ff01872de64b5f9b40867e7a0", "count": 10, "bytes": 1280, "min_size": 128, "max_size": 128, "orphaned_count": 10, "reachable_pre_dlclose": 10, "stack": "libvulkan_lvp.so+0x150de4"}],
        },
    }
    v_up = evaluate_dependency_aware_verdicts(ev_up)
    if v_up["transitive_dependency_memory_correctness"] != UPSTREAM_DEPENDENCY_BLOCKED or v_up["complete_execution_stack_correctness"] != UPSTREAM_DEPENDENCY_BLOCKED or v_up["release_approved"]:
        print("SELF_TEST_FAIL: upstream blocked verdict invariant violated", file=sys.stderr)
        return 1
    tampered_v = dict(v_up)
    tampered_v["complete_execution_stack_correctness"] = VERIFIED_PASS
    if verify_verdict_artifact(tampered_v)[0]:
        print("SELF_TEST_FAIL: forged PASS atop UPSTREAM_DEPENDENCY_BLOCKED was accepted", file=sys.stderr)
        return 1
    ev_sc = dict(ev_up)
    ev_sc["self_certified_override"] = True
    if evaluate_dependency_aware_verdicts(ev_sc)["complete_execution_stack_correctness"] != VERIFIED_FAIL:
        print("SELF_TEST_FAIL: self-certified verdict override was not rejected", file=sys.stderr)
        return 1

    print("VERIFY_TRACK21B_SELF_TEST PASS (all negative controls rejected cleanly)")
    return 0


def verify_staged_tree(repo_root: Path) -> int:
    ok_cl, cl_info = check_manifest_closure(repo_root, staged_only=True)
    ev_path = repo_root / "TRACK21B_RELEASE_EVIDENCE.json"
    ev_ok = False
    ev_verdict = "MISSING"
    if ev_path.is_file():
        ev_data = json.loads(ev_path.read_text(encoding="utf-8"))
        ev_verdict = ev_data.get("ef7_adjudication", {}).get("verdict", "MISSING")
        recorded = ev_data.get("artifact_sha256", {})
        manifest_pairs = "\n".join(f"{k}:{recorded.get(k, '')}" for k in sorted(TRACK21B_MANIFEST_FILES)).encode("utf-8")
        recomputed = sha256_bytes(manifest_pairs)
        ev_ok = (ev_verdict == "PASS" and ev_data.get("canonical_manifest_sha256") == recomputed)

    if not ok_cl or not ev_ok:
        print(f"STAGED_CLOSURE_FAIL ok_closure={ok_cl} ev_ok={ev_ok} ev_verdict={ev_verdict} info={json.dumps(cl_info)}", file=sys.stderr)
        return 7
    print(f"STAGED_CLOSURE_PASS staged_count={len(TRACK21B_MANIFEST_FILES) + 1} manifest_sha256={ev_data['canonical_manifest_sha256']}")
    return 0


def verify_release(
    evidence_dir: Path,
    run_fresh: bool = True,
    write_evidence: bool = True,
    run_hw_dzn: bool = False,
    dependency_evidence_path: Path | None = None,
    dependency_out_path: Path | None = None,
    require_full_stack: bool = False,
) -> int:
    # Rule #2 & #10: Missing external spirv-val / glslangValidator MUST return BLOCKED (exit 3), never PASS
    if not shutil.which("spirv-val") or not shutil.which("glslangValidator"):
        print("RELEASE_BLOCKED: missing external oracle spirv-val or glslangValidator on PATH", file=sys.stderr)
        return 3

    before_snap = snapshot_files(REPO_ROOT, PROTECTED_CORE_AND_T21A_FILES + UNRELATED_WORKSPACE_FILES)

    static_files = [
        "TRACK21B_REPO_AUDIT.json",
        "TRACK21B_COMPUTE_CONTRACT.md",
        "TRACK21B_IMPLEMENTATION_PLAN.md",
        "TRACK21B_INVARIANT_REGISTRY.json",
    ]
    report_files = [
        "TRACK21B_DIFFERENTIAL_REPORT.json",
        "TRACK21B_MUTATION_REPORT.json",
        "TRACK21B_TRANSITIVE_MUTATION_REPORT.json",
        "TRACK21B_PERFORMANCE_REPORT.json",
    ]
    pre_check = static_files if (run_fresh and evidence_dir.resolve() == REPO_ROOT.resolve()) else (static_files + report_files)
    missing = [f for f in pre_check if not (evidence_dir / f).is_file()]
    if missing:
        print(f"RELEASE_BLOCKED: Missing Track 21B deliverables in {evidence_dir}: {missing}", file=sys.stderr)
        return 2

    dirty_protected = ""
    if (REPO_ROOT / ".git").exists() and shutil.which("git"):
        dirty_protected = subprocess.check_output(
            ["git", "status", "--porcelain", "--", *PROTECTED_CORE_AND_T21A_FILES],
            cwd=str(REPO_ROOT),
            text=True,
        ).strip()
    if dirty_protected:
        print(f"RELEASE_BLOCKED: Protected core compiler, BOOTSTRAP_BASELINES.tsv, or Track 21A freeze modified:\n{dirty_protected}", file=sys.stderr)
        return 4

    c_suite_rc = 1
    diff_rc = 1
    mut_rc = 1
    c_suite_ran = False
    if run_fresh and evidence_dir.resolve() == REPO_ROOT.resolve():
        c_suite_rc = subprocess.run(["bash", "tests/track21b/run_vulkan_suite.sh"], cwd=str(REPO_ROOT)).returncode
        c_suite_ran = True
        diff_rc = subprocess.run([sys.executable, "tests/track21b/differential_oracle_21b.py"], cwd=str(REPO_ROOT)).returncode
        mut_rc = subprocess.run([sys.executable, "tests/track21b/mutation_observatory_21b.py"], cwd=str(REPO_ROOT)).returncode
        trans_mut_rc = subprocess.run([sys.executable, "tests/track21b/transitive_mutation_controls.py"], cwd=str(REPO_ROOT)).returncode
        if diff_rc == 3 or mut_rc == 3:
            print(f"RELEASE_BLOCKED: External oracle blocked suite execution (diff={diff_rc}, mut={mut_rc})", file=sys.stderr)
            return 3
        if c_suite_rc != 0 or diff_rc != 0 or mut_rc != 0 or trans_mut_rc != 0:
            print(f"RELEASE_FAILED: Suite execution failed (c={c_suite_rc}, diff={diff_rc}, mut={mut_rc}, trans_mut={trans_mut_rc})", file=sys.stderr)
            return 5

    missing_rep = [f for f in report_files if not (evidence_dir / f).is_file()]
    if missing_rep:
        print(f"RELEASE_BLOCKED: Missing Track 21B report files in {evidence_dir}: {missing_rep}", file=sys.stderr)
        return 2

    diff_rep = json.loads((evidence_dir / "TRACK21B_DIFFERENTIAL_REPORT.json").read_text(encoding="utf-8"))
    mut_rep = json.loads((evidence_dir / "TRACK21B_MUTATION_REPORT.json").read_text(encoding="utf-8"))
    trans_mut_rep = json.loads((evidence_dir / "TRACK21B_TRANSITIVE_MUTATION_REPORT.json").read_text(encoding="utf-8"))
    perf_rep = json.loads((evidence_dir / "TRACK21B_PERFORMANCE_REPORT.json").read_text(encoding="utf-8"))

    if diff_rep.get("verdict") == "BLOCKED" or mut_rep.get("verdict") == "BLOCKED":
        print("RELEASE_BLOCKED: Report indicates BLOCKED external oracle", file=sys.stderr)
        return 3

    spv_ok, spv_status, spv_shas = check_independent_spirv_val()
    if spv_status == "BLOCKED_MISSING_SPIRV_TOOLS":
        print("RELEASE_BLOCKED: spirv-val unavailable during independent SPIR-V validation", file=sys.stderr)
        return 3

    isa_ok, isa_counts = check_portable_isa_v2(Path("/tmp/t21b_build"))
    tamper_ok, tamper_controls = run_v2_tamper_controls()

    rt = ZccVulkanComputeRuntime("/tmp/libzcc_compute_v2.so")
    try:
        vkc = rt.probe_vulkan(enable_validation_layer=True)
        if vkc.vulkan_available != 1 or vkc.validation_layer_enabled != 1:
            print("RELEASE_BLOCKED: Vulkan ICD or VK_LAYER_KHRONOS_validation unavailable", file=sys.stderr)
            return 3
        vk_probe_dict = {
            "vulkan_available": vkc.vulkan_available,
            "validation_layer_enabled": vkc.validation_layer_enabled,
            "device_name": vkc.device_name.decode(),
            "device_type": vkc.device_type_str.decode(),
            "physical_device_type": vkc.physical_device_type,
            "vendor_id": f"0x{vkc.vendor_id:04x}",
            "api_version": vkc.api_version,
        }
        id_ok, id_msg = vk_identity_consistent(vk_probe_dict)
    finally:
        rt.shutdown()

    diff_ok = (
        diff_rep.get("verdict") == "PASS"
        and diff_rep.get("failed", 1) == 0
        and diff_rep.get("total_checks", 0) > 0
        and diff_rep.get("replay_13_ops_count", 0) == 13
        and diff_rep.get("hardware", {}).get("vulkan_device_type") == vk_probe_dict["device_type"]
    )
    mut_ok = (
        mut_rep.get("verdict") == "PASS"
        and mut_rep.get("survived", 1) == 0
        and mut_rep.get("false_positives_on_clean_control", 1) == 0
        and mut_rep.get("killed", 0) == mut_rep.get("total_mutants", -1)
        and mut_rep.get("unknown_signature_probe", {}).get("preserved_as_unknown") is True
    )
    trans_mut_ok = (
        trans_mut_rep.get("verdict") == "PASS"
        and trans_mut_rep.get("survived", 1) == 0
        and trans_mut_rep.get("false_positives_on_clean_control", 1) == 0
        and trans_mut_rep.get("total_controls", 0) >= 9
        and trans_mut_rep.get("killed", 0) == trans_mut_rep.get("total_controls", -1)
    )
    dep_verdict: Dict[str, Any] | None = None
    dep_ok = trans_mut_ok
    dep_upstream_blocked = False
    dep_insufficient = False
    if dependency_evidence_path is not None:
        if not dependency_evidence_path.is_file():
            print(f"RELEASE_BLOCKED: missing --dependency-evidence file: {dependency_evidence_path}", file=sys.stderr)
            return 3
        dep_ev = json.loads(dependency_evidence_path.read_text(encoding="utf-8"))
        dep_verdict = evaluate_dependency_aware_verdicts(dep_ev)
        v_ok, v_msg = verify_verdict_artifact(dep_verdict)
        if not v_ok:
            print(f"RELEASE_FAILED: invalid dependency verdict artifact: {v_msg}", file=sys.stderr)
            return 5
        if dependency_out_path is not None:
            dependency_out_path.write_text(json.dumps(dep_verdict, indent=2) + "\n", encoding="utf-8")
        print(
            f"TRACK21B_DEPENDENCY_VERDICT lane={dep_verdict['lane']} "
            f"zcc={dep_verdict['zcc_owned_resource_correctness']} "
            f"vk_api={dep_verdict['vulkan_api_lifecycle_correctness']} "
            f"transitive={dep_verdict['transitive_dependency_memory_correctness']} "
            f"stack={dep_verdict['complete_execution_stack_correctness']}"
        )
        stack_v = dep_verdict["complete_execution_stack_correctness"]
        if stack_v == INSUFFICIENT_EVIDENCE:
            dep_ok = False
            dep_insufficient = True
        elif stack_v == UPSTREAM_DEPENDENCY_BLOCKED:
            dep_upstream_blocked = True
            dep_ok = (not require_full_stack) and (
                dep_verdict["zcc_owned_resource_correctness"] == VERIFIED_PASS
                and dep_verdict["vulkan_api_lifecycle_correctness"] == VERIFIED_PASS
            )
        else:
            dep_ok = trans_mut_ok and (stack_v == VERIFIED_PASS)
    perf_ok = len(perf_rep.get("results", [])) >= 12
    c_ok = (c_suite_ran and c_suite_rc == 0) if run_fresh else True

    hw_live_ran = False
    if run_hw_dzn and evidence_dir.resolve() == REPO_ROOT.resolve():
        hw_rc = subprocess.run([sys.executable, "tests/track21b/verify_hw_vulkan_dzn.py"], cwd=str(REPO_ROOT)).returncode
        if hw_rc == 3:
            print("RELEASE_BLOCKED: --hw-dzn requested but Dozen /dev/dxg hardware ICD unavailable", file=sys.stderr)
            return 3
        if hw_rc != 0:
            print(f"RELEASE_FAILED: verify_hw_vulkan_dzn failed rc={hw_rc}", file=sys.stderr)
            return 5
        hw_live_ran = True

    hw_rep_path = REPO_ROOT / "TRACK21B_HW_VULKAN_REPORT.json"
    hw_ok, hw_msg = validate_existing_report(hw_rep_path) if hw_rep_path.is_file() else (False, "missing_hw_report")
    hw_rep = json.loads(hw_rep_path.read_text(encoding="utf-8")) if hw_rep_path.is_file() else {}
    hw_devs = hw_rep.get("devices", {}) if isinstance(hw_rep, dict) else {}

    closure_ok, closure_info = check_manifest_closure(REPO_ROOT, staged_only=False)

    after_snap = snapshot_files(REPO_ROOT, PROTECTED_CORE_AND_T21A_FILES + UNRELATED_WORKSPACE_FILES)
    purity_drifts = compare_snapshots(before_snap, after_snap)
    purity_ok = (len(purity_drifts) == 0)
    if not purity_ok:
        print(f"RELEASE_BLOCKED: INV-VERIFIER-PURITY violated on: {purity_drifts}", file=sys.stderr)
        return 6

    ptx_active = bool(diff_rep.get("hardware", {}).get("ptx_available", 0))
    ptx_tag = "PTX_SM120" if ptx_active else "PTX_SM120(skipped:cuda_available=0)"
    nv_dzn = hw_devs.get("nvidia_rtx5070", {})
    amd_dzn = hw_devs.get("amd_radeon", {})

    def _hw_dzn_gate(entry: Dict[str, Any], vendor_hex: str, label: str) -> Dict[str, Any]:
        live_here = hw_live_ran or (str(vk_probe_dict.get("vendor_id", "")).lower() == vendor_hex)
        rec_status = entry.get("status", "NOT_RUN") if hw_ok else "NOT_RUN"
        return {
            "status": "PASS" if (live_here and rec_status == "PASS") else ("NOT_RUN" if rec_status == "PASS" else rec_status),
            "target": f"{label} via Mesa Dozen (dzn) D3D12 bridge ({vendor_hex})",
            "evidence_class": "OBSERVED" if live_here else ("REPORTED" if hw_ok else "UNVERIFIED"),
            "executed_in_this_run": live_here,
            "recorded_workstation_status": rec_status,
            "recorded_report": "TRACK21B_HW_VULKAN_REPORT.json" if hw_rep_path.is_file() else None,
            "validation_msg": hw_msg,
            "differential": entry.get("differential", {}),
        }

    gates = {
        "T21-G01": {
            "status": "PASS" if spv_ok else "FAIL",
            "evidence": {
                "independent_validator": "spirv-val --target-env vulkan1.0",
                "status": spv_status,
                "validated_modules": len(spv_shas),
                "deterministic_2x": spv_ok,
                "module_sha256": spv_shas,
            },
        },
        "T21-G02-CPU": {
            "status": "PASS" if (c_ok and diff_ok and id_ok) else "FAIL",
            "evidence": {
                "oracle_type": (
                    "Software Vulkan CPU Execution Oracle (Mesa llvmpipe)"
                    if vk_probe_dict["device_type"] == "CPU"
                    else f"Active Vulkan ICD ({vk_probe_dict['device_type']})"
                ),
                "physical_device_type": vk_probe_dict["device_type"],
                "device_name": vk_probe_dict["device_name"],
                "vendor_id": vk_probe_dict["vendor_id"],
                "validation_layer_enabled": True,
                "identity_check": id_msg,
            },
        },
        "T21B-GPU-NV-DZN": _hw_dzn_gate(nv_dzn, "0x10de", "NVIDIA GeForce RTX 5070 Laptop GPU"),
        "T21B-GPU-AMD-DZN": _hw_dzn_gate(amd_dzn, "0x1002", "AMD Radeon(TM) Graphics"),
        "T21B-GPU-NV-NATIVE": {
            "status": "DEFERRED",
            "target": "Native Win64 NVIDIA GeForce RTX 5070 Vulkan ICD 1.4.303 (0x10de)",
            "reason": "Unsigned native Windows executables blocked by Windows Smart App Control (VerifiedAndReputablePolicyState=1).",
        },
        "T21B-GPU-AMD-NATIVE": {
            "status": "DEFERRED",
            "target": "Native Win64 AMD Radeon Graphics Vulkan ICD 1.4.329 (0x1002)",
            "reason": "Unsigned native Windows executables blocked by Windows Smart App Control (VerifiedAndReputablePolicyState=1).",
        },
        "T21B-G03": {"status": "PASS" if (isa_ok and diff_ok) else "FAIL", "evidence": {"portable_avx_opcode_counts": isa_counts}},
        "T21B-G04": {"status": "PASS" if (c_ok and diff_ok) else "FAIL", "evidence": f"Scalar C99, AVX2_FMA, {ptx_tag}, and VULKAN_SPV verified cleanly"},
        "T21B-G05": {"status": "PASS" if diff_ok else "FAIL", "evidence": f"{diff_rep.get('passed')}/{diff_rep.get('total_checks')} PASS across Scalar/AVX2/{'PTX/' if ptx_active else ''}Vulkan SPIR-V"},
        "T21B-G06": {"status": "PASS" if c_ok else "FAIL", "evidence": f"Full UBSan ({'CPU+PTX+Vulkan' if ptx_active else 'CPU+Vulkan'}) + ASan+LSan+UBSan (CPU+Vulkan+Validation Layer) 0 errors"},
        "T21B-G07": {"status": "PASS" if mut_ok else "FAIL", "evidence": f"{mut_rep.get('killed')}/{mut_rep.get('total_mutants')} controlled mutants killed; false_pos=0; unknown preserved"},
        "T21B-G08": {"status": "PASS" if diff_ok else "FAIL", "evidence": "13/13 operations deterministic 3x replay on VULKAN_SPV"},
        "T21B-G09": {"status": "PASS" if tamper_ok else "FAIL", "evidence": tamper_controls},
        "T21B-TRANSITIVE": {
            "status": UPSTREAM_DEPENDENCY_BLOCKED if dep_upstream_blocked else ("PASS" if dep_ok else "FAIL"),
            "evidence": {
                "transitive_mutation_controls": f"{trans_mut_rep.get('killed')}/{trans_mut_rep.get('total_controls')} KILLED",
                "dependency_verdict": dep_verdict,
            },
        },
        "T21B-G10": {
            "status": "PASS" if (c_ok and spv_ok and id_ok and isa_ok and diff_ok and mut_ok and trans_mut_ok and dep_ok and perf_ok and tamper_ok and closure_ok and purity_ok and not dirty_protected) else "FAIL",
            "evidence": {"release_closure": closure_info, "verifier_purity_drifts": purity_drifts},
        },
    }

    active_keys = ["T21-G01", "T21-G02-CPU", "T21B-G03", "T21B-G04", "T21B-G05", "T21B-G06", "T21B-G07", "T21B-G08", "T21B-G09", "T21B-TRANSITIVE", "T21B-G10"]
    if run_hw_dzn:
        active_keys.extend(["T21B-GPU-NV-DZN", "T21B-GPU-AMD-DZN"])
    active_pass = sum(1 for k in active_keys if gates[k]["status"] == "PASS")
    other_keys = [k for k in active_keys if k != "T21B-TRANSITIVE"]
    other_all_pass = all(gates[k]["status"] == "PASS" for k in other_keys)
    if dep_insufficient:
        verdict = INSUFFICIENT_EVIDENCE
    elif dep_upstream_blocked:
        verdict = UPSTREAM_DEPENDENCY_BLOCKED
    else:
        verdict = "PASS" if active_pass == len(active_keys) else "FAIL"

    artifact_hashes = {f: sha256_file(REPO_ROOT / f) for f in sorted(TRACK21B_MANIFEST_FILES) if (REPO_ROOT / f).is_file()}
    manifest_pairs = "\n".join(f"{k}:{artifact_hashes.get(k, '')}" for k in sorted(TRACK21B_MANIFEST_FILES)).encode("utf-8")
    canonical_manifest_sha = sha256_bytes(manifest_pairs)
    for aux in ("/tmp/libzcc_compute_v2.so", "/tmp/zcc-spirv"):
        if Path(aux).is_file():
            artifact_hashes[aux] = sha256_file(Path(aux))

    git_head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=str(REPO_ROOT), text=True).strip() if ((REPO_ROOT / ".git").exists() and shutil.which("git")) else "UNKNOWN"

    release_record = {
        "schema_version": "track21b.release_evidence.v1",
        "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "governing_rule_version": "ZCC-EF-7 v1.0.0 + ZCC-EF-0",
        "evidence_tier": "E3" if run_fresh else "E2",
        "git_head_sha": git_head,
        "manifest_file_count": len(TRACK21B_MANIFEST_FILES),
        "canonical_manifest_sha256": canonical_manifest_sha,
        "vulkan_execution_oracle": vk_probe_dict,
        "oversized_files": closure_info["oversized"],
        "dirty_protected_files": dirty_protected,
        "verifier_purity_drifts": purity_drifts,
        "release_closure": closure_info,
        "artifact_sha256": artifact_hashes,
        "gates": gates,
        "host_limitations": [
            "Default WSL2/Ubuntu CI Vulkan ICD exposes Mesa llvmpipe (VK_PHYSICAL_DEVICE_TYPE_CPU, vendor 0x10005); hardware GPUs (0x10de RTX 5070, 0x1002 AMD Radeon) are verified separately over Mesa Dozen (dzn) D3D12 (/dev/dxg) in TRACK21B_HW_VULKAN_REPORT.json.",
            "Native Win64 proprietary Vulkan ICD execution gates T21B-GPU-NV-NATIVE (0x10de) and T21B-GPU-AMD-NATIVE (0x1002) remain DEFERRED because unsigned native Win64 binaries are blocked by enforced Windows Smart App Control.",
            "NVIDIA proprietary WSL2 libcuda.so.1 triggers an internal ASan double-free inside cuInit(0), and WSL2 D3D12 UMDs (libd3d12core.so) retain global allocations across dlclose; strict LSan leak checks execute cleanly against Mesa llvmpipe.",
        ],
        "ef7_adjudication": {
            "claim": "ZCC Track 21B Verified SPIR-V and Vulkan Compute satisfies T21-G01, T21-G02-CPU, and T21B-G03..G10 while preserving Track 21A and core bootstrap baselines; hardware GPU Dozen D3D12 evidence (0x10de RTX 5070 & 0x1002 AMD Radeon) is tracked in TRACK21B_HW_VULKAN_REPORT.json, and direct unsigned Win64 native ICD gates remain DEFERRED.",
            "evidence_class": "OBSERVED",
            "evidence_tier": "E3" if run_fresh else "E2",
            "verdict": verdict,
        },
    }

    if write_evidence:
        (evidence_dir / "TRACK21B_RELEASE_EVIDENCE.json").write_text(json.dumps(release_record, indent=2) + "\n", encoding="utf-8")

    print(
        f"TRACK21B_RELEASE_VERIFIER verdict={verdict} active_gates={active_pass}/{len(active_keys)} "
        f"hw_dzn_report={'PASS' if hw_ok else 'UNVERIFIED'} deferred=T21B-GPU-NV-NATIVE,T21B-GPU-AMD-NATIVE "
        f"vk_dev={vk_probe_dict['device_type']} manifest_sha256={canonical_manifest_sha[:16]}"
    )
    if verdict == "PASS":
        return 0
    if verdict == INSUFFICIENT_EVIDENCE:
        return 3
    if verdict == UPSTREAM_DEPENDENCY_BLOCKED:
        if not require_full_stack and dep_ok and other_all_pass:
            return 0
        return 4
    return 5


def main() -> int:
    parser = argparse.ArgumentParser(description="Independent Fail-Closed Release Verifier for ZCC Track 21B")
    parser.add_argument("--self-test", action="store_true", help="Run ZCC-EF-0 negative self-tests")
    parser.add_argument("--no-rerun", action="store_true", help="Verify existing artifacts without re-running suites")
    parser.add_argument("--verify-staged", action="store_true", help="Verify exact staged closure and index SHA-256 bindings")
    parser.add_argument("--hw-dzn", action="store_true", help="Also execute live Mesa Dozen D3D12 hardware GPU verification")
    parser.add_argument("--dependency-evidence", default="", help="Optional dependency-aware evidence JSON to evaluate")
    parser.add_argument("--dependency-out", default="", help="Optional output path for dependency verdict JSON")
    parser.add_argument("--require-full-stack", action="store_true", help="Fail closed unless full execution stack is VERIFIED_PASS")
    parser.add_argument("--verify-dir", default=str(REPO_ROOT), help="Directory containing Track 21B deliverables")
    args = parser.parse_args()

    if args.self_test:
        return run_verifier_self_test()
    if args.verify_staged:
        return verify_staged_tree(REPO_ROOT)
    return verify_release(
        Path(args.verify_dir),
        run_fresh=not args.no_rerun,
        write_evidence=(not args.no_rerun and Path(args.verify_dir).resolve() == REPO_ROOT.resolve()),
        run_hw_dzn=args.hw_dzn,
        dependency_evidence_path=Path(args.dependency_evidence) if args.dependency_evidence else None,
        dependency_out_path=Path(args.dependency_out) if args.dependency_out else None,
        require_full_stack=args.require_full_stack,
    )


if __name__ == "__main__":
    raise SystemExit(main())
