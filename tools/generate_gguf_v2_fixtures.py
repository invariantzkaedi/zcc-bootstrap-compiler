#!/usr/bin/env python3
"""
Sovereign Pure-Python GGUF v3 Real-World HuggingFace Fixture & Oracle Generator
================================================================================
Zero external dependencies (stdlib math + struct only).
Supports normative GGML block encoders & decoders for:
  - F32  (0), F16 (1), Q4_0 (2), Q8_0 (8), Q4_K (12), Q6_K (14), BF16 (30)
Produces 3 deterministic Track 9 GGUF fixtures in build_win64/:
  1. zcc_hf_qwen2_20l_q4km.gguf  (qwen2, 20 layers, dim=256, hidden=512, Q4_K+Q6_K+Q8_0,
                                  QKV biases, GPT-2 BPE vocab + merges + ChatML specials)
  2. zcc_hf_llama_spm_q4_0.gguf  (llama, 2 layers, dim=256, hidden=512, Q4_0, SentencePiece SPM)
  3. zcc_hf_llama_bf16.gguf      (llama, 2 layers, dim=256, hidden=512, BF16, GPT-2 BPE)
Also exports independent pure-Python reference tokenizer & forward oracle:
  - reference_tokenize_gguf(gguf_path, text, add_bos=True, chat=False)
  - reference_forward_gguf_v2(gguf_path, prompt_ids, steps)
"""

import argparse
import hashlib
import math
import operator
import os
import random
import struct
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_lua_win64 import BUILD_DIR

GGUF_MAGIC = 0x46554747
GGML_F32 = 0
GGML_F16 = 1
GGML_Q4_0 = 2
GGML_Q8_0 = 8
GGML_Q4_K = 12
GGML_Q6_K = 14
GGML_BF16 = 30

TYPE_U32 = 4
TYPE_I32 = 5
TYPE_F32 = 6
TYPE_BOOL = 7
TYPE_STR = 8
TYPE_ARR = 9


def f16_u16_to_f32(h: int) -> float:
    return struct.unpack("<e", struct.pack("<H", h & 0xFFFF))[0]


def f32_to_f16_u16(x: float) -> int:
    try:
        return struct.unpack("<H", struct.pack("<e", float(x)))[0]
    except OverflowError:
        return 0x7C00 if x > 0 else 0xFC00


def bf16_u16_to_f32(h: int) -> float:
    return struct.unpack("<f", struct.pack("<I", (h & 0xFFFF) << 16))[0]


def f32_to_bf16_u16(x: float) -> int:
    u = struct.unpack("<I", struct.pack("<f", float(x)))[0]
    if (u & 0x7FFFFFFF) > 0x7F800000:
        return ((u >> 16) | 0x0040) & 0xFFFF
    lsb = (u >> 16) & 1
    u = (u + 0x7FFF + lsb) & 0xFFFFFFFF
    return (u >> 16) & 0xFFFF


def gpt2_bytes_to_unicode() -> tuple[list[str], dict[str, int]]:
    bs = list(range(33, 127)) + list(range(161, 173)) + list(range(174, 256))
    cs = list(bs)
    n = 0
    for b in range(256):
        if b not in bs:
            bs.append(b)
            cs.append(256 + n)
            n += 1
    b2u = [""] * 256
    for b, c in zip(bs, cs):
        b2u[b] = chr(c)
    u2b = {b2u[b]: b for b in range(256)}
    return b2u, u2b


def _clamp_int(v: int, lo: int, hi: int) -> int:
    return lo if v < lo else (hi if v > hi else v)


def encode_f32(vals: list[float]) -> bytes:
    return struct.pack(f"<{len(vals)}f", *vals)


def decode_f32(raw: bytes, n: int) -> list[float]:
    return list(struct.unpack_from(f"<{n}f", raw, 0))


def encode_f16(vals: list[float]) -> bytes:
    return struct.pack(f"<{len(vals)}e", *vals)


def decode_f16(raw: bytes, n: int) -> list[float]:
    return list(struct.unpack_from(f"<{n}e", raw, 0))


def encode_bf16(vals: list[float]) -> bytes:
    u16s = [f32_to_bf16_u16(v) for v in vals]
    return struct.pack(f"<{len(u16s)}H", *u16s)


def decode_bf16(raw: bytes, n: int) -> list[float]:
    u16s = struct.unpack_from(f"<{n}H", raw, 0)
    return [bf16_u16_to_f32(h) for h in u16s]


def encode_q8_0(vals: list[float]) -> bytes:
    assert len(vals) % 32 == 0
    out = bytearray()
    for off in range(0, len(vals), 32):
        blk = vals[off : off + 32]
        amax = max(abs(v) for v in blk)
        d = (amax / 127.0) if amax > 0.0 else 0.0
        d_u16 = f32_to_f16_u16(d)
        d_q = f16_u16_to_f32(d_u16)
        qs = [_clamp_int(int(round(v / d_q)), -127, 127) if d_q > 0.0 else 0 for v in blk]
        out += struct.pack("<H32b", d_u16, *qs)
    return bytes(out)


def decode_q8_0(raw: bytes, n: int) -> list[float]:
    assert n % 32 == 0
    out = [0.0] * n
    nb = n // 32
    for b in range(nb):
        off = b * 34
        d = f16_u16_to_f32(struct.unpack_from("<H", raw, off)[0])
        qs = struct.unpack_from("<32b", raw, off + 2)
        base = b * 32
        for j in range(32):
            out[base + j] = d * qs[j]
    return out


