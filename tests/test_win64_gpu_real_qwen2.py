#!/usr/bin/env python3
"""Track 11 Verification: Real Trained Qwen2.5 GGUFs on Sovereign ZCC Win64 + RTX 5070.

Tests compiler-emitted sm_120 PTX + Win64 nvcuda.dll runtime on real trained GGUFs:
  - H:\\models\\zkaedi-prime-dpo-q4_k_m.gguf   (986 MB Qwen2.5-1.5B Q4_K/Q6_K/F32)
  - H:\\models\\zkaedi-trinity-dpo-f16.gguf   (3.09 GB Qwen2.5-1.5B F16 Tensor Core)
  - H:\\models\\zcc-mini-v7-q4_k_m.gguf       (4.68 GB >4 GiB Qwen2.5-7B Q4_K/Q6_K/F32)

Verifies dual-compiler PTX parity (including >4 GiB streaming), low-RAM streaming NumPy
1.5B differential oracle (logits + BPE + NeoX RoPE), live English/math/C generation,
multi-turn KV memory + /reset + --clean UX, F16 Tensor Core execution, AUTO_CTX
VRAM fitting, and ZCC-EF-0 negative controls.
"""

from __future__ import annotations

import os
import re
import struct
import subprocess
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
VENV_PYTHON = "/home/ZKAEDI/.venv/bin/python"

if __name__ == "__main__":
    try:
        import numpy as np  # noqa: F401
    except ImportError:
        if os.path.isfile(VENV_PYTHON) and os.path.realpath(sys.executable) != os.path.realpath(VENV_PYTHON):
            os.execv(VENV_PYTHON, [VENV_PYTHON] + sys.argv)
        raise

import numpy as np

if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_lua_win64 import (
    BUILD_DIR,
    CMD_EXE,
    ZCC_BIN,
    ZCC_WIN64,
    run_fc_b,
    sha256_file,
    to_rel_posix,
    to_rel_win,
)
from tools.build_gpu_win64 import audit_pe32plus_multi_dll
from tools.generate_gguf_track10_bench import _np_rmsnorm, _np_rope, np_decode_tensor
from tools.generate_gguf_v2_fixtures import (
    GGUF_MAGIC,
    _next_bpe_pretoken_len,
    _rd_str,
    _rd_u32,
    _rd_u64,
    gpt2_bytes_to_unicode,
)
from tests.test_win64_gpu_llm import (
    LLM_EXE,
    LLM_HOST_EXE,
    extract_prefixed_kv,
    extract_quoted_field,
    parse_token_ids,
    read_f32_bin,
)
from tests.test_win64_gpu_track10 import _extract_all_prefixed_kv


MODELS = {
    "prime_1_5b": {
        "wsl": "/mnt/h/models/zkaedi-prime-dpo-q4_k_m.gguf",
        "win": r"H:\models\zkaedi-prime-dpo-q4_k_m.gguf",
        "ptx_host": os.path.join(BUILD_DIR, "zkaedi_prime_1_5b_sm120_host.ptx"),
        "ptx_win": os.path.join(BUILD_DIR, "zkaedi_prime_1_5b_sm120.ptx"),
        "expect_f16_tc": False,
        "expect_unused": ("_bf16", "_q4_0"),
        "expect_used": ("q4_k", "q6_k"),
    },
    "trinity_1_5b_f16": {
        "wsl": "/mnt/h/models/zkaedi-trinity-dpo-f16.gguf",
        "win": r"H:\models\zkaedi-trinity-dpo-f16.gguf",
        "ptx_host": os.path.join(BUILD_DIR, "zkaedi_trinity_1_5b_f16_sm120_host.ptx"),
        "ptx_win": os.path.join(BUILD_DIR, "zkaedi_trinity_1_5b_f16_sm120.ptx"),
        "expect_f16_tc": True,
        "expect_unused": ("_q4_k", "_q6_k", "_q8_0", "_q4_0", "_bf16"),
        "expect_used": ("zcc_aot_mc_gemm_tc_f16", "zcc_aot_mc_embed_f16", "zcc_aot_mc_prefill_tc_gemm"),
    },
    "mini_7b": {
        "wsl": "/mnt/h/models/zcc-mini-v7-q4_k_m.gguf",
        "win": r"H:\models\zcc-mini-v7-q4_k_m.gguf",
        "ptx_host": os.path.join(BUILD_DIR, "zcc_mini_7b_sm120_host.ptx"),
        "ptx_win": os.path.join(BUILD_DIR, "zcc_mini_7b_sm120.ptx"),
        "expect_f16_tc": False,
        "expect_unused": ("_bf16", "_q4_0"),
        "expect_used": ("q4_k", "q6_k"),
    },
}

