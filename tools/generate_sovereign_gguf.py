#!/usr/bin/env python3
"""
Sovereign GGUF v3 Model Trainer & Independent Pure-Python Reference Oracle
==========================================================================
Trains a 2-layer Llama GQA Transformer (dim=64, hidden=128, heads=4, kv_heads=2,
head_dim=16, vocab=128 ASCII) using exact backpropagation in NumPy and exports
standard GGUF v3 model files in three GGML tensor formats:
  - build_win64/zcc_tiny_llama_f32.gguf  (GGML_TYPE_F32  = 0)
  - build_win64/zcc_tiny_llama_f16.gguf  (GGML_TYPE_F16  = 1)
  - build_win64/zcc_tiny_llama_q8_0.gguf (GGML_TYPE_Q8_0 = 8, 34-byte blocks)

Also provides `read_gguf_model()` and `reference_forward_gguf()` written purely in
Python stdlib (`struct` + `math`, zero NumPy) as an independent verification oracle
for `build_win64/zcc_gpu_llm.exe`.
"""

import argparse
import math
import os
import struct
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(REPO_ROOT, "build_win64")

GGUF_MAGIC = 0x46554747  # "GGUF" little-endian
GGUF_VERSION = 3
GGUF_ALIGNMENT = 32

GGUF_TYPE_UINT8 = 0
GGUF_TYPE_INT8 = 1
GGUF_TYPE_UINT16 = 2
GGUF_TYPE_INT16 = 3
GGUF_TYPE_UINT32 = 4
GGUF_TYPE_INT32 = 5
GGUF_TYPE_FLOAT32 = 6
GGUF_TYPE_BOOL = 7
GGUF_TYPE_STRING = 8
GGUF_TYPE_ARRAY = 9
GGUF_TYPE_UINT64 = 10
GGUF_TYPE_INT64 = 11
GGUF_TYPE_FLOAT64 = 12

GGML_TYPE_F32 = 0
GGML_TYPE_F16 = 1
GGML_TYPE_Q8_0 = 8

CORPUS_SENTENCES = [
    "ZCC compiles C99 directly into native PTX ISA 8.7!\n",
    "RTX 5070 Tensor Core mma executes without CUDA SDK.\n",
    "Blackwell sm_120 runs live GPU kernels via nvcuda.dll!\n",
    "Sovereign Win64 PE32+ needs zero MSVC or MinGW.\n",
    "Transformer RMSNorm RoPE GQA SwiGLU run in VRAM!\n",
    "Zero nvcc pure machine code drives real AI inference.\n",
]

CFG = {
    "dim": 64,
    "hidden_dim": 128,
    "n_layers": 2,
    "n_heads": 4,
    "n_kv_heads": 2,
    "head_dim": 16,
    "vocab_size": 128,
    "max_seq_len": 128,
    "rms_eps": 1e-5,
    "rope_theta": 10000.0,
}


def f32_to_f16_roundtrip(val: float) -> float:
    return struct.unpack("<e", struct.pack("<e", float(val)))[0]


def _pack_gguf_str(s: str) -> bytes:
    b = s.encode("utf-8")
    return struct.pack("<Q", len(b)) + b


def _pack_kv_str(k: str, v: str) -> bytes:
    return _pack_gguf_str(k) + struct.pack("<I", GGUF_TYPE_STRING) + _pack_gguf_str(v)


def _pack_kv_u32(k: str, v: int) -> bytes:
    return _pack_gguf_str(k) + struct.pack("<II", GGUF_TYPE_UINT32, v & 0xFFFFFFFF)


def _pack_kv_f32(k: str, v: float) -> bytes:
    return _pack_gguf_str(k) + struct.pack("<If", GGUF_TYPE_FLOAT32, float(v))


def _pack_kv_str_array(k: str, arr: list[str]) -> bytes:
    out = [_pack_gguf_str(k), struct.pack("<IIQ", GGUF_TYPE_ARRAY, GGUF_TYPE_STRING, len(arr))]
    for item in arr:
        out.append(_pack_gguf_str(item))
    return b"".join(out)


def _align_up(n: int, align: int = GGUF_ALIGNMENT) -> int:
    return (n + align - 1) & ~(align - 1)