def encode_q4_0(vals: list[float]) -> bytes:
    assert len(vals) % 32 == 0
    out = bytearray()
    for off in range(0, len(vals), 32):
        blk = vals[off : off + 32]
        amax = max(abs(v) for v in blk)
        d = (amax / 7.5) if amax > 0.0 else 0.0
        d_u16 = f32_to_f16_u16(d)
        d_q = f16_u16_to_f32(d_u16)
        qs = bytearray(16)
        for j in range(16):
            q0 = _clamp_int(int(round(blk[j] / d_q)) + 8, 0, 15) if d_q > 0.0 else 8
            q1 = _clamp_int(int(round(blk[j + 16] / d_q)) + 8, 0, 15) if d_q > 0.0 else 8
            qs[j] = q0 | (q1 << 4)
        out += struct.pack("<H", d_u16)
        out += qs
    return bytes(out)


def decode_q4_0(raw: bytes, n: int) -> list[float]:
    assert n % 32 == 0
    out = [0.0] * n
    nb = n // 32
    for b in range(nb):
        off = b * 18
        d = f16_u16_to_f32(struct.unpack_from("<H", raw, off)[0])
        base = b * 32
        for j in range(16):
            qb = raw[off + 2 + j]
            out[base + j] = d * ((qb & 15) - 8)
            out[base + j + 16] = d * ((qb >> 4) - 8)
    return out


def _q4k_scale_min_pair(scales: bytes, j: int) -> tuple[int, int]:
    if j < 4:
        return scales[j] & 63, scales[j + 4] & 63
    sc = (scales[j + 4] & 15) | ((scales[j - 4] >> 6) << 4)
    mn = (scales[j + 4] >> 4) | ((scales[j] >> 6) << 4)
    return sc, mn


def encode_q4_k(vals: list[float]) -> bytes:
    assert len(vals) % 256 == 0
    out = bytearray()
    for off in range(0, len(vals), 256):
        sb = vals[off : off + 256]
        inv_scales = []
        mins = []
        for j in range(8):
            grp = sb[j * 32 : (j + 1) * 32]
            lo = min(0.0, min(grp))
            hi = max(0.0, max(grp))
            m_j = -lo
            span_j = hi + m_j
            inv_scales.append(span_j / 15.0)
            mins.append(m_j)
        max_sc = max(inv_scales)
        max_mn = max(mins)
        d = (max_sc / 63.0) if max_sc > 0.0 else 0.0
        dmin = (max_mn / 63.0) if max_mn > 0.0 else 0.0
        d_u16 = f32_to_f16_u16(d)
        dmin_u16 = f32_to_f16_u16(dmin)
        d_q = f16_u16_to_f32(d_u16)
        dmin_q = f16_u16_to_f32(dmin_u16)

        sc_i = [_clamp_int(int(round(s / d_q)), 0, 63) if d_q > 0.0 else 0 for s in inv_scales]
        mn_i = [_clamp_int(int(round(m / dmin_q)), 0, 63) if dmin_q > 0.0 else 0 for m in mins]

        scales = bytearray(12)
        for j in range(4):
            scales[j] = (sc_i[j] & 63) | ((sc_i[j + 4] >> 4) << 6)
            scales[j + 4] = (mn_i[j] & 63) | ((mn_i[j + 4] >> 4) << 6)
            scales[j + 8] = (sc_i[j + 4] & 15) | ((mn_i[j + 4] & 15) << 4)

        qs = bytearray(128)
        for c in range(4):
            j0 = 2 * c
            j1 = 2 * c + 1
            ds0 = d_q * sc_i[j0]
            dm0 = dmin_q * mn_i[j0]
            ds1 = d_q * sc_i[j1]
            dm1 = dmin_q * mn_i[j1]
            for l in range(32):
                v0 = sb[c * 64 + l]
                v1 = sb[c * 64 + 32 + l]
                q0 = _clamp_int(int(round((v0 + dm0) / ds0)), 0, 15) if ds0 > 0.0 else 0
                q1 = _clamp_int(int(round((v1 + dm1) / ds1)), 0, 15) if ds1 > 0.0 else 0
                qs[c * 32 + l] = q0 | (q1 << 4)

        out += struct.pack("<HH", d_u16, dmin_u16)
        out += scales
        out += qs
    return bytes(out)


def decode_q4_k(raw: bytes, n: int) -> list[float]:
    assert n % 256 == 0
    out = [0.0] * n
    nsb = n // 256
    for sb in range(nsb):
        off = sb * 144
        d, dmin = struct.unpack_from("<HH", raw, off)
        df = f16_u16_to_f32(d)
        dmf = f16_u16_to_f32(dmin)
        scales = raw[off + 4 : off + 16]
        qs = raw[off + 16 : off + 144]
        base = sb * 256
        for c in range(4):
            sc0, mn0 = _q4k_scale_min_pair(scales, 2 * c)
            sc1, mn1 = _q4k_scale_min_pair(scales, 2 * c + 1)
            ds0, dm0 = df * sc0, dmf * mn0
            ds1, dm1 = df * sc1, dmf * mn1
            for l in range(32):
                qb = qs[c * 32 + l]
                out[base + c * 64 + l] = ds0 * (qb & 15) - dm0
                out[base + c * 64 + 32 + l] = ds1 * (qb >> 4) - dm1
    return out


def encode_q6_k(vals: list[float]) -> bytes:
    assert len(vals) % 256 == 0
    out = bytearray()
    for off in range(0, len(vals), 256):
        sb = vals[off : off + 256]
        grp_max = [max(abs(v) for v in sb[g * 16 : (g + 1) * 16]) for g in range(16)]
        d = (max(g / 31.0 for g in grp_max) / 127.0) if max(grp_max) > 0.0 else 0.0
        d_u16 = f32_to_f16_u16(d)
        d_q = f16_u16_to_f32(d_u16)

        sc = [0] * 16
        qvals = [0] * 256
        for g in range(16):
            s_i = _clamp_int(int(round((grp_max[g] / 31.0) / d_q)), -127, 127) if d_q > 0.0 else 0
            if s_i == 0 and grp_max[g] > 0.0:
                s_i = 1
            sc[g] = s_i
            sf = d_q * s_i
            for i in range(16):
                idx = g * 16 + i
                q = _clamp_int(int(round(sb[idx] / sf)), -32, 31) + 32 if sf > 0.0 else 32
                qvals[idx] = q

        ql = bytearray(128)
        qh = bytearray(64)
        for h in range(2):
            for l in range(32):
                b0 = h * 128 + l
                q1 = qvals[b0]
                q2 = qvals[b0 + 32]
                q3 = qvals[b0 + 64]
                q4 = qvals[b0 + 96]
                ql[h * 64 + l] = (q1 & 15) | ((q3 & 15) << 4)
                ql[h * 64 + l + 32] = (q2 & 15) | ((q4 & 15) << 4)
                qh[h * 32 + l] = (
                    ((q1 >> 4) & 3)
                    | (((q2 >> 4) & 3) << 2)
                    | (((q3 >> 4) & 3) << 4)
                    | (((q4 >> 4) & 3) << 6)
                )

        out += ql
        out += qh
        out += struct.pack("<16bH", *sc, d_u16)
    return bytes(out)


