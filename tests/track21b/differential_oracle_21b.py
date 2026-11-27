#!/usr/bin/env python3
"""ZCC Track 21B — Independent Differential Numerical Oracle & Vulkan/SPIR-V Verification Suite.

Enforces ZCC-EF-0, ZCC-EF-7, INV-T21B-001..006:
- Fails closed (verdict="BLOCKED", exit 3) if independent Khronos spirv-val,
  glslangValidator, libvulkan, or VK_LAYER_KHRONOS_validation is missing.
- Verifies 2x deterministic .spv binary emission + spirv-val (--target-env vulkan1.0)
  on all 13 Track 21B kernels.
- Independently compares SCALAR_C99 (FP64_REF & FP32_FMA), AVX2_FMA, PTX_SM120, and
  VULKAN_SPV (Mesa llvmpipe CPU ICD with validation layer enabled) against IEEE-754
  binary64 Python oracles across normal, prime/boundary, strided, unaligned (+1/+4/+8),
  extreme/subnormal, NaN/+Inf/-Inf, replay determinism, and negative capability limits.
"""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import math
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

from sdk.zcc_vulkan_compute import (  # noqa: E402
    ABI_V2_VERSION,
    ACCUM_FP32_FMA,
    ACCUM_FP64_REF,
    BACKEND_AVX2_FMA,
    BACKEND_PTX_SM120,
    BACKEND_SCALAR_C99,
    BACKEND_VULKAN_GPU,
    BACKEND_VULKAN_SPV,
    DET_FIXED_REDUCTION,
    DTYPE_F32,
    DTYPE_Q4_0,
    DTYPE_Q4_K,
    DTYPE_Q8_0,
    OP_DEQUANT_Q4_0,
    OP_DEQUANT_Q4_K,
    OP_DEQUANT_Q8_0,
    OP_DOT_F32,
    OP_GEMM_F32,
    OP_MATVEC_Q4_0,
    OP_MATVEC_Q4_K,
    OP_MATVEC_Q8_0,
    OP_RMSNORM_F32,
    OP_SOFTMAX_F32,
    OP_VEC_ADD_F32,
    OP_VEC_FMA_F32,
    OP_VEC_MUL_F32,
    OWNER_BORROWED_HOST,
    OWNER_READONLY,
    POLICY_AUTO_FALLBACK,
    POLICY_EXACT_REQUIRED,
    VK_DEVTYPE_CPU,
    ZccComputeV2Receipt,
    ZccTensorDesc,
    ZccVulkanCaps,
    ZccVulkanComputeRuntime,
    ZccVulkanExecOptions,
)
from tests.track21a.differential_oracle import (  # noqa: E402
    compare_vectors,
    make_f32_desc,
    make_q4_0_bytes,
    make_q4_k_bytes,
    make_q8_0_bytes,
    make_quant_desc,
    oracle_dequant_q4_0,
    oracle_dequant_q4_k,
    oracle_dequant_q8_0,
    oracle_dot,
    oracle_gemm,
    oracle_matvec_quant,
    oracle_rmsnorm,
    oracle_softmax,
    oracle_vec_add,
    oracle_vec_fma,
    oracle_vec_mul,
    read_f32_desc,
    round_f32,
)


def make_quant_desc_offset(
    raw: bytes,
    dtype: int,
    shape: Tuple[int, ...],
    byte_offset: int = 0,
) -> Tuple[ZccTensorDesc, ctypes.Array]:
    """Allocate a quantized host buffer shifted by `byte_offset` bytes."""
    backing = (ctypes.c_uint8 * (byte_offset + len(raw) + 16))()
    ctypes.memmove(ctypes.addressof(backing) + byte_offset, raw, len(raw))
    desc = ZccTensorDesc()
    desc.struct_size = ctypes.sizeof(ZccTensorDesc)
    desc.dtype = dtype
    desc.rank = len(shape)
    desc.ownership_flags = OWNER_BORROWED_HOST
    for i, d in enumerate(shape):
        desc.shape[i] = d
    desc.data = ctypes.addressof(backing) + byte_offset
    desc.nbytes = len(raw)
    desc.alignment = 1
    return desc, backing


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_spirv_modules_and_determinism(cli_path: str) -> Tuple[bool, str, List[Dict[str, Any]]]:
    spirv_val = shutil.which("spirv-val")
    glslang = shutil.which("glslangValidator")
    if not spirv_val or not glslang:
        return False, "BLOCKED_MISSING_SPIRV_TOOLS", []

    records: List[Dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="t21b_spv_diff1_") as d1, tempfile.TemporaryDirectory(
        prefix="t21b_spv_diff2_"
    ) as d2:
        for d in (d1, d2):
            p = subprocess.run([cli_path, "--emit-all", d], capture_output=True, text=True)
            if p.returncode != 0:
                return False, f"CLI_FAIL:{p.stderr.strip()}", records

        files1 = sorted(Path(d1).glob("*.spv"))
        files2 = sorted(Path(d2).glob("*.spv"))
        if len(files1) != 13 or [f.name for f in files1] != [f.name for f in files2]:
            return False, f"UNEXPECTED_SPV_COUNT:{len(files1)}", records

        for f1, f2 in zip(files1, files2):
            s1 = sha256_file(f1)
            s2 = sha256_file(f2)
            v = subprocess.run(
                [spirv_val, "--target-env", "vulkan1.0", str(f1)],
                capture_output=True,
                text=True,
            )
            ok = (s1 == s2 and v.returncode == 0)
            records.append(
                {
                    "module": f1.name,
                    "bytes": f1.stat().st_size,
                    "sha256": s1,
                    "deterministic": (s1 == s2),
                    "spirv_val_rc": v.returncode,
                    "spirv_val_err": v.stderr.strip(),
                    "status": "PASS" if ok else "FAIL",
                }
            )
            if not ok:
                return False, f"SPIRV_CHECK_FAILED:{f1.name}", records
    return True, "OK", records


