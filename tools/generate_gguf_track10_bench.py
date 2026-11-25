#!/usr/bin/env python3
"""
Track 10 Real-Scale (~86 MB) Qwen2.5 Q4_K_M GGUF Generator & NumPy Oracle
===========================================================================
Generates build_win64/zcc_hf_qwen2_real_scale_q4km.gguf (12L, dim=1024,
hidden=2816, heads=8, kv_heads=2, max_seq=512, vocab=512, Q4_K+Q6_K+Q8_0+F32
QKV biases) that exceeds the RTX 5070 Laptop GPU's 32 MB L2 cache for realistic
GDDR7 bandwidth, FP16/Q8_0 KV-cache, and batched SM 12.0 Tensor Core prefill
benchmarking.

Also provides fast NumPy-accelerated GGML encoders (byte-identical to
tools/generate_gguf_v2_fixtures.py) and `reference_forward_track10()`.
"""

import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_lua_win64 import BUILD_DIR
from tools.generate_gguf_v2_fixtures import (
    GGUF_MAGIC,
    GGML_F32,
    GGML_Q4_K,
    GGML_Q6_K,
    GGML_Q8_0,
    TYPE_ARR,
    TYPE_BOOL,
    TYPE_F32,
    TYPE_I32,
    TYPE_STR,
    TYPE_U32,
    _ggml_nbytes,
    _next_bpe_pretoken_len,
    _rd_str,
    _rd_u32,
    _rd_u64,
    _rd_val,
    build_gpt2_bpe_vocab,
    gpt2_bytes_to_unicode,
    reference_forward_gguf_v2,
    reference_tokenize_gguf,
    write_gguf_v3,
)

VENV_PYTHON = "/home/ZKAEDI/.venv/bin/python"

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    np = None
    HAS_NUMPY = False


def np_encode_f32(arr) -> bytes:
    return np.asarray(arr, dtype="<f4").tobytes()


def np_encode_q8_0(arr) -> bytes:
    x = np.asarray(arr, dtype=np.float32).reshape(-1, 32)
    amax = np.max(np.abs(x), axis=1)
    d = np.where(amax > 0.0, amax / 127.0, 0.0)
    d_f16 = d.astype(np.float16)
    d_q = d_f16.astype(np.float64)[:, None]
    with np.errstate(divide="ignore", invalid="ignore"):
        q = np.where(d_q > 0.0, np.rint(x.astype(np.float64) / d_q), 0.0)
    qi = np.clip(q, -127, 127).astype(np.int8)
    rec = np.empty(x.shape[0], dtype=[("d", "<u2"), ("qs", "32i1")])
    rec["d"] = d_f16.view("<u2")
    rec["qs"] = qi
    return rec.tobytes()


def np_decode_q8_0(raw: bytes, shape) -> "np.ndarray":
    dt = np.dtype([("d", "<u2"), ("qs", "32i1")])
    rec = np.frombuffer(raw, dtype=dt)
    d = rec["d"].view(np.float16).astype(np.float32)[:, None]
    return (d * rec["qs"].astype(np.float32)).reshape(shape)