def decode_q6_k(raw: bytes, n: int) -> list[float]:
    assert n % 256 == 0
    out = [0.0] * n
    nsb = n // 256
    for sb in range(nsb):
        off = sb * 210
        ql = raw[off : off + 128]
        qh = raw[off + 128 : off + 192]
        sc = struct.unpack_from("<16b", raw, off + 192)
        d = f16_u16_to_f32(struct.unpack_from("<H", raw, off + 208)[0])
        base = sb * 256
        for h in range(2):
            qlh = ql[h * 64 : (h + 1) * 64]
            qhh = qh[h * 32 : (h + 1) * 32]
            sch = sc[h * 8 : (h + 1) * 8]
            for l in range(32):
                is_i = l >> 4
                ql0 = qlh[l]
                ql1 = qlh[l + 32]
                qh0 = qhh[l]
                q1 = ((ql0 & 15) | ((qh0 & 3) << 4)) - 32
                q2 = ((ql1 & 15) | (((qh0 >> 2) & 3) << 4)) - 32
                q3 = ((ql0 >> 4) | (((qh0 >> 4) & 3) << 4)) - 32
                q4 = ((ql1 >> 4) | (((qh0 >> 6) & 3) << 4)) - 32
                b = base + h * 128 + l
                out[b] = d * sch[is_i] * q1
                out[b + 32] = d * sch[is_i + 2] * q2
                out[b + 64] = d * sch[is_i + 4] * q3
                out[b + 96] = d * sch[is_i + 6] * q4
    return out


def encode_tensor(vals: list[float], ggml_type: int) -> bytes:
    if ggml_type == GGML_F32:
        return encode_f32(vals)
    if ggml_type == GGML_F16:
        return encode_f16(vals)
    if ggml_type == GGML_BF16:
        return encode_bf16(vals)
    if ggml_type == GGML_Q4_0:
        return encode_q4_0(vals)
    if ggml_type == GGML_Q8_0:
        return encode_q8_0(vals)
    if ggml_type == GGML_Q4_K:
        return encode_q4_k(vals)
    if ggml_type == GGML_Q6_K:
        return encode_q6_k(vals)
    raise ValueError(f"Unsupported GGML type {ggml_type}")


def decode_tensor(raw: bytes, dims: list[int], ggml_type: int) -> list[float]:
    numel = 1
    for d in dims:
        numel *= d
    if ggml_type == GGML_F32:
        return decode_f32(raw, numel)
    if ggml_type == GGML_F16:
        return decode_f16(raw, numel)
    if ggml_type == GGML_BF16:
        return decode_bf16(raw, numel)
    if ggml_type == GGML_Q4_0:
        return decode_q4_0(raw, numel)
    if ggml_type == GGML_Q8_0:
        return decode_q8_0(raw, numel)
    if ggml_type == GGML_Q4_K:
        return decode_q4_k(raw, numel)
    if ggml_type == GGML_Q6_K:
        return decode_q6_k(raw, numel)
    raise ValueError(f"Unsupported GGML type {ggml_type}")


def _gguf_str(s: str) -> bytes:
    b = s.encode("utf-8")
    return struct.pack("<Q", len(b)) + b


def _gguf_kv(key: str, vtype: int, val) -> bytes:
    buf = bytearray(_gguf_str(key))
    buf += struct.pack("<I", vtype)
    if vtype == TYPE_U32:
        buf += struct.pack("<I", int(val))
    elif vtype == TYPE_I32:
        buf += struct.pack("<i", int(val))
    elif vtype == TYPE_F32:
        buf += struct.pack("<f", float(val))
    elif vtype == TYPE_BOOL:
        buf += struct.pack("<B", 1 if val else 0)
    elif vtype == TYPE_STR:
        buf += _gguf_str(val)
    elif vtype == TYPE_ARR:
        elem_t, items = val
        buf += struct.pack("<IQ", elem_t, len(items))
        if elem_t == TYPE_STR:
            for item in items:
                buf += _gguf_str(item)
        elif elem_t == TYPE_F32:
            buf += struct.pack(f"<{len(items)}f", *items)
        elif elem_t == TYPE_I32:
            buf += struct.pack(f"<{len(items)}i", *items)
        elif elem_t == TYPE_U32:
            buf += struct.pack(f"<{len(items)}I", *items)
        else:
            raise ValueError(f"Unsupported array elem_t {elem_t}")
    else:
        raise ValueError(f"Unsupported kv vtype {vtype}")
    return bytes(buf)