def write_blocked_report(out_path: Path, reason: str, details: Dict[str, Any]) -> int:
    report = {
        "schema_version": "track21b.differential_report.v1",
        "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "verdict": "BLOCKED",
        "blocked_reason": reason,
        "external_oracles": details,
        "total_checks": 0,
        "passed": 0,
        "failed": 0,
        "cases": [],
        "edge_checks": [],
        "replay_checks": [],
    }
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"TRACK21B_DIFF_ORACLE verdict=BLOCKED reason={reason}")
    return 3


def collect_performance_report(
    rt: ZccVulkanComputeRuntime,
    rng: random.Random,
    cpu_avx2_ok: bool,
    ptx_available: bool,
    vk_caps: ZccVulkanCaps,
) -> Dict[str, Any]:
    """Measure real execution latency across Scalar, AVX2, PTX, and Vulkan SPIR-V."""
    m_g, k_g, n_g = 128, 512, 128
    a_g = round_f32([rng.uniform(-1.0, 1.0) for _ in range(m_g * k_g)])
    b_g = round_f32([rng.uniform(-1.0, 1.0) for _ in range(k_g * n_g)])
    da_g, _ba_g = make_f32_desc(a_g, (m_g, k_g))
    db_g, _bb_g = make_f32_desc(b_g, (k_g, n_g))

    rows_n, dim_n = 128, 512
    x_n = round_f32([rng.uniform(-2.0, 2.0) for _ in range(rows_n * dim_n)])
    w_n = round_f32([rng.uniform(0.5, 1.5) for _ in range(dim_n)])
    dx_n, _bx_n = make_f32_desc(x_n, (rows_n, dim_n))
    dw_n, _bw_n = make_f32_desc(w_n, (dim_n,))

    q4k_raw = make_q4_k_bytes(rng, rows_n, dim_n)
    dq4k, _bq4k = make_quant_desc(q4k_raw, DTYPE_Q4_K, (rows_n, dim_n))
    x_q = round_f32([rng.uniform(-1.0, 1.0) for _ in range(dim_n)])
    dxq, _bxq = make_f32_desc(x_q, (dim_n,))

    workloads = [
        ("GEMM_F32_128x512x128", OP_GEMM_F32, [da_g, db_g], (m_g, n_g)),
        ("RMSNORM_F32_128x512", OP_RMSNORM_F32, [dx_n, dw_n], (rows_n, dim_n)),
        ("SOFTMAX_F32_128x512", OP_SOFTMAX_F32, [dx_n], (rows_n, dim_n)),
        ("MATVEC_Q4_K_128x512", OP_MATVEC_Q4_K, [dq4k, dxq], (rows_n,)),
    ]
    bench_backends: List[Tuple[str, int, Any, bool]] = [
        ("SCALAR_C99", BACKEND_SCALAR_C99, None, False),
    ]
    if cpu_avx2_ok:
        bench_backends.append(("AVX2_FMA", BACKEND_AVX2_FMA, None, False))
    if ptx_available:
        bench_backends.append(("PTX_SM120", BACKEND_PTX_SM120, None, False))
    ctx_val = rt.create_context(enable_validation_layer=True)
    ctx_fast = rt.create_context(enable_validation_layer=False)
    vk_dev_label = vk_caps.device_type_str.decode()
    bench_backends.append((f"VULKAN_SPV_{vk_dev_label}", BACKEND_VULKAN_SPV, None, True))
    bench_backends.append(("VULKAN_SPV_CTX_VAL", BACKEND_VULKAN_SPV, ctx_val, True))
    bench_backends.append(("VULKAN_SPV_CTX_FAST", BACKEND_VULKAN_SPV, ctx_fast, False))

    results: List[Dict[str, Any]] = []
    try:
        for label, op_id, in_descs, out_shape in workloads:
            n_out = 1
            for d in out_shape:
                n_out *= d
            out_d, _out_b = make_f32_desc([0.0] * n_out, out_shape)
            for bname, bid, bctx, req_val in bench_backends:
                rc_w, _, _ = rt.exec_v2(
                    op_id, in_descs, out_d, backend=bid, policy=POLICY_EXACT_REQUIRED,
                    accum=ACCUM_FP32_FMA, require_validation_layer=req_val, ctx=bctx,
                )
                iters = 3
                t0 = time.perf_counter()
                last_json: Dict[str, Any] = {}
                for _ in range(iters):
                    rc_r, last_json, _ = rt.exec_v2(
                        op_id, in_descs, out_d, backend=bid, policy=POLICY_EXACT_REQUIRED,
                        accum=ACCUM_FP32_FMA, require_validation_layer=req_val, ctx=bctx,
                    )
                avg_ms = ((time.perf_counter() - t0) / iters) * 1000.0
                results.append({
                    "workload": label,
                    "backend": bname,
                    "device_type": vk_caps.device_type_str.decode() if bid == BACKEND_VULKAN_SPV else bname,
                    "status_code": rc_r if rc_w == 0 else rc_w,
                    "avg_ms": round(avg_ms, 3),
                    "output_sha256": last_json.get("output_buffer_sha256", ""),
                })
    finally:
        ctx_fast.close()
        ctx_val.close()
    return {
        "schema_version": "track21b.performance_report.v1",
        "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "note": (
            f"VULKAN_SPV executed on {vk_caps.device_type_str.decode()} ICD "
            f"({vk_caps.device_name.decode()}); compares stateless vs persistent zcc_vulkan_ctx_t "
            "(validated and non-validation release); native GPU gates T21B-GPU-NV and T21B-GPU-AMD are DEFERRED."
        ),
        "results": results,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="ZCC Track 21B Differential Oracle")
    parser.add_argument("--lib-path", default="/tmp/libzcc_compute_v2.so")
    parser.add_argument("--cli-path", default="/tmp/zcc-spirv")
    parser.add_argument("--report-out", default=str(REPO_ROOT / "TRACK21B_DIFFERENTIAL_REPORT.json"))
    parser.add_argument("--perf-out", default=str(REPO_ROOT / "TRACK21B_PERFORMANCE_REPORT.json"))
    args = parser.parse_args()

    out_path = Path(args.report_out)
    spirv_val_bin = shutil.which("spirv-val")
    glslang_bin = shutil.which("glslangValidator")
    if not spirv_val_bin or not glslang_bin:
        return write_blocked_report(
            out_path,
            "missing_external_spirv_tools",
            {"spirv_val": spirv_val_bin, "glslangValidator": glslang_bin},
        )

    spv_ok, spv_reason, spv_records = check_spirv_modules_and_determinism(args.cli_path)
    if not spv_ok:
        if spv_reason == "BLOCKED_MISSING_SPIRV_TOOLS":
            return write_blocked_report(out_path, spv_reason, {"spirv_val": spirv_val_bin})
        report = {
            "schema_version": "track21b.differential_report.v1",
            "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "verdict": "FAIL",
            "reason": spv_reason,
            "spirv_modules": spv_records,
        }
        out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"TRACK21B_DIFF_ORACLE verdict=FAIL reason={spv_reason}")
        return 1

    rt = ZccVulkanComputeRuntime(args.lib_path)
    try:
        cpu_caps, ptx_caps = rt.probe_caps()
        vk_caps = rt.probe_vulkan(enable_validation_layer=True)
        if vk_caps.vulkan_available != 1 or vk_caps.validation_layer_enabled != 1:
            return write_blocked_report(
                out_path,
                "missing_vulkan_or_validation_layer",
                {
                    "spirv_val": spirv_val_bin,
                    "glslangValidator": glslang_bin,
                    "vulkan_available": vk_caps.vulkan_available,
                    "validation_layer_available": vk_caps.validation_layer_available,
                    "validation_layer_enabled": vk_caps.validation_layer_enabled,
                },
            )

        rng = random.Random(21022)
        vk_dev_type = vk_caps.device_type_str.decode()
        backends = [
            ("SCALAR_C99_FP64", BACKEND_SCALAR_C99, ACCUM_FP64_REF),
            ("SCALAR_C99_FMA", BACKEND_SCALAR_C99, ACCUM_FP32_FMA),
        ]
        if cpu_caps.has_avx2 and cpu_caps.has_fma:
            backends.append(("AVX2_FMA", BACKEND_AVX2_FMA, ACCUM_FP32_FMA))
        if ptx_caps.cuda_available == 1:
            backends.append(("PTX_SM120", BACKEND_PTX_SM120, ACCUM_FP32_FMA))
        backends.append((f"VULKAN_SPV_{vk_dev_type}", BACKEND_VULKAN_SPV, ACCUM_FP32_FMA))

        cases: List[Dict[str, Any]] = []
        replay_checks: List[Dict[str, Any]] = []
        edge_checks: List[Dict[str, Any]] = []

        def run_one(
            case_name: str,
            op_id: int,
            inputs: List[Tuple[ZccTensorDesc, Any]],
            out_shape: Tuple[int, ...],
            out_strides: Optional[Tuple[int, ...]],
            out_offset: int,
            ref: List[float],
            atol: float,
            rtol: float,
            eps: float = 1e-5,
            expect_nan: bool = False,
            softmax_dim: Optional[int] = None,
            skip_fp64: bool = False,
            cancel_tol: Optional[float] = None,
            record_replay_vk: bool = False,
        ) -> None:
            in_arr = (ZccTensorDesc * len(inputs))(*(d for d, _ in inputs))
            for bname, bid, acc in backends:
                if skip_fp64 and acc == ACCUM_FP64_REF:
                    continue
                if bid == BACKEND_VULKAN_SPV and acc == ACCUM_FP64_REF:
                    continue
                atol_use = (1e-5 if acc == ACCUM_FP64_REF else cancel_tol) if cancel_tol is not None else atol

                out_desc, out_buf = make_f32_desc([0.0] * len(ref), out_shape, out_strides, out_offset, align=1)
                opts = ZccVulkanExecOptions()
                rt.lib.zcc_vulkan_exec_options_init(ctypes.byref(opts))
                opts.base.requested_backend = bid
                opts.base.selection_policy = POLICY_EXACT_REQUIRED
                opts.base.accum_policy = acc
                opts.base.determinism_mode = DET_FIXED_REDUCTION
                opts.base.rmsnorm_eps = ctypes.c_float(eps)
                opts.require_validation_layer = 1

                rec = ZccComputeV2Receipt()
                rc = rt.lib.zcc_compute_exec_v2(
                    op_id, in_arr, len(inputs), ctypes.byref(out_desc), ctypes.byref(opts), ctypes.byref(rec)
                )
                if rc != 0:
                    cases.append(
                        {
                            "case": case_name,
                            "backend": bname,
                            "status": "FAIL",
                            "detail": f"exec_v2 rc={rc} err={rec.base.error_message.decode()}",
                        }
                    )
                    continue
                if rec.base.backend_executed != bid or rec.base.fallback_occurred != 0:
                    cases.append(
                        {
                            "case": case_name,
                            "backend": bname,
                            "status": "FAIL",
                            "detail": f"unexpected backend={rec.base.backend_executed} fb={rec.base.fallback_occurred}",
                        }
                    )
                    continue
                if rt.lib.zcc_receipt_v2_verify_canonical(ctypes.byref(rec), None, 0) != 0:
                    cases.append({"case": case_name, "backend": bname, "status": "FAIL", "detail": "v2 receipt verify failed"})
                    continue
                if bid == BACKEND_VULKAN_SPV:
                    if (
                        rec.validation_error_count != 0
                        or rec.sync_barrier_count < 2
                        or rec.vulkan_caps.physical_device_type != vk_caps.physical_device_type
                    ):
                        cases.append(
                            {
                                "case": case_name,
                                "backend": bname,
                                "status": "FAIL",
                                "detail": (
                                    f"vk invariant valerr={rec.validation_error_count} "
                                    f"bar={rec.sync_barrier_count} devtype={rec.vulkan_caps.device_type_str.decode()}"
                                ),
                            }
                        )
                        continue

                got = read_f32_desc(out_desc, out_buf, out_offset)
                ok, ae, re, ulp, msg = compare_vectors(
                    got, ref, atol_use, rtol, expect_nan=expect_nan, check_softmax_sum=softmax_dim
                )
                cases.append(
                    {
                        "case": case_name,
                        "backend": bname,
                        "device_type": rec.vulkan_caps.device_type_str.decode() if bid == BACKEND_VULKAN_SPV else "HOST_OR_CUDA",
                        "status": "PASS" if ok else "FAIL",
                        "max_abs_err": None if math.isinf(ae) else round(ae, 10),
                        "max_rel_err": None if math.isinf(re) else round(re, 10),
                        "max_ulp": ulp,
                        "output_sha256": rec.base.output_buffer_sha256.decode(),
                        "spirv_sha256": rec.spirv_module_sha256_hex.decode(),
                        "detail": msg,
                    }
                )

                if record_replay_vk and bid == BACKEND_VULKAN_SPV and ok:
                    shas = [rec.base.output_buffer_sha256.decode()]
                    spvs = [rec.spirv_module_sha256_hex.decode()]
                    for _ in range(2):
                        od2, ob2 = make_f32_desc([0.0] * len(ref), out_shape, out_strides, out_offset, align=1)
                        rec2 = ZccComputeV2Receipt()
                        rc2 = rt.lib.zcc_compute_exec_v2(
                            op_id, in_arr, len(inputs), ctypes.byref(od2), ctypes.byref(opts), ctypes.byref(rec2)
                        )
                        if rc2 != 0 or rec2.validation_error_count != 0:
                            shas.append("EXEC_ERR")
                        else:
                            shas.append(rec2.base.output_buffer_sha256.decode())
                            spvs.append(rec2.spirv_module_sha256_hex.decode())
                    rep_ok = len(set(shas)) == 1 and len(set(spvs)) == 1 and shas[0] != "EXEC_ERR"
                    replay_checks.append(
                        {
                            "case": case_name,
                            "op_id": op_id,
                            "status": "PASS" if rep_ok else "FAIL",
                            "output_sha256_3x": shas,
                            "spirv_sha256": spvs[0],
                        }
                    )

        # 1. Elementwise (normal, prime, unaligned +4/+1, non-contiguous stride)
        for tag, n, off, stride, rep_flag in [
            ("normal_256", 256, 0, None, True),
            ("prime_1", 1, 0, None, False),
            ("prime_17_unaligned4", 17, 4, None, False),
            ("boundary_33_unaligned1", 33, 1, None, False),
            ("strided_23_s12", 23, 8, (12,), False),
        ]:
            a = round_f32([rng.uniform(-50.0, 50.0) for _ in range(n)])
            b = round_f32([rng.uniform(-50.0, 50.0) for _ in range(n)])
            c = round_f32([rng.uniform(-50.0, 50.0) for _ in range(n)])
            da = make_f32_desc(a, (n,), stride, off, align=1)
            db = make_f32_desc(b, (n,), stride, off, align=1)
            dc = make_f32_desc(c, (n,), stride, off, align=1)
            run_one(f"vec_add_{tag}", OP_VEC_ADD_F32, [da, db], (n,), stride, off, oracle_vec_add(a, b), 1e-6, 1e-6, record_replay_vk=rep_flag)
            run_one(f"vec_mul_{tag}", OP_VEC_MUL_F32, [da, db], (n,), stride, off, oracle_vec_mul(a, b), 1e-6, 1e-6, record_replay_vk=rep_flag)
            run_one(f"vec_fma_{tag}", OP_VEC_FMA_F32, [da, db, dc], (n,), stride, off, oracle_vec_fma(a, b, c), 1e-5, 1e-5, record_replay_vk=rep_flag)

        # 2. Dot products + extreme/subnormal + cancellation condition-number bound
        a63 = round_f32([rng.uniform(-2.0, 2.0) for _ in range(63)])
        b63 = round_f32([rng.uniform(-2.0, 2.0) for _ in range(63)])
        run_one("dot_prime_63", OP_DOT_F32, [make_f32_desc(a63, (63,)), make_f32_desc(b63, (63,))], (1,), None, 0, oracle_dot(a63, b63), 1e-4, 1e-4, record_replay_vk=True)

        a_sub = round_f32([1e-18, 1e3, 1e-18, 3.5, 1e-20])
        b_sub = round_f32([1e-18, 0.25, 1.0, 2.0, 1.0])
        run_one("dot_extreme_subnormal", OP_DOT_F32, [make_f32_desc(a_sub, (5,)), make_f32_desc(b_sub, (5,))], (1,), None, 0, oracle_dot(a_sub, b_sub), 1e-4, 1e-4)

        a_cat = round_f32([1e-18, 1e9, -1e9, 1e-18, 3.5])
        b_cat = round_f32([1e-18, 1.0, 1.0, 1.0, 2.0])
        cond_sum = sum(abs(x * y) for x, y in zip(a_cat, b_cat))
        fp32_cond_tol = len(a_cat) * 1.5e-7 * cond_sum
        run_one("dot_cancellation_fp64_vs_cond_bound", OP_DOT_F32, [make_f32_desc(a_cat, (5,)), make_f32_desc(b_cat, (5,))], (1,), None, 0, oracle_dot(a_cat, b_cat), 1e-5, 1e-5, cancel_tol=fp32_cond_tol)

        # 3. GEMM (normal, prime + unaligned +4, padded row stride, transposed stride)
        for tag, m, k, n, sa, sb, sc, rep_flag in [
            ("gemm_16x32x24", 16, 32, 24, None, None, None, True),
            ("gemm_prime_7x19x11_unaligned4", 7, 19, 11, None, None, None, False),
            ("gemm_padded_stride", 5, 13, 9, ((13 + 3) * 4, 4), ((9 + 5) * 4, 4), ((9 + 2) * 4, 4), False),
            ("gemm_transposed_A_stride", 4, 6, 5, (4, 4 * 4), None, None, False),
        ]:
            off = 4 if "unaligned4" in tag else 0
            a = round_f32([rng.uniform(-1.5, 1.5) for _ in range(m * k)])
            b = round_f32([rng.uniform(-1.5, 1.5) for _ in range(k * n)])
            da = make_f32_desc(a, (m, k), sa, off, align=1)
            db = make_f32_desc(b, (k, n), sb, off, align=1)
            run_one(tag, OP_GEMM_F32, [da, db], (m, n), sc, off, oracle_gemm(a, b, m, k, n), 1e-4, 1e-4, record_replay_vk=rep_flag)

        # 4. RMSNorm (normal, prime padded, extreme magnitudes)
        for tag, rows, dim, scale, rs, rep_flag in [
            ("rmsnorm_8x128", 8, 128, 1.0, None, True),
            ("rmsnorm_prime_3x37_padded", 3, 37, 1.0, ((37 + 5) * 4, 4), False),
            ("rmsnorm_extreme_mag", 2, 64, 1e5, None, False),
        ]:
            x = round_f32([rng.uniform(-2.0, 2.0) * scale for _ in range(rows * dim)])
            w = round_f32([rng.uniform(0.5, 1.5) for _ in range(dim)])
            dx = make_f32_desc(x, (rows, dim), rs)
            dw = make_f32_desc(w, (dim,))
            run_one(tag, OP_RMSNORM_F32, [dx, dw], (rows, dim), rs, 0, oracle_rmsnorm(x, w, rows, dim, 1e-5), 1e-4, 2e-4, record_replay_vk=rep_flag)

        # 5. Softmax (normal, extreme logits, -Inf, NaN, +Inf)
        for tag, rows, dim, xvals, exp_nan, rep_flag in [
            ("softmax_6x64", 6, 64, round_f32([rng.uniform(-5.0, 5.0) for _ in range(6 * 64)]), False, True),
            ("softmax_prime_3x19_extreme", 3, 19, round_f32([1000.0 + (i % 19) * 0.25 for i in range(3 * 19)]), False, False),
            ("softmax_neginf", 1, 5, [float("-inf"), -1.0, 0.0, 1.0, float("-inf")], False, False),
            ("softmax_nan_prop", 1, 4, [1.0, float("nan"), 2.0, 3.0], True, False),
            ("softmax_posinf", 1, 4, [1.0, float("inf"), 2.0, 3.0], True, False),
        ]:
            dx = make_f32_desc(xvals, (rows, dim))
            run_one(
                tag,
                OP_SOFTMAX_F32,
                [dx],
                (rows, dim),
                None,
                0,
                oracle_softmax(xvals, rows, dim),
                1e-4,
                2e-4,
                expect_nan=exp_nan,
                softmax_dim=dim,
                record_replay_vk=rep_flag,
            )

        # 6. NaN / Inf elementwise propagation
        nan_a = [1.0, float("nan"), float("inf"), float("-inf"), 0.0]
        nan_b = [2.0, 3.0, 4.0, -5.0, float("inf")]
        run_one(
            "vec_mul_nan_inf",
            OP_VEC_MUL_F32,
            [make_f32_desc(nan_a, (5,)), make_f32_desc(nan_b, (5,))],
            (5,),
            None,
            0,
            oracle_vec_mul(nan_a, nan_b),
            1e-6,
            1e-6,
            expect_nan=True,
        )

        # 7. Quantized Q8_0, Q4_0, Q4_K (aligned and unaligned +1 byte host offsets!)
        q8_raw = make_q8_0_bytes(rng, 5, 96)
        q8_ref = oracle_dequant_q8_0(q8_raw, 5, 96)
        x_96 = round_f32([rng.uniform(-1.5, 1.5) for _ in range(96)])
        run_one("dequant_q8_0_5x96", OP_DEQUANT_Q8_0, [make_quant_desc(q8_raw, DTYPE_Q8_0, (5, 96))], (5, 96), None, 0, q8_ref, 1e-5, 1e-5, skip_fp64=True, record_replay_vk=True)
        run_one("matvec_q8_0_5x96_unaligned1", OP_MATVEC_Q8_0, [make_quant_desc_offset(q8_raw, DTYPE_Q8_0, (5, 96), 1), make_f32_desc(x_96, (96,), byte_offset=4, align=1)], (5,), None, 4, oracle_matvec_quant(q8_ref, x_96, 5, 96), 5e-4, 5e-4, record_replay_vk=True)

        q40_raw = make_q4_0_bytes(rng, 4, 128)
        q40_ref = oracle_dequant_q4_0(q40_raw, 4, 128)
        x_128 = round_f32([rng.uniform(-1.5, 1.5) for _ in range(128)])
        run_one("dequant_q4_0_4x128", OP_DEQUANT_Q4_0, [make_quant_desc(q40_raw, DTYPE_Q4_0, (4, 128))], (4, 128), None, 0, q40_ref, 1e-5, 1e-5, skip_fp64=True, record_replay_vk=True)
        run_one("matvec_q4_0_4x128_unaligned1", OP_MATVEC_Q4_0, [make_quant_desc_offset(q40_raw, DTYPE_Q4_0, (4, 128), 1), make_f32_desc(x_128, (128,))], (4,), None, 0, oracle_matvec_quant(q40_ref, x_128, 4, 128), 5e-4, 5e-4, record_replay_vk=True)

        q4k_raw = make_q4_k_bytes(rng, 3, 512)
        q4k_ref = oracle_dequant_q4_k(q4k_raw, 3, 512)
        x_512 = round_f32([rng.uniform(-1.0, 1.0) for _ in range(512)])
        run_one("dequant_q4_k_3x512", OP_DEQUANT_Q4_K, [make_quant_desc(q4k_raw, DTYPE_Q4_K, (3, 512))], (3, 512), None, 0, q4k_ref, 1e-5, 1e-5, skip_fp64=True, record_replay_vk=True)
        run_one("matvec_q4_k_3x512_unaligned1", OP_MATVEC_Q4_K, [make_quant_desc_offset(q4k_raw, DTYPE_Q4_K, (3, 512), 1), make_f32_desc(x_512, (512,))], (3,), None, 0, oracle_matvec_quant(q4k_ref, x_512, 3, 512), 5e-4, 5e-4, record_replay_vk=True)

        # 8. Vulkan capability negotiation, honest CPU vs HW GPU identity & negative limits
        d_ok1, _b1 = make_f32_desc([1.0, 2.0], (2,))
        d_ok2, _b2 = make_f32_desc([3.0, 4.0], (2,))
        d_oko, _bo = make_f32_desc([0.0, 0.0], (2,))
        is_hw_gpu = vk_caps.physical_device_type in (1, 2)

        rc_gpu, j_gpu, r_gpu = rt.exec_v2(OP_VEC_ADD_F32, [d_ok1, d_ok2], d_oko, backend=BACKEND_VULKAN_GPU, policy=POLICY_EXACT_REQUIRED)
        rc_fb, j_fb, r_fb = rt.exec_v2(OP_VEC_ADD_F32, [d_ok1, d_ok2], d_oko, backend=BACKEND_VULKAN_GPU, policy=POLICY_AUTO_FALLBACK)
        if is_hw_gpu:
            edge_checks.append(
                {
                    "check": "vulkan_gpu_executes_on_hw_icd_exact",
                    "rc": rc_gpu,
                    "device_type": j_gpu["vulkan_caps"]["device_type"],
                    "executed": j_gpu["backend_executed"],
                    "fallback": r_gpu.base.fallback_occurred,
                    "status": "PASS" if (
                        rc_gpu == 0
                        and r_gpu.base.backend_executed == BACKEND_VULKAN_GPU
                        and r_gpu.base.fallback_occurred == 0
                        and read_f32_desc(d_oko, _bo) == [4.0, 6.0]
                    ) else "FAIL",
                }
            )
            edge_checks.append(
                {
                    "check": "vulkan_gpu_auto_fallback_stays_gpu_on_hw_icd",
                    "rc": rc_fb,
                    "executed": j_fb["backend_executed"],
                    "fallback": r_fb.base.fallback_occurred,
                    "status": "PASS" if (rc_fb == 0 and r_fb.base.backend_executed == BACKEND_VULKAN_GPU and r_fb.base.fallback_occurred == 0) else "FAIL",
                }
            )
            cpu_clamped = ZccVulkanCaps()
            ctypes.memmove(ctypes.byref(cpu_clamped), ctypes.byref(vk_caps), ctypes.sizeof(ZccVulkanCaps))
            cpu_clamped.physical_device_type = VK_DEVTYPE_CPU
            cpu_clamped.device_type_str = b"CPU"
            rc_c_gpu, j_c_gpu, _ = rt.exec_v2(
                OP_VEC_ADD_F32, [d_ok1, d_ok2], d_oko, backend=BACKEND_VULKAN_GPU, policy=POLICY_EXACT_REQUIRED,
                restricted_vk_caps=cpu_clamped,
            )
            edge_checks.append(
                {
                    "check": "vulkan_gpu_rejected_when_clamped_to_cpu_exact",
                    "rc": rc_c_gpu,
                    "device_type": j_c_gpu["vulkan_caps"]["device_type"],
                    "status": "PASS" if (rc_c_gpu == -11 and j_c_gpu["vulkan_caps"]["device_type"] == "CPU") else "FAIL",
                }
            )
        else:
            edge_checks.append(
                {
                    "check": "vulkan_gpu_rejected_on_cpu_icd_exact",
                    "rc": rc_gpu,
                    "device_type": j_gpu["vulkan_caps"]["device_type"],
                    "status": "PASS" if (rc_gpu == -11 and j_gpu["vulkan_caps"]["device_type"] == "CPU") else "FAIL",
                }
            )
            edge_checks.append(
                {
                    "check": "vulkan_gpu_auto_fallback_to_vulkan_spv",
                    "rc": rc_fb,
                    "executed": j_fb["backend_executed"],
                    "fallback": r_fb.base.fallback_occurred,
                    "status": "PASS" if (rc_fb == 0 and r_fb.base.backend_executed == BACKEND_VULKAN_SPV and r_fb.base.fallback_occurred == 1) else "FAIL",
                }
            )

        rc_fp64, j_fp64, _ = rt.exec_v2(OP_DOT_F32, [d_ok1, d_ok2], make_f32_desc([0.0], (1,))[0], backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED, accum=ACCUM_FP64_REF)
        edge_checks.append(
            {
                "check": "vulkan_spv_rejects_fp64_ref_exact",
                "rc": rc_fp64,
                "err": j_fp64["error_message"],
                "status": "PASS" if rc_fp64 == -11 else "FAIL",
            }
        )

        # Restricted vk_caps controls
        no_vk = ZccVulkanCaps()
        ctypes.memmove(ctypes.byref(no_vk), ctypes.byref(vk_caps), ctypes.sizeof(ZccVulkanCaps))
        no_vk.vulkan_available = 0
        rc_novk, _, _ = rt.exec_v2(OP_VEC_ADD_F32, [d_ok1, d_ok2], d_oko, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED, restricted_vk_caps=no_vk)
        edge_checks.append({"check": "restricted_caps_no_vulkan_rejected", "rc": rc_novk, "status": "PASS" if rc_novk == -12 else "FAIL"})

        no_val = ZccVulkanCaps()
        ctypes.memmove(ctypes.byref(no_val), ctypes.byref(vk_caps), ctypes.sizeof(ZccVulkanCaps))
        no_val.validation_layer_available = 0
        no_val.validation_layer_enabled = 0
        rc_noval, _, _ = rt.exec_v2(
            OP_VEC_ADD_F32, [d_ok1, d_ok2], d_oko, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED,
            require_validation_layer=True, restricted_vk_caps=no_val,
        )
        edge_checks.append({"check": "restricted_caps_missing_validation_rejected", "rc": rc_noval, "status": "PASS" if rc_noval == -11 else "FAIL"})

        if is_hw_gpu:
            cpu_clamped2 = ZccVulkanCaps()
            ctypes.memmove(ctypes.byref(cpu_clamped2), ctypes.byref(vk_caps), ctypes.sizeof(ZccVulkanCaps))
            cpu_clamped2.physical_device_type = VK_DEVTYPE_CPU
            cpu_clamped2.device_type_str = b"CPU"
            rc_strict_gpu, _, _ = rt.exec_v2(
                OP_VEC_ADD_F32, [d_ok1, d_ok2], d_oko, backend=BACKEND_VULKAN_GPU, policy=POLICY_AUTO_FALLBACK,
                require_hardware_gpu=True, restricted_vk_caps=cpu_clamped2,
            )
            edge_checks.append({"check": "vulkan_gpu_require_hw_forbids_cpu_fallback", "rc": rc_strict_gpu, "status": "PASS" if rc_strict_gpu == -11 else "FAIL"})
        else:
            rc_strict_gpu, _, _ = rt.exec_v2(
                OP_VEC_ADD_F32, [d_ok1, d_ok2], d_oko, backend=BACKEND_VULKAN_GPU, policy=POLICY_AUTO_FALLBACK,
                require_hardware_gpu=True,
            )
            edge_checks.append({"check": "vulkan_gpu_require_hw_forbids_cpu_fallback", "rc": rc_strict_gpu, "status": "PASS" if rc_strict_gpu == -11 else "FAIL"})

        small_push = ZccVulkanCaps()
        ctypes.memmove(ctypes.byref(small_push), ctypes.byref(vk_caps), ctypes.sizeof(ZccVulkanCaps))
        small_push.max_push_constants_size = 32
        rc_sp, _, _ = rt.exec_v2(OP_VEC_ADD_F32, [d_ok1, d_ok2], d_oko, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED, restricted_vk_caps=small_push)
        edge_checks.append({"check": "restricted_caps_small_push_rejected", "rc": rc_sp, "status": "PASS" if rc_sp == -11 else "FAIL"})

        small_ssbo = ZccVulkanCaps()
        ctypes.memmove(ctypes.byref(small_ssbo), ctypes.byref(vk_caps), ctypes.sizeof(ZccVulkanCaps))
        small_ssbo.max_storage_buffer_range = 4
        rc_sb, _, _ = rt.exec_v2(OP_VEC_ADD_F32, [d_ok1, d_ok2], d_oko, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED, restricted_vk_caps=small_ssbo)
        edge_checks.append({"check": "restricted_caps_small_ssbo_rejected", "rc": rc_sb, "status": "PASS" if rc_sb == -11 else "FAIL"})

        # HMAC-attested Track 21B receipt verification check
        hmac_key = b"zcc-track21b-diff-hmac-key"
        rc_hm, _, rec_hm = rt.exec_v2(
            OP_VEC_ADD_F32, [d_ok1, d_ok2], d_oko, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED,
            require_validation_layer=True, hmac_key=hmac_key,
        )
        key_arr = (ctypes.c_uint8 * len(hmac_key)).from_buffer_copy(hmac_key)
        rc_hm_ok = rt.lib.zcc_receipt_v2_verify_canonical(ctypes.byref(rec_hm), key_arr, len(hmac_key))
        bad_key = (ctypes.c_uint8 * 3)(1, 2, 3)
        rc_hm_bad = rt.lib.zcc_receipt_v2_verify_canonical(ctypes.byref(rec_hm), bad_key, 3)
        edge_checks.append({
            "check": "vulkan_receipt_hmac_attested_and_wrong_key_rejected",
            "rc_exec": rc_hm,
            "rc_verify_ok": rc_hm_ok,
            "rc_verify_bad": rc_hm_bad,
            "status": "PASS" if (rc_hm == 0 and rc_hm_ok == 0 and rc_hm_bad == -16) else "FAIL",
        })

        d_bad1, _bb1 = make_f32_desc([1.0, 2.0], (2,))
        d_bad2, _bb2 = make_f32_desc([1.0, 2.0, 3.0], (3,))
        d_ro, _bro = make_f32_desc([0.0, 0.0], (2,), owner=OWNER_READONLY | OWNER_BORROWED_HOST)
        rc_bad, _, _ = rt.exec_v2(OP_VEC_ADD_F32, [d_bad1, d_bad2], d_ro, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED)
        edge_checks.append({"check": "vulkan_invalid_shape_readonly_rejected", "rc": rc_bad, "status": "PASS" if rc_bad != 0 else "FAIL"})

        # Non-Vulkan SCALAR_C99 omits redundant Vulkan ICD/validation probe cleanly
        rc_sc0, j_sc0, rec_sc0 = rt.exec_v2(OP_VEC_ADD_F32, [d_ok1, d_ok2], d_oko, backend=BACKEND_SCALAR_C99, policy=POLICY_EXACT_REQUIRED)
        sc_canon_ok = (rt.lib.zcc_receipt_v2_verify_canonical(ctypes.byref(rec_sc0), None, 0) == 0)
        edge_checks.append({
            "check": "scalar_skips_redundant_vulkan_probe",
            "rc": rc_sc0,
            "vk_type": j_sc0["vulkan_caps"]["device_type"],
            "vk_avail": j_sc0["vulkan_caps"]["vulkan_available"],
            "status": "PASS" if (rc_sc0 == 0 and sc_canon_ok and j_sc0["vulkan_caps"]["device_type"] == "NONE" and j_sc0["vulkan_caps"]["vulkan_available"] == 0) else "FAIL",
        })

        # Persistent zcc_vulkan_ctx_t parity + shrink/grow + replay + fast ctx validation gate
        with rt.create_context(enable_validation_layer=True) as pctx:
            d_ctx1, b_ctx1 = make_f32_desc([0.0, 0.0], (2,))
            d_ctx2, b_ctx2 = make_f32_desc([0.0, 0.0], (2,))
            d_stat, b_stat = make_f32_desc([0.0, 0.0], (2,))
            rc_p0, _, _ = rt.exec_v2(OP_VEC_ADD_F32, [d_ok1, d_ok2], d_stat, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED)
            rc_p1, j_p1, rec_p1 = pctx.exec_v2(OP_VEC_ADD_F32, [d_ok1, d_ok2], d_ctx1, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED, hmac_key=hmac_key)
            rc_p2, j_p2, _ = pctx.exec_v2(OP_VEC_ADD_F32, [d_ok1, d_ok2], d_ctx2, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED, hmac_key=hmac_key)
            p_hmac_ok = (rt.lib.zcc_receipt_v2_verify_canonical(ctypes.byref(rec_p1), key_arr, len(hmac_key)) == 0)
            # Grow buffer on same ctx via 256-elem VEC_MUL_F32, then verify tiny 1-elem DOT_F32 shrink
            ga, _bga = make_f32_desc([float(i % 7) for i in range(256)], (256,))
            gb, _bgb = make_f32_desc([0.5 for _ in range(256)], (256,))
            go_v, _bgov = make_f32_desc([0.0] * 256, (256,))
            go_s, _bgos = make_f32_desc([0.0] * 256, (256,))
            rc_g0, _, _ = rt.exec_v2(OP_VEC_MUL_F32, [ga, gb], go_s, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED)
            rc_g1, j_g1, _ = pctx.exec_v2(OP_VEC_MUL_F32, [ga, gb], go_v, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED)
            sd_o, _bsdo = make_f32_desc([0.0], (1,))
            rc_s1, _, _ = pctx.exec_v2(OP_DOT_F32, [d_ok1, d_ok2], sd_o, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED)
            dot_got = struct.unpack("<f", bytes(_bsdo)[:4])[0]
            with rt.create_context(enable_validation_layer=False) as fctx:
                rc_f_rej, _, _ = fctx.exec_v2(OP_VEC_ADD_F32, [d_ok1, d_ok2], d_ctx2, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED, require_validation_layer=True)
                rc_f_ok, j_f_ok, _ = fctx.exec_v2(OP_VEC_ADD_F32, [d_ok1, d_ok2], d_ctx2, backend=BACKEND_VULKAN_SPV, policy=POLICY_EXACT_REQUIRED, require_validation_layer=False)
            p_ok = (
                rc_p0 == 0 and rc_p1 == 0 and rc_p2 == 0 and p_hmac_ok
                and j_p1["output_buffer_sha256"] == j_p2["output_buffer_sha256"]
                and bytes(b_ctx1) == bytes(b_stat)
                and j_p1["sync_barrier_count"] == 2 and j_p1["validation_error_count"] == 0
                and rc_g0 == 0 and rc_g1 == 0 and j_g1["output_buffer_sha256"] != ""
                and bytes(_bgov) == bytes(_bgos)
                and rc_s1 == 0 and abs(dot_got - 11.0) < 1e-5
                and rc_f_rej != 0 and rc_f_ok == 0 and j_f_ok["output_buffer_sha256"] == j_p1["output_buffer_sha256"]
            )
            edge_checks.append({
                "check": "persistent_ctx_parity_grow_shrink_and_val_gate",
                "rc_p": [rc_p0, rc_p1, rc_p2, rc_g0, rc_g1, rc_s1, rc_f_rej, rc_f_ok],
                "dot_got": dot_got,
                "status": "PASS" if p_ok else "FAIL",
            })

        all_checks = cases + replay_checks + edge_checks
        failed = [c for c in all_checks if c["status"] != "PASS"]
        verdict = "PASS" if (not failed and len(replay_checks) == 13) else "FAIL"

        report = {
            "schema_version": "track21b.differential_report.v1",
            "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "external_oracles": {
                "spirv_val": spirv_val_bin,
                "glslangValidator": glslang_bin,
                "vulkan_validation_layer": bool(vk_caps.validation_layer_enabled),
            },
            "hardware": {
                "cpu_avx2": cpu_caps.has_avx2,
                "cpu_fma": cpu_caps.has_fma,
                "ptx_available": ptx_caps.cuda_available,
                "ptx_device": ptx_caps.device_name.decode(),
                "vulkan_available": vk_caps.vulkan_available,
                "vulkan_device_name": vk_caps.device_name.decode(),
                "vulkan_device_type": vk_caps.device_type_str.decode(),
                "vulkan_physical_device_type": vk_caps.physical_device_type,
                "vulkan_vendor_id": f"0x{vk_caps.vendor_id:04x}",
                "vulkan_api_version": vk_caps.api_version,
                "vulkan_validation_enabled": vk_caps.validation_layer_enabled,
            },
            "spirv_modules": spv_records,
            "total_checks": len(all_checks),
            "passed": len(all_checks) - len(failed),
            "failed": len(failed),
            "replay_13_ops_count": len(replay_checks),
            "verdict": verdict,
            "cases": cases,
            "replay_checks": replay_checks,
            "edge_checks": edge_checks,
        }
        out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        perf_rep = collect_performance_report(
            rt,
            rng,
            bool(cpu_caps.has_avx2 and cpu_caps.has_fma),
            bool(ptx_caps.cuda_available),
            vk_caps,
        )
        Path(args.perf_out).write_text(
            json.dumps(perf_rep, indent=2) + "\n", encoding="utf-8"
        )
        print(
            f"TRACK21B_DIFF_ORACLE total={len(all_checks)} passed={report['passed']} "
            f"failed={len(failed)} replays={len(replay_checks)}/13 perf={len(perf_rep['results'])} "
            f"vk_dev={vk_caps.device_type_str.decode()}({vk_caps.device_name.decode()}) verdict={verdict}"
        )
        if failed:
            for item in failed[:15]:
                print("  DIFF21B_FAIL:", item, file=sys.stderr)
            return 1
        return 0
    finally:
        rt.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