def np_encode_q4_k(arr) -> bytes:
    sb = np.asarray(arr, dtype=np.float64).reshape(-1, 8, 32)
    lo = np.minimum(0.0, sb.min(axis=2))
    hi = np.maximum(0.0, sb.max(axis=2))
    m_j = -lo
    inv_scales = (hi + m_j) / 15.0
    max_sc = inv_scales.max(axis=1)
    max_mn = m_j.max(axis=1)
    d_f16 = np.where(max_sc > 0.0, max_sc / 63.0, 0.0).astype(np.float16)
    dm_f16 = np.where(max_mn > 0.0, max_mn / 63.0, 0.0).astype(np.float16)
    d_q = d_f16.astype(np.float64)[:, None]
    dm_q = dm_f16.astype(np.float64)[:, None]
    with np.errstate(divide="ignore", invalid="ignore"):
        sc_i = np.where(d_q > 0.0, np.rint(inv_scales / d_q), 0.0)
        mn_i = np.where(dm_q > 0.0, np.rint(m_j / dm_q), 0.0)
    sc_i = np.clip(sc_i, 0, 63).astype(np.uint8)
    mn_i = np.clip(mn_i, 0, 63).astype(np.uint8)

    scales = np.empty((sb.shape[0], 12), dtype=np.uint8)
    scales[:, 0:4] = (sc_i[:, 0:4] & 63) | ((sc_i[:, 4:8] >> 4) << 6)
    scales[:, 4:8] = (mn_i[:, 0:4] & 63) | ((mn_i[:, 4:8] >> 4) << 6)
    scales[:, 8:12] = (sc_i[:, 4:8] & 15) | ((mn_i[:, 4:8] & 15) << 4)

    ds = (d_q * sc_i).reshape(-1, 4, 2, 1)
    dm = (dm_q * mn_i).reshape(-1, 4, 2, 1)
    sb4 = sb.reshape(-1, 4, 2, 32)
    with np.errstate(divide="ignore", invalid="ignore"):
        qv = np.where(ds > 0.0, np.rint((sb4 + dm) / ds), 0.0)
    qv = np.clip(qv, 0, 15).astype(np.uint8)
    qs = (qv[:, :, 0, :] | (qv[:, :, 1, :] << 4)).reshape(-1, 128)

    rec = np.empty(sb.shape[0], dtype=[("d", "<u2"), ("dm", "<u2"), ("scales", "12u1"), ("qs", "128u1")])
    rec["d"] = d_f16.view("<u2")
    rec["dm"] = dm_f16.view("<u2")
    rec["scales"] = scales
    rec["qs"] = qs
    return rec.tobytes()


def np_decode_q4_k(raw: bytes, shape) -> "np.ndarray":
    dt = np.dtype([("d", "<u2"), ("dm", "<u2"), ("scales", "12u1"), ("qs", "128u1")])
    rec = np.frombuffer(raw, dtype=dt)
    df = rec["d"].view(np.float16).astype(np.float32)
    dmf = rec["dm"].view(np.float16).astype(np.float32)
    sc_bytes = rec["scales"]
    sc = np.empty((rec.shape[0], 8), dtype=np.uint8)
    mn = np.empty((rec.shape[0], 8), dtype=np.uint8)
    sc[:, 0:4] = sc_bytes[:, 0:4] & 63
    mn[:, 0:4] = sc_bytes[:, 4:8] & 63
    sc[:, 4:8] = (sc_bytes[:, 8:12] & 15) | ((sc_bytes[:, 0:4] >> 6) << 4)
    mn[:, 4:8] = (sc_bytes[:, 8:12] >> 4) | ((sc_bytes[:, 4:8] >> 6) << 4)
    ds = (df[:, None] * sc.astype(np.float32)).reshape(-1, 4, 2, 1)
    dm = (dmf[:, None] * mn.astype(np.float32)).reshape(-1, 4, 2, 1)
    qs = rec["qs"].reshape(-1, 4, 32)
    out = np.empty((rec.shape[0], 4, 2, 32), dtype=np.float32)
    out[:, :, 0, :] = ds[:, :, 0, :] * (qs & 15).astype(np.float32) - dm[:, :, 0, :]
    out[:, :, 1, :] = ds[:, :, 1, :] * (qs >> 4).astype(np.float32) - dm[:, :, 1, :]
    return out.reshape(shape)


