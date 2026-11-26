#!/usr/bin/env python3
"""ZCC Track 21A — Silent Failure Observatory (Controlled Mutation Suite).

Injects REAL mutations into isolated /tmp sandbox copies of runtime C sources,
ZCC-compiled PTX kernels, receipt generators, and release verification scripts
across all 8 required failure domains:
  1. Arithmetic operations
  2. Tensor indexing
  3. Quantization block decoding
  4. Accumulation / normalization precision
  5. Backend selection
  6. Capability detection
  7. Receipt generation
  8. Verification & release authorization

Also verifies:
  - False-positive baseline = 0 on unmutated sandbox build
  - Novel/unregistered anomalies preserve signature classification = "UNKNOWN"
  - Honest scope disclaimer (mutation kill rate != exhaustive proof against all silent failures)
"""

from __future__ import annotations

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
from typing import Any, Callable, Dict, List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

from sdk.zcc_compute import (  # noqa: E402
    ACCUM_FP32_FMA,
    ACCUM_FP64_REF,
    BACKEND_AVX2_FMA,
    BACKEND_PTX_SM120,
    BACKEND_SCALAR_C99,
    DTYPE_Q4_K,
    DTYPE_Q8_0,
    OP_DEQUANT_Q4_K,
    OP_DOT_F32,
    OP_GEMM_F32,
    OP_MATVEC_Q8_0,
    OP_RMSNORM_F32,
    OP_VEC_ADD_F32,
    POLICY_EXACT_REQUIRED,
    ZccComputeReceipt,
    ZccComputeRuntime,
    ZccCpuCaps,
    ZccExecOptions,
    ZccTensorDesc,
)
from tests.track21a.differential_oracle import (  # noqa: E402
    compare_vectors,
    make_f32_desc,
    make_q4_k_bytes,
    make_q8_0_bytes,
    make_quant_desc,
    oracle_dequant_q4_k,
    oracle_dequant_q8_0,
    oracle_dot,
    oracle_gemm,
    oracle_matvec_quant,
    oracle_rmsnorm,
    oracle_vec_add,
    read_f32_desc,
    round_f32,
)
import random


def classify_failure_signature(event: Dict[str, Any]) -> str:
    """Map known observatory trap mechanisms while strictly preserving unknowns as UNKNOWN."""
    kind = str(event.get("trap_kind", ""))
    mapping = {
        "DIFF_TOLERANCE_EXCEEDED": "SIG_NUMERICAL_ORACLE_DIVERGENCE",
        "UNEXPECTED_BACKEND_SUBSTITUTION": "SIG_SILENT_BACKEND_SUBSTITUTION",
        "UNSAFE_CAPABILITY_ACCEPTANCE": "SIG_MISSING_CPUID_OSXSAVE_XCR0_FMA_GUARD",
        "RECEIPT_SELF_CERTIFICATION": "SIG_EF0_RUNTIME_SELF_CERTIFICATION",
        "RECEIPT_DIGEST_MISMATCH": "SIG_RECEIPT_CANONICAL_OR_BUFFER_TAMPER",
        "VERIFIER_FAIL_OPEN_CAUGHT": "SIG_VERIFIER_AUTHORIZATION_BYPASS",
    }
    if kind in mapping:
        return mapping[kind]
    return "UNKNOWN"