LLAMA_TIERB_PTX = os.path.join(BUILD_DIR, "zcc_hf_llama_spm_q4_0_sm120.ptx")
OUT_BIN_DIR = os.path.join(BUILD_DIR, "track11_real_runs")


def _run_host_ptx(model_wsl: str, out_ptx: str, timeout_s: int = 300) -> subprocess.CompletedProcess:
    cmd = [
        ZCC_BIN,
        "--target=ptx",
        "--gpu-arch=sm_120",
        "--model",
        model_wsl,
        "-o",
        to_rel_posix(out_ptx),
    ]
    return subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
        errors="replace",
        timeout=timeout_s,
    )


def _run_win64_ptx(model_win: str, out_ptx: str, timeout_s: int = 300) -> subprocess.CompletedProcess:
    cmd = [
        CMD_EXE,
        "/c",
        to_rel_win(ZCC_WIN64),
        "--target=ptx",
        "--gpu-arch=sm_120",
        "--model",
        model_win,
        "-o",
        to_rel_win(out_ptx),
    ]
    return subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
        errors="replace",
        timeout=timeout_s,
    )


def _run_llm(args: list[str], stdin_text: str | None = None, timeout_s: int = 180) -> subprocess.CompletedProcess:
    if sys.platform == "win32":
        cmd = [LLM_EXE] + list(args)
    else:
        cmd = [CMD_EXE, "/c", to_rel_win(LLM_EXE)] + list(args)
    return subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        input=stdin_text,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
        errors="replace",
        timeout=timeout_s,
    )