def np_encode_q6_k(arr) -> bytes:
    sb = np.asarray(arr, dtype=np.float64).reshape(-1, 16, 16)
    gmax = np.abs(sb).max(axis=2)
    d_max = (gmax / 31.0).max(axis=1)
    d_f16 = np.where(gmax.max(axis=1) > 0.0, d_max / 127.0, 0.0).astype(np.float16)
    d_q = d_f16.astype(np.float64)[:, None]
    with np.errstate(divide="ignore", invalid="ignore"):
        sc_i = np.where(d_q > 0.0, np.rint((gmax / 31.0) / d_q), 0.0)
    sc_i = np.clip(sc_i, -127, 127).astype(np.int32)
    sc_i = np.where((sc_i == 0) & (gmax > 0.0), 1, sc_i).astype(np.int8)
    sf = (d_q * sc_i.astype(np.float64))[:, :, None]
    with np.errstate(divide="ignore", invalid="ignore"):
        qv = np.where(sf > 0.0, np.rint(sb / sf), 0.0)
    qv = (np.clip(qv, -32, 31) + 32).astype(np.uint8).reshape(-1, 2, 4, 32)
    q1, q2, q3, q4 = qv[:, :, 0, :], qv[:, :, 1, :], qv[:, :, 2, :], qv[:, :, 3, :]
    ql = np.empty((sb.shape[0], 2, 64), dtype=np.uint8)
    ql[:, :, 0:32] = (q1 & 15) | ((q3 & 15) << 4)
    ql[:, :, 32:64] = (q2 & 15) | ((q4 & 15) << 4)
    qh = ((q1 >> 4) & 3) | (((q2 >> 4) & 3) << 2) | (((q3 >> 4) & 3) << 4) | (((q4 >> 4) & 3) << 6)
    rec = np.empty(sb.shape[0], dtype=[("ql", "128u1"), ("qh", "64u1"), ("sc", "16i1"), ("d", "<u2")])
    rec["ql"] = ql.reshape(-1, 128)
    rec["qh"] = qh.reshape(-1, 64)
    rec["sc"] = sc_i
    rec["d"] = d_f16.view("<u2")
    return rec.tobytes()


def np_decode_q6_k(raw: bytes, shape) -> "np.ndarray":
    dt = np.dtype([("ql", "128u1"), ("qh", "64u1"), ("sc", "16i1"), ("d", "<u2")])
    rec = np.frombuffer(raw, dtype=dt)
    d = rec["d"].view(np.float16).astype(np.float32)[:, None, None]
    ql = rec["ql"].reshape(-1, 2, 64)
    qh = rec["qh"].reshape(-1, 2, 32)
    sc = rec["sc"].reshape(-1, 2, 8).astype(np.float32)
    is_i = (np.arange(32) >> 4)[None, None, :]
    ql0 = ql[:, :, 0:32]
    ql1 = ql[:, :, 32:64]
    q1 = ((ql0 & 15) | ((qh & 3) << 4)).astype(np.int32) - 32
    q2 = ((ql1 & 15) | (((qh >> 2) & 3) << 4)).astype(np.int32) - 32
    q3 = ((ql0 >> 4) | (((qh >> 4) & 3) << 4)).astype(np.int32) - 32
    q4 = ((ql1 >> 4) | (((qh >> 6) & 3) << 4)).astype(np.int32) - 32
    out = np.empty((rec.shape[0], 2, 4, 32), dtype=np.float32)
    out[:, :, 0, :] = d * np.take_along_axis(sc, is_i, axis=2) * q1
    out[:, :, 1, :] = d * np.take_along_axis(sc, is_i + 2, axis=2) * q2
    out[:, :, 2, :] = d * np.take_along_axis(sc, is_i + 4, axis=2) * q3
    out[:, :, 3, :] = d * np.take_along_axis(sc, is_i + 6, axis=2) * q4
    return out.reshape(shape)


def np_decode_tensor(raw: bytes, dims: list[int], ggml_type: int) -> "np.ndarray":
    # In GGUF dims = [K, N], tensor row-major in file is (N, K)
    shape = (dims[1], dims[0]) if len(dims) == 2 else (dims[0],)
    if ggml_type == GGML_F32:
        return np.frombuffer(raw, dtype="<f4").reshape(shape).copy()
    if ggml_type == 1:
        return np.frombuffer(raw, dtype="<f2").astype(np.float32).reshape(shape).copy()
    if ggml_type == 30:
        u = np.frombuffer(raw, dtype="<u2").astype(np.uint32) << 16
        return u.view(np.float32).reshape(shape).copy()
    if ggml_type == GGML_Q8_0:
        return np_decode_q8_0(raw, shape)
    if ggml_type == GGML_Q4_K:
        return np_decode_q4_k(raw, shape)
    if ggml_type == GGML_Q6_K:
        return np_decode_q6_k(raw, shape)
    raise ValueError(f"Unsupported type {ggml_type}")