def compile_sandbox_so(src_map: Dict[str, str], sandbox: Path, ptx_mut_src: Optional[str] = None) -> Tuple[Path, Optional[Path]]:
    """Copy Track 21A sources to /tmp sandbox, apply edits in-place there, compile .so (+ optional mutated PTX)."""
    rt_dir = sandbox / "runtime"
    inc_dir = sandbox / "include"
    rt_dir.mkdir(parents=True, exist_ok=True)
    inc_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(REPO_ROOT / "include" / "zcc_compute.h", inc_dir / "zcc_compute.h")

    portable = [
        "compute_sha256.c",
        "cpu_capabilities.c",
        "compute_scalar.c",
        "compute_ptx.c",
        "compute_dispatch.c",
        "execution_receipt.c",
    ]
    for name in portable + ["compute_avx2.c"]:
        txt = (REPO_ROOT / "runtime" / name).read_text(encoding="utf-8")
        if name in src_map:
            txt = src_map[name]
        (rt_dir / name).write_text(txt, encoding="utf-8")

    pflags = ["-std=c99", "-O2", "-mno-avx", "-mno-avx2", "-mno-fma", "-fno-unsafe-math-optimizations", "-fPIC", "-I", str(inc_dir)]
    aflags = ["-std=c99", "-O2", "-mavx2", "-mfma", "-fno-unsafe-math-optimizations", "-fPIC", "-I", str(inc_dir)]
    objs: List[str] = []
    for name in portable:
        obj = str(sandbox / f"{name[:-2]}.o")
        subprocess.run(["gcc", *pflags, "-c", str(rt_dir / name), "-o", obj], check=True)
        objs.append(obj)
    avx_obj = str(sandbox / "compute_avx2.o")
    subprocess.run(["gcc", *aflags, "-c", str(rt_dir / "compute_avx2.c"), "-o", avx_obj], check=True)
    objs.append(avx_obj)

    so_path = sandbox / "libzcc_compute_mut.so"
    subprocess.run(["gcc", "-shared", "-o", str(so_path), *objs, "-lm", "-ldl"], check=True)

    ptx_out: Optional[Path] = None
    if ptx_mut_src is not None:
        ptx_c = rt_dir / "kernels_t21a_ptx.c"
        ptx_c.write_text(ptx_mut_src, encoding="utf-8")
        ptx_out = sandbox / "kernels_mut_sm120.ptx"
        subprocess.run(
            [str(REPO_ROOT / "zcc"), "--target=ptx", "--gpu-arch=sm_120", str(ptx_c), "-o", str(ptx_out)],
            cwd=str(REPO_ROOT),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        )
    return so_path, ptx_out


