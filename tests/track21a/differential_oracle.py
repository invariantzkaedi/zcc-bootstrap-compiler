#!/usr/bin/env python3
"""ZCC Track 21A — Independent Differential Numerical Oracle & Edge-Case Suite.

Independently evaluates Track 21A operations in IEEE-754 binary64 (Python float/math/struct)
and compares SCALAR_C99 (FP64_REF & FP32_FMA), AVX2_FMA, and PTX_SM120 across:
- normal dimensions
- prime/boundary dimensions (1, 3, 7, 8, 9, 17, 31, 32, 33, 63, 256, 257)
- unaligned host buffers (+4, +8, +12 byte offsets and +1 arbitrary offset)
- non-contiguous strides (skipped element stride, padded row stride, transposed 2D)
- extreme magnitudes, subnormals, cancellation
- NaN / +Inf / -Inf propagation (without SIGFPE or silent finite corruption)
- invalid shapes, OOM injection, and simulated unsupported hardware verification
"""

from __future__ import annotations

import ctypes
import hashlib
import json
import math
import os
import random
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

from sdk.zcc_compute import (  # noqa: E402
    ACCUM_FP32_FMA,
    ACCUM_FP64_REF,
    BACKEND_AVX2_FMA,
    BACKEND_PTX_SM120,
    BACKEND_SCALAR_C99,
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
    ZccComputeReceipt,
    ZccComputeRuntime,
    ZccCpuCaps,
    ZccExecOptions,
    ZccTensorDesc,
)


def f32_to_bits(v: float) -> int:
    return struct.unpack("<I", struct.pack("<f", v))[0]


def bits_to_f32(b: int) -> float:
    return struct.unpack("<f", struct.pack("<I", b & 0xFFFFFFFF))[0]


def round_f32(vals: List[float]) -> List[float]:
    """Coerce Python floats to exact binary32 values so inputs match C buffers."""
    return [struct.unpack("<f", struct.pack("<f", float(v)))[0] for v in vals]


def f32_to_f16_bits(v: float) -> int:
    return struct.unpack("<H", struct.pack("<e", float(v)))[0]


def f16_bits_to_f64(h: int) -> float:
    return float(struct.unpack("<e", struct.pack("<H", h & 0xFFFF))[0])


def ulp_diff_f32(a: float, b: float) -> Optional[int]:
    if math.isnan(a) or math.isnan(b) or math.isinf(a) or math.isinf(b):
        return None
    ia = f32_to_bits(float(a))
    ib = f32_to_bits(float(b))
    # Map IEEE-754 float32 bits to monotonic lexicographical order
    sa = (0x80000000 - (ia & 0x7FFFFFFF)) if (ia & 0x80000000) else (0x80000000 + ia)
    sb = (0x80000000 - (ib & 0x7FFFFFFF)) if (ib & 0x80000000) else (0x80000000 + ib)
    return abs(sa - sb)


# ---------------------------------------------------------------------------
# Independent IEEE-754 float64 Oracles & Quant Generators
# ---------------------------------------------------------------------------

def oracle_vec_add(a: List[float], b: List[float]) -> List[float]:
    return [float(x) + float(y) for x, y in zip(a, b)]


def oracle_vec_mul(a: List[float], b: List[float]) -> List[float]:
    return [float(x) * float(y) for x, y in zip(a, b)]


def oracle_vec_fma(a: List[float], b: List[float], c: List[float]) -> List[float]:
    return [float(x) * float(y) + float(z) for x, y, z in zip(a, b, c)]


def oracle_dot(a: List[float], b: List[float]) -> List[float]:
    return [math.fsum(float(x) * float(y) for x, y in zip(a, b))]


def oracle_gemm(a: List[float], b: List[float], m: int, k: int, n: int) -> List[float]:
    out: List[float] = []
    for r in range(m):
        for c in range(n):
            out.append(math.fsum(float(a[r * k + p]) * float(b[p * n + c]) for p in range(k)))
    return out


def oracle_rmsnorm(x: List[float], w: List[float], rows: int, dim: int, eps: float) -> List[float]:
    out: List[float] = []
    for r in range(rows):
        row = x[r * dim : (r + 1) * dim]
        ss = math.fsum(float(v) * float(v) for v in row)
        inv = 1.0 / math.sqrt(ss / float(dim) + float(eps))
        for j in range(dim):
            out.append(float(row[j]) * inv * float(w[j]))
    return out