def _np_det_vec(tag: str, n: int, scale: float) -> "np.ndarray":
    seed = int.from_bytes(hashlib.sha256(tag.encode("utf-8")).digest()[:8], "little")
    rng = np.random.Generator(np.random.PCG64(seed))
    return rng.uniform(-scale, scale, size=n).astype(np.float32)


def _np_det_norm(tag: str, n: int) -> "np.ndarray":
    seed = int.from_bytes(hashlib.sha256(tag.encode("utf-8")).digest()[:8], "little")
    rng = np.random.Generator(np.random.PCG64(seed))
    return rng.uniform(0.85, 1.15, size=n).astype(np.float32)


def generate_track10_real_scale_fixture(out_dir: str = BUILD_DIR) -> dict:
    if not HAS_NUMPY:
        if os.path.isfile(VENV_PYTHON):
            subprocess.check_call([VENV_PYTHON, __file__, "--out-dir", out_dir])
            out_p = os.path.join(out_dir, "zcc_hf_qwen2_real_scale_q4km.gguf")
            with open(out_p, "rb") as f:
                sha = hashlib.sha256(f.read()).hexdigest()
            return {"path": out_p, "bytes": os.path.getsize(out_p), "sha256": sha}
        raise RuntimeError("NumPy required to generate real-scale ~86 MB GGUF fixture fast.")

    arch = "qwen2"
    n_layers = 12
    dim = 1024
    hidden = 2816
    n_heads = 8
    n_kv_heads = 2
    head_dim = dim // n_heads
    kv_dim = n_kv_heads * head_dim
    max_seq = 512
    vocab = 512
    rms_eps = 1e-6
    rope_theta = 1000000.0
    scale_mat = 0.025

    bpe_toks, bpe_types, bpe_merges, bpe_bos, bpe_eos = build_gpt2_bpe_vocab(vocab)
    kvs = [
        ("general.architecture", TYPE_STR, arch),
        ("general.alignment", TYPE_U32, 32),
        (f"{arch}.block_count", TYPE_U32, n_layers),
        (f"{arch}.embedding_length", TYPE_U32, dim),
        (f"{arch}.feed_forward_length", TYPE_U32, hidden),
        (f"{arch}.attention.head_count", TYPE_U32, n_heads),
        (f"{arch}.attention.head_count_kv", TYPE_U32, n_kv_heads),
        (f"{arch}.context_length", TYPE_U32, max_seq),
        (f"{arch}.attention.layer_norm_rms_epsilon", TYPE_F32, rms_eps),
        (f"{arch}.rope.freq_base", TYPE_F32, rope_theta),
        ("tokenizer.ggml.model", TYPE_STR, "gpt2"),
        ("tokenizer.ggml.tokens", TYPE_ARR, (TYPE_STR, bpe_toks)),
        ("tokenizer.ggml.token_type", TYPE_ARR, (TYPE_I32, bpe_types)),
        ("tokenizer.ggml.merges", TYPE_ARR, (TYPE_STR, bpe_merges)),
        ("tokenizer.ggml.bos_token_id", TYPE_U32, bpe_bos),
        ("tokenizer.ggml.eos_token_id", TYPE_U32, bpe_eos),
        ("tokenizer.ggml.add_bos_token", TYPE_BOOL, False),
    ]

    tag0 = "zcc_hf_qwen2_real_scale_q4km.gguf"
    tensors: list[tuple[str, list[int], int, bytes]] = []
    tensors.append(("token_embd.weight", [dim, vocab], GGML_Q8_0, np_encode_q8_0(_np_det_vec(f"{tag0}:emb", vocab * dim, 0.25))))
    for l in range(n_layers):
        tensors.append((f"blk.{l}.attn_norm.weight", [dim], GGML_F32, np_encode_f32(_np_det_norm(f"{tag0}:an:{l}", dim))))
        tensors.append((f"blk.{l}.attn_q.weight", [dim, dim], GGML_Q4_K, np_encode_q4_k(_np_det_vec(f"{tag0}:wq:{l}", dim * dim, scale_mat))))
        tensors.append((f"blk.{l}.attn_k.weight", [dim, kv_dim], GGML_Q4_K, np_encode_q4_k(_np_det_vec(f"{tag0}:wk:{l}", kv_dim * dim, scale_mat))))
        tensors.append((f"blk.{l}.attn_v.weight", [dim, kv_dim], GGML_Q6_K, np_encode_q6_k(_np_det_vec(f"{tag0}:wv:{l}", kv_dim * dim, scale_mat))))
        tensors.append((f"blk.{l}.attn_q.bias", [dim], GGML_F32, np_encode_f32(_np_det_vec(f"{tag0}:bq:{l}", dim, 0.15))))
        tensors.append((f"blk.{l}.attn_k.bias", [kv_dim], GGML_F32, np_encode_f32(_np_det_vec(f"{tag0}:bk:{l}", kv_dim, 0.15))))
        tensors.append((f"blk.{l}.attn_v.bias", [kv_dim], GGML_F32, np_encode_f32(_np_det_vec(f"{tag0}:bv:{l}", kv_dim, 0.15))))
        tensors.append((f"blk.{l}.attn_output.weight", [dim, dim], GGML_Q4_K, np_encode_q4_k(_np_det_vec(f"{tag0}:wo:{l}", dim * dim, scale_mat))))
        tensors.append((f"blk.{l}.ffn_norm.weight", [dim], GGML_F32, np_encode_f32(_np_det_norm(f"{tag0}:fn:{l}", dim))))
        tensors.append((f"blk.{l}.ffn_gate.weight", [dim, hidden], GGML_Q4_K, np_encode_q4_k(_np_det_vec(f"{tag0}:wg:{l}", hidden * dim, scale_mat))))
        tensors.append((f"blk.{l}.ffn_up.weight", [dim, hidden], GGML_Q4_K, np_encode_q4_k(_np_det_vec(f"{tag0}:wu:{l}", hidden * dim, scale_mat))))
        tensors.append((f"blk.{l}.ffn_down.weight", [hidden, dim], GGML_Q6_K, np_encode_q6_k(_np_det_vec(f"{tag0}:wd:{l}", dim * hidden, scale_mat))))

    tensors.append(("output_norm.weight", [dim], GGML_F32, np_encode_f32(_np_det_norm(f"{tag0}:on", dim))))
    tensors.append(("output.weight", [dim, vocab], GGML_Q6_K, np_encode_q6_k(_np_det_vec(f"{tag0}:wout", vocab * dim, 0.15))))

    out_p = os.path.join(out_dir, tag0)
    sha = write_gguf_v3(out_p, kvs, tensors, align=32)
    sz = os.path.getsize(out_p)
    print(f"[GGUF-T10-GEN] {out_p}: {sz} bytes ({sz / (1024*1024):.2f} MiB), SHA256={sha}")
    return {"path": out_p, "bytes": sz, "sha256": sha}