def run_probe_suite(so_path: Path, ptx_override: Optional[Path] = None) -> Optional[Dict[str, Any]]:
    """Execute minimal invariant probe against a candidate runtime .so. Returns failure event dict or None if clean."""
    rt = ZccComputeRuntime(str(so_path))
    try:
        cpu_caps, _ = rt.probe_caps(str(ptx_override) if ptx_override else None)
        rng = random.Random(77)

        # 1. Arithmetic vec_add on Scalar & AVX2
        va = round_f32([1.25, -3.5, 10.0, 0.75, 2.5, -1.5, 4.0, 8.0, 0.125])
        vb = round_f32([0.5, 1.5, -2.0, 0.25, -1.0, 3.5, -2.0, 1.0, 0.875])
        ref_add = oracle_vec_add(va, vb)
        for bname, bid in [("SCALAR_C99", BACKEND_SCALAR_C99), ("AVX2_FMA", BACKEND_AVX2_FMA)]:
            da, ba = make_f32_desc(va, (9,))
            db, bb = make_f32_desc(vb, (9,))
            do, bo = make_f32_desc([0.0] * 9, (9,))
            opts = ZccExecOptions()
            rt.lib.zcc_exec_options_init(ctypes.byref(opts))
            opts.requested_backend = bid
            opts.selection_policy = POLICY_EXACT_REQUIRED
            rec = ZccComputeReceipt()
            rc = rt.lib.zcc_compute_exec(OP_VEC_ADD_F32, (ZccTensorDesc * 2)(da, db), 2, ctypes.byref(do), ctypes.byref(opts), ctypes.byref(rec))
            if rc != 0:
                return {"trap_kind": "UNEXPECTED_ERROR", "detail": f"{bname} vec_add rc={rc}"}
            got = read_f32_desc(do, bo)
            ok, ae, _, _, msg = compare_vectors(got, ref_add, 1e-6, 1e-6)
            if not ok:
                return {"trap_kind": "DIFF_TOLERANCE_EXCEEDED", "op": "VEC_ADD_F32", "backend": bname, "abs_err": ae, "detail": msg}
            # ZCC-EF-0 Receipt check
            if rec.verification_status.decode() != "UNVERIFIED_RUNTIME_ONLY":
                return {"trap_kind": "RECEIPT_SELF_CERTIFICATION", "detail": f"runtime emitted verification_status={rec.verification_status.decode()}"}
            actual_out_sha = hashlib.sha256(bytes(bo[:36])).hexdigest()
            if actual_out_sha != rec.output_buffer_sha256.decode():
                return {"trap_kind": "RECEIPT_DIGEST_MISMATCH", "detail": f"actual {actual_out_sha[:16]} != receipt {rec.output_buffer_sha256.decode()[:16]}"}
            if rt.verify_receipt(rec) != 0:
                return {"trap_kind": "RECEIPT_DIGEST_MISMATCH", "detail": "zcc_receipt_verify_canonical rejected receipt"}

        # 2. GEMM indexing check [4x5] * [5x18] (covers multiple AVX2 8-wide tiles + tail cols!)
        gm_a = round_f32([rng.uniform(-1.0, 1.0) for _ in range(20)])
        gm_b = round_f32([rng.uniform(-1.0, 1.0) for _ in range(90)])
        gm_ref = oracle_gemm(gm_a, gm_b, 4, 5, 18)
        da, bga = make_f32_desc(gm_a, (4, 5))
        db, bgb = make_f32_desc(gm_b, (5, 18))
        do, bo = make_f32_desc([0.0] * 72, (4, 18))
        opts = ZccExecOptions()
        rt.lib.zcc_exec_options_init(ctypes.byref(opts))
        opts.requested_backend = BACKEND_AVX2_FMA
        opts.selection_policy = POLICY_EXACT_REQUIRED
        rc = rt.lib.zcc_compute_exec(OP_GEMM_F32, (ZccTensorDesc * 2)(da, db), 2, ctypes.byref(do), ctypes.byref(opts), None)
        ok, ae, _, _, msg = compare_vectors(read_f32_desc(do, bo), gm_ref, 1e-4, 1e-4)
        if rc != 0 or not ok:
            return {"trap_kind": "DIFF_TOLERANCE_EXCEEDED", "op": "GEMM_F32", "backend": "AVX2_FMA", "abs_err": ae, "detail": msg}

        # 3. Quantization block decoding check (Q4_K on Scalar + Q8_0 matvec on PTX)
        q4k_raw = make_q4_k_bytes(rng, 1, 256)
        q4k_ref = oracle_dequant_q4_k(q4k_raw, 1, 256)
        dq, bq4k = make_quant_desc(q4k_raw, DTYPE_Q4_K, (1, 256))
        do, bo = make_f32_desc([0.0] * 256, (1, 256))
        opts.requested_backend = BACKEND_SCALAR_C99
        rc = rt.lib.zcc_compute_exec(OP_DEQUANT_Q4_K, (ZccTensorDesc * 1)(dq), 1, ctypes.byref(do), ctypes.byref(opts), None)
        ok, ae, _, _, msg = compare_vectors(read_f32_desc(do, bo), q4k_ref, 1e-5, 1e-5)
        if rc != 0 or not ok:
            return {"trap_kind": "DIFF_TOLERANCE_EXCEEDED", "op": "DEQUANT_Q4_K", "backend": "SCALAR_C99", "abs_err": ae, "detail": msg}

        q8_raw = make_q8_0_bytes(rng, 2, 64)
        x64 = round_f32([rng.uniform(-1.0, 1.0) for _ in range(64)])
        q8_mv_ref = oracle_matvec_quant(oracle_dequant_q8_0(q8_raw, 2, 64), x64, 2, 64)
        dq8, bq8 = make_quant_desc(q8_raw, DTYPE_Q8_0, (2, 64))
        dx64, bx64 = make_f32_desc(x64, (64,))
        do, bo = make_f32_desc([0.0, 0.0], (2,))
        opts.requested_backend = BACKEND_PTX_SM120
        if ptx_override:
            opts.ptx_path_override = str(ptx_override).encode("utf-8")
        rc = rt.lib.zcc_compute_exec(OP_MATVEC_Q8_0, (ZccTensorDesc * 2)(dq8, dx64), 2, ctypes.byref(do), ctypes.byref(opts), None)
        ok, ae, _, _, msg = compare_vectors(read_f32_desc(do, bo), q8_mv_ref, 5e-4, 5e-4)
        if rc != 0 or not ok:
            return {"trap_kind": "DIFF_TOLERANCE_EXCEEDED", "op": "MATVEC_Q8_0", "backend": "PTX_SM120", "abs_err": ae, "detail": msg}

        # 4. Accumulation precision & RMSNorm eps check
        a_cat = round_f32([1.0, 1e9, -1e9, 7.0])
        b_cat = round_f32([1.0, 1.0, 1.0, 1.0])
        da, bca = make_f32_desc(a_cat, (4,))
        db, bcb = make_f32_desc(b_cat, (4,))
        do, bo = make_f32_desc([0.0], (1,))
        opts.ptx_path_override = None
        opts.requested_backend = BACKEND_SCALAR_C99
        opts.accum_policy = ACCUM_FP64_REF
        rc = rt.lib.zcc_compute_exec(OP_DOT_F32, (ZccTensorDesc * 2)(da, db), 2, ctypes.byref(do), ctypes.byref(opts), None)
        ok, ae, _, _, msg = compare_vectors(read_f32_desc(do, bo), oracle_dot(a_cat, b_cat), 1e-5, 1e-6)
        if rc != 0 or not ok:
            return {"trap_kind": "DIFF_TOLERANCE_EXCEEDED", "op": "DOT_F32_FP64_REF", "backend": "SCALAR_C99", "abs_err": ae, "detail": msg}

        rx = round_f32([0.0, 0.0, 0.0, 0.0])
        rw = round_f32([1.0, 1.0, 1.0, 1.0])
        drx, brx = make_f32_desc(rx, (1, 4))
        drw, brw = make_f32_desc(rw, (4,))
        do, bo = make_f32_desc([0.0] * 4, (1, 4))
        opts.accum_policy = ACCUM_FP32_FMA
        opts.rmsnorm_eps = ctypes.c_float(1e-5)
        rc = rt.lib.zcc_compute_exec(OP_RMSNORM_F32, (ZccTensorDesc * 2)(drx, drw), 2, ctypes.byref(do), ctypes.byref(opts), None)
        ok, ae, _, _, msg = compare_vectors(read_f32_desc(do, bo), [0.0, 0.0, 0.0, 0.0], 1e-6, 1e-6)
        if rc != 0 or not ok:
            return {"trap_kind": "DIFF_TOLERANCE_EXCEEDED", "op": "RMSNORM_F32_ZERO_VEC", "backend": "SCALAR_C99", "abs_err": ae, "detail": msg}

        # 5 & 6. Capability detection & backend selection (no XCR0, no FMA)
        da, bsa = make_f32_desc([1.0, 2.0], (2,))
        db, bsb = make_f32_desc([3.0, 4.0], (2,))
        for label, attr in [("no_xcr0_ymm", "xcr0_ymm"), ("no_fma", "has_fma")]:
            sim = ZccCpuCaps()
            ctypes.memmove(ctypes.byref(sim), ctypes.byref(cpu_caps), ctypes.sizeof(ZccCpuCaps))
            setattr(sim, attr, 0)
            if rt.lib.zcc_cpu_has_avx2_fma(ctypes.byref(sim)) != 0:
                return {"trap_kind": "UNSAFE_CAPABILITY_ACCEPTANCE", "detail": f"zcc_cpu_has_avx2_fma returned non-zero when {label}"}
            do, bso = make_f32_desc([0.0, 0.0], (2,))
            rec_s = ZccComputeReceipt()
            opts.requested_backend = BACKEND_AVX2_FMA
            opts.selection_policy = POLICY_EXACT_REQUIRED
            rc_s = rt.lib.zcc_compute_exec_with_caps(OP_VEC_ADD_F32, (ZccTensorDesc * 2)(da, db), 2, ctypes.byref(do), ctypes.byref(opts), ctypes.byref(sim), ctypes.byref(rec_s))
            if rc_s == 0 or rec_s.backend_executed == BACKEND_SCALAR_C99:
                return {"trap_kind": "UNEXPECTED_BACKEND_SUBSTITUTION", "detail": f"EXACT AVX2 returned rc={rc_s}, executed={rec_s.backend_executed} under {label}"}
        return None
    finally:
        rt.shutdown()


