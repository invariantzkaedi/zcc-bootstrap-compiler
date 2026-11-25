#!/usr/bin/env python3
"""
Sovereign Win64 + Blackwell SM 12.0 GPU GGUF Inference Verification Suite
========================================================================
Tests (ZCC-EF-0 & ZCC-EF-7 Evidence Tier E2):
  1. Triple-compiler PTX byte identity (./zcc, zcc_sov_stage3.exe, zcc.wasm)
     and SM 12.0 MMA / warp-shuffle / fp16 PTX opcode presence.
  2. Sovereign Win64 PE32+ byte identity (./zcc vs zcc_sov_stage3.exe) and
     strict live-System32 DLL import audit (msvcrt.dll + nvcuda.dll ONLY).
  3. Live RTX 5070 F32 GGUF forward pass differential vs pure-Python oracle
     (max_abs_diff < 1e-4, 100% greedy token parity over 40 steps).
  4. Live RTX 5070 F16 SM 12.0 Tensor Core (mma.sync.aligned.m16n8k16) +
     --no-tensor-core vector differential vs pure-Python oracle + 512^3 GEMM bench.
  5. Live RTX 5070 Q8_0 GPU-native 34-byte block dequantization differential vs
     pure-Python oracle (max_abs_diff < 2e-3, 100% greedy token parity).
  6. Multi-prompt coherence, RoPE/KV position sensitivity, and determinism.
  7. ZCC-EF-0 adversarial negative controls (NC-1..NC-7).
"""

import os
import re
import struct
import sys
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_lua_win64 import BUILD_DIR, ZCC_BIN, ZCC_WIN64, run_fc_b, run_win_cmd, sha256_file
from tools.build_gpu_win64 import audit_pe32plus_multi_dll
from tools.generate_sovereign_gguf import CORPUS_SENTENCES, reference_forward_gguf
from tests.test_wasm_wasi_selfhost import ZCC_WASM, find_node_bin, run_node_wasi

LLM_EXE = os.path.join(BUILD_DIR, "zcc_gpu_llm.exe")
LLM_HOST_EXE = os.path.join(BUILD_DIR, "zcc_gpu_llm_host_zcc.exe")
KERNELS_C = os.path.join(REPO_ROOT, "src", "win64_gpu_llm_kernels.c")
PTX_HOST = os.path.join(BUILD_DIR, "gpu_llm_kernels_host.ptx")
PTX_WIN64 = os.path.join(BUILD_DIR, "gpu_llm_kernels.ptx")

GGUF_F32 = os.path.join(BUILD_DIR, "zcc_tiny_llama_f32.gguf")
GGUF_F16 = os.path.join(BUILD_DIR, "zcc_tiny_llama_f16.gguf")
GGUF_Q8_0 = os.path.join(BUILD_DIR, "zcc_tiny_llama_q8_0.gguf")


def parse_kv_line(line: str) -> dict:
    cleaned = re.sub(r'[a-zA-Z0-9_]+="[^"]*"', "", line)
    return {m.group(1): m.group(2) for m in re.finditer(r"([a-zA-Z0-9_]+)=([^\s]+)", cleaned)}


def extract_prefixed_kv(stdout: str, prefix: str) -> dict:
    for raw in stdout.splitlines():
        line = raw.strip()
        if line.startswith(prefix + " ") or line == prefix:
            return parse_kv_line(line)
    raise AssertionError(f"Expected output line starting with {prefix!r} in:\n{stdout}")


def extract_quoted_field(stdout: str, prefix: str, field: str) -> str:
    for raw in stdout.splitlines():
        line = raw.strip()
        if line.startswith(prefix + " "):
            m = re.search(rf'{re.escape(field)}="((?:[^"\\]|\\.)*)"', line)
            if m:
                return (
                    m.group(1)
                    .replace('\\n', "\n")
                    .replace('\\r', "\r")
                    .replace('\\"', '"')
                )
    raise AssertionError(f"Expected {field}=\"...\" on {prefix!r} line in:\n{stdout}")