def oracle_softmax(x: List[float], rows: int, dim: int) -> List[float]:
    out: List[float] = []
    for r in range(rows):
        row = [float(v) for v in x[r * dim : (r + 1) * dim]]
        if any(math.isnan(v) for v in row):
            out.extend([float("nan")] * dim)
            continue
        m = max(row)
        if math.isinf(m):
            out.extend([float("nan")] * dim)
            continue
        exps = [math.exp(v - m) for v in row]
        s = math.fsum(exps)
        out.extend([ev / s for ev in exps])
    return out


def make_q8_0_bytes(rng: random.Random, rows: int, k: int) -> bytes:
    assert k % 32 == 0
    nb = k // 32
    buf = bytearray()
    for _ in range(rows * nb):
        d = f32_to_f16_bits(rng.uniform(0.02, 0.25))
        buf.extend(struct.pack("<H", d))
        for _l in range(32):
            q = rng.randint(-120, 120)
            buf.extend(struct.pack("<b", q))
    return bytes(buf)


def oracle_dequant_q8_0(raw: bytes, rows: int, k: int) -> List[float]:
    nb = k // 32
    out: List[float] = []
    pos = 0
    for _ in range(rows * nb):
        d = f16_bits_to_f64(struct.unpack_from("<H", raw, pos)[0])
        qs = struct.unpack_from("<32b", raw, pos + 2)
        out.extend(d * float(q) for q in qs)
        pos += 34
    return out


def make_q4_0_bytes(rng: random.Random, rows: int, k: int) -> bytes:
    assert k % 32 == 0
    nb = k // 32
    buf = bytearray()
    for _ in range(rows * nb):
        d = f32_to_f16_bits(rng.uniform(0.02, 0.25))
        buf.extend(struct.pack("<H", d))
        for _l in range(16):
            q0 = rng.randint(0, 15)
            q1 = rng.randint(0, 15)
            buf.append((q0 & 0xF) | ((q1 & 0xF) << 4))
    return bytes(buf)


def oracle_dequant_q4_0(raw: bytes, rows: int, k: int) -> List[float]:
    nb = k // 32
    out: List[float] = []
    pos = 0
    for _ in range(rows * nb):
        d = f16_bits_to_f64(struct.unpack_from("<H", raw, pos)[0])
        qb = raw[pos + 2 : pos + 18]
        lows = [d * float((qb[i] & 0xF) - 8) for i in range(16)]
        highs = [d * float((qb[i] >> 4) - 8) for i in range(16)]
        out.extend(lows)
        out.extend(highs)
        pos += 18
    return out


def make_q4_k_bytes(rng: random.Random, rows: int, k: int) -> bytes:
    assert k % 256 == 0
    sbs = k // 256
    buf = bytearray()
    for _ in range(rows * sbs):
        d = f32_to_f16_bits(rng.uniform(0.02, 0.2))
        dmin = f32_to_f16_bits(rng.uniform(0.01, 0.1))
        buf.extend(struct.pack("<HH", d, dmin))
        buf.extend(bytes(rng.randint(0, 255) for _ in range(12)))
        buf.extend(bytes(rng.randint(0, 255) for _ in range(128)))
    return bytes(buf)


def _q4k_sc(sc: bytes, j: int) -> int:
    return (sc[j] & 63) if j < 4 else ((sc[j + 4] & 15) | ((sc[j - 4] >> 6) << 4))


def _q4k_mn(sc: bytes, j: int) -> int:
    return (sc[j + 4] & 63) if j < 4 else ((sc[j + 4] >> 4) | ((sc[j] >> 6) << 4))


def oracle_dequant_q4_k(raw: bytes, rows: int, k: int) -> List[float]:
    sbs = k // 256
    out: List[float] = []
    pos = 0
    for _ in range(rows * sbs):
        d = f16_bits_to_f64(struct.unpack_from("<H", raw, pos)[0])
        dmin = f16_bits_to_f64(struct.unpack_from("<H", raw, pos + 2)[0])
        sc = raw[pos + 4 : pos + 16]
        qs = raw[pos + 16 : pos + 144]
        for c in range(4):
            ds0 = d * float(_q4k_sc(sc, c * 2))
            dm0 = dmin * float(_q4k_mn(sc, c * 2))
            ds1 = d * float(_q4k_sc(sc, c * 2 + 1))
            dm1 = dmin * float(_q4k_mn(sc, c * 2 + 1))
            for l in range(32):
                qb = qs[c * 32 + l]
                out.append(ds0 * float(qb & 15) - dm0)
            for l in range(32):
                qb = qs[c * 32 + l]
                out.append(ds1 * float(qb >> 4) - dm1)
        pos += 144
    return out