def write_gguf_v3(path: str, kv_list: list[tuple[str, int, object]], tensors: list[tuple[str, list[int], int, bytes]], align: int = 32) -> str:
    hdr = bytearray()
    hdr += struct.pack("<IIQQ", GGUF_MAGIC, 3, len(tensors), len(kv_list))
    for k, vt, val in kv_list:
        hdr += _gguf_kv(k, vt, val)

    cur_off = 0
    tensor_blobs = []
    for name, dims, ggml_type, payload in tensors:
        hdr += _gguf_str(name)
        hdr += struct.pack("<I", len(dims))
        for d in dims:
            hdr += struct.pack("<Q", d)
        hdr += struct.pack("<IQ", ggml_type, cur_off)
        pad = (-len(payload)) % align
        tensor_blobs.append(payload + (b"\x00" * pad))
        cur_off += len(payload) + pad

    pad_meta = (-len(hdr)) % align
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(hdr)
        if pad_meta:
            f.write(b"\x00" * pad_meta)
        for blob in tensor_blobs:
            f.write(blob)

    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def build_gpt2_bpe_vocab(vocab_size: int = 384) -> tuple[list[str], list[int], list[str], int, int]:
    b2u, _ = gpt2_bytes_to_unicode()
    tokens = [b2u[b] for b in range(256)]
    tok_types = [1] * 256

    specials = ["<|endoftext|>", "<|im_start|>", "<|im_end|>"]
    for sp in specials:
        tokens.append(sp)
        tok_types.append(3)

    eos_id = 256
    bos_id = 257

    tok_to_id = {s: idx for idx, s in enumerate(tokens)}
    merges: list[str] = []

    target_words = [
        "user", "assistant", "\n",
        "ZCC", " compiles", " Blackwell", " GPU", " Qwen2", " Tensor", " Core",
        " PTX", " CUDA", " Graph", " hello", " world", " token", " zero",
        " toolchain", " sovereign", " matrix", " verify", " benchmark",
    ]
    for raw_w in target_words:
        if raw_w == "\n":
            continue
        raw_bytes = raw_w.encode("utf-8")
        syms = [b2u[b] for b in raw_bytes]
        while len(syms) > 1 and len(tokens) < vocab_size:
            a, b = syms[0], syms[1]
            ab = a + b
            if a in tok_to_id and b in tok_to_id:
                pair_str = f"{a} {b}"
                if pair_str not in merges and ab not in tok_to_id:
                    tok_to_id[ab] = len(tokens)
                    tokens.append(ab)
                    tok_types.append(1)
                    merges.append(pair_str)
            syms = [ab] + syms[2:]

    filler = 0
    while len(tokens) < vocab_size:
        t_str = f"zcc_tok_{filler}"
        tokens.append(t_str)
        tok_types.append(1)
        filler += 1

    return tokens, tok_types, merges, bos_id, eos_id


def build_spm_vocab(vocab_size: int = 384) -> tuple[list[str], list[float], list[int], int, int]:
    tokens = ["<unk>", "<s>", "</s>"]
    scores = [-100.0, 0.0, 0.0]
    tok_types = [2, 3, 3]
    bos_id, eos_id = 1, 2

    for b in range(256):
        tokens.append(f"<0x{b:02X}>")
        scores.append(0.0)
        tok_types.append(6)

    seen = set(tokens)
    tokens.append("\u2581")
    scores.append(1.0)
    tok_types.append(1)
    seen.add("\u2581")

    for ch in "etaoinsrhldcumfpgwybvkxjqzETAOINSRHLDCUMFPGWYBVKXJQZ0123456789.,!?:-_/\n":
        if len(tokens) < vocab_size and ch not in seen:
            tokens.append(ch)
            scores.append(1.5)
            tok_types.append(1)
            seen.add(ch)

    spm_words = [
        "\u2581ZCC", "\u2581Blackwell", "\u2581Llama", "\u2581PTX", "\u2581Q4_0",
        "\u2581RTX", "\u2581hello", "\u2581world", "\u2581compiles", "\u2581GPU",
    ]
    base_sc = 10.0
    for w in spm_words:
        cur = w[0]
        for ch in w[1:]:
            cur = cur + ch
            if cur not in seen and len(tokens) < vocab_size:
                tokens.append(cur)
                scores.append(base_sc)
                tok_types.append(1)
                seen.add(cur)
                base_sc += 0.25

    idx = 0
    while len(tokens) < vocab_size:
        s = f"\u2581sp{idx}"
        if s not in seen:
            tokens.append(s)
            scores.append(-5.0)
            tok_types.append(1)
            seen.add(s)
        idx += 1

    return tokens, scores, tok_types, bos_id, eos_id


def _det_vec(tag: str, n: int, scale: float) -> list[float]:
    seed = int.from_bytes(hashlib.sha256(tag.encode("utf-8")).digest()[:8], "little")
    rng = random.Random(seed)
    return [rng.uniform(-scale, scale) for _ in range(n)]


def _det_norm(tag: str, n: int) -> list[float]:
    seed = int.from_bytes(hashlib.sha256(tag.encode("utf-8")).digest()[:8], "little")
    rng = random.Random(seed)
    return [rng.uniform(0.85, 1.15) for _ in range(n)]