def _np_rmsnorm(x: "np.ndarray", w: "np.ndarray", eps: float) -> "np.ndarray":
    inv = 1.0 / np.sqrt(np.mean(x * x, dtype=np.float32) + np.float32(eps))
    return (x * inv * w).astype(np.float32)


def _np_rope(q: "np.ndarray", k: "np.ndarray", pos: int, n_heads: int, n_kv_heads: int, head_dim: int, theta: float, neox: bool = False) -> None:
    half = head_dim >> 1
    p_idx = np.arange(half, dtype=np.float64)
    freq = np.power(float(theta), -(2.0 * p_idx) / float(head_dim))
    ang = float(pos) * freq
    c = np.cos(ang).astype(np.float32)
    s = np.sin(ang).astype(np.float32)
    if neox:
        qh = q.reshape(n_heads, 2, half)
        kh = k.reshape(n_kv_heads, 2, half)
        q0, q1 = qh[:, 0, :].copy(), qh[:, 1, :].copy()
        qh[:, 0, :] = q0 * c - q1 * s
        qh[:, 1, :] = q0 * s + q1 * c
        k0, k1 = kh[:, 0, :].copy(), kh[:, 1, :].copy()
        kh[:, 0, :] = k0 * c - k1 * s
        kh[:, 1, :] = k0 * s + k1 * c
    else:
        qh = q.reshape(n_heads, half, 2)
        kh = k.reshape(n_kv_heads, half, 2)
        q0, q1 = qh[:, :, 0].copy(), qh[:, :, 1].copy()
        qh[:, :, 0] = q0 * c - q1 * s
        qh[:, :, 1] = q0 * s + q1 * c
        k0, k1 = kh[:, :, 0].copy(), kh[:, :, 1].copy()
        kh[:, :, 0] = k0 * c - k1 * s
        kh[:, :, 1] = k0 * s + k1 * c