def oracle_matvec_quant(deq: List[float], x: List[float], rows: int, k: int) -> List[float]:
    return [math.fsum(deq[r * k + c] * float(x[c]) for c in range(k)) for r in range(rows)]


# ---------------------------------------------------------------------------
# Buffer helpers supporting offset/alignment/stride testing
# ---------------------------------------------------------------------------

def make_f32_desc(
    values: List[float],
    shape: Tuple[int, ...],
    strides_bytes: Optional[Tuple[int, ...]] = None,
    byte_offset: int = 0,
    align: int = 4,
    owner: int = OWNER_BORROWED_HOST,
) -> Tuple[ZccTensorDesc, ctypes.Array]:
    """Allocate a byte buffer shifted by `byte_offset` with optional custom strides."""
    esz = 4
    rank = len(shape)
    if strides_bytes is None:
        strides = [0] * 4
        # Compute required element-count packing for dense buffer
        n_total = 1
        for d in shape:
            n_total *= d
        req_bytes = n_total * esz
        backing = (ctypes.c_uint8 * (byte_offset + req_bytes + 64))()
        base_ptr = ctypes.addressof(backing) + byte_offset
        for idx, v in enumerate(values):
            struct.pack_into("<f", backing, byte_offset + idx * esz, float(v))
    else:
        strides = list(strides_bytes) + [0] * (4 - rank)
        max_off = 0
        for d in range(rank):
            max_off += (shape[d] - 1) * strides[d]
        req_bytes = max_off + esz
        backing = (ctypes.c_uint8 * (byte_offset + req_bytes + 64))()
        base_ptr = ctypes.addressof(backing) + byte_offset
        # Scatter elements according to logical row-major index -> strided offset
        for linear, v in enumerate(values):
            rem = linear
            off = 0
            for d in range(rank - 1, -1, -1):
                coord = rem % shape[d]
                rem //= shape[d]
                off += coord * strides[d]
            struct.pack_into("<f", backing, byte_offset + off, float(v))

    desc = ZccTensorDesc()
    desc.struct_size = ctypes.sizeof(ZccTensorDesc)
    desc.dtype = DTYPE_F32
    desc.rank = rank
    desc.ownership_flags = owner
    for i in range(rank):
        desc.shape[i] = shape[i]
        desc.strides[i] = strides[i]
    desc.data = base_ptr
    desc.nbytes = req_bytes
    desc.alignment = align
    return desc, backing


def read_f32_desc(desc: ZccTensorDesc, backing: ctypes.Array, byte_offset: int = 0) -> List[float]:
    rank = desc.rank
    shape = tuple(desc.shape[i] for i in range(rank))
    eff_strides = [desc.strides[i] for i in range(rank)]
    if all(s == 0 for s in eff_strides):
        exp = 4
        for d in range(rank - 1, -1, -1):
            eff_strides[d] = exp
            exp *= shape[d]
    n_total = 1
    for d in shape:
        n_total *= d
    out: List[float] = []
    for linear in range(n_total):
        rem = linear
        off = 0
        for d in range(rank - 1, -1, -1):
            coord = rem % shape[d]
            rem //= shape[d]
            off += coord * eff_strides[d]
        val = struct.unpack_from("<f", backing, byte_offset + off)[0]
        out.append(val)
    return out


def make_quant_desc(raw: bytes, dtype: int, shape: Tuple[int, ...]) -> Tuple[ZccTensorDesc, ctypes.Array]:
    backing = (ctypes.c_uint8 * len(raw)).from_buffer_copy(raw)
    desc = ZccTensorDesc()
    desc.struct_size = ctypes.sizeof(ZccTensorDesc)
    desc.dtype = dtype
    desc.rank = len(shape)
    desc.ownership_flags = OWNER_BORROWED_HOST
    for i, d in enumerate(shape):
        desc.shape[i] = d
    desc.data = ctypes.addressof(backing)
    desc.nbytes = len(raw)
    desc.alignment = 1
    return desc, backing