def generate_all_v2_fixtures(out_dir: str = BUILD_DIR) -> dict[str, dict]:
    dim = 256
    hidden = 512
    n_heads = 4
    n_kv_heads = 2
    head_dim = dim // n_heads
    kv_dim = n_kv_heads * head_dim
    vocab = 384

    bpe_toks, bpe_types, bpe_merges, bpe_bos, bpe_eos = build_gpt2_bpe_vocab(vocab)
    spm_toks, spm_scores, spm_types, spm_bos, spm_eos = build_spm_vocab(vocab)

    specs = [
        {
            "filename": "zcc_hf_qwen2_20l_q4km.gguf",
            "arch": "qwen2",
            "n_layers": 20,
            "max_seq": 512,
            "rms_eps": 1e-6,
            "rope_theta": 1000000.0,
            "tok_mode": "gpt2",
            "t_emb": GGML_Q8_0,
            "t_mat": GGML_Q4_K,
            "t_down": GGML_Q6_K,
            "t_out": GGML_Q6_K,
            "has_bias": True,
        },
        {
            "filename": "zcc_hf_llama_spm_q4_0.gguf",
            "arch": "llama",
            "n_layers": 2,
            "max_seq": 256,
            "rms_eps": 1e-5,
            "rope_theta": 10000.0,
            "tok_mode": "llama",
            "t_emb": GGML_Q4_0,
            "t_mat": GGML_Q4_0,
            "t_down": GGML_Q4_0,
            "t_out": GGML_Q4_0,
            "has_bias": False,
        },
        {
            "filename": "zcc_hf_llama_bf16.gguf",
            "arch": "llama",
            "n_layers": 2,
            "max_seq": 256,
            "rms_eps": 1e-5,
            "rope_theta": 10000.0,
            "tok_mode": "gpt2",
            "t_emb": GGML_BF16,
            "t_mat": GGML_BF16,
            "t_down": GGML_BF16,
            "t_out": GGML_BF16,
            "has_bias": False,
        },
    ]

    results = {}
    for sp in specs:
        arch = sp["arch"]
        n_layers = sp["n_layers"]
        scale_mat = 0.08 if n_layers <= 4 else 0.045
        kvs = [
            ("general.architecture", TYPE_STR, arch),
            ("general.alignment", TYPE_U32, 32),
            (f"{arch}.block_count", TYPE_U32, n_layers),
            (f"{arch}.embedding_length", TYPE_U32, dim),
            (f"{arch}.feed_forward_length", TYPE_U32, hidden),
            (f"{arch}.attention.head_count", TYPE_U32, n_heads),
            (f"{arch}.attention.head_count_kv", TYPE_U32, n_kv_heads),
            (f"{arch}.context_length", TYPE_U32, sp["max_seq"]),
            (f"{arch}.attention.layer_norm_rms_epsilon", TYPE_F32, sp["rms_eps"]),
            (f"{arch}.rope.freq_base", TYPE_F32, sp["rope_theta"]),
        ]
        if sp["tok_mode"] == "gpt2":
            kvs.extend([
                ("tokenizer.ggml.model", TYPE_STR, "gpt2"),
                ("tokenizer.ggml.tokens", TYPE_ARR, (TYPE_STR, bpe_toks)),
                ("tokenizer.ggml.token_type", TYPE_ARR, (TYPE_I32, bpe_types)),
                ("tokenizer.ggml.merges", TYPE_ARR, (TYPE_STR, bpe_merges)),
                ("tokenizer.ggml.bos_token_id", TYPE_U32, bpe_bos),
                ("tokenizer.ggml.eos_token_id", TYPE_U32, bpe_eos),
                ("tokenizer.ggml.add_bos_token", TYPE_BOOL, False),
            ])
        else:
            kvs.extend([
                ("tokenizer.ggml.model", TYPE_STR, "llama"),
                ("tokenizer.ggml.tokens", TYPE_ARR, (TYPE_STR, spm_toks)),
                ("tokenizer.ggml.scores", TYPE_ARR, (TYPE_F32, spm_scores)),
                ("tokenizer.ggml.token_type", TYPE_ARR, (TYPE_I32, spm_types)),
                ("tokenizer.ggml.bos_token_id", TYPE_U32, spm_bos),
                ("tokenizer.ggml.eos_token_id", TYPE_U32, spm_eos),
                ("tokenizer.ggml.add_bos_token", TYPE_BOOL, True),
            ])

        tag0 = sp["filename"]
        tensors: list[tuple[str, list[int], int, bytes]] = []
        tensors.append(("token_embd.weight", [dim, vocab], sp["t_emb"], encode_tensor(_det_vec(f"{tag0}:emb", vocab * dim, 0.35), sp["t_emb"])))
        for l in range(n_layers):
            tensors.append((f"blk.{l}.attn_norm.weight", [dim], GGML_F32, encode_f32(_det_norm(f"{tag0}:an:{l}", dim))))
            tensors.append((f"blk.{l}.attn_q.weight", [dim, dim], sp["t_mat"], encode_tensor(_det_vec(f"{tag0}:wq:{l}", dim * dim, scale_mat), sp["t_mat"])))
            tensors.append((f"blk.{l}.attn_k.weight", [dim, kv_dim], sp["t_mat"], encode_tensor(_det_vec(f"{tag0}:wk:{l}", kv_dim * dim, scale_mat), sp["t_mat"])))
            tensors.append((f"blk.{l}.attn_v.weight", [dim, kv_dim], sp["t_mat"], encode_tensor(_det_vec(f"{tag0}:wv:{l}", kv_dim * dim, scale_mat), sp["t_mat"])))
            if sp["has_bias"]:
                tensors.append((f"blk.{l}.attn_q.bias", [dim], GGML_F32, encode_f32(_det_vec(f"{tag0}:bq:{l}", dim, 0.25))))
                tensors.append((f"blk.{l}.attn_k.bias", [kv_dim], GGML_F32, encode_f32(_det_vec(f"{tag0}:bk:{l}", kv_dim, 0.25))))
                tensors.append((f"blk.{l}.attn_v.bias", [kv_dim], GGML_F32, encode_f32(_det_vec(f"{tag0}:bv:{l}", kv_dim, 0.25))))
            tensors.append((f"blk.{l}.attn_output.weight", [dim, dim], sp["t_mat"], encode_tensor(_det_vec(f"{tag0}:wo:{l}", dim * dim, scale_mat), sp["t_mat"])))
            tensors.append((f"blk.{l}.ffn_norm.weight", [dim], GGML_F32, encode_f32(_det_norm(f"{tag0}:fn:{l}", dim))))
            tensors.append((f"blk.{l}.ffn_gate.weight", [dim, hidden], sp["t_mat"], encode_tensor(_det_vec(f"{tag0}:wg:{l}", hidden * dim, scale_mat), sp["t_mat"])))
            tensors.append((f"blk.{l}.ffn_up.weight", [dim, hidden], sp["t_mat"], encode_tensor(_det_vec(f"{tag0}:wu:{l}", hidden * dim, scale_mat), sp["t_mat"])))
            tensors.append((f"blk.{l}.ffn_down.weight", [hidden, dim], sp["t_down"], encode_tensor(_det_vec(f"{tag0}:wd:{l}", dim * hidden, scale_mat), sp["t_down"])))

        tensors.append(("output_norm.weight", [dim], GGML_F32, encode_f32(_det_norm(f"{tag0}:on", dim))))
        tensors.append(("output.weight", [dim, vocab], sp["t_out"], encode_tensor(_det_vec(f"{tag0}:wout", vocab * dim, 0.22), sp["t_out"])))

        out_p = os.path.join(out_dir, sp["filename"])
        sha = write_gguf_v3(out_p, kvs, tensors, align=32)
        results[sp["filename"]] = {"path": out_p, "bytes": os.path.getsize(out_p), "sha256": sha}
        print(f"[GGUF-V2-GEN] {out_p}: {results[sp['filename']]['bytes']} bytes, SHA256={sha}")
    return results