def np_load_gguf_v2(path: str, decode_tensors: bool = True) -> dict:
    with open(path, "rb") as f:
        buf = f.read()
    magic, version, n_tensors, n_kv = struct.unpack_from("<IIQQ", buf, 0)
    assert magic == GGUF_MAGIC and version == 3
    off = 24
    kvs = {}
    for _ in range(n_kv):
        k, off = _rd_str(buf, off)
        vt, off = _rd_u32(buf, off)
        val, off = _rd_val(buf, off, vt)
        kvs[k] = val
    t_info = []
    for _ in range(n_tensors):
        name, off = _rd_str(buf, off)
        nd, off = _rd_u32(buf, off)
        dims = []
        for _ in range(nd):
            d, off = _rd_u64(buf, off)
            dims.append(d)
        gt, off = _rd_u32(buf, off)
        toff, off = _rd_u64(buf, off)
        t_info.append((name, dims, gt, toff))
    align = kvs.get("general.alignment", 32)
    data_start = (off + align - 1) & ~(align - 1)
    tensors = {}
    if decode_tensors:
        for name, dims, gt, toff in t_info:
            nb = _ggml_nbytes(dims, gt)
            tensors[name] = np_decode_tensor(buf[data_start + toff : data_start + toff + nb], dims, gt)
    return {"kvs": kvs, "tensors": tensors}