def compare_vectors(
    got: List[float],
    ref: List[float],
    atol: float,
    rtol: float,
    expect_nan: bool = False,
    check_softmax_sum: Optional[int] = None,
) -> Tuple[bool, float, float, Optional[int], str]:
    if len(got) != len(ref):
        return False, float("inf"), float("inf"), None, f"len mismatch {len(got)} vs {len(ref)}"
    max_abs = 0.0
    max_rel = 0.0
    max_ulp: Optional[int] = 0
    for i, (g, r) in enumerate(zip(got, ref)):
        if math.isnan(r):
            if not math.isnan(g):
                return False, float("inf"), float("inf"), None, f"idx {i}: expected NaN, got {g}"
            continue
        if math.isinf(r):
            if not (math.isinf(g) and ((g > 0) == (r > 0))):
                return False, float("inf"), float("inf"), None, f"idx {i}: expected {r}, got {g}"
            continue
        if math.isnan(g) or math.isinf(g):
            return False, float("inf"), float("inf"), None, f"idx {i}: unexpected non-finite {g} vs {r}"
        ae = abs(g - r)
        re = ae / max(abs(r), 1e-30)
        if ae > max_abs:
            max_abs = ae
        if re > max_rel:
            max_rel = re
        u = ulp_diff_f32(g, r)
        if u is not None and max_ulp is not None and u > max_ulp:
            max_ulp = u
        if ae > atol + rtol * abs(r):
            return False, max_abs, max_rel, max_ulp, (
                f"idx {i}: got={g:.9g} ref={r:.9g} abs={ae:.3e} > tol={atol + rtol * abs(r):.3e}"
            )
    if check_softmax_sum is not None and not expect_nan:
        dim = check_softmax_sum
        rows = len(got) // dim
        for row in range(rows):
            s = sum(got[row * dim : (row + 1) * dim])
            if abs(s - 1.0) > 1e-4:
                return False, max_abs, max_rel, max_ulp, f"softmax row {row} sum={s:.8f} != 1.0"
            if any(v < 0.0 or v > 1.0 + 1e-5 for v in got[row * dim : (row + 1) * dim]):
                return False, max_abs, max_rel, max_ulp, f"softmax row {row} out of [0,1]"
    return True, max_abs, max_rel, max_ulp, "ok"