def _encode_tensor_bytes(arr_flat: list[float], dims: tuple[int, ...], ggml_type: int) -> bytes:
    if ggml_type == GGML_TYPE_F32:
        return struct.pack(f"<{len(arr_flat)}f", *[float(x) for x in arr_flat])
    if ggml_type == GGML_TYPE_F16:
        return struct.pack(f"<{len(arr_flat)}e", *[float(x) for x in arr_flat])
    if ggml_type == GGML_TYPE_Q8_0:
        if len(dims) < 1 or (dims[0] % 32) != 0:
            raise ValueError(f"Q8_0 requires inner dimension multiple of 32, got {dims}")
        out = bytearray()
        for off in range(0, len(arr_flat), 32):
            blk = arr_flat[off : off + 32]
            amax = max(abs(x) for x in blk)
            if amax == 0.0:
                out.extend(b"\x00" * 34)
            else:
                d_bytes = struct.pack("<e", amax / 127.0)
                d = struct.unpack("<e", d_bytes)[0]
                if d == 0.0:
                    out.extend(b"\x00" * 34)
                else:
                    qs = [max(-127, min(127, int(round(x / d)))) for x in blk]
                    out.extend(d_bytes)
                    out.extend(struct.pack("<32b", *qs))
        return bytes(out)
    raise ValueError(f"Unsupported ggml_type={ggml_type}")


def write_gguf_model(out_path: str, weights: dict, cfg: dict, weight_ggml_type: int) -> None:
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    vocab_tokens = []
    for i in range(cfg["vocab_size"]):
        if 32 <= i < 127:
            vocab_tokens.append(chr(i))
        elif i == 10:
            vocab_tokens.append("\\n")
        else:
            vocab_tokens.append(f"<0x{i:02x}>")

    kv_entries = [
        _pack_kv_str("general.architecture", "llama"),
        _pack_kv_str("general.name", "zcc-sovereign-blackwell-tiny-llama"),
        _pack_kv_u32("general.alignment", GGUF_ALIGNMENT),
        _pack_kv_u32("llama.block_count", cfg["n_layers"]),
        _pack_kv_u32("llama.embedding_length", cfg["dim"]),
        _pack_kv_u32("llama.feed_forward_length", cfg["hidden_dim"]),
        _pack_kv_u32("llama.attention.head_count", cfg["n_heads"]),
        _pack_kv_u32("llama.attention.head_count_kv", cfg["n_kv_heads"]),
        _pack_kv_u32("llama.context_length", cfg["max_seq_len"]),
        _pack_kv_f32("llama.attention.layer_norm_rms_epsilon", cfg["rms_eps"]),
        _pack_kv_f32("llama.rope.freq_base", cfg["rope_theta"]),
        _pack_kv_str_array("tokenizer.ggml.tokens", vocab_tokens),
    ]

    # Order tensors deterministically. Norm vectors stay F32 (ggml standard); 2D matrices use target type.
    ordered_names = ["token_embd.weight"]
    for l in range(cfg["n_layers"]):
        ordered_names.extend([
            f"blk.{l}.attn_norm.weight",
            f"blk.{l}.attn_q.weight",
            f"blk.{l}.attn_k.weight",
            f"blk.{l}.attn_v.weight",
            f"blk.{l}.attn_output.weight",
            f"blk.{l}.ffn_norm.weight",
            f"blk.{l}.ffn_gate.weight",
            f"blk.{l}.ffn_up.weight",
            f"blk.{l}.ffn_down.weight",
        ])
    ordered_names.extend(["output_norm.weight", "output.weight"])

    tensor_infos = []
    data_blobs = []
    cur_data_off = 0
    for name in ordered_names:
        arr = weights[name]
        # Shape in NumPy: 2D weight is (N_out, K_in); GGUF stores dims[0]=K_in (contiguous), dims[1]=N_out
        if len(arr.shape) == 1:
            gguf_dims = (int(arr.shape[0]),)
            ttype = GGML_TYPE_F32
        elif len(arr.shape) == 2:
            gguf_dims = (int(arr.shape[1]), int(arr.shape[0]))
            ttype = weight_ggml_type
        else:
            raise ValueError(f"Unexpected rank for {name}: {arr.shape}")

        raw = _encode_tensor_bytes(arr.reshape(-1).tolist(), gguf_dims, ttype)
        cur_data_off = _align_up(cur_data_off, GGUF_ALIGNMENT)
        info = bytearray()
        info.extend(_pack_gguf_str(name))
        info.extend(struct.pack("<I", len(gguf_dims)))
        for d in gguf_dims:
            info.extend(struct.pack("<Q", d))
        info.extend(struct.pack("<IQ", ttype, cur_data_off))
        tensor_infos.append(bytes(info))
        data_blobs.append((cur_data_off, raw))
        cur_data_off += len(raw)

    header = struct.pack("<IIQQ", GGUF_MAGIC, GGUF_VERSION, len(ordered_names), len(kv_entries))
    meta_bytes = header + b"".join(kv_entries) + b"".join(tensor_infos)
    pad_hdr = _align_up(len(meta_bytes), GGUF_ALIGNMENT) - len(meta_bytes)

    total_data_size = _align_up(cur_data_off, GGUF_ALIGNMENT)
    data_buf = bytearray(total_data_size)
    for off, raw in data_blobs:
        data_buf[off : off + len(raw)] = raw

    with open(out_path, "wb") as f:
        f.write(meta_bytes)
        if pad_hdr:
            f.write(b"\x00" * pad_hdr)
        f.write(data_buf)