def reference_forward_track10(gguf_path: str, prompt_ids: list[int], steps: int, ignore_eos: bool = True) -> dict:
    if not HAS_NUMPY:
        if os.path.isfile(VENV_PYTHON):
            proc = subprocess.run(
                [VENV_PYTHON, __file__, "--ref-json", gguf_path, "--steps", str(steps),
                 "--ids", ",".join(str(x) for x in prompt_ids)] + (["--ignore-eos"] if ignore_eos else []),
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=120,
            )
            if proc.returncode == 0:
                return json.loads(proc.stdout)
        return reference_forward_gguf_v2(gguf_path, prompt_ids, steps, ignore_eos=ignore_eos)

    m = np_load_gguf_v2(gguf_path, decode_tensors=True)
    kvs = m["kvs"]
    ts = m["tensors"]
    arch = kvs["general.architecture"]
    nl = kvs[f"{arch}.block_count"]
    dim = kvs[f"{arch}.embedding_length"]
    nh = kvs[f"{arch}.attention.head_count"]
    nkv = kvs[f"{arch}.attention.head_count_kv"]
    hd = dim // nh
    kv_group = nh // nkv
    eps = kvs.get(f"{arch}.attention.layer_norm_rms_epsilon", 1e-5)
    theta = kvs.get(f"{arch}.rope.freq_base", 10000.0)

    emb = ts["token_embd.weight"]
    vocab = emb.shape[0]
    out_norm = ts["output_norm.weight"]
    wout = ts["output.weight"] if "output.weight" in ts else emb

    layers = []
    for l in range(nl):
        ld = {
            "an": ts[f"blk.{l}.attn_norm.weight"],
            "wq": ts[f"blk.{l}.attn_q.weight"],
            "wk": ts[f"blk.{l}.attn_k.weight"],
            "wv": ts[f"blk.{l}.attn_v.weight"],
            "wo": ts[f"blk.{l}.attn_output.weight"],
            "fn": ts[f"blk.{l}.ffn_norm.weight"],
            "wg": ts[f"blk.{l}.ffn_gate.weight"],
            "wu": ts[f"blk.{l}.ffn_up.weight"],
            "wd": ts[f"blk.{l}.ffn_down.weight"],
            "bq": ts.get(f"blk.{l}.attn_q.bias"),
            "bk": ts.get(f"blk.{l}.attn_k.bias"),
            "bv": ts.get(f"blk.{l}.attn_v.bias"),
        }
        layers.append(ld)

    max_len = len(prompt_ids) + steps + 4
    k_cache = np.zeros((nl, max_len, nkv, hd), dtype=np.float32)
    v_cache = np.zeros((nl, max_len, nkv, hd), dtype=np.float32)
    inv_sqrt_hd = np.float32(1.0 / (hd ** 0.5))

    def step_tok(tok: int, pos: int) -> "np.ndarray":
        x = emb[tok].copy()
        for l in range(nl):
            lw = layers[l]
            xb = _np_rmsnorm(x, lw["an"], eps)
            q = lw["wq"] @ xb
            k = lw["wk"] @ xb
            v = lw["wv"] @ xb
            if lw["bq"] is not None: q = q + lw["bq"]
            if lw["bk"] is not None: k = k + lw["bk"]
            if lw["bv"] is not None: v = v + lw["bv"]
            _np_rope(q, k, pos, nh, nkv, hd, theta, neox=(arch == "qwen2"))
            k_cache[l, pos] = k.reshape(nkv, hd)
            v_cache[l, pos] = v.reshape(nkv, hd)

            qh = q.reshape(nh, hd)
            kh_all = k_cache[l, : pos + 1]
            vh_all = v_cache[l, : pos + 1]
            attn_out = np.empty((nh, hd), dtype=np.float32)
            for h in range(nh):
                kvh = h // kv_group
                scores = (kh_all[:, kvh, :] @ qh[h]) * inv_sqrt_hd
                scores = scores - np.max(scores)
                probs = np.exp(scores)
                probs /= np.sum(probs)
                attn_out[h] = probs @ vh_all[:, kvh, :]
            x = x + (lw["wo"] @ attn_out.reshape(dim))
            xb2 = _np_rmsnorm(x, lw["fn"], eps)
            hb = lw["wg"] @ xb2
            hb2 = lw["wu"] @ xb2
            silu = hb / (1.0 + np.exp(-hb))
            x = x + (lw["wd"] @ (silu * hb2))
        xf = _np_rmsnorm(x, out_norm, eps)
        return (wout @ xf).astype(np.float32)

    pos = 0
    logits = None
    for tid in prompt_ids:
        logits = step_tok(tid if 0 <= tid < vocab else 0, pos)
        pos += 1
    prompt_logits = [float(v) for v in logits]
    gen_ids: list[int] = []
    tmodel = kvs.get("tokenizer.ggml.model", "")
    eos_id = kvs.get("tokenizer.ggml.eos_token_id")
    for _ in range(steps):
        nxt = int(np.argmax(logits))
        if tmodel != "" and not ignore_eos and eos_id is not None and nxt == eos_id:
            break
        gen_ids.append(nxt)
        logits = step_tok(nxt, pos)
        pos += 1
    return {"prompt_logits": prompt_logits, "gen_ids": gen_ids}