# ========================================================================= #
# Independent Pure-Python GGUF Parser, Tokenizer & Reference Forward Oracle #
# ========================================================================= #
def _rd_u32(buf: bytes, off: int) -> tuple[int, int]:
    return struct.unpack_from("<I", buf, off)[0], off + 4


def _rd_u64(buf: bytes, off: int) -> tuple[int, int]:
    return struct.unpack_from("<Q", buf, off)[0], off + 8


def _rd_str(buf: bytes, off: int) -> tuple[str, int]:
    ln, off = _rd_u64(buf, off)
    return buf[off : off + ln].decode("utf-8", errors="replace"), off + ln


def _rd_val(buf: bytes, off: int, vt: int):
    if vt in (0, 1, 7):
        return buf[off], off + 1
    if vt in (2, 3):
        return struct.unpack_from("<H", buf, off)[0], off + 2
    if vt == 4:
        return struct.unpack_from("<I", buf, off)[0], off + 4
    if vt == 5:
        return struct.unpack_from("<i", buf, off)[0], off + 4
    if vt == 6:
        return struct.unpack_from("<f", buf, off)[0], off + 4
    if vt == 8:
        return _rd_str(buf, off)
    if vt == 9:
        et, off = _rd_u32(buf, off)
        cnt, off = _rd_u64(buf, off)
        items = []
        for _ in range(cnt):
            val, off = _rd_val(buf, off, et)
            items.append(val)
        return items, off
    if vt in (10, 11, 12):
        return _rd_u64(buf, off)
    raise ValueError(f"Unsupported GGUF value type {vt}")