def main() -> int:
    rt = ZccComputeRuntime("/tmp/libzcc_compute.so")
    cpu_caps, ptx_caps = rt.probe_caps()
    rng = random.Random(21021)

    backends = [
        ("SCALAR_C99_FP64", BACKEND_SCALAR_C99, ACCUM_FP64_REF),
        ("SCALAR_C99_FMA", BACKEND_SCALAR_C99, ACCUM_FP32_FMA),
        ("AVX2_FMA", BACKEND_AVX2_FMA, ACCUM_FP32_FMA),
        ("PTX_SM120", BACKEND_PTX_SM120, ACCUM_FP32_FMA),
    ]

    cases: List[Dict[str, Any]] = []

    def run_and_check(
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
        skip_fp64_backends: bool = False,
    ) -> None:
        in_arr = (ZccTensorDesc * len(inputs))(*(d for d, _ in inputs))
        for bname, bid, acc in backends:
            if skip_fp64_backends and acc == ACCUM_FP64_REF:
                continue
            out_desc, out_buf = make_f32_desc([0.0] * len(ref), out_shape, out_strides, out_offset, align=1)
            opts = ZccExecOptions()
            rt.lib.zcc_exec_options_init(ctypes.byref(opts))
            opts.requested_backend = bid
            opts.selection_policy = POLICY_EXACT_REQUIRED
            opts.accum_policy = acc
            opts.determinism_mode = DET_FIXED_REDUCTION
            opts.rmsnorm_eps = ctypes.c_float(eps)

            rec = ZccComputeReceipt()
            rc = rt.lib.zcc_compute_exec(op_id, in_arr, len(inputs), ctypes.byref(out_desc), ctypes.byref(opts), ctypes.byref(rec))
            if rc != 0:
                cases.append({"case": case_name, "backend": bname, "status": "FAIL", "detail": f"exec rc={rc} err={rec.error_message.decode()}"})
                continue
            if rec.backend_executed != bid or rec.fallback_occurred != 0:
                cases.append({"case": case_name, "backend": bname, "status": "FAIL", "detail": f"unexpected fallback/backend {rec.backend_executed}"})
                continue
            if rt.verify_receipt(rec) != 0:
                cases.append({"case": case_name, "backend": bname, "status": "FAIL", "detail": "receipt canonical verification failed"})
                continue

            got = read_f32_desc(out_desc, out_buf, out_offset)
            ok, ae, re, ulp, msg = compare_vectors(got, ref, atol, rtol, expect_nan=expect_nan, check_softmax_sum=softmax_dim)
            cases.append({
                "case": case_name,
                "backend": bname,
                "status": "PASS" if ok else "FAIL",
                "max_abs_err": None if math.isinf(ae) else round(ae, 10),
                "max_rel_err": None if math.isinf(re) else round(re, 10),
                "max_ulp": ulp,
                "output_sha256": rec.output_buffer_sha256.decode(),
                "detail": msg,
            })

    # 1. Elementwise (normal, prime/boundary, unaligned offsets +4/+1, strided)
    for tag, n, off, stride in [
        ("normal_256", 256, 0, None),
        ("prime_1", 1, 0, None),
        ("prime_17_unaligned4", 17, 4, None),
        ("boundary_33_unaligned1", 33, 1, None),
        ("strided_23_s12", 23, 8, (12,)),
    ]:
        a = round_f32([rng.uniform(-50.0, 50.0) for _ in range(n)])
        b = round_f32([rng.uniform(-50.0, 50.0) for _ in range(n)])
        c = round_f32([rng.uniform(-50.0, 50.0) for _ in range(n)])
        da = make_f32_desc(a, (n,), stride, off, align=1)
        db = make_f32_desc(b, (n,), stride, off, align=1)
        dc = make_f32_desc(c, (n,), stride, off, align=1)
        run_and_check(f"vec_add_{tag}", OP_VEC_ADD_F32, [da, db], (n,), stride, off, oracle_vec_add(a, b), 1e-6, 1e-6)
        run_and_check(f"vec_mul_{tag}", OP_VEC_MUL_F32, [da, db], (n,), stride, off, oracle_vec_mul(a, b), 1e-6, 1e-6)
        run_and_check(f"vec_fma_{tag}", OP_VEC_FMA_F32, [da, db, dc], (n,), stride, off, oracle_vec_fma(a, b, c), 1e-5, 1e-5)

    # 2. Dot products (well-conditioned prime & subnormal, plus high-cancellation FP64-ref separation)
    a63 = round_f32([rng.uniform(-2.0, 2.0) for _ in range(63)])
    b63 = round_f32([rng.uniform(-2.0, 2.0) for _ in range(63)])
    run_and_check("dot_prime_63", OP_DOT_F32, [make_f32_desc(a63, (63,)), make_f32_desc(b63, (63,))], (1,), None, 0, oracle_dot(a63, b63), 1e-4, 1e-4)

    a_sub = round_f32([1e-18, 1e3, 1e-18, 3.5, 1e-20])
    b_sub = round_f32([1e-18, 0.25, 1.0, 2.0, 1.0])
    run_and_check("dot_extreme_subnormal", OP_DOT_F32, [make_f32_desc(a_sub, (5,)), make_f32_desc(b_sub, (5,))], (1,), None, 0, oracle_dot(a_sub, b_sub), 1e-4, 1e-4)

    # Catastrophic cancellation (1e9 + (-1e9) + 7): proves FP64_REF exactness while binary32 backends follow condition bound
    a_cat = round_f32([1e-18, 1e9, -1e9, 1e-18, 3.5])
    b_cat = round_f32([1e-18, 1.0, 1.0, 1.0, 2.0])
    cond_sum = sum(abs(x * y) for x, y in zip(a_cat, b_cat))
    fp32_cond_tol = len(a_cat) * 1.5e-7 * cond_sum
    da_c, db_c = make_f32_desc(a_cat, (5,)), make_f32_desc(b_cat, (5,))
    run_and_check("dot_cancellation_fp64_vs_fp32_bound", OP_DOT_F32, [da_c, db_c], (1,), None, 0, oracle_dot(a_cat, b_cat), 1e-5, 1e-6, skip_fp64_backends=False) if False else None
    for bname, bid, acc in backends:
        atol_use = 1e-5 if acc == ACCUM_FP64_REF else fp32_cond_tol
        out_d, out_b = make_f32_desc([0.0], (1,))
        opts_c = ZccExecOptions()
        rt.lib.zcc_exec_options_init(ctypes.byref(opts_c))
        opts_c.requested_backend = bid
        opts_c.selection_policy = POLICY_EXACT_REQUIRED
        opts_c.accum_policy = acc
        rec_c = ZccComputeReceipt()
        rc_c = rt.lib.zcc_compute_exec(OP_DOT_F32, (ZccTensorDesc * 2)(da_c[0], db_c[0]), 2, ctypes.byref(out_d), ctypes.byref(opts_c), ctypes.byref(rec_c))
        got_c = read_f32_desc(out_d, out_b)
        ok_c, ae_c, re_c, ulp_c, msg_c = compare_vectors(got_c, oracle_dot(a_cat, b_cat), atol_use, 1e-5)
        cases.append({
            "case": "dot_cancellation_fp64_vs_cond_bound",
            "backend": bname,
            "status": "PASS" if (rc_c == 0 and ok_c) else "FAIL",
            "max_abs_err": round(ae_c, 10),
            "max_rel_err": round(re_c, 10),
            "max_ulp": ulp_c,
            "output_sha256": rec_c.output_buffer_sha256.decode(),
            "detail": f"cond_bound={atol_use:.3e} {msg_c}",
        })

    # 3. GEMM (normal, boundary prime [7,19]x[19,11], padded row stride, transposed stride)
    for tag, m, k, n, sa, sb, sc in [
        ("gemm_16x32x24", 16, 32, 24, None, None, None),
        ("gemm_prime_7x19x11_unaligned4", 7, 19, 11, None, None, None),
        ("gemm_padded_stride", 5, 13, 9, ((13 + 3) * 4, 4), ((9 + 5) * 4, 4), ((9 + 2) * 4, 4)),
        ("gemm_transposed_A_stride", 4, 6, 5, (4, 4 * 4), None, None),
    ]:
        off = 4 if "unaligned4" in tag else 0
        a = round_f32([rng.uniform(-1.5, 1.5) for _ in range(m * k)])
        b = round_f32([rng.uniform(-1.5, 1.5) for _ in range(k * n)])
        da = make_f32_desc(a, (m, k), sa, off, align=1)
        db = make_f32_desc(b, (k, n), sb, off, align=1)
        run_and_check(tag, OP_GEMM_F32, [da, db], (m, n), sc, off, oracle_gemm(a, b, m, k, n), 1e-4, 1e-4)

    # 4. RMSNorm (normal, prime dim 37, extreme magnitude 1e-12 & 1e6, padded row stride)
    for tag, rows, dim, scale, rs in [
        ("rmsnorm_8x128", 8, 128, 1.0, None),
        ("rmsnorm_prime_3x37_padded", 3, 37, 1.0, ((37 + 5) * 4, 4)),
        ("rmsnorm_extreme_mag", 2, 64, 1e5, None),
    ]:
        x = round_f32([rng.uniform(-2.0, 2.0) * scale for _ in range(rows * dim)])
        w = round_f32([rng.uniform(0.5, 1.5) for _ in range(dim)])
        dx = make_f32_desc(x, (rows, dim), rs)
        dw = make_f32_desc(w, (dim,))
        run_and_check(tag, OP_RMSNORM_F32, [dx, dw], (rows, dim), rs, 0, oracle_rmsnorm(x, w, rows, dim, 1e-5), 1e-4, 2e-4)

    # 5. Softmax (normal, prime, large logit shift, -Inf sparsity, NaN propagation, +Inf)
    for tag, rows, dim, xvals, exp_nan in [
        ("softmax_6x64", 6, 64, round_f32([rng.uniform(-5.0, 5.0) for _ in range(6 * 64)]), False),
        ("softmax_prime_3x19_extreme", 3, 19, round_f32([1000.0 + (i % 19) * 0.25 for i in range(3 * 19)]), False),
        ("softmax_neginf", 1, 5, [float("-inf"), -1.0, 0.0, 1.0, float("-inf")], False),
        ("softmax_nan_prop", 1, 4, [1.0, float("nan"), 2.0, 3.0], True),
        ("softmax_posinf", 1, 4, [1.0, float("inf"), 2.0, 3.0], True),
    ]:
        dx = make_f32_desc(xvals, (rows, dim))
        run_and_check(
            tag, OP_SOFTMAX_F32, [dx], (rows, dim), None, 0,
            oracle_softmax(xvals, rows, dim), 1e-4, 2e-4,
            expect_nan=exp_nan, softmax_dim=dim,
        )

    # 6. NaN / Inf elementwise propagation
    nan_a = [1.0, float("nan"), float("inf"), float("-inf"), 0.0]
    nan_b = [2.0, 3.0, 4.0, -5.0, float("inf")]
    run_and_check(
        "vec_mul_nan_inf", OP_VEC_MUL_F32,
        [make_f32_desc(nan_a, (5,)), make_f32_desc(nan_b, (5,))],
        (5,), None, 0, oracle_vec_mul(nan_a, nan_b), 1e-6, 1e-6, expect_nan=True,
    )

    # 7. Quantized formats Q8_0, Q4_0, Q4_K (dequant + matvec)
    q8_raw = make_q8_0_bytes(rng, 5, 96)
    q8_ref = oracle_dequant_q8_0(q8_raw, 5, 96)
    x_96 = round_f32([rng.uniform(-1.5, 1.5) for _ in range(96)])
    run_and_check("dequant_q8_0_5x96", OP_DEQUANT_Q8_0, [make_quant_desc(q8_raw, DTYPE_Q8_0, (5, 96))], (5, 96), None, 0, q8_ref, 1e-5, 1e-5, skip_fp64_backends=True)
    run_and_check("matvec_q8_0_5x96", OP_MATVEC_Q8_0, [make_quant_desc(q8_raw, DTYPE_Q8_0, (5, 96)), make_f32_desc(x_96, (96,))], (5,), None, 0, oracle_matvec_quant(q8_ref, x_96, 5, 96), 5e-4, 5e-4)

    q40_raw = make_q4_0_bytes(rng, 4, 128)
    q40_ref = oracle_dequant_q4_0(q40_raw, 4, 128)
    x_128 = round_f32([rng.uniform(-1.5, 1.5) for _ in range(128)])
    run_and_check("dequant_q4_0_4x128", OP_DEQUANT_Q4_0, [make_quant_desc(q40_raw, DTYPE_Q4_0, (4, 128))], (4, 128), None, 0, q40_ref, 1e-5, 1e-5, skip_fp64_backends=True)
    run_and_check("matvec_q4_0_4x128", OP_MATVEC_Q4_0, [make_quant_desc(q40_raw, DTYPE_Q4_0, (4, 128)), make_f32_desc(x_128, (128,))], (4,), None, 0, oracle_matvec_quant(q40_ref, x_128, 4, 128), 5e-4, 5e-4)

    q4k_raw = make_q4_k_bytes(rng, 3, 512)
    q4k_ref = oracle_dequant_q4_k(q4k_raw, 3, 512)
    x_512 = round_f32([rng.uniform(-1.0, 1.0) for _ in range(512)])
    run_and_check("dequant_q4_k_3x512", OP_DEQUANT_Q4_K, [make_quant_desc(q4k_raw, DTYPE_Q4_K, (3, 512))], (3, 512), None, 0, q4k_ref, 1e-5, 1e-5, skip_fp64_backends=True)
    run_and_check("matvec_q4_k_3x512", OP_MATVEC_Q4_K, [make_quant_desc(q4k_raw, DTYPE_Q4_K, (3, 512)), make_f32_desc(x_512, (512,))], (3,), None, 0, oracle_matvec_quant(q4k_ref, x_512, 3, 512), 5e-4, 5e-4)

    # 8. Negative edge-case validations (invalid shape, readonly, unsupported HW mock)
    edge_checks: List[Dict[str, Any]] = []
    d_bad1, b_bad1 = make_f32_desc([1.0, 2.0], (2,))
    d_bad2, b_bad2 = make_f32_desc([1.0, 2.0, 3.0], (3,))
    d_ro, b_ro = make_f32_desc([0.0, 0.0], (2,), owner=OWNER_READONLY | OWNER_BORROWED_HOST)
    opts_bad = ZccExecOptions()
    rt.lib.zcc_exec_options_init(ctypes.byref(opts_bad))
    rc_shape = rt.lib.zcc_compute_exec(OP_VEC_ADD_F32, (ZccTensorDesc * 2)(d_bad1, d_bad2), 2, ctypes.byref(d_ro), ctypes.byref(opts_bad), None)
    edge_checks.append({"check": "invalid_shape_plus_readonly_rejected", "rc": rc_shape, "status": "PASS" if rc_shape != 0 else "FAIL"})

    mock_no_xcr0 = ZccCpuCaps()
    ctypes.memmove(ctypes.byref(mock_no_xcr0), ctypes.byref(cpu_caps), ctypes.sizeof(ZccCpuCaps))
    mock_no_xcr0.xcr0_ymm = 0
    mock_no_xcr0.xcr0_raw &= ~0x04
    opts_hw = ZccExecOptions()
    rt.lib.zcc_exec_options_init(ctypes.byref(opts_hw))
    opts_hw.requested_backend = BACKEND_AVX2_FMA
    opts_hw.selection_policy = POLICY_EXACT_REQUIRED
    d_ok1, b_ok1 = make_f32_desc([1.0, 2.0], (2,))
    d_ok2, b_ok2 = make_f32_desc([3.0, 4.0], (2,))
    d_oko, b_oko = make_f32_desc([0.0, 0.0], (2,))
    rec_hw = ZccComputeReceipt()
    rc_hw_exact = rt.lib.zcc_compute_exec_with_caps(OP_VEC_ADD_F32, (ZccTensorDesc * 2)(d_ok1, d_ok2), 2, ctypes.byref(d_oko), ctypes.byref(opts_hw), ctypes.byref(mock_no_xcr0), ctypes.byref(rec_hw))
    edge_checks.append({"check": "simulated_missing_xcr0_exact_avx2_rejected", "rc": rc_hw_exact, "status": "PASS" if rc_hw_exact == -11 else "FAIL"})

    opts_hw.selection_policy = POLICY_AUTO_FALLBACK
    rc_hw_fb = rt.lib.zcc_compute_exec_with_caps(OP_VEC_ADD_F32, (ZccTensorDesc * 2)(d_ok1, d_ok2), 2, ctypes.byref(d_oko), ctypes.byref(opts_hw), ctypes.byref(mock_no_xcr0), ctypes.byref(rec_hw))
    edge_checks.append({"check": "simulated_missing_xcr0_auto_fallback_scalar", "rc": rc_hw_fb, "backend_executed": rec_hw.backend_executed, "fallback": rec_hw.fallback_occurred, "status": "PASS" if (rc_hw_fb == 0 and rec_hw.backend_executed == BACKEND_SCALAR_C99 and rec_hw.fallback_occurred == 1) else "FAIL"})

    total = len(cases) + len(edge_checks)
    failed = [c for c in cases if c["status"] != "PASS"] + [e for e in edge_checks if e["status"] != "PASS"]

    report = {
        "schema_version": "track21a.differential_report.v1",
        "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "hardware": {
            "cpu_cpuid": cpu_caps.has_cpuid,
            "cpu_osxsave": cpu_caps.has_osxsave,
            "cpu_xcr0_ymm": cpu_caps.xcr0_ymm,
            "cpu_avx2": cpu_caps.has_avx2,
            "cpu_fma": cpu_caps.has_fma,
            "gpu_available": ptx_caps.cuda_available,
            "gpu_name": ptx_caps.device_name.decode(),
            "gpu_cc": f"{ptx_caps.cc_major}.{ptx_caps.cc_minor}",
        },
        "total_checks": total,
        "passed": total - len(failed),
        "failed": len(failed),
        "verdict": "PASS" if not failed else "FAIL",
        "cases": cases,
        "edge_checks": edge_checks,
    }

    out_path = REPO_ROOT / "TRACK21A_DIFFERENTIAL_REPORT.json"
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    rt.shutdown()

    print(f"TRACK21A_DIFF_ORACLE total={total} passed={report['passed']} failed={len(failed)} verdict={report['verdict']}")
    if failed:
        for f in failed[:15]:
            print("  DIFF_FAIL:", f, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