def reference_tokenize_track10(gguf_path: str, text: str, no_bos: bool = False, chat: bool = False) -> list[int]:
    with open(gguf_path, "rb") as f:
        buf = f.read(524288)
    magic, version, _, n_kv = struct.unpack_from("<IIQQ", buf, 0)
    assert magic == GGUF_MAGIC and version == 3
    off = 24
    kvs = {}
    for _ in range(n_kv):
        k, off = _rd_str(buf, off)
        vt, off = _rd_u32(buf, off)
        val, off = _rd_val(buf, off, vt)
        kvs[k] = val
    arch = kvs.get("general.architecture", "llama")
    tokens = kvs.get("tokenizer.ggml.tokens", [])
    tok_to_id = {s: idx for idx, s in enumerate(tokens)}
    specials = [sp for sp in ("<|endoftext|>", "<|im_start|>", "<|im_end|>") if sp in tok_to_id]
    merges = {}
    for rank, m in enumerate(kvs.get("tokenizer.ggml.merges", [])):
        p = m.split(" ")
        if len(p) == 2:
            merges[(p[0], p[1])] = rank
    if chat:
        if arch == "qwen2" or "<|im_start|>" in tok_to_id:
            text = f"<|im_start|>user\n{text}<|im_end|>\n<|im_start|>assistant\n"
        else:
            text = f"[INST] {text} [/INST]"
    out_ids: list[int] = []
    if not no_bos and kvs.get("tokenizer.ggml.add_bos_token", False) and "tokenizer.ggml.bos_token_id" in kvs:
        out_ids.append(int(kvs["tokenizer.ggml.bos_token_id"]))
    b2u, _ = gpt2_bytes_to_unicode()
    i = 0
    while i < len(text):
        matched = None
        for sp in specials:
            if text.startswith(sp, i):
                matched = sp
                break
        if matched:
            out_ids.append(tok_to_id[matched])
            i += len(matched)
            continue
        nxt_sp = len(text)
        for sp in specials:
            pos = text.find(sp, i)
            if pos != -1 and pos < nxt_sp:
                nxt_sp = pos
        seg = text[i:nxt_sp].encode("utf-8")
        p0 = 0
        while p0 < len(seg):
            pl = _next_bpe_pretoken_len(seg[p0:])
            syms = [b2u[b] for b in seg[p0 : p0 + pl]]
            while len(syms) > 1:
                best_r = 1 << 30
                best_i = -1
                for j in range(len(syms) - 1):
                    r = merges.get((syms[j], syms[j + 1]))
                    if r is not None and r < best_r:
                        best_r = r
                        best_i = j
                if best_i == -1:
                    break
                syms = syms[:best_i] + [syms[best_i] + syms[best_i + 1]] + syms[best_i + 2 :]
            for sym in syms:
                if sym in tok_to_id:
                    out_ids.append(tok_to_id[sym])
            p0 += pl
        i = nxt_sp
    return out_ids


def main() -> int:
    if not HAS_NUMPY and os.path.isfile(VENV_PYTHON):
        os.execv(VENV_PYTHON, [VENV_PYTHON, *sys.argv])

    ap = argparse.ArgumentParser(description="Track 10 Real-Scale ~86 MB GGUF Fixture & NumPy Oracle")
    ap.add_argument("--out-dir", default=BUILD_DIR)
    ap.add_argument("--verify-encoders", action="store_true")
    ap.add_argument("--ref-json", default=None)
    ap.add_argument("--steps", type=int, default=8)
    ap.add_argument("--ids", default="0")
    ap.add_argument("--ignore-eos", action="store_true")
    args = ap.parse_args()

    if args.ref_json:
        pids = [int(x) for x in args.ids.split(",") if x.strip()]
        res = reference_forward_track10(args.ref_json, pids, args.steps, ignore_eos=args.ignore_eos)
        print(json.dumps(res))
        return 0

    if args.verify_encoders:
        from tools.generate_gguf_v2_fixtures import encode_q4_k, encode_q6_k, encode_q8_0, _det_vec
        v = _det_vec("enc_parity_check", 1024, 0.25)
        assert np_encode_q8_0(v) == encode_q8_0(v), "Q8_0 NumPy encoder mismatch!"
        assert np_encode_q4_k(v) == encode_q4_k(v), "Q4_K NumPy encoder mismatch!"
        assert np_encode_q6_k(v) == encode_q6_k(v), "Q6_K NumPy encoder mismatch!"
        print("[ENCODER-PARITY] Q8_0, Q4_K, Q6_K NumPy vs pure-Python byte-identical OK")
        if "--no-gen" in sys.argv:
            return 0

    os.makedirs(args.out_dir, exist_ok=True)
    generate_track10_real_scale_fixture(args.out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