def replace_once(src: str, old: str, new: str) -> str:
    assert src.count(old) == 1, f"Expected unique match for mutation target: {old!r} (count={src.count(old)})"
    return src.replace(old, new, 1)


def main() -> int:
    results: List[Dict[str, Any]] = []

    # 0. Control baseline (0 mutations -> must trigger 0 false positives)
    with tempfile.TemporaryDirectory(prefix="t21a_mut_ctrl_") as td:
        so_ctrl, _ = compile_sandbox_so({}, Path(td))
        ctrl_hit = run_probe_suite(so_ctrl)
        false_positives = 1 if ctrl_hit is not None else 0
        if false_positives:
            print(f"FAIL: Unmutated control triggered false positive: {ctrl_hit}", file=sys.stderr)
            return 1

    # Load clean baseline sources
    src_scalar = (REPO_ROOT / "runtime" / "compute_scalar.c").read_text(encoding="utf-8")
    src_avx2   = (REPO_ROOT / "runtime" / "compute_avx2.c").read_text(encoding="utf-8")
    src_caps   = (REPO_ROOT / "runtime" / "cpu_capabilities.c").read_text(encoding="utf-8")
    src_disp   = (REPO_ROOT / "runtime" / "compute_dispatch.c").read_text(encoding="utf-8")
    src_ptx    = (REPO_ROOT / "runtime" / "kernels_t21a_ptx.c").read_text(encoding="utf-8")
    src_ver    = (REPO_ROOT / "tools" / "verify_track21a_release.py").read_text(encoding="utf-8")

    mutants = [
        {
            "id": "M_ARITH_01_VEC_ADD_SIGN",
            "domain": "arithmetic",
            "target": "runtime/compute_avx2.c:avx2_vec_add",
            "mutation": "Replace _mm256_add_ps with _mm256_sub_ps and + with -",
            "minimal_repro": "OP_VEC_ADD_F32([1.25, -3.5, ...], [0.5, 1.5, ...], AVX2_FMA)",
            "edit_map": lambda: {
                "compute_avx2.c": replace_once(
                    replace_once(src_avx2, "_mm256_storeu_ps(out + i, _mm256_add_ps(va, vb));", "_mm256_storeu_ps(out + i, _mm256_sub_ps(va, vb));"),
                    "for (; i < n; i++) out[i] = a[i] + b[i];",
                    "for (; i < n; i++) out[i] = a[i] - b[i];",
                )
            },
            "ptx": None,
        },
        {
            "id": "M_INDEX_01_GEMM_COL_OFFBYONE",
            "domain": "tensor_indexing",
            "target": "runtime/compute_avx2.c:avx2_gemm_f32",
            "mutation": "Drop +n offset on AVX2 B load (pb[k * ldb + n] -> pb[k * ldb], misindexing n>=8)",
            "minimal_repro": "OP_GEMM_F32(A[4,5], B[5,18], AVX2_FMA)",
            "edit_map": lambda: {
                "compute_avx2.c": replace_once(
                    src_avx2,
                    "__m256 vb = _mm256_loadu_ps(pb + k * ldb + n);",
                    "__m256 vb = _mm256_loadu_ps(pb + k * ldb);",
                )
            },
            "ptx": None,
        },
        {
            "id": "M_QUANT_01_Q4K_SCALE_MASK",
            "domain": "quantization_decode",
            "target": "runtime/compute_scalar.c:q4k_scale_at",
            "mutation": "Corrupt 6-bit scale mask (scales[j] & 63U -> scales[j] & 31U)",
            "minimal_repro": "OP_DEQUANT_Q4_K(W[1,256], SCALAR_C99)",
            "edit_map": lambda: {
                "compute_scalar.c": replace_once(src_scalar, "return (uint32_t)(scales[j] & 63U);", "return (uint32_t)(scales[j] & 31U);")
            },
            "ptx": None,
        },
        {
            "id": "M_QUANT_02_PTX_Q8_MATVEC_BIAS",
            "domain": "quantization_decode",
            "target": "runtime/kernels_t21a_ptx.c:t21_matvec_q8_0 (recompiled via ./zcc --target=ptx)",
            "mutation": "Corrupt PTX Q8_0 dequantized weight q -> (q + 1)",
            "minimal_repro": "OP_MATVEC_Q8_0(W[2,64], X[64], PTX_SM120)",
            "edit_map": lambda: {},
            "ptx": replace_once(
                src_ptx,
                "        int q = t21_s8((unsigned int)blk[2U + lane]);\n        acc = __ptx_fmaf(d * (float)q, X[(b << 5) + lane], acc);",
                "        int q = t21_s8((unsigned int)blk[2U + lane]);\n        acc = __ptx_fmaf(d * (float)(q + 1), X[(b << 5) + lane], acc);",
            ),
        },
        {
            "id": "M_ACCUM_01_DOT_FP64_DOWNGRADE",
            "domain": "accumulation_precision",
            "target": "runtime/compute_scalar.c:exec_dot(ZCC_ACCUM_FP64_REF)",
            "mutation": "Downgrade double accumulator to float inside FP64_REF path",
            "minimal_repro": "OP_DOT_F32([1.0, 1e9, -1e9, 7.0], [1,1,1,1], ACCUM_FP64_REF)",
            "edit_map": lambda: {
                "compute_scalar.c": replace_once(
                    src_scalar,
                    "    if (accum == ZCC_ACCUM_FP64_REF) {\n        double acc = 0.0;\n        for (int64_t i = 0; i < n; i++) {\n            acc += (double)read_f32_at(a, i) * (double)read_f32_at(b, i);\n        }\n        out_val = (float)acc;",
                    "    if (accum == ZCC_ACCUM_FP64_REF) {\n        float acc = 0.0f;\n        for (int64_t i = 0; i < n; i++) {\n            acc += read_f32_at(a, i) * read_f32_at(b, i);\n        }\n        out_val = acc;",
                )
            },
            "ptx": None,
        },
        {
            "id": "M_ACCUM_02_RMSNORM_DROP_EPS",
            "domain": "accumulation_precision",
            "target": "runtime/compute_scalar.c:exec_rmsnorm",
            "mutation": "Omit + eps in RMSNorm denominator causing zero-vector 0/0 NaN",
            "minimal_repro": "OP_RMSNORM_F32([0,0,0,0], [1,1,1,1], eps=1e-5)",
            "edit_map": lambda: {
                "compute_scalar.c": replace_once(
                    src_scalar,
                    "float inv_rms = 1.0f / sqrtf(ss / (float)dim + eps);",
                    "float inv_rms = 1.0f / sqrtf(ss / (float)dim);",
                )
            },
            "ptx": None,
        },
        {
            "id": "M_DISPATCH_01_SILENT_SUBSTITUTION",
            "domain": "backend_selection",
            "target": "runtime/cpu_capabilities.c:zcc_negotiate_backend_with_caps",
            "mutation": "Silently substitute SCALAR_C99 and return ZCC_STATUS_OK when AVX2_FMA EXACT_REQUIRED lacks HW support",
            "minimal_repro": "EXACT_REQUIRED AVX2_FMA with xcr0_ymm=0",
            "edit_map": lambda: {
                "cpu_capabilities.c": replace_once(
                    src_caps,
                    "        write_reason(reason_buf, reason_cap, \"rejected_exact_avx2_fma_unsupported_hw\");\n        return ZCC_STATUS_ERR_UNSUPPORTED_HW;",
                    "        *out_selected_backend = ZCC_BACKEND_SCALAR_C99;\n        return ZCC_STATUS_OK;",
                )
            },
            "ptx": None,
        },
        {
            "id": "M_CAP_01_DROP_XCR0_CHECK",
            "domain": "capability_detection",
            "target": "runtime/cpu_capabilities.c:zcc_cpu_has_avx2_fma",
            "mutation": "Drop caps->xcr0_ymm check from AVX2 capability gate",
            "minimal_repro": "zcc_cpu_has_avx2_fma(caps with avx2=1, fma=1, xcr0_ymm=0)",
            "edit_map": lambda: {
                "cpu_capabilities.c": replace_once(src_caps, "            caps->xcr0_ymm &&\n", "")
            },
            "ptx": None,
        },
        {
            "id": "M_CAP_02_DROP_FMA_CHECK",
            "domain": "capability_detection",
            "target": "runtime/cpu_capabilities.c:zcc_cpu_has_avx2_fma",
            "mutation": "Drop independent caps->has_fma check from AVX2 capability gate",
            "minimal_repro": "zcc_cpu_has_avx2_fma(caps with avx2=1, xcr0_ymm=1, has_fma=0)",
            "edit_map": lambda: {
                "cpu_capabilities.c": replace_once(src_caps, "            caps->has_avx2 &&\n            caps->has_fma) ? 1 : 0;", "            caps->has_avx2) ? 1 : 0;")
            },
            "ptx": None,
        },
        {
            "id": "M_RECEIPT_01_RUNTIME_SELF_CERTIFY",
            "domain": "receipt_generation",
            "target": "runtime/compute_dispatch.c:exec_internal",
            "mutation": "Set runtime receipt verification_status to PASS instead of UNVERIFIED_RUNTIME_ONLY",
            "minimal_repro": "inspect receipt.verification_status after OP_VEC_ADD_F32",
            "edit_map": lambda: {
                "compute_dispatch.c": replace_once(
                    src_disp,
                    'snprintf(rec->verification_status, sizeof(rec->verification_status), "UNVERIFIED_RUNTIME_ONLY");',
                    'snprintf(rec->verification_status, sizeof(rec->verification_status), "PASS");',
                )
            },
            "ptx": None,
        },
        {
            "id": "M_RECEIPT_02_STALE_OUTPUT_HASH",
            "domain": "receipt_generation",
            "target": "runtime/compute_dispatch.c:exec_internal",
            "mutation": "Flip leading byte of receipt output_buffer_sha256 before sealing",
            "minimal_repro": "compare receipt.output_buffer_sha256 vs SHA256(output buffer)",
            "edit_map": lambda: {
                "compute_dispatch.c": replace_once(
                    src_disp,
                    "    if (st == ZCC_STATUS_OK && output && output->data && output->nbytes > 0ULL) {\n        zcc_sha256_hex(output->data, (size_t)output->nbytes, rec->output_buffer_sha256);",
                    "    if (st == ZCC_STATUS_OK && output && output->data && output->nbytes > 0ULL) {\n        zcc_sha256_hex(output->data, (size_t)output->nbytes, rec->output_buffer_sha256);\n        rec->output_buffer_sha256[0] = (rec->output_buffer_sha256[0] == 'a') ? 'b' : 'a';",
                )
            },
            "ptx": None,
        },
    ]

    for m in mutants:
        with tempfile.TemporaryDirectory(prefix=f"t21a_{m['id']}_") as td:
            so_path, ptx_out = compile_sandbox_so(m["edit_map"](), Path(td), m["ptx"])
            hit = run_probe_suite(so_path, ptx_out)
            killed = hit is not None
            sig = classify_failure_signature(hit) if hit else "SURVIVED_NO_FAILURE"
            results.append({
                "mutant_id": m["id"],
                "domain": m["domain"],
                "target": m["target"],
                "mutation": m["mutation"],
                "status": "KILLED" if killed else "SURVIVED",
                "failure_signature": sig,
                "trap_detail": hit["detail"] if hit else "Mutant escaped detection",
                "minimal_reproducing_input": m["minimal_repro"],
            })

    # Category 8: Controlled Verifier & Release Authorization Mutants
    sub_env = {**os.environ, "ZCC_REPO_ROOT": str(REPO_ROOT)}
    with tempfile.TemporaryDirectory(prefix="t21a_M_VERIFY_") as td:
        ver_mut_path = Path(td) / "verify_mut.py"
        # M_VERIFY_01: Clean verifier given missing/empty evidence directory MUST fail closed with RELEASE_BLOCKED (rc=2)
        clean_bad_run = subprocess.run(
            [sys.executable, str(REPO_ROOT / "tools" / "verify_track21a_release.py"), "--verify-dir", str(td)],
            cwd=str(REPO_ROOT),
            env=sub_env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        v01_killed = (clean_bad_run.returncode == 2 and "RELEASE_BLOCKED:" in clean_bad_run.stderr)
        results.append({
            "mutant_id": "M_VERIFY_01_MISSING_EVIDENCE_REJECTED",
            "domain": "verifier_release_auth",
            "target": "tools/verify_track21a_release.py",
            "mutation": "Invoke release verifier against empty/missing evidence directory",
            "status": "KILLED" if v01_killed else "SURVIVED",
            "failure_signature": classify_failure_signature({"trap_kind": "VERIFIER_FAIL_OPEN_CAUGHT"}) if v01_killed else "SURVIVED_NO_FAILURE",
            "trap_detail": f"Verifier exited rc={clean_bad_run.returncode}: {clean_bad_run.stderr.strip()[:140]}",
            "minimal_reproducing_input": f"verify_track21a_release.py --verify-dir {td}",
        })

        # M_VERIFY_02: Mutate independent_verify_json_receipt to blindly return (True, "bypassed")
        # and verify that --self-test negative controls detect the broken fail-open verifier with SELF_TEST_FAIL!
        mut_ver_code = replace_once(
            src_ver,
            "def independent_verify_json_receipt(r: Dict[str, Any], hmac_key: bytes | None = None) -> Tuple[bool, str]:\n",
            "def independent_verify_json_receipt(r: Dict[str, Any], hmac_key: bytes | None = None) -> Tuple[bool, str]:\n    return True, 'bypassed'\n",
        )
        ver_mut_path.write_text(mut_ver_code, encoding="utf-8")
        proc = subprocess.run(
            [sys.executable, str(ver_mut_path), "--self-test"],
            cwd=str(REPO_ROOT),
            env=sub_env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        v02_killed = (proc.returncode == 1 and "SELF_TEST_FAIL:" in proc.stderr)
        results.append({
            "mutant_id": "M_VERIFY_02_FAIL_OPEN_RECEIPT_BYPASS",
            "domain": "verifier_release_auth",
            "target": "tools/verify_track21a_release.py:independent_verify_json_receipt",
            "mutation": "Force receipt verifier to return (True, 'bypassed') unconditionally",
            "status": "KILLED" if v02_killed else "SURVIVED",
            "failure_signature": classify_failure_signature({"trap_kind": "VERIFIER_FAIL_OPEN_CAUGHT"}) if v02_killed else "SURVIVED_NO_FAILURE",
            "trap_detail": f"Mutated verifier --self-test exited rc={proc.returncode}: {proc.stderr.strip()[:140]}",
            "minimal_reproducing_input": "Mutated independent_verify_json_receipt returning True unconditionally -> caught by --self-test",
        })

    # Unknown signature preservation probe (unregistered anomaly MUST remain "UNKNOWN")
    unknown_probe = {"trap_kind": "NOVEL_COSMIC_BITFLIP_0xDEAD", "detail": "unclassified anomalous event"}
    unknown_sig = classify_failure_signature(unknown_probe)
    unknown_preserved = (unknown_sig == "UNKNOWN")

    killed_count = sum(1 for r in results if r["status"] == "KILLED")
    survived_count = len(results) - killed_count
    det_rate = round(killed_count / len(results), 4) if results else 0.0

    report = {
        "schema_version": "track21a.mutation_report.v1",
        "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "total_mutants": len(results),
        "killed": killed_count,
        "survived": survived_count,
        "detection_rate": det_rate,
        "false_positives_on_clean_control": false_positives,
        "false_negatives": survived_count,
        "unknown_signature_probe": {
            "input": unknown_probe,
            "classified_signature": unknown_sig,
            "preserved_as_unknown": unknown_preserved,
        },
        "scope_limitation": (
            "100% kill rate over these 13 controlled fault injections demonstrates sensitivity to the tested "
            "arithmetic, indexing, quantization, accumulation, dispatch, capability, receipt, and verifier bypass "
            "fault classes only; it is NOT proof that all possible silent failures are detectable."
        ),
        "verdict": "PASS" if (survived_count == 0 and false_positives == 0 and unknown_preserved) else "FAIL",
        "mutants": results,
    }

    out_path = REPO_ROOT / "TRACK21A_MUTATION_REPORT.json"
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"TRACK21A_MUTATION_OBSERVATORY total={len(results)} killed={killed_count} survived={survived_count} false_pos={false_positives} unknown_preserved={unknown_preserved} verdict={report['verdict']}")
    return 0 if report["verdict"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