# =========================================================================
# NumPy Trainer (Exact Forward + Backprop + AdamW)
# =========================================================================

def train_sovereign_tiny_llama(steps: int = 450, seed: int = 2026) -> dict:
    import numpy as np

    rng = np.random.default_rng(seed)
    D = CFG["dim"]
    H = CFG["hidden_dim"]
    L = CFG["n_layers"]
    NH = CFG["n_heads"]
    NKV = CFG["n_kv_heads"]
    HD = CFG["head_dim"]
    KVD = NKV * HD
    V = CFG["vocab_size"]
    EPS = CFG["rms_eps"]
    THETA = CFG["rope_theta"]
    KV_GROUP = NH // NKV

    params = {
        "token_embd.weight": (rng.standard_normal((V, D)) * 0.08).astype(np.float32),
        "output_norm.weight": np.ones((D,), dtype=np.float32),
        "output.weight": (rng.standard_normal((V, D)) * 0.08).astype(np.float32),
    }
    for l in range(L):
        params[f"blk.{l}.attn_norm.weight"] = np.ones((D,), dtype=np.float32)
        params[f"blk.{l}.attn_q.weight"] = (rng.standard_normal((D, D)) * 0.08).astype(np.float32)
        params[f"blk.{l}.attn_k.weight"] = (rng.standard_normal((KVD, D)) * 0.08).astype(np.float32)
        params[f"blk.{l}.attn_v.weight"] = (rng.standard_normal((KVD, D)) * 0.08).astype(np.float32)
        params[f"blk.{l}.attn_output.weight"] = (rng.standard_normal((D, D)) * 0.05).astype(np.float32)
        params[f"blk.{l}.ffn_norm.weight"] = np.ones((D,), dtype=np.float32)
        params[f"blk.{l}.ffn_gate.weight"] = (rng.standard_normal((H, D)) * 0.08).astype(np.float32)
        params[f"blk.{l}.ffn_up.weight"] = (rng.standard_normal((H, D)) * 0.08).astype(np.float32)
        params[f"blk.{l}.ffn_down.weight"] = (rng.standard_normal((D, H)) * 0.05).astype(np.float32)

    seqs = [[ord(ch) for ch in s] for s in CORPUS_SENTENCES]
    max_T = max(len(s) - 1 for s in seqs)

    # Precompute RoPE cos/sin for 0..max_T-1 using exact adjacent pairs (2p, 2p+1)
    inv_freq = np.array([1.0 / (THETA ** (2.0 * p / HD)) for p in range(HD // 2)], dtype=np.float32)
    pos_vec = np.arange(max_T, dtype=np.float32)[:, None]
    angles = pos_vec * inv_freq[None, :]
    cos_tab = np.cos(angles).astype(np.float32)
    sin_tab = np.sin(angles).astype(np.float32)

    def apply_rope(x, T, n_h):
        # x: (T, n_h * HD) -> reshape (T, n_h, HD//2, 2)
        xr = x.reshape(T, n_h, HD // 2, 2)
        c = cos_tab[:T, None, :]
        s = sin_tab[:T, None, :]
        x0 = xr[..., 0]
        x1 = xr[..., 1]
        out = np.empty_like(xr)
        out[..., 0] = x0 * c - x1 * s
        out[..., 1] = x0 * s + x1 * c
        return out.reshape(T, n_h * HD)

    def rope_backward(dy, T, n_h):
        dr = dy.reshape(T, n_h, HD // 2, 2)
        c = cos_tab[:T, None, :]
        s = sin_tab[:T, None, :]
        y0 = dr[..., 0]
        y1 = dr[..., 1]
        dx = np.empty_like(dr)
        dx[..., 0] = y0 * c + y1 * s
        dx[..., 1] = -y0 * s + y1 * c
        return dx.reshape(T, n_h * HD)

    def rmsnorm_fwd(x, w):
        # x: (T, D)
        inv_rms = 1.0 / np.sqrt(np.mean(x * x, axis=-1, keepdims=True) + EPS)
        return x * inv_rms * w[None, :], inv_rms

    def rmsnorm_bwd(dy, x, w, inv_rms):
        g = dy * w[None, :]
        s = np.sum(g * x, axis=-1, keepdims=True)
        dx = inv_rms * (g - x * (inv_rms * inv_rms / D) * s)
        dw = np.sum(dy * x * inv_rms, axis=0)
        return dx, dw

    m_state = {k: np.zeros_like(v) for k, v in params.items()}
    v_state = {k: np.zeros_like(v) for k, v in params.items()}
    beta1, beta2, adam_eps = 0.9, 0.999, 1e-8

    last_loss = 0.0
    for step in range(1, steps + 1):
        grads = {k: np.zeros_like(v) for k, v in params.items()}
        total_loss = 0.0
        total_toks = 0

        for seq in seqs:
            inp = np.array(seq[:-1], dtype=np.int64)
            tgt = np.array(seq[1:], dtype=np.int64)
            T = len(inp)
            mask = np.triu(np.full((T, T), -1e9, dtype=np.float32), k=1)

            x = params["token_embd.weight"][inp].copy()
            layer_cache = []
            for l in range(L):
                xn, inv_r1 = rmsnorm_fwd(x, params[f"blk.{l}.attn_norm.weight"])
                Wq = params[f"blk.{l}.attn_q.weight"]
                Wk = params[f"blk.{l}.attn_k.weight"]
                Wv = params[f"blk.{l}.attn_v.weight"]
                Wo = params[f"blk.{l}.attn_output.weight"]

                q_pre = xn @ Wq.T
                k_pre = xn @ Wk.T
                v_att = xn @ Wv.T
                q_rot = apply_rope(q_pre, T, NH)
                k_rot = apply_rope(k_pre, T, NKV)

                qh = q_rot.reshape(T, NH, HD)
                kh = k_rot.reshape(T, NKV, HD)
                vh = v_att.reshape(T, NKV, HD)
                attn_out = np.zeros((T, NH, HD), dtype=np.float32)
                probs_list = []
                scale = 1.0 / math.sqrt(HD)
                for h in range(NH):
                    kvh = h // KV_GROUP
                    scores = (qh[:, h, :] @ kh[:, kvh, :].T) * scale + mask
                    scores_m = scores - np.max(scores, axis=-1, keepdims=True)
                    ex = np.exp(scores_m)
                    p = ex / np.sum(ex, axis=-1, keepdims=True)
                    probs_list.append(p)
                    attn_out[:, h, :] = p @ vh[:, kvh, :]

                attn_flat = attn_out.reshape(T, D)
                attn_proj = attn_flat @ Wo.T
                x_mid = x + attn_proj

                fn, inv_r2 = rmsnorm_fwd(x_mid, params[f"blk.{l}.ffn_norm.weight"])
                Wg = params[f"blk.{l}.ffn_gate.weight"]
                Wu = params[f"blk.{l}.ffn_up.weight"]
                Wd = params[f"blk.{l}.ffn_down.weight"]
                g_pre = fn @ Wg.T
                u_val = fn @ Wu.T
                sig = 1.0 / (1.0 + np.exp(-np.clip(g_pre, -20.0, 20.0)))
                silu = g_pre * sig
                ff_h = silu * u_val
                ff_out = ff_h @ Wd.T
                x_next = x_mid + ff_out

                layer_cache.append((
                    x, xn, inv_r1, qh, kh, vh, probs_list, attn_flat, x_mid,
                    fn, inv_r2, g_pre, sig, silu, u_val, ff_h
                ))
                x = x_next

            xf, inv_rf = rmsnorm_fwd(x, params["output_norm.weight"])
            Wout = params["output.weight"]
            logits = xf @ Wout.T
            l_max = np.max(logits, axis=-1, keepdims=True)
            ex = np.exp(logits - l_max)
            probs = ex / np.sum(ex, axis=-1, keepdims=True)
            total_loss += float(-np.sum(np.log(np.clip(probs[np.arange(T), tgt], 1e-12, 1.0))))
            total_toks += T

            dlogits = probs.copy()
            dlogits[np.arange(T), tgt] -= 1.0
            grads["output.weight"] += dlogits.T @ xf
            dxf = dlogits @ Wout
            dx, dwo_norm = rmsnorm_bwd(dxf, x, params["output_norm.weight"], inv_rf)
            grads["output_norm.weight"] += dwo_norm

            for l in range(L - 1, -1, -1):
                (
                    lx, xn, inv_r1, qh, kh, vh, probs_list, attn_flat, x_mid,
                    fn, inv_r2, g_pre, sig, silu, u_val, ff_h
                ) = layer_cache[l]
                Wq = params[f"blk.{l}.attn_q.weight"]
                Wk = params[f"blk.{l}.attn_k.weight"]
                Wv = params[f"blk.{l}.attn_v.weight"]
                Wo = params[f"blk.{l}.attn_output.weight"]
                Wg = params[f"blk.{l}.ffn_gate.weight"]
                Wu = params[f"blk.{l}.ffn_up.weight"]
                Wd = params[f"blk.{l}.ffn_down.weight"]

                # FFN backward
                dff_h = dx @ Wd
                grads[f"blk.{l}.ffn_down.weight"] += dx.T @ ff_h
                dsilu = dff_h * u_val
                du = dff_h * silu
                dg_pre = dsilu * (sig * (1.0 + g_pre * (1.0 - sig)))
                grads[f"blk.{l}.ffn_gate.weight"] += dg_pre.T @ fn
                grads[f"blk.{l}.ffn_up.weight"] += du.T @ fn
                dfn = dg_pre @ Wg + du @ Wu
                dx_mid_fn, dw_fn = rmsnorm_bwd(dfn, x_mid, params[f"blk.{l}.ffn_norm.weight"], inv_r2)
                grads[f"blk.{l}.ffn_norm.weight"] += dw_fn
                dx_mid = dx + dx_mid_fn

                # Attention backward
                dattn_flat = dx_mid @ Wo
                grads[f"blk.{l}.attn_output.weight"] += dx_mid.T @ attn_flat
                dattn = dattn_flat.reshape(T, NH, HD)
                dqh = np.zeros_like(qh)
                dkh = np.zeros_like(kh)
                dvh = np.zeros_like(vh)
                scale = 1.0 / math.sqrt(HD)
                for h in range(NH):
                    kvh = h // KV_GROUP
                    p = probs_list[h]
                    do = dattn[:, h, :]
                    dvh[:, kvh, :] += p.T @ do
                    dp = do @ vh[:, kvh, :].T
                    ds = p * (dp - np.sum(p * dp, axis=-1, keepdims=True))
                    dqh[:, h, :] += (ds @ kh[:, kvh, :]) * scale
                    dkh[:, kvh, :] += (ds.T @ qh[:, h, :]) * scale

                dq_pre = rope_backward(dqh.reshape(T, D), T, NH)
                dk_pre = rope_backward(dkh.reshape(T, KVD), T, NKV)
                dv_att = dvh.reshape(T, KVD)
                grads[f"blk.{l}.attn_q.weight"] += dq_pre.T @ xn
                grads[f"blk.{l}.attn_k.weight"] += dk_pre.T @ xn
                grads[f"blk.{l}.attn_v.weight"] += dv_att.T @ xn
                dxn = dq_pre @ Wq + dk_pre @ Wk + dv_att @ Wv
                dx_attn, dw_an = rmsnorm_bwd(dxn, lx, params[f"blk.{l}.attn_norm.weight"], inv_r1)
                grads[f"blk.{l}.attn_norm.weight"] += dw_an
                dx = dx_mid + dx_attn

            np.add.at(grads["token_embd.weight"], inp, dx)

        lr = 0.015 * (0.5 + 0.5 * math.cos(math.pi * step / steps))
        bc1 = 1.0 - beta1 ** step
        bc2 = 1.0 - beta2 ** step
        inv_tok = 1.0 / float(total_toks)
        for k, v in params.items():
            g = grads[k] * inv_tok
            m_state[k] = beta1 * m_state[k] + (1.0 - beta1) * g
            v_state[k] = beta2 * v_state[k] + (1.0 - beta2) * (g * g)
            m_hat = m_state[k] / bc1
            v_hat = v_state[k] / bc2
            params[k] = v - lr * m_hat / (np.sqrt(v_hat) + adam_eps)

        last_loss = total_loss / float(total_toks)

    return {"params": params, "final_loss": last_loss, "steps": steps}


# =========================================================================
# Independent Pure-Python GGUF v3 Reader & IEEE-754 Transformer Reference Oracle
# (Zero NumPy, zero external deps — runs in plain system `python3`)
# =========================================================================

def _read_gguf_str(data: bytes, off: int) -> tuple[str, int]:
    slen = struct.unpack_from("<Q", data, off)[0]
    off += 8
    return data[off : off + slen].decode("utf-8", errors="replace"), off + slen


def _skip_gguf_val(data: bytes, off: int, vtype: int) -> tuple[object, int]:
    fixed_fmt = {
        GGUF_TYPE_UINT8: "<B",
        GGUF_TYPE_INT8: "<b",
        GGUF_TYPE_UINT16: "<H",
        GGUF_TYPE_INT16: "<h",
        GGUF_TYPE_UINT32: "<I",
        GGUF_TYPE_INT32: "<i",
        GGUF_TYPE_FLOAT32: "<f",
        GGUF_TYPE_BOOL: "<?",
        GGUF_TYPE_UINT64: "<Q",
        GGUF_TYPE_INT64: "<q",
        GGUF_TYPE_FLOAT64: "<d",
    }
    if vtype in fixed_fmt:
        fmt = fixed_fmt[vtype]
        val = struct.unpack_from(fmt, data, off)[0]
        return val, off + struct.calcsize(fmt)
    if vtype == GGUF_TYPE_STRING:
        return _read_gguf_str(data, off)
    if vtype == GGUF_TYPE_ARRAY:
        elem_t, count = struct.unpack_from("<IQ", data, off)
        off += 12
        items = []
        for _ in range(count):
            v, off = _skip_gguf_val(data, off, elem_t)
            items.append(v)
        return items, off
    raise ValueError(f"Unsupported GGUF metadata type {vtype}")


def read_gguf_model(gguf_path: str) -> dict:
    with open(gguf_path, "rb") as f:
        data = f.read()
    magic, version, n_tensors, n_kv = struct.unpack_from("<IIQQ", data, 0)
    if magic != GGUF_MAGIC or version != GGUF_VERSION:
        raise ValueError(f"Invalid GGUF header magic=0x{magic:08x} version={version}")
    off = 24
    meta = {}
    for _ in range(n_kv):
        key, off = _read_gguf_str(data, off)
        vtype = struct.unpack_from("<I", data, off)[0]
        off += 4
        val, off = _skip_gguf_val(data, off, vtype)
        meta[key] = val

    t_specs = []
    for _ in range(n_tensors):
        name, off = _read_gguf_str(data, off)
        ndims = struct.unpack_from("<I", data, off)[0]
        off += 4
        dims = list(struct.unpack_from(f"<{ndims}Q", data, off))
        off += 8 * ndims
        ttype, t_off = struct.unpack_from("<IQ", data, off)
        off += 12
        t_specs.append((name, dims, ttype, t_off))

    align = int(meta.get("general.alignment", 32))
    data_start = _align_up(off, align)

    tensors = {}
    tensor_types = {}
    for name, dims, ttype, t_off in t_specs:
        abs_off = data_start + t_off
        numel = 1
        for d in dims:
            numel *= d
        if ttype == GGML_TYPE_F32:
            vals = list(struct.unpack_from(f"<{numel}f", data, abs_off))
        elif ttype == GGML_TYPE_F16:
            vals = [float(x) for x in struct.unpack_from(f"<{numel}e", data, abs_off)]
        elif ttype == GGML_TYPE_Q8_0:
            vals = []
            pos = abs_off
            for _ in range(numel // 32):
                d = struct.unpack_from("<e", data, pos)[0]
                qs = struct.unpack_from("<32b", data, pos + 2)
                vals.extend(float(d) * float(q) for q in qs)
                pos += 34
        else:
            raise ValueError(f"Unsupported GGML tensor type {ttype} for {name}")
        tensors[name] = (dims, vals)
        tensor_types[name] = ttype

    return {"meta": meta, "tensors": tensors, "tensor_types": tensor_types}


def _matvec_py(W_flat: list[float], x: list[float], N: int, K: int) -> list[float]:
    y = [0.0] * N
    for n in range(N):
        base = n * K
        s = 0.0
        for k in range(K):
            s += W_flat[base + k] * x[k]
        y[n] = s
    return y


def _rmsnorm_py(x: list[float], w: list[float], eps: float) -> list[float]:
    dim = len(x)
    ss = sum(v * v for v in x) / float(dim)
    inv_rms = 1.0 / math.sqrt(ss + eps)
    return [x[i] * inv_rms * w[i] for i in range(dim)]


def reference_forward_gguf(gguf_path: str, prompt_ids: list[int], steps: int, quantize_act_f16: bool = False) -> dict:
    m = read_gguf_model(gguf_path)
    meta = m["meta"]
    T = m["tensors"]
    dim = int(meta["llama.embedding_length"])
    n_layers = int(meta["llama.block_count"])
    n_heads = int(meta["llama.attention.head_count"])
    n_kv = int(meta.get("llama.attention.head_count_kv", n_heads))
    head_dim = dim // n_heads
    kv_dim = n_kv * head_dim
    hidden_dim = int(meta["llama.feed_forward_length"])
    eps = float(meta.get("llama.attention.layer_norm_rms_epsilon", 1e-5))
    theta_base = float(meta.get("llama.rope.freq_base", 10000.0))
    vocab_size = T["token_embd.weight"][0][1]

    k_cache = [[None] * (len(prompt_ids) + steps + 1) for _ in range(n_layers)]
    v_cache = [[None] * (len(prompt_ids) + steps + 1) for _ in range(n_layers)]

    def lin(name: str, vec: list[float], out_n: int, in_k: int) -> list[float]:
        v_in = [f32_to_f16_roundtrip(z) for z in vec] if quantize_act_f16 else vec
        return _matvec_py(T[name][1], v_in, out_n, in_k)

    def forward_one(tok: int, pos: int) -> list[float]:
        emb_flat = T["token_embd.weight"][1]
        x = emb_flat[tok * dim : (tok + 1) * dim]
        for l in range(n_layers):
            xn = _rmsnorm_py(x, T[f"blk.{l}.attn_norm.weight"][1], eps)
            q = lin(f"blk.{l}.attn_q.weight", xn, dim, dim)
            k = lin(f"blk.{l}.attn_k.weight", xn, kv_dim, dim)
            v = lin(f"blk.{l}.attn_v.weight", xn, kv_dim, dim)

            for h in range(n_heads):
                off = h * head_dim
                for p in range(head_dim // 2):
                    freq = 1.0 / (theta_base ** ((2.0 * p) / head_dim))
                    ang = float(pos) * freq
                    c, s = math.cos(ang), math.sin(ang)
                    x0, x1 = q[off + 2 * p], q[off + 2 * p + 1]
                    q[off + 2 * p] = x0 * c - x1 * s
                    q[off + 2 * p + 1] = x0 * s + x1 * c
            for h in range(n_kv):
                off = h * head_dim
                for p in range(head_dim // 2):
                    freq = 1.0 / (theta_base ** ((2.0 * p) / head_dim))
                    ang = float(pos) * freq
                    c, s = math.cos(ang), math.sin(ang)
                    x0, x1 = k[off + 2 * p], k[off + 2 * p + 1]
                    k[off + 2 * p] = x0 * c - x1 * s
                    k[off + 2 * p + 1] = x0 * s + x1 * c

            k_cache[l][pos] = k
            v_cache[l][pos] = v

            attn_out = [0.0] * dim
            kv_group = n_heads // n_kv
            scale = 1.0 / math.sqrt(head_dim)
            for h in range(n_heads):
                kvh = h // kv_group
                qh = q[h * head_dim : (h + 1) * head_dim]
                scores = []
                for t in range(pos + 1):
                    kt = k_cache[l][t][kvh * head_dim : (kvh + 1) * head_dim]
                    scores.append(sum(qh[d] * kt[d] for d in range(head_dim)) * scale)
                mx = max(scores)
                ex = [math.exp(sc - mx) for sc in scores]
                sm = sum(ex)
                pr = [e / sm for e in ex]
                for d in range(head_dim):
                    attn_out[h * head_dim + d] = sum(
                        pr[t] * v_cache[l][t][kvh * head_dim + d] for t in range(pos + 1)
                    )

            proj = lin(f"blk.{l}.attn_output.weight", attn_out, dim, dim)
            x = [x[i] + proj[i] for i in range(dim)]

            fn = _rmsnorm_py(x, T[f"blk.{l}.ffn_norm.weight"][1], eps)
            g = lin(f"blk.{l}.ffn_gate.weight", fn, hidden_dim, dim)
            u = lin(f"blk.{l}.ffn_up.weight", fn, hidden_dim, dim)
            ff = [(g[i] / (1.0 + math.exp(-g[i]))) * u[i] for i in range(hidden_dim)]
            fo = lin(f"blk.{l}.ffn_down.weight", ff, dim, hidden_dim)
            x = [x[i] + fo[i] for i in range(dim)]

        xf = _rmsnorm_py(x, T["output_norm.weight"][1], eps)
        out_name = "output.weight" if "output.weight" in T else "token_embd.weight"
        return lin(out_name, xf, vocab_size, dim)

    pos = 0
    logits = None
    for tok in prompt_ids:
        logits = forward_one(tok, pos)
        pos += 1
    first_logits = list(logits)

    gen_ids = []
    for _ in range(steps):
        nxt = max(range(vocab_size), key=lambda i: logits[i])
        gen_ids.append(nxt)
        logits = forward_one(nxt, pos)
        pos += 1

    return {
        "prompt_logits": first_logits,
        "generated_ids": gen_ids,
        "generated_text": "".join(chr(t) if (t == 10 or 32 <= t < 127) else "?" for t in gen_ids),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Train sovereign 2-layer Llama GQA model & emit F32/F16/Q8_0 GGUF v3 files.")
    parser.add_argument("--out-dir", default=BUILD_DIR)
    parser.add_argument("--steps", type=int, default=450)
    parser.add_argument("--seed", type=int, default=2026)
    args = parser.parse_args()

    res = train_sovereign_tiny_llama(steps=args.steps, seed=args.seed)
    params = res["params"]
    print(f"[TRAIN] steps={res['steps']} final_loss={res['final_loss']:.6f}", flush=True)
    if res["final_loss"] > 0.05:
        raise RuntimeError(f"Training did not converge cleanly: loss={res['final_loss']:.6f}")

    paths = {}
    for tag, gtype in (("f32", GGML_TYPE_F32), ("f16", GGML_TYPE_F16), ("q8_0", GGML_TYPE_Q8_0)):
        out_p = os.path.join(args.out_dir, f"zcc_tiny_llama_{tag}.gguf")
        write_gguf_model(out_p, params, CFG, gtype)
        paths[tag] = out_p
        print(f"[GGUF:{tag.upper()}] {out_p} ({os.path.getsize(out_p)} bytes)", flush=True)

    # Quick sanity decode across first 3 prompts using pure-Python F32 reference oracle
    for s in CORPUS_SENTENCES[:3]:
        prefix = s[:10]
        pids = [ord(c) for c in prefix]
        ref = reference_forward_gguf(paths["f32"], pids, len(s) - len(prefix))
        if ref["generated_text"] != s[len(prefix):]:
            raise RuntimeError(f"Oracle decode mismatch on {prefix!r}: got {ref['generated_text']!r}, expected {s[len(prefix):]!r}")
        print(f"[ORACLE-CHECK] {prefix!r} -> {ref['generated_text'].strip()!r}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