def read_f32_bin(path: str, count: int) -> list[float]:
    with open(path, "rb") as f:
        data = f.read()
    expected = count * 4
    if len(data) != expected:
        raise AssertionError(f"{path}: expected {expected} bytes, got {len(data)}")
    return list(struct.unpack(f"<{count}f", data))


def parse_token_ids(stdout: str) -> list[int]:
    kv = extract_prefixed_kv(stdout, "GPU_LLM_TOKENS")
    raw = kv["ids"]
    if raw == "none":
        return []
    return [int(tok) for tok in raw.split(",")]


class TestWin64GpuLlm(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (
            ZCC_BIN,
            ZCC_WIN64,
            ZCC_WASM,
            LLM_EXE,
            LLM_HOST_EXE,
            PTX_HOST,
            PTX_WIN64,
            GGUF_F32,
            GGUF_F16,
            GGUF_Q8_0,
        )
        missing = [p for p in required if not os.path.isfile(p)]
        if missing:
            raise unittest.SkipTest(
                f"Missing required GPU LLM artifacts ({missing}); run `python3 tools/build_gpu_llm_win64.py` first."
            )

    def test_01_ptx_triple_compiler_byte_identity(self) -> None:
        sha_host = sha256_file(PTX_HOST)
        sha_win = sha256_file(PTX_WIN64)
        self.assertEqual(sha_host, sha_win, "Host ./zcc PTX vs Win64 zcc_sov_stage3.exe PTX SHA mismatch")

        fc = run_fc_b(PTX_HOST, PTX_WIN64)
        self.assertEqual(fc.returncode, 0, fc.stdout)
        self.assertIn("no differences encountered", fc.stdout.lower())

        ptx_wasm = os.path.join(BUILD_DIR, "gpu_llm_kernels_wasm_test.ptx")
        try:
            node_bin = find_node_bin()
            res = run_node_wasi(
                node_bin,
                ZCC_WASM,
                [
                    "zcc",
                    "--target=ptx",
                    "--ptx-arch=sm_120",
                    "src/win64_gpu_llm_kernels.c",
                    "-o",
                    "build_win64/gpu_llm_kernels_wasm_test.ptx",
                ],
            )
            self.assertEqual(res.returncode, 0, res.stderr)
            sha_wasm = sha256_file(ptx_wasm)
            self.assertEqual(
                sha_host,
                sha_wasm,
                f"Host ./zcc ({sha_host}) != WASI zcc.wasm ({sha_wasm}) PTX!",
            )
        finally:
            if os.path.exists(ptx_wasm):
                os.remove(ptx_wasm)

        with open(PTX_WIN64, "r", encoding="utf-8") as f:
            ptx_text = f.read()

        for marker in (
            ".version 8.7",
            ".target sm_120",
            "mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32",
            "shfl.sync.down.b32",
            "cvt.f32.f16",
            "cvt.rn.f16.f32",
        ):
            self.assertIn(marker, ptx_text, f"Missing expected Blackwell PTX marker {marker!r}")

        expected_entries = (
            "llm_embed_f32",
            "llm_embed_f16",
            "llm_embed_q8_0",
            "llm_rmsnorm_f32",
            "llm_f32_to_f16",
            "llm_gemm_tensor_f16",
            "llm_matvec_f32",
            "llm_matvec_f16",
            "llm_matvec_q8_0",
            "llm_rope_f32",
            "llm_kv_store_f32",
            "llm_attn_gqa_f32",
            "llm_swiglu_f32",
            "llm_add_f32",
            "llm_softmax_f32",
        )
        for entry in expected_entries:
            self.assertIn(f".visible .entry {entry}(", ptx_text)

    def test_02_pe32plus_nvcuda_audit_and_sovereign_parity(self) -> None:
        self.assertEqual(sha256_file(LLM_HOST_EXE), sha256_file(LLM_EXE))
        fc = run_fc_b(LLM_HOST_EXE, LLM_EXE)
        self.assertEqual(fc.returncode, 0, fc.stdout)
        self.assertIn("no differences encountered", fc.stdout.lower())

        info = audit_pe32plus_multi_dll(
            LLM_EXE,
            required_dlls=("msvcrt.dll", "nvcuda.dll"),
            allowed_dlls=("msvcrt.dll", "nvcuda.dll"),
        )
        dlls = set(info["dlls"].keys())
        self.assertEqual(dlls, {"msvcrt.dll", "nvcuda.dll"})
        for req_api in ("cuInit", "cuModuleLoadDataEx", "cuLaunchKernel", "cuCtxCreate_v2"):
            self.assertIn(req_api, info["dlls"]["nvcuda.dll"])

    def test_03_live_rtx5070_f32_differential_vs_independent_python_oracle(self) -> None:
        prompt = "Blackwell "
        steps = 44
        dump_win = r"build_win64\test_f32_logits.bin"
        dump_posix = os.path.join(BUILD_DIR, "test_f32_logits.bin")
        try:
            res = run_win_cmd(
                LLM_EXE,
                [
                    "--model",
                    r"build_win64\zcc_tiny_llama_f32.gguf",
                    "--prompt",
                    prompt,
                    "--steps",
                    str(steps),
                    "--dump-logits",
                    dump_win,
                ],
                timeout_s=120,
            )
            self.assertEqual(res.returncode, 0, res.stderr + res.stdout)
            dev = extract_prefixed_kv(res.stdout, "GPU_LLM_DEVICE")
            self.assertEqual(dev["cc"], "12.0")
            mdl = extract_prefixed_kv(res.stdout, "GPU_LLM_MODEL")
            self.assertEqual(mdl["wtype"], "F32")
            self.assertEqual(mdl["tensor_core"], "0")

            gpu_logits = read_f32_bin(dump_posix, 128)
            gpu_ids = parse_token_ids(res.stdout)
            gen_txt = extract_quoted_field(res.stdout, "GPU_LLM_TEXT", "generated")
            self.assertTrue(
                gen_txt.startswith("sm_120 runs live GPU kernels via nvcuda.dll!"),
                f"Unexpected F32 completion: {gen_txt!r}",
            )

            ref = reference_forward_gguf(GGUF_F32, [ord(c) for c in prompt], steps, quantize_act_f16=False)
            max_diff = max(abs(a - b) for a, b in zip(gpu_logits, ref["prompt_logits"]))
            self.assertLess(max_diff, 1e-4, f"F32 logit drift too large: max_diff={max_diff}")
            self.assertEqual(gpu_ids, ref["generated_ids"], "F32 GPU vs Python oracle token mismatch")
        finally:
            if os.path.exists(dump_posix):
                os.remove(dump_posix)

    def test_04_live_rtx5070_f16_tensor_core_differential_and_gemm_bench(self) -> None:
        prompt = "ZCC compiles "
        steps = 40
        dump_tc_win = r"build_win64\test_f16_tc_logits.bin"
        dump_tc_posix = os.path.join(BUILD_DIR, "test_f16_tc_logits.bin")
        dump_vec_win = r"build_win64\test_f16_vec_logits.bin"
        dump_vec_posix = os.path.join(BUILD_DIR, "test_f16_vec_logits.bin")
        try:
            res_tc = run_win_cmd(
                LLM_EXE,
                [
                    "--model",
                    r"build_win64\zcc_tiny_llama_f16.gguf",
                    "--prompt",
                    prompt,
                    "--steps",
                    str(steps),
                    "--bench-gemm",
                    "--dump-logits",
                    dump_tc_win,
                ],
                timeout_s=120,
            )
            self.assertEqual(res_tc.returncode, 0, res_tc.stderr + res_tc.stdout)
            mdl_tc = extract_prefixed_kv(res_tc.stdout, "GPU_LLM_MODEL")
            self.assertEqual(mdl_tc["wtype"], "F16")
            self.assertEqual(mdl_tc["tensor_core"], "1")

            bench = extract_prefixed_kv(res_tc.stdout, "GPU_LLM_GEMM_BENCH_OK")
            self.assertEqual(bench["M"], "512")
            self.assertEqual(bench["N"], "512")
            self.assertEqual(bench["K"], "512")
            self.assertGreater(float(bench["tflops"]), 0.0)
            self.assertLess(float(bench["max_err"]), 5e-2)

            tc_logits = read_f32_bin(dump_tc_posix, 128)
            tc_ids = parse_token_ids(res_tc.stdout)
            gen_txt = extract_quoted_field(res_tc.stdout, "GPU_LLM_TEXT", "generated")
            self.assertTrue(
                gen_txt.startswith("C99 directly into native PTX ISA 8.7!"),
                f"Unexpected F16 Tensor Core completion: {gen_txt!r}",
            )

            res_vec = run_win_cmd(
                LLM_EXE,
                [
                    "--model",
                    r"build_win64\zcc_tiny_llama_f16.gguf",
                    "--prompt",
                    prompt,
                    "--steps",
                    str(steps),
                    "--no-tensor-core",
                    "--dump-logits",
                    dump_vec_win,
                ],
                timeout_s=120,
            )
            self.assertEqual(res_vec.returncode, 0, res_vec.stderr + res_vec.stdout)
            mdl_vec = extract_prefixed_kv(res_vec.stdout, "GPU_LLM_MODEL")
            self.assertEqual(mdl_vec["tensor_core"], "0")
            vec_logits = read_f32_bin(dump_vec_posix, 128)
            vec_ids = parse_token_ids(res_vec.stdout)

            ref_tc = reference_forward_gguf(GGUF_F16, [ord(c) for c in prompt], steps, quantize_act_f16=True)
            ref_vec = reference_forward_gguf(GGUF_F16, [ord(c) for c in prompt], steps, quantize_act_f16=False)

            diff_tc = max(abs(a - b) for a, b in zip(tc_logits, ref_tc["prompt_logits"]))
            diff_vec = max(abs(a - b) for a, b in zip(vec_logits, ref_vec["prompt_logits"]))
            diff_tc_vs_vec = max(abs(a - b) for a, b in zip(tc_logits, vec_logits))
            diff_delta_parity = max(
                abs((a - b) - (c - d))
                for a, b, c, d in zip(tc_logits, vec_logits, ref_tc["prompt_logits"], ref_vec["prompt_logits"])
            )

            self.assertLess(diff_tc, 1e-2, f"F16 Tensor Core vs FP16-act oracle drift: {diff_tc}")
            self.assertLess(diff_vec, 1e-2, f"F16 vector vs FP32-act oracle drift: {diff_vec}")
            self.assertLess(diff_tc_vs_vec, 2e-2, f"F16 TC vs Vector divergence: {diff_tc_vs_vec}")
            self.assertLess(diff_delta_parity, 1e-3, f"TC-vs-Vec delta diverged from FP16-quantization oracle delta: {diff_delta_parity}")
            self.assertEqual(tc_ids, ref_tc["generated_ids"], "F16 Tensor Core token mismatch")
            self.assertEqual(vec_ids, ref_vec["generated_ids"], "F16 Vector token mismatch")
        finally:
            for p in (dump_tc_posix, dump_vec_posix):
                if os.path.exists(p):
                    os.remove(p)

    def test_05_live_rtx5070_q8_0_gpu_dequant_differential(self) -> None:
        prompt = "RTX 5070 "
        steps = 42
        dump_win = r"build_win64\test_q8_logits.bin"
        dump_posix = os.path.join(BUILD_DIR, "test_q8_logits.bin")
        try:
            res = run_win_cmd(
                LLM_EXE,
                [
                    "--model",
                    r"build_win64\zcc_tiny_llama_q8_0.gguf",
                    "--prompt",
                    prompt,
                    "--steps",
                    str(steps),
                    "--dump-logits",
                    dump_win,
                ],
                timeout_s=120,
            )
            self.assertEqual(res.returncode, 0, res.stderr + res.stdout)
            mdl = extract_prefixed_kv(res.stdout, "GPU_LLM_MODEL")
            self.assertEqual(mdl["wtype"], "Q8_0")
            self.assertEqual(mdl["tensor_core"], "0")

            gpu_logits = read_f32_bin(dump_posix, 128)
            gpu_ids = parse_token_ids(res.stdout)
            gen_txt = extract_quoted_field(res.stdout, "GPU_LLM_TEXT", "generated")
            self.assertTrue(
                gen_txt.startswith("Tensor Core mma executes without CUDA SDK."),
                f"Unexpected Q8_0 completion: {gen_txt!r}",
            )

            ref = reference_forward_gguf(GGUF_Q8_0, [ord(c) for c in prompt], steps, quantize_act_f16=False)
            max_diff = max(abs(a - b) for a, b in zip(gpu_logits, ref["prompt_logits"]))
            self.assertLess(max_diff, 2e-3, f"Q8_0 logit drift too large: max_diff={max_diff}")
            self.assertEqual(gpu_ids, ref["generated_ids"], "Q8_0 GPU vs Python oracle token mismatch")
        finally:
            if os.path.exists(dump_posix):
                os.remove(dump_posix)

    def test_06_multi_prompt_coherence_and_kv_cache_causality(self) -> None:
        prompts_and_models = (
            (CORPUS_SENTENCES[3], 16, r"build_win64\zcc_tiny_llama_f16.gguf", GGUF_F16, True),
            (CORPUS_SENTENCES[4], 16, r"build_win64\zcc_tiny_llama_q8_0.gguf", GGUF_Q8_0, False),
            (CORPUS_SENTENCES[5], 10, r"build_win64\zcc_tiny_llama_f32.gguf", GGUF_F32, False),
        )
        for sent, split_idx, model_win, model_posix, use_tc in prompts_and_models:
            prefix = sent[:split_idx]
            expected_tail = sent[split_idx:]
            res = run_win_cmd(
                LLM_EXE,
                ["--model", model_win, "--prompt", prefix, "--steps", str(len(expected_tail))],
                timeout_s=120,
            )
            self.assertEqual(res.returncode, 0, res.stderr + res.stdout)
            gen_txt = extract_quoted_field(res.stdout, "GPU_LLM_TEXT", "generated")
            self.assertEqual(gen_txt, expected_tail, f"Mismatch on prompt {prefix!r}")
            ref = reference_forward_gguf(
                model_posix,
                [ord(c) for c in prefix],
                len(expected_tail),
                quantize_act_f16=use_tc,
            )
            self.assertEqual(parse_token_ids(res.stdout), ref["generated_ids"])

        # Determinism check (two identical greedy runs produce bit-identical logits_fnv + tokens)
        r1 = run_win_cmd(
            LLM_EXE,
            ["--model", r"build_win64\zcc_tiny_llama_f32.gguf", "--prompt", "AB", "--steps", "8"],
            timeout_s=120,
        )
        r2 = run_win_cmd(
            LLM_EXE,
            ["--model", r"build_win64\zcc_tiny_llama_f32.gguf", "--prompt", "AB", "--steps", "8"],
            timeout_s=120,
        )
        # Position / order causality: "AB" != "BA" in RoPE + causal KV cache
        r_swap = run_win_cmd(
            LLM_EXE,
            ["--model", r"build_win64\zcc_tiny_llama_f32.gguf", "--prompt", "BA", "--steps", "8"],
            timeout_s=120,
        )
        for r in (r1, r2, r_swap):
            self.assertEqual(r.returncode, 0, r.stderr + r.stdout)
        ok1 = extract_prefixed_kv(r1.stdout, "GPU_LLM_OK")
        ok2 = extract_prefixed_kv(r2.stdout, "GPU_LLM_OK")
        ok_sw = extract_prefixed_kv(r_swap.stdout, "GPU_LLM_OK")
        self.assertEqual(ok1["logits_fnv"], ok2["logits_fnv"])
        self.assertEqual(parse_token_ids(r1.stdout), parse_token_ids(r2.stdout))
        self.assertNotEqual(ok1["logits_fnv"], ok_sw["logits_fnv"])

        # Prefix length / position sensitivity ("ZCC compiles" ends with 's' -> next token ' ')
        r_short = run_win_cmd(
            LLM_EXE,
            ["--model", r"build_win64\zcc_tiny_llama_f16.gguf", "--prompt", "ZCC compiles", "--steps", "1"],
            timeout_s=120,
        )
        self.assertEqual(r_short.returncode, 0, r_short.stderr + r_short.stdout)
        self.assertEqual(parse_token_ids(r_short.stdout), [ord(" ")])

    def test_07_ef0_negative_controls(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR) as tmpdir:
            rel_tmp = os.path.basename(tmpdir)

            # NC-1: Corrupt GGUF magic must fail closed
            nc1_p = os.path.join(tmpdir, "nc1_bad_magic.gguf")
            with open(GGUF_F32, "rb") as fin:
                orig_f32 = fin.read()
            with open(nc1_p, "wb") as fout:
                fout.write(b"BAD!" + orig_f32[4:])
            r1 = run_win_cmd(
                LLM_EXE,
                ["--model", rf"build_win64\{rel_tmp}\nc1_bad_magic.gguf", "--prompt", "ZCC ", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r1.returncode, 0, "NC-1 failed to reject bad GGUF magic")
            self.assertIn("GGUF_PARSE_ERROR", r1.stderr + r1.stdout)

            # NC-2: Truncated GGUF must fail closed
            nc2_p = os.path.join(tmpdir, "nc2_trunc.gguf")
            with open(nc2_p, "wb") as fout:
                fout.write(orig_f32[:128])
            r2 = run_win_cmd(
                LLM_EXE,
                ["--model", rf"build_win64\{rel_tmp}\nc2_trunc.gguf", "--prompt", "ZCC ", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r2.returncode, 0, "NC-2 failed to reject truncated GGUF")
            self.assertIn("GGUF_PARSE_ERROR", r2.stderr + r2.stdout)

            # NC-3: Corrupt tensor ggml_type inside tensor directory must fail closed
            nc3_p = os.path.join(tmpdir, "nc3_bad_tensor_type.gguf")
            # Locate first tensor record by searching for "token_embd.weight" (prefixed by uint64 len)
            needle = b"token_embd.weight"
            idx = orig_f32.find(needle)
            self.assertGreater(idx, 0)
            after_name = idx + len(needle)
            n_dims = struct.unpack_from("<I", orig_f32, after_name)[0]
            type_off = after_name + 4 + 8 * n_dims
            mut_type = bytearray(orig_f32)
            struct.pack_into("<I", mut_type, type_off, 99)
            with open(nc3_p, "wb") as fout:
                fout.write(mut_type)
            r3 = run_win_cmd(
                LLM_EXE,
                ["--model", rf"build_win64\{rel_tmp}\nc3_bad_tensor_type.gguf", "--prompt", "ZCC ", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r3.returncode, 0, "NC-3 failed to reject unsupported GGML type")

            # NC-4: Missing model path must fail closed
            r4 = run_win_cmd(
                LLM_EXE,
                ["--model", rf"build_win64\{rel_tmp}\nonexistent.gguf", "--prompt", "ZCC ", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r4.returncode, 0, "NC-4 failed on nonexistent file")
            self.assertIn("GGUF_PARSE_ERROR", r4.stderr + r4.stdout)

            # NC-5: Corrupted tensor weight triggers differential mismatch vs uncorrupted oracle
            nc5_p = os.path.join(tmpdir, "nc5_corrupt_weights.gguf")
            nc5_dump_win = rf"build_win64\{rel_tmp}\nc5_logits.bin"
            nc5_dump_posix = os.path.join(tmpdir, "nc5_logits.bin")
            mut_w = bytearray(orig_f32)
            # Flip weights inside output_norm / token_embd tail (safe data payload near end of file)
            payload_pos = len(mut_w) - 1024
            struct.pack_into("<f", mut_w, payload_pos, 42.0)
            with open(nc5_p, "wb") as fout:
                fout.write(mut_w)
            r5 = run_win_cmd(
                LLM_EXE,
                [
                    "--model",
                    rf"build_win64\{rel_tmp}\nc5_corrupt_weights.gguf",
                    "--prompt",
                    "Blackwell ",
                    "--steps",
                    "4",
                    "--dump-logits",
                    nc5_dump_win,
                ],
                timeout_s=60,
            )
            self.assertEqual(r5.returncode, 0, r5.stderr + r5.stdout)
            corrupt_logits = read_f32_bin(nc5_dump_posix, 128)
            clean_ref = reference_forward_gguf(GGUF_F32, [ord(c) for c in "Blackwell "], 4)
            corrupt_diff = max(abs(a - b) for a, b in zip(corrupt_logits, clean_ref["prompt_logits"]))
            self.assertGreater(corrupt_diff, 0.01, "NC-5 failed to detect corrupted weight against oracle")

            # NC-6: NaN weight injection triggers runtime non-finite logit guard
            nc6_p = os.path.join(tmpdir, "nc6_nan_poison.gguf")
            mut_nan = bytearray(orig_f32)
            for off in range(len(mut_nan) - 512, len(mut_nan), 4):
                struct.pack_into("<I", mut_nan, off, 0x7FC00000)
            with open(nc6_p, "wb") as fout:
                fout.write(mut_nan)
            r6 = run_win_cmd(
                LLM_EXE,
                [
                    "--model",
                    rf"build_win64\{rel_tmp}\nc6_nan_poison.gguf",
                    "--prompt",
                    "Blackwell ",
                    "--steps",
                    "1",
                ],
                timeout_s=60,
            )
            self.assertNotEqual(r6.returncode, 0, "NC-6 failed to catch NaN GPU logit poisoning")
            self.assertIn("Non-finite GPU logit", r6.stderr + r6.stdout)

            # NC-8: Non-power-of-two general.alignment must fail closed
            nc8_p = os.path.join(tmpdir, "nc8_bad_align.gguf")
            al_key = b"general.alignment"
            al_idx = orig_f32.find(al_key)
            self.assertGreater(al_idx, 0)
            vt_al, cur_al = struct.unpack_from("<II", orig_f32, al_idx + len(al_key))
            self.assertEqual(vt_al, 4)
            mut_al = bytearray(orig_f32)
            struct.pack_into("<I", mut_al, al_idx + len(al_key) + 4, 6)
            with open(nc8_p, "wb") as fout:
                fout.write(mut_al)
            r8 = run_win_cmd(
                LLM_EXE,
                ["--model", rf"build_win64\{rel_tmp}\nc8_bad_align.gguf", "--prompt", "ZCC ", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r8.returncode, 0, "NC-8 failed to reject non-power-of-two alignment")
            self.assertIn("non-power-of-two", r8.stderr + r8.stdout)

            # NC-9: Tensor dimension 64-bit multiplication overflow must fail closed
            nc9_p = os.path.join(tmpdir, "nc9_dim_overflow.gguf")
            emb_idx = orig_f32.find(b"token_embd.weight")
            self.assertGreater(emb_idx, 0)
            emb_nd = struct.unpack_from("<I", orig_f32, emb_idx + len(b"token_embd.weight"))[0]
            self.assertEqual(emb_nd, 2)
            mut_ov = bytearray(orig_f32)
            struct.pack_into("<Q", mut_ov, emb_idx + len(b"token_embd.weight") + 4 + 8, 0xFFFFFFFFFFFFFFF0)
            with open(nc9_p, "wb") as fout:
                fout.write(mut_ov)
            r9 = run_win_cmd(
                LLM_EXE,
                ["--model", rf"build_win64\{rel_tmp}\nc9_dim_overflow.gguf", "--prompt", "ZCC ", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r9.returncode, 0, "NC-9 failed to reject tensor dim overflow")
            self.assertIn("size overflow", r9.stderr + r9.stdout)

        # NC-7: PE32+ import auditor rejects missing required DLL and unexpected extra DLL
        with self.assertRaises(AssertionError):
            audit_pe32plus_multi_dll(LLM_EXE, required_dlls=("msvcrt.dll", "nvcuda.dll", "KERNEL32.dll"))
        with self.assertRaises(AssertionError):
            audit_pe32plus_multi_dll(LLM_EXE, required_dlls=("msvcrt.dll",), allowed_dlls=("msvcrt.dll",))

        # NC-10: Invalid --temp values (negative, NaN, trailing garbage) must fail closed
        for bad_temp in ("-0.5", "nan", "1.0abc"):
            r10 = run_win_cmd(
                LLM_EXE,
                ["--model", r"build_win64\zcc_tiny_llama_f32.gguf", "--prompt", "ZCC ", "--steps", "1", "--temp", bad_temp],
                timeout_s=60,
            )
            self.assertNotEqual(r10.returncode, 0, f"NC-10 failed to reject --temp {bad_temp!r}")
            self.assertIn("INVALID_ARG", r10.stderr + r10.stdout)

    def test_08_real_temp_sampling_distribution(self) -> None:
        r_greedy = run_win_cmd(
            LLM_EXE,
            ["--model", r"build_win64\zcc_tiny_llama_f16.gguf", "--prompt", "ZCC ", "--steps", "20", "--temp", "0.0"],
            timeout_s=60,
        )
        r_low = run_win_cmd(
            LLM_EXE,
            ["--model", r"build_win64\zcc_tiny_llama_f16.gguf", "--prompt", "ZCC ", "--steps", "20", "--temp", "0.0001", "--seed", "101"],
            timeout_s=60,
        )
        r_hi1 = run_win_cmd(
            LLM_EXE,
            ["--model", r"build_win64\zcc_tiny_llama_f16.gguf", "--prompt", "ZCC ", "--steps", "20", "--temp", "50.0", "--seed", "101"],
            timeout_s=60,
        )
        r_hi2 = run_win_cmd(
            LLM_EXE,
            ["--model", r"build_win64\zcc_tiny_llama_f16.gguf", "--prompt", "ZCC ", "--steps", "20", "--temp", "50.0", "--seed", "202"],
            timeout_s=60,
        )
        for r in (r_greedy, r_low, r_hi1, r_hi2):
            self.assertEqual(r.returncode, 0, r.stderr + r.stdout)

        ids_g = parse_token_ids(r_greedy.stdout)
        ids_lo = parse_token_ids(r_low.stdout)
        ids_h1 = parse_token_ids(r_hi1.stdout)
        ids_h2 = parse_token_ids(r_hi2.stdout)

        self.assertEqual(ids_lo, ids_g, "Low temp (0.0001) should match greedy on converged distribution")
        self.assertNotEqual(ids_h1, ids_g, "High temp (50.0) failed to diverge from greedy sampling")
        self.assertNotEqual(ids_h1, ids_h2, "High temp (50.0) failed to vary across PRNG seeds")


if __name__ == "__main__":
    unittest.main()