def _ggml_nbytes(dims: list[int], t: int) -> int:
    numel = 1
    for d in dims:
        numel *= d
    if t == GGML_F32:
        return numel * 4
    if t in (GGML_F16, GGML_BF16):
        return numel * 2
    if t == GGML_Q4_0:
        return (numel // 32) * 18
    if t == GGML_Q8_0:
        return (numel // 32) * 34
    if t == GGML_Q4_K:
        return (numel // 256) * 144
    if t == GGML_Q6_K:
        return (numel // 256) * 210
    raise ValueError(f"Unsupported type {t}")


def load_gguf_v2(path: str) -> dict:
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
    for name, dims, gt, toff in t_info:
        nb = _ggml_nbytes(dims, gt)
        tensors[name] = decode_tensor(buf[data_start + toff : data_start + toff + nb], dims, gt)
    return {"kvs": kvs, "tensors": tensors}


def _next_bpe_pretoken_len(s: bytes) -> int:
    c0 = s[0]
    n = len(s)
    if c0 in (10, 13):
        p = 0
        while p < n and s[p] in (10, 13):
            p += 1
        return p
    if c0 == ord("'") and n >= 2:
        l1 = chr(s[1]).lower()
        if l1 in ("s", "t", "m", "d") and (n == 2 or not (chr(s[2]).isalpha() or s[2] >= 0x80)):
            return 2
        if n >= 3:
            l2 = chr(s[2]).lower()
            if (l1 + l2) in ("re", "ve", "ll") and (n == 3 or not (chr(s[3]).isalpha() or s[3] >= 0x80)):
                return 3
    is_let = lambda b: (65 <= b <= 90) or (97 <= b <= 122) or b >= 0x80
    is_dig = lambda b: 48 <= b <= 57
    is_spc = lambda b: b in (32, 9, 10, 13, 11, 12)
    if is_let(c0):
        p = 1
        while p < n and is_let(s[p]):
            p += 1
        return p
    if c0 not in (10, 13) and not is_let(c0) and not is_dig(c0) and n >= 2 and is_let(s[1]):
        p = 2
        while p < n and is_let(s[p]):
            p += 1
        return p
    if is_dig(c0):
        return 1
    if c0 == 32 and n >= 2:
        c1 = s[1]
        if not is_spc(c1) and not is_dig(c1):
            p = 2
            while p < n and not is_spc(s[p]) and not is_let(s[p]) and not is_dig(s[p]):
                p += 1
            while p < n and s[p] in (10, 13):
                p += 1
            return p
    if is_spc(c0):
        p = 0
        while p < n and is_spc(s[p]) and s[p] not in (10, 13):
            p += 1
        if p < n and s[p] in (10, 13):
            while p < n and s[p] in (10, 13):
                p += 1
            return p
        if p > 1 and p < n and not is_dig(s[p]):
            return p - 1
        return p
    p = 1
    while p < n and not is_spc(s[p]) and not is_let(s[p]) and not is_dig(s[p]):
        p += 1
    while p < n and s[p] in (10, 13):
        p += 1
    return p


def reference_tokenize_gguf(gguf_path: str, text: str, no_bos: bool = False, chat: bool = False) -> list[int]:
    model = load_gguf_v2(gguf_path)
    kvs = model["kvs"]
    arch = kvs.get("general.architecture", "llama")
    tmodel = kvs.get("tokenizer.ggml.model", "")
    tokens = kvs.get("tokenizer.ggml.tokens", [])
    tok_to_id = {}
    for idx, s in enumerate(tokens):
        if s not in tok_to_id:
            tok_to_id[s] = idx

    if chat:
        if arch == "qwen2" or "<|im_start|>" in tok_to_id:
            text = f"<|im_start|>user\n{text}<|im_end|>\n<|im_start|>assistant\n"
        else:
            text = f"[INST] {text} [/INST]"

    if tmodel not in ("gpt2", "llama"):
        vocab = kvs.get("tokenizer.ggml.tokens", [])
        vlen = len(vocab) if vocab else 128
        return [b if b < vlen else 32 for b in text.encode("utf-8")]

    tok_types = kvs.get("tokenizer.ggml.token_type", [])
    bos_id = kvs.get("tokenizer.ggml.bos_token_id")
    eos_id = kvs.get("tokenizer.ggml.eos_token_id")
    eot_id = kvs.get("tokenizer.ggml.eot_token_id")
    add_bos = kvs.get("tokenizer.ggml.add_bos_token")
    if add_bos is None:
        add_bos = 1 if (tmodel == "llama" and bos_id is not None) else 0

    out_ids = []
    if add_bos and not no_bos and bos_id is not None:
        out_ids.append(int(bos_id))

    specials = []
    for idx, s in enumerate(tokens):
        if not s:
            continue
        is_sp = (idx < len(tok_types) and tok_types[idx] == 3) or idx in (bos_id, eos_id, eot_id)
        if is_sp:
            specials.append((s, idx))
    specials.sort(key=lambda kv: (-len(kv[0]), kv[1]))

    b2u, _ = gpt2_bytes_to_unicode()
    merge_ranks = {}
    for r, ms in enumerate(kvs.get("tokenizer.ggml.merges", [])):
        if " " in ms:
            a, b = ms.split(" ", 1)
            ida, idb = tok_to_id.get(a), tok_to_id.get(b)
            if ida is not None and idb is not None and (ida, idb) not in merge_ranks:
                merge_ranks[(ida, idb)] = r
    scores = kvs.get("tokenizer.ggml.scores", [])

    def encode_plain(seg: str):
        if tmodel == "gpt2":
            raw = seg.encode("utf-8")
            p = 0
            while p < len(raw):
                wl = _next_bpe_pretoken_len(raw[p:])
                ids = [tok_to_id[b2u[b]] for b in raw[p : p + wl] if b2u[b] in tok_to_id]
                while len(ids) > 1:
                    best = None
                    for i in range(len(ids) - 1):
                        rk = merge_ranks.get((ids[i], ids[i + 1]))
                        if rk is not None and (best is None or rk < best[0]):
                            best = (rk, i)
                    if best is None:
                        break
                    _, idx = best
                    merged = tok_to_id.get(tokens[ids[idx]] + tokens[ids[idx + 1]])
                    if merged is None:
                        break
                    ids = ids[:idx] + [merged] + ids[idx + 2 :]
                out_ids.extend(ids)
                p += wl
        else:
            tstr = "\u2581" + seg.replace(" ", "\u2581")
            ids = []
            for ch in tstr:
                if ch in tok_to_id:
                    ids.append(tok_to_id[ch])
                else:
                    for b in ch.encode("utf-8"):
                        ids.append(tok_to_id.get(f"<0x{b:02X}>", 0))
            while len(ids) > 1:
                best = None
                for i in range(len(ids) - 1):
                    mid = tok_to_id.get(tokens[ids[i]] + tokens[ids[i + 1]])
                    if mid is not None and not (mid < len(tok_types) and tok_types[mid] == 3):
                        sc = scores[mid] if mid < len(scores) else 0.0
                        if best is None or sc > best[0]:
                            best = (sc, i, mid)
                if best is None:
                    break
                _, idx, mid = best
                ids = ids[:idx] + [mid] + ids[idx + 2 :]
            out_ids.extend(ids)

    pos = 0
    seg_start = 0
    n = len(text)
    while pos <= n:
        hit = None
        if pos < n:
            for sp_s, sp_id in specials:
                if text.startswith(sp_s, pos):
                    hit = (sp_s, sp_id)
                    break
        if pos == n or hit is not None:
            if pos > seg_start:
                encode_plain(text[seg_start:pos])
            if hit is not None:
                out_ids.append(hit[1])
                pos += len(hit[0])
                seg_start = pos
                continue
            break
        pos += 1
    return out_ids


def _rmsnorm(x: list[float], w: list[float], eps: float) -> list[float]:
    ms = sum(v * v for v in x) / len(x)
    scale = 1.0 / math.sqrt(ms + eps)
    return [xv * scale * wv for xv, wv in zip(x, w)]


def _matvec_rows(rows: list[list[float]], x: list[float]) -> list[float]:
    mul = operator.mul
    return [sum(map(mul, row, x)) for row in rows]


def _slice_rows(flat: list[float], n_rows: int, n_cols: int) -> list[list[float]]:
    return [flat[r * n_cols : (r + 1) * n_cols] for r in range(n_rows)]


def _rope(vec: list[float], pos: int, n_heads: int, head_dim: int, theta: float, neox: bool = False) -> None:
    half = head_dim >> 1
    for h in range(n_heads):
        base = h * head_dim
        for p in range(half):
            ang = pos * (theta ** (-float(2 * p) / float(head_dim)))
            ca, sa = math.cos(ang), math.sin(ang)
            i0 = (base + p) if neox else (base + 2 * p)
            i1 = (base + p + half) if neox else (base + 2 * p + 1)
            v0, v1 = vec[i0], vec[i1]
            vec[i0] = v0 * ca - v1 * sa
            vec[i1] = v0 * sa + v1 * ca


def reference_forward_gguf_v2(gguf_path: str, prompt_ids: list[int], steps: int, ignore_eos: bool = False) -> dict:
    mdl = load_gguf_v2(gguf_path)
    kvs, T = mdl["kvs"], mdl["tensors"]
    arch = kvs["general.architecture"]
    n_layers = kvs[f"{arch}.block_count"]
    dim = kvs[f"{arch}.embedding_length"]
    hidden = kvs[f"{arch}.feed_forward_length"]
    n_heads = kvs[f"{arch}.attention.head_count"]
    n_kv_heads = kvs[f"{arch}.attention.head_count_kv"]
    rms_eps = kvs.get(f"{arch}.attention.layer_norm_rms_epsilon", 1e-5)
    rope_theta = kvs.get(f"{arch}.rope.freq_base", 10000.0)
    head_dim = dim // n_heads
    kv_dim = n_kv_heads * head_dim
    kv_group = n_heads // n_kv_heads
    vocab = len(T["token_embd.weight"]) // dim

    tmodel = kvs.get("tokenizer.ggml.model", "")
    eos_id = kvs.get("tokenizer.ggml.eos_token_id")
    eot_id = kvs.get("tokenizer.ggml.eot_token_id")

    emb_rows = _slice_rows(T["token_embd.weight"], vocab, dim)
    out_norm = T["output_norm.weight"]
    wout_rows = _slice_rows(T["output.weight"], vocab, dim) if "output.weight" in T else emb_rows

    layer_weights = []
    for l in range(n_layers):
        layer_weights.append({
            "an": T[f"blk.{l}.attn_norm.weight"],
            "wq": _slice_rows(T[f"blk.{l}.attn_q.weight"], dim, dim),
            "wk": _slice_rows(T[f"blk.{l}.attn_k.weight"], kv_dim, dim),
            "wv": _slice_rows(T[f"blk.{l}.attn_v.weight"], kv_dim, dim),
            "bq": T.get(f"blk.{l}.attn_q.bias"),
            "bk": T.get(f"blk.{l}.attn_k.bias"),
            "bv": T.get(f"blk.{l}.attn_v.bias"),
            "wo": _slice_rows(T[f"blk.{l}.attn_output.weight"], dim, dim),
            "fn": T[f"blk.{l}.ffn_norm.weight"],
            "wg": _slice_rows(T[f"blk.{l}.ffn_gate.weight"], hidden, dim),
            "wu": _slice_rows(T[f"blk.{l}.ffn_up.weight"], hidden, dim),
            "wd": _slice_rows(T[f"blk.{l}.ffn_down.weight"], dim, hidden),
        })

    k_cache: list[list[list[float]]] = [[] for _ in range(n_layers)]
    v_cache: list[list[list[float]]] = [[] for _ in range(n_layers)]
    inv_sqrt_hd = 1.0 / math.sqrt(head_dim)
    mul = operator.mul

    def step_tok(tok: int, pos: int) -> list[float]:
        x = list(emb_rows[tok])
        for l in range(n_layers):
            lw = layer_weights[l]
            xb = _rmsnorm(x, lw["an"], rms_eps)
            q = _matvec_rows(lw["wq"], xb)
            k = _matvec_rows(lw["wk"], xb)
            v = _matvec_rows(lw["wv"], xb)
            if lw["bq"] is not None:
                q = [a + b for a, b in zip(q, lw["bq"])]
            if lw["bk"] is not None:
                k = [a + b for a, b in zip(k, lw["bk"])]
            if lw["bv"] is not None:
                v = [a + b for a, b in zip(v, lw["bv"])]
            _rope(q, pos, n_heads, head_dim, rope_theta, neox=(arch == "qwen2"))
            _rope(k, pos, n_kv_heads, head_dim, rope_theta, neox=(arch == "qwen2"))
            k_cache[l].append(k)
            v_cache[l].append(v)

            att_out = [0.0] * dim
            kc_l = k_cache[l]
            vc_l = v_cache[l]
            for h in range(n_heads):
                qh = q[h * head_dim : (h + 1) * head_dim]
                kv_h = h // kv_group
                ks = kv_h * head_dim
                ke = ks + head_dim
                scores_t = [sum(map(mul, qh, kc_l[t][ks:ke])) * inv_sqrt_hd for t in range(pos + 1)]
                mx = max(scores_t)
                exps = [math.exp(s - mx) for s in scores_t]
                inv_s = 1.0 / sum(exps)
                probs = [e * inv_s for e in exps]
                oh = [0.0] * head_dim
                for t, pr in enumerate(probs):
                    vt = vc_l[t][ks:ke]
                    for d in range(head_dim):
                        oh[d] += pr * vt[d]
                att_out[h * head_dim : (h + 1) * head_dim] = oh

            o_proj = _matvec_rows(lw["wo"], att_out)
            x = [a + b for a, b in zip(x, o_proj)]

            xb2 = _rmsnorm(x, lw["fn"], rms_eps)
            hb = _matvec_rows(lw["wg"], xb2)
            hb2 = _matvec_rows(lw["wu"], xb2)
            act_ffn = [(g / (1.0 + math.exp(-g))) * u for g, u in zip(hb, hb2)]
            down = _matvec_rows(lw["wd"], act_ffn)
            x = [a + b for a, b in zip(x, down)]

        xf = _rmsnorm(x, out_norm, rms_eps)
        return _matvec_rows(wout_rows, xf)

    logits: list[float] = []
    pos = 0
    for pid in prompt_ids:
        logits = step_tok(pid, pos)
        pos += 1
    prompt_logits = list(logits)

    gen_ids: list[int] = []
    for _ in range(steps):
        nxt = max(range(vocab), key=lambda idx: logits[idx])
        if tmodel in ("gpt2", "llama") and not ignore_eos and nxt in (eos_id, eot_id):
            break
        gen_ids.append(nxt)
        logits = step_tok(nxt, pos)
        pos += 1
    return {"prompt_logits": prompt_logits, "generated_ids": gen_ids}


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Track 9 real-world GGUF fixtures (Q4_K/Q6_K/Q4_0/BF16, BPE/SPM).")
    parser.add_argument("--out-dir", default=BUILD_DIR)
    args = parser.parse_args()
    generate_all_v2_fixtures(args.out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