def _ggml_row_nbytes(cols: int, ggml_type: int) -> int:
    if ggml_type == 0:
        return cols * 4
    if ggml_type in (1, 30):
        return cols * 2
    if ggml_type == 2:
        return (cols // 32) * 18
    if ggml_type == 8:
        return (cols // 32) * 34
    if ggml_type == 12:
        return (cols // 256) * 144
    if ggml_type == 14:
        return (cols // 256) * 210
    raise ValueError(f"Unsupported ggml_type={ggml_type}")


def _ggml_tensor_nbytes(dims: list[int], ggml_type: int) -> int:
    rows = 1
    for d in dims[1:]:
        rows *= d
    return rows * _ggml_row_nbytes(dims[0], ggml_type)


class StreamingGgufReader:
    """Fast chunked GGUF v3 header parser + on-demand single-tensor disk reader (<120 MB RAM)."""

    def __init__(self, path: str) -> None:
        self.path = path
        self._fp = open(path, "rb")
        self.kvs, self.tensors = self._parse_header()

    def close(self) -> None:
        if self._fp is not None:
            self._fp.close()
            self._fp = None

    def __enter__(self) -> "StreamingGgufReader":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def _parse_header(self) -> tuple[dict, dict[str, tuple[list[int], int, int, int]]]:
        buf = bytearray(self._fp.read(16 * 1024 * 1024))
        off = 0

        def ensure(n: int) -> None:
            while off + n > len(buf):
                chunk = self._fp.read(max(8 * 1024 * 1024, (off + n) - len(buf)))
                if not chunk:
                    raise EOFError("Unexpected EOF while parsing GGUF header")
                buf.extend(chunk)

        def rd_u32() -> int:
            nonlocal off
            ensure(4)
            v = struct.unpack_from("<I", buf, off)[0]
            off += 4
            return v

        def rd_u64() -> int:
            nonlocal off
            ensure(8)
            v = struct.unpack_from("<Q", buf, off)[0]
            off += 8
            return v

        def rd_str() -> str:
            nonlocal off
            ln = rd_u64()
            ensure(ln)
            s = bytes(buf[off : off + ln]).decode("utf-8", errors="replace")
            off += ln
            return s

        def rd_val(vt: int):
            nonlocal off
            if vt in (0, 1, 7):
                ensure(1)
                v = buf[off]
                off += 1
                return v
            if vt in (2, 3):
                ensure(2)
                v = struct.unpack_from("<H", buf, off)[0]
                off += 2
                return v
            if vt == 4:
                return rd_u32()
            if vt == 5:
                ensure(4)
                v = struct.unpack_from("<i", buf, off)[0]
                off += 4
                return v
            if vt == 6:
                ensure(4)
                v = struct.unpack_from("<f", buf, off)[0]
                off += 4
                return v
            if vt == 8:
                return rd_str()
            if vt == 9:
                et = rd_u32()
                cnt = rd_u64()
                if et == 8:
                    return [rd_str() for _ in range(cnt)]
                if et in (4, 5, 6):
                    ensure(4 * cnt)
                    fmt = {4: "I", 5: "i", 6: "f"}[et]
                    vals = list(struct.unpack_from(f"<{cnt}{fmt}", buf, off))
                    off += 4 * cnt
                    return vals
                return [rd_val(et) for _ in range(cnt)]
            if vt in (10, 11, 12):
                return rd_u64()
            raise ValueError(f"Unsupported GGUF value type {vt}")

        ensure(24)
        magic, version, n_tensors, n_kv = struct.unpack_from("<IIQQ", buf, 0)
        off = 24
        if magic != GGUF_MAGIC or version != 3:
            raise ValueError(f"Unexpected GGUF header magic={magic:#x} version={version}")

        kvs: dict = {}
        for _ in range(n_kv):
            k = rd_str()
            vt = rd_u32()
            kvs[k] = rd_val(vt)

        t_info = []
        for _ in range(n_tensors):
            name = rd_str()
            nd = rd_u32()
            dims = [rd_u64() for _ in range(nd)]
            gt = rd_u32()
            rel_off = rd_u64()
            t_info.append((name, dims, gt, rel_off))

        align = int(kvs.get("general.alignment", 32))
        data_start = (off + align - 1) & ~(align - 1)
        tensors = {
            name: (dims, gt, data_start + rel_off, _ggml_tensor_nbytes(dims, gt))
            for name, dims, gt, rel_off in t_info
        }
        return kvs, tensors

    def read_tensor(self, name: str) -> np.ndarray:
        dims, gt, abs_off, nbytes = self.tensors[name]
        self._fp.seek(abs_off)
        raw = self._fp.read(nbytes)
        if len(raw) != nbytes:
            raise EOFError(f"Short read for {name}: {len(raw)} != {nbytes}")
        return np_decode_tensor(raw, dims, gt)

    def read_embedding_rows(self, token_ids: list[int]) -> np.ndarray:
        dims, gt, abs_off, _ = self.tensors["token_embd.weight"]
        cols = dims[0]
        rbytes = _ggml_row_nbytes(cols, gt)
        out = np.empty((len(token_ids), cols), dtype=np.float32)
        for idx, tid in enumerate(token_ids):
            self._fp.seek(abs_off + int(tid) * rbytes)
            raw = self._fp.read(rbytes)
            out[idx] = np_decode_tensor(raw, [cols], gt)
        return out

    def lm_head_matvec(self, x: np.ndarray, chunk_rows: int = 4096) -> np.ndarray:
        tname = "output.weight" if "output.weight" in self.tensors else "token_embd.weight"
        dims, gt, abs_off, _ = self.tensors[tname]
        cols, vocab = dims[0], dims[1]
        rbytes = _ggml_row_nbytes(cols, gt)
        logits = np.empty(vocab, dtype=np.float32)
        self._fp.seek(abs_off)
        row0 = 0
        while row0 < vocab:
            cnt = min(chunk_rows, vocab - row0)
            raw = self._fp.read(cnt * rbytes)
            if len(raw) != cnt * rbytes:
                raise EOFError(f"Short lm_head chunk read at row {row0}")
            mat = np_decode_tensor(raw, [cols, cnt], gt)
            logits[row0 : row0 + cnt] = mat @ x
            row0 += cnt
        return logits


def reference_tokenize_qwen2(kvs: dict, text: str) -> list[int]:
    tokens = kvs["tokenizer.ggml.tokens"]
    tok_types = kvs.get("tokenizer.ggml.token_type", [])
    tok_to_id = {s: i for i, s in enumerate(tokens) if s}

    specials = []
    for idx, s in enumerate(tokens):
        if not s:
            continue
        if idx < len(tok_types) and tok_types[idx] in (3, 4):
            specials.append((s, idx))
    specials.sort(key=lambda kv: (-len(kv[0]), kv[1]))

    b2u, _ = gpt2_bytes_to_unicode()
    merge_ranks: dict[tuple[int, int], int] = {}
    for rank, ms in enumerate(kvs.get("tokenizer.ggml.merges", [])):
        if " " in ms:
            a, b = ms.split(" ", 1)
            ida, idb = tok_to_id.get(a), tok_to_id.get(b)
            if ida is not None and idb is not None and (ida, idb) not in merge_ranks:
                merge_ranks[(ida, idb)] = rank

    out_ids: list[int] = []

    def encode_plain(seg: str) -> None:
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


def streaming_numpy_prefill_logits(gguf_path: str, prompt_chat: str) -> dict:
    with StreamingGgufReader(gguf_path) as gguf:
        kvs, T = gguf.kvs, gguf.tensors
        arch = kvs["general.architecture"]
        n_layers = int(kvs[f"{arch}.block_count"])
        dim = int(kvs[f"{arch}.embedding_length"])
        hidden = int(kvs[f"{arch}.feed_forward_length"])
        n_heads = int(kvs[f"{arch}.attention.head_count"])
        n_kv_heads = int(kvs[f"{arch}.attention.head_count_kv"])
        rms_eps = float(kvs.get(f"{arch}.attention.layer_norm_rms_epsilon", 1e-6))
        rope_theta = float(kvs.get(f"{arch}.rope.freq_base", 1000000.0))
        head_dim = dim // n_heads
        kv_group = n_heads // n_kv_heads
        neox = (arch == "qwen2")

        prompt_ids = reference_tokenize_qwen2(kvs, prompt_chat)
        seq_len = len(prompt_ids)
        X = gguf.read_embedding_rows(prompt_ids)

        for l in range(n_layers):
            an = gguf.read_tensor(f"blk.{l}.attn_norm.weight")
            xb = np.vstack([_np_rmsnorm(X[t], an, rms_eps) for t in range(seq_len)])

            wq = gguf.read_tensor(f"blk.{l}.attn_q.weight")
            Q = (xb @ wq.T).astype(np.float32)
            del wq
            if f"blk.{l}.attn_q.bias" in T:
                Q = Q + gguf.read_tensor(f"blk.{l}.attn_q.bias")

            wk = gguf.read_tensor(f"blk.{l}.attn_k.weight")
            K = (xb @ wk.T).astype(np.float32)
            del wk
            if f"blk.{l}.attn_k.bias" in T:
                K = K + gguf.read_tensor(f"blk.{l}.attn_k.bias")

            wv = gguf.read_tensor(f"blk.{l}.attn_v.weight")
            V = (xb @ wv.T).astype(np.float32)
            del wv
            if f"blk.{l}.attn_v.bias" in T:
                V = V + gguf.read_tensor(f"blk.{l}.attn_v.bias")

            for t in range(seq_len):
                _np_rope(Q[t], K[t], t, n_heads, n_kv_heads, head_dim, rope_theta, neox=neox)

            Qh = Q.reshape(seq_len, n_heads, head_dim)
            Kh = K.reshape(seq_len, n_kv_heads, head_dim)
            Vh = V.reshape(seq_len, n_kv_heads, head_dim)
            att_out = np.zeros((seq_len, dim), dtype=np.float32)
            scale = np.float32(1.0 / np.sqrt(float(head_dim)))

            for t in range(seq_len):
                out_t = np.zeros((n_heads, head_dim), dtype=np.float32)
                for h in range(n_heads):
                    kvh = h // kv_group
                    scores = (Kh[: t + 1, kvh, :] @ Qh[t, h, :]) * scale
                    mx = float(np.max(scores))
                    probs = np.exp(scores - mx).astype(np.float32)
                    probs /= np.sum(probs, dtype=np.float32)
                    out_t[h] = probs @ Vh[: t + 1, kvh, :]
                att_out[t] = out_t.reshape(dim)

            wo = gguf.read_tensor(f"blk.{l}.attn_output.weight")
            X = (X + att_out @ wo.T).astype(np.float32)
            del wo

            fn = gguf.read_tensor(f"blk.{l}.ffn_norm.weight")
            xb2 = np.vstack([_np_rmsnorm(X[t], fn, rms_eps) for t in range(seq_len)])

            wg = gguf.read_tensor(f"blk.{l}.ffn_gate.weight")
            g = (xb2 @ wg.T).astype(np.float32)
            del wg
            wu = gguf.read_tensor(f"blk.{l}.ffn_up.weight")
            u = (xb2 @ wu.T).astype(np.float32)
            del wu
            hb = ((g / (1.0 + np.exp(-g))) * u).astype(np.float32)

            wd = gguf.read_tensor(f"blk.{l}.ffn_down.weight")
            X = (X + hb @ wd.T).astype(np.float32)
            del wd

        on = gguf.read_tensor("output_norm.weight")
        x_last = _np_rmsnorm(X[-1], on, rms_eps)
        logits = gguf.lm_head_matvec(x_last)
        return {
            "prompt_ids": prompt_ids,
            "logits": logits,
            "vocab_size": len(logits),
            "top1_id": int(np.argmax(logits)),
            "top5_ids": [int(i) for i in np.argsort(-logits)[:5]],
        }


class TestWin64GpuRealQwen2(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        os.makedirs(OUT_BIN_DIR, exist_ok=True)
        for req in (ZCC_BIN, ZCC_WIN64, LLM_EXE, LLM_HOST_EXE, LLAMA_TIERB_PTX):
            if not os.path.exists(req):
                raise RuntimeError(f"Missing build artifact: {req}")
        for key, info in MODELS.items():
            if not os.path.exists(info["wsl"]):
                raise RuntimeError(f"Missing real GGUF model ({key}): {info['wsl']}")
        cls.ensure_all_ptx_compiled()

    @classmethod
    def ensure_all_ptx_compiled(cls) -> None:
        cls.ptx_report: dict[str, dict] = {}
        for key, info in MODELS.items():
            r_host = _run_host_ptx(info["wsl"], info["ptx_host"], timeout_s=300)
            if r_host.returncode != 0:
                raise AssertionError(f"Host ./zcc PTX compile failed for {key}:\n{r_host.stdout}\n{r_host.stderr}")
            r_win = _run_win64_ptx(info["win"], info["ptx_win"], timeout_s=300)
            if r_win.returncode != 0:
                raise AssertionError(f"Win64 zcc_sov_stage3.exe PTX compile failed for {key}:\n{r_win.stdout}\n{r_win.stderr}")
            fc = run_fc_b(info["ptx_host"], info["ptx_win"])
            if fc.returncode != 0:
                raise AssertionError(f"PTX mismatch host vs Win64 for {key}:\n{fc.stdout}\n{fc.stderr}")
            sha_h = sha256_file(info["ptx_host"])
            sha_w = sha256_file(info["ptx_win"])
            if sha_h != sha_w:
                raise AssertionError(f"SHA256 mismatch for {key}: {sha_h} vs {sha_w}")
            cls.ptx_report[key] = {
                "gguf_bytes": os.path.getsize(info["wsl"]),
                "ptx_bytes": os.path.getsize(info["ptx_win"]),
                "sha256": sha_w,
            }

    def test_01_real_models_dual_compiler_ptx_parity_including_gt4gib(self) -> None:
        self.assertGreater(self.ptx_report["mini_7b"]["gguf_bytes"], 4 * 1024 * 1024 * 1024)
        for key, info in MODELS.items():
            with open(info["ptx_win"], "r", encoding="utf-8") as f:
                ptx = f.read()
            self.assertIn("rope=neox", ptx, f"{key} PTX must record NeoX RoPE")
            self.assertIn("zcc_aot_model_meta", ptx)
            self.assertIn("0x5a4343414f543032", ptx)
            for req in info["expect_used"]:
                self.assertIn(req, ptx, f"{key} PTX missing expected kernel symbol {req}")
            for dead in info["expect_unused"]:
                self.assertNotIn(f"zcc_aot_mc_dequant{dead}", ptx, f"{key} PTX leaked dead format {dead}")
                self.assertNotIn(f"zcc_aot_mc_matvec{dead}", ptx, f"{key} PTX leaked dead matvec {dead}")
            m = re.search(r"// Model:.*tc=(\d)", ptx)
            self.assertIsNotNone(m)
            self.assertEqual(m.group(1), "1" if info["expect_f16_tc"] else "0")

            # Verify NeoX RoPE PTX pair stride for head_dim=128 (half_hd=64 -> +256B f32 / +128B f16)
            self.assertIn("[%rd12+256]", ptx)
            self.assertNotIn("[%rd12+4]", ptx)
            print(
                f"[T11-T01] {key}: gguf={self.ptx_report[key]['gguf_bytes']}B "
                f"ptx={self.ptx_report[key]['ptx_bytes']}B sha256={self.ptx_report[key]['sha256'][:16]}..."
            )

    def test_02_streaming_numpy_1_5b_differential_oracle(self) -> None:
        prompt = "What is the capital of France?"
        chat_prompt = f"<|im_start|>user\n{prompt}<|im_end|>\n<|im_start|>assistant\n"
        oracle = streaming_numpy_prefill_logits(MODELS["prime_1_5b"]["wsl"], chat_prompt)
        ref_logits = oracle["logits"]
        ref_top1 = oracle["top1_id"]
        ref_top5 = set(oracle["top5_ids"])
        vocab = oracle["vocab_size"]

        for kv_type, pb, tag in (("f32", 1, "f32_pb1"), ("f16", 64, "f16_pb64")):
            dump_path = os.path.join(OUT_BIN_DIR, f"prime_1_5b_{tag}_logits.bin")
            res = _run_llm(
                [
                    "--aot-ptx",
                    to_rel_win(MODELS["prime_1_5b"]["ptx_win"]),
                    "--model",
                    MODELS["prime_1_5b"]["win"],
                    "--kv-type",
                    kv_type,
                    "--prefill-batch",
                    str(pb),
                    "--ctx",
                    "2048",
                    "--chat",
                    "--prompt",
                    prompt,
                    "--steps",
                    "32",
                    "--dump-logits",
                    to_rel_win(dump_path),
                ]
            )
            self.assertEqual(res.returncode, 0, f"Run failed ({tag}):\n{res.stdout}\n{res.stderr}")
            ok_kv = extract_prefixed_kv(res.stdout, "GPU_LLM_OK")
            mdl_kv = extract_prefixed_kv(res.stdout, "GPU_LLM_MODEL")
            meta_kv = extract_prefixed_kv(res.stdout, "GPU_LLM_AOT_META")
            gen_txt = extract_quoted_field(res.stdout, "GPU_LLM_TEXT", "generated")

            self.assertEqual(mdl_kv["arch"], "qwen2")
            self.assertEqual(mdl_kv["layers"], "28")
            self.assertEqual(mdl_kv["vocab"], "151936")
            self.assertEqual(meta_kv["launches_per_token"], "1")
            if pb > 1:
                pf_kv = extract_prefixed_kv(res.stdout, "GPU_LLM_PREFILL")
                self.assertEqual(int(pf_kv["prompt_tokens"]), len(oracle["prompt_ids"]))

            gpu_logits = np.array(read_f32_bin(dump_path, vocab), dtype=np.float32)
            diff = np.abs(gpu_logits - ref_logits)
            max_diff = float(np.max(diff))
            mean_diff = float(np.mean(diff))
            gpu_top1 = int(ok_kv["top1_id"])
            gpu_top5 = set(int(i) for i in np.argsort(-gpu_logits)[:5])
            overlap = len(gpu_top5 & ref_top5)

            self.assertEqual(gpu_top1, ref_top1, f"{tag}: top1 {gpu_top1} != oracle {ref_top1}")
            self.assertGreaterEqual(overlap, 4, f"{tag}: top5 overlap={overlap} (gpu={sorted(gpu_top5)} vs ref={sorted(ref_top5)})")
            self.assertLess(max_diff, 0.1, f"{tag}: max_diff={max_diff} >= 0.1")
            self.assertIn("Paris", gen_txt)
            print(
                f"[T11-T02] {tag}: top1={gpu_top1}=={ref_top1} top5_overlap={overlap}/5 "
                f"max_diff={max_diff:.5f} mean_diff={mean_diff:.6f} tok/s={ok_kv['tok_per_sec']} gen={gen_txt!r}"
            )

    def test_03_live_1_5b_q4km_english_code_and_clean_multiturn_repl(self) -> None:
        # Part A: Arithmetic + C code one-shot prompts
        r_math = _run_llm(
            [
                "--aot-ptx",
                to_rel_win(MODELS["prime_1_5b"]["ptx_win"]),
                "--model",
                MODELS["prime_1_5b"]["win"],
                "--kv-type",
                "f16",
                "--prefill-batch",
                "64",
                "--ctx",
                "2048",
                "--chat",
                "--prompt",
                "What is 17 + 25? Answer with just the number.",
                "--steps",
                "32",
            ]
        )
        self.assertEqual(r_math.returncode, 0, r_math.stderr)
        math_txt = extract_quoted_field(r_math.stdout, "GPU_LLM_TEXT", "generated")
        self.assertIn("42", math_txt)

        r_code = _run_llm(
            [
                "--aot-ptx",
                to_rel_win(MODELS["prime_1_5b"]["ptx_win"]),
                "--model",
                MODELS["prime_1_5b"]["win"],
                "--kv-type",
                "f16",
                "--prefill-batch",
                "64",
                "--ctx",
                "2048",
                "--chat",
                "--prompt",
                "Write a 3-line C function int add(int a, int b) that returns their sum.",
                "--steps",
                "64",
            ]
        )
        self.assertEqual(r_code.returncode, 0, r_code.stderr)
        code_txt = extract_quoted_field(r_code.stdout, "GPU_LLM_TEXT", "generated")
        self.assertIn("add(", code_txt)
        self.assertIn("return", code_txt)

        # Part B: Machine-readable multi-turn REPL with secret recall + /reset + stderr You> prompt
        repl_in = (
            "My secret code is 7391. Reply with only OK.\n"
            "What is my secret code? Reply with only the 4 digits.\n"
            "/reset\n"
            "What is my secret code? If unknown say UNKNOWN.\n"
            "/quit\n"
        )
        r_repl = _run_llm(
            [
                "--aot-ptx",
                to_rel_win(MODELS["prime_1_5b"]["ptx_win"]),
                "--model",
                MODELS["prime_1_5b"]["win"],
                "--kv-type",
                "f16",
                "--prefill-batch",
                "64",
                "--ctx",
                "2048",
                "--chat",
                "--steps",
                "32",
                "--interactive",
            ],
            stdin_text=repl_in,
        )
        self.assertEqual(r_repl.returncode, 0, r_repl.stderr)
        self.assertIn("You>", r_repl.stderr)
        self.assertIn("GPU_LLM_INTERACTIVE_READY", r_repl.stdout)
        self.assertIn("GPU_LLM_REPL_RESET kv_cleared=1 pos=0", r_repl.stdout)
        turns = _extract_all_prefixed_kv(r_repl.stdout, "GPU_LLM_TURN")
        self.assertEqual(len(turns), 3)
        t1_text = turns[1]["text"]
        t2_text = turns[2]["text"]
        self.assertIn("7391", t1_text, f"Expected recall of 7391 in turn 1, got {t1_text!r}")
        self.assertNotIn("7391", t2_text, f"/reset failed to clear KV cache: {t2_text!r}")

        # Part C: --clean interactive UX
        r_clean = _run_llm(
            [
                "--aot-ptx",
                to_rel_win(MODELS["prime_1_5b"]["ptx_win"]),
                "--model",
                MODELS["prime_1_5b"]["win"],
                "--kv-type",
                "f16",
                "--prefill-batch",
                "64",
                "--ctx",
                "2048",
                "--chat",
                "--clean",
                "--steps",
                "32",
                "--interactive",
            ],
            stdin_text="What is the capital of Japan? Answer in one short sentence.\n/quit\n",
        )
        self.assertEqual(r_clean.returncode, 0, r_clean.stderr)
        self.assertIn("Assistant> ", r_clean.stdout)
        self.assertIn("Tokyo", r_clean.stdout)
        self.assertNotIn("GPU_LLM_TURN", r_clean.stdout)
        self.assertNotIn("GPU_LLM_MODEL", r_clean.stdout)
        self.assertIn("GPU_LLM_MODEL", r_clean.stderr)
        self.assertIn("You> ", r_clean.stderr)
        self.assertIn("gen tok", r_clean.stderr)
        print(f"[T11-T03] math={math_txt!r} repl_recall={t1_text!r} reset_after={t2_text!r}")

    def test_04_live_1_5b_f16_tensor_core_real_english(self) -> None:
        r = _run_llm(
            [
                "--aot-ptx",
                to_rel_win(MODELS["trinity_1_5b_f16"]["ptx_win"]),
                "--model",
                MODELS["trinity_1_5b_f16"]["win"],
                "--kv-type",
                "q8_0",
                "--prefill-batch",
                "64",
                "--ctx",
                "2048",
                "--chat",
                "--prompt",
                "What is the capital of Japan? Answer in one short sentence.",
                "--steps",
                "48",
            ]
        )
        self.assertEqual(r.returncode, 0, f"Trinity F16 failed:\n{r.stdout}\n{r.stderr}")
        mdl_kv = extract_prefixed_kv(r.stdout, "GPU_LLM_MODEL")
        meta_kv = extract_prefixed_kv(r.stdout, "GPU_LLM_AOT_META")
        kv_kv = extract_prefixed_kv(r.stdout, "GPU_LLM_KV")
        ok_kv = extract_prefixed_kv(r.stdout, "GPU_LLM_OK")
        gen_txt = extract_quoted_field(r.stdout, "GPU_LLM_TEXT", "generated")

        self.assertEqual(mdl_kv["tensor_core"], "1")
        self.assertEqual(mdl_kv["wtype"], "F16")
        self.assertEqual(mdl_kv["mode"], "aot_graph")
        self.assertEqual(meta_kv["launches_per_token"], "1")
        self.assertEqual(kv_kv["type"], "q8_0")
        self.assertIn("Tokyo", gen_txt)
        print(f"[T11-T04] trinity F16 TC: tok/s={ok_kv['tok_per_sec']} gen={gen_txt!r}")

    def test_05_live_7b_gt4gib_q4km_and_auto_ctx(self) -> None:
        r = _run_llm(
            [
                "--aot-ptx",
                to_rel_win(MODELS["mini_7b"]["ptx_win"]),
                "--model",
                MODELS["mini_7b"]["win"],
                "--kv-type",
                "q8_0",
                "--prefill-batch",
                "64",
                "--ctx",
                "2048",
                "--chat",
                "--prompt",
                "What is the capital of France? Answer in one short sentence.",
                "--steps",
                "48",
            ],
            timeout_s=240,
        )
        self.assertEqual(r.returncode, 0, f"7B >4GiB failed:\n{r.stdout}\n{r.stderr}")
        mdl_kv = extract_prefixed_kv(r.stdout, "GPU_LLM_MODEL")
        meta_kv = extract_prefixed_kv(r.stdout, "GPU_LLM_AOT_META")
        ok_kv = extract_prefixed_kv(r.stdout, "GPU_LLM_OK")
        gen_txt = extract_quoted_field(r.stdout, "GPU_LLM_TEXT", "generated")

        self.assertEqual(mdl_kv["layers"], "28")
        self.assertEqual(mdl_kv["dim"], "3584")
        self.assertEqual(mdl_kv["vocab"], "152064")
        self.assertEqual(meta_kv["launches_per_token"], "1")
        self.assertGreater(int(meta_kv["wblob_bytes"]), 4 * 1024 * 1024 * 1024)
        self.assertIn("Paris", gen_txt)

        # AUTO_CTX without --ctx on native max_seq_len=32768 must clamp safely to fit RTX 5070 VRAM
        r_auto = _run_llm(
            [
                "--aot-ptx",
                to_rel_win(MODELS["mini_7b"]["ptx_win"]),
                "--model",
                MODELS["mini_7b"]["win"],
                "--kv-type",
                "f32",
                "--prefill-batch",
                "32",
                "--chat",
                "--prompt",
                "Say hello.",
                "--steps",
                "8",
            ],
            timeout_s=240,
        )
        self.assertEqual(r_auto.returncode, 0, f"7B AUTO_CTX failed:\n{r_auto.stdout}\n{r_auto.stderr}")
        self.assertIn("AUTO_CTX: clamped context 32768 ->", r_auto.stderr)
        self.assertIn("GPU_LLM_OK", r_auto.stdout)
        print(f"[T11-T05] 7B >4GiB: tok/s={ok_kv['tok_per_sec']} wblob={meta_kv['wblob_bytes']} gen={gen_txt!r}")

    def test_06_t11_ef0_negative_controls(self) -> None:
        # T11-NC-1: NeoX RoPE PTX vs Llama interleaved RoPE PTX discrimination
        with open(LLAMA_TIERB_PTX, "r", encoding="utf-8") as f:
            llama_ptx = f.read()
        with open(MODELS["prime_1_5b"]["ptx_win"], "r", encoding="utf-8") as f:
            qwen_ptx = f.read()
        self.assertIn("rope=norm", llama_ptx)
        self.assertIn("[%rd12+4]", llama_ptx)
        self.assertNotIn("[%rd12+256]", llama_ptx)
        self.assertIn("rope=neox", qwen_ptx)
        self.assertIn("[%rd12+256]", qwen_ptx)

        # T11-NC-2: Cross-model PTX/GGUF mismatch must fail closed
        r_mis = _run_llm(
            [
                "--aot-ptx",
                to_rel_win(MODELS["prime_1_5b"]["ptx_win"]),
                "--model",
                MODELS["trinity_1_5b_f16"]["win"],
                "--ctx",
                "512",
                "--prompt",
                "Hi",
                "--steps",
                "4",
            ]
        )
        self.assertNotEqual(r_mis.returncode, 0)
        self.assertIn("AOT_FINGERPRINT_MISMATCH", r_mis.stderr)

        # T11-NC-3: Strict PE32+ IAT audit + explicit --ctx overflow fails closed
        iat = audit_pe32plus_multi_dll(
            LLM_EXE,
            required_dlls=("msvcrt.dll", "nvcuda.dll"),
            allowed_dlls=("msvcrt.dll", "nvcuda.dll"),
        )
        self.assertEqual(set(iat["dlls"].keys()), {"msvcrt.dll", "nvcuda.dll"})

        r_oom = _run_llm(
            [
                "--aot-ptx",
                to_rel_win(MODELS["prime_1_5b"]["ptx_win"]),
                "--model",
                MODELS["prime_1_5b"]["win"],
                "--kv-type",
                "f32",
                "--ctx",
                "131072",
                "--prompt",
                "Hi",
                "--steps",
                "4",
            ]
        )
        self.assertNotEqual(r_oom.returncode, 0)
        self.assertIn("VRAM_BUDGET_EXCEEDED", r_oom.stderr)


if __name__ == "__main__":
    unittest.main()
