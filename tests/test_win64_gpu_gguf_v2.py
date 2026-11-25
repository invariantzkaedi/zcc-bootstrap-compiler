#!/usr/bin/env python3
"""
Track 9 Verification Suite: Real-World HF GGUF Engine + Multi-CTA AOT + CUDA Graphs
===================================================================================
Tests (ZCC-EF-0 & ZCC-EF-7 Evidence Tier E2):
  1. Tier B ZCCAOT02 triple-compiler PTX byte identity (./zcc, zcc_sov_stage3.exe,
     zcc.wasm) + dead-format elimination across Qwen2 Q4_K_M, Llama Q4_0, Llama BF16.
  2. Live RTX 5070 Qwen2 20-layer Q4_K_M + QKV biases + GPT-2 BPE + ChatML (`--chat`)
     differential vs pure-Python oracle (< 5e-3), CUDA Graph vs --no-cuda-graph exact
     0.0 parity, and CUDA Graph multi-CTA speedup benchmark (`launches_per_token=1`).
  3. Live RTX 5070 Llama Q4_0 + SentencePiece (`--sp` / `<0xNN>` byte fallback)
     differential vs pure-Python oracle (< 2e-3) + generic vs CUDA Graph parity (< 1e-4).
  4. Live RTX 5070 Llama BF16 differential vs oracle (< 2e-3) + `--ctx 64` KV override.
  5. Sovereign single-binary standalone model `.exe` (`zcc_tiny_llama_standalone.exe`)
     SHA-256 + `fc /b` parity, strict System32 `{msvcrt,nvcuda}.dll` audit, and live
     RTX 5070 execution from a brand-new EMPTY directory with 0.0 diff vs `--aot-ptx`.
  6. Production sampler (`--top-k`, `--top-p`, `--min-p`, `--repeat-penalty`, `--stream`).
  7. ZCC-EF-0 adversarial negative controls (`V2-NC-1..V2-NC-7`).
"""

import os
import shutil
import statistics
import struct
import subprocess
import sys
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_lua_win64 import (
    BUILD_DIR,
    CMD_EXE,
    ZCC_BIN,
    ZCC_WIN64,
    run_fc_b,
    run_win_cmd,
    sha256_file,
    to_rel_posix,
    to_rel_win,
)
from tools.build_gpu_win64 import audit_pe32plus_multi_dll
from tools.generate_gguf_v2_fixtures import (
    reference_forward_gguf_v2,
    reference_tokenize_gguf,
)
from tests.test_wasm_wasi_selfhost import ZCC_WASM, find_node_bin, run_node_wasi
from tests.test_win64_gpu_llm import (
    GGUF_F16,
    LLM_EXE,
    extract_prefixed_kv,
    extract_quoted_field,
    parse_token_ids,
    read_f32_bin,
)

QWEN2_GGUF = os.path.join(BUILD_DIR, "zcc_hf_qwen2_20l_q4km.gguf")
Q4_0_GGUF = os.path.join(BUILD_DIR, "zcc_hf_llama_spm_q4_0.gguf")
BF16_GGUF = os.path.join(BUILD_DIR, "zcc_hf_llama_bf16.gguf")

QWEN2_PTX = os.path.join(BUILD_DIR, "zcc_hf_qwen2_20l_q4km_sm120.ptx")
Q4_0_PTX = os.path.join(BUILD_DIR, "zcc_hf_llama_spm_q4_0_sm120.ptx")
BF16_PTX = os.path.join(BUILD_DIR, "zcc_hf_llama_bf16_sm120.ptx")

AOT_PTX_F16 = os.path.join(BUILD_DIR, "zcc_tiny_llama_f16_sm120.ptx")
STANDALONE_EXE = os.path.join(BUILD_DIR, "zcc_tiny_llama_standalone.exe")
STANDALONE_HOST_EXE = os.path.join(BUILD_DIR, "zcc_tiny_llama_standalone_host_zcc.exe")


class TestWin64GpuGgufV2(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (
            ZCC_BIN,
            ZCC_WIN64,
            ZCC_WASM,
            LLM_EXE,
            QWEN2_GGUF,
            Q4_0_GGUF,
            BF16_GGUF,
            QWEN2_PTX,
            Q4_0_PTX,
            BF16_PTX,
            GGUF_F16,
            AOT_PTX_F16,
            STANDALONE_EXE,
            STANDALONE_HOST_EXE,
        )
        missing = [p for p in required if not os.path.isfile(p)]
        if missing:
            raise unittest.SkipTest(
                f"Missing Track 9 artifacts ({missing}); run `make build-gpu-gguf-v2-win64` first."
            )

    def test_01_tier_b_triple_compiler_ptx_parity_and_dead_format_elim(self) -> None:
        node_bin = find_node_bin()
        common_req = (
            ".version 8.7",
            ".target sm_120",
            "0x5a4343414f543032",
            ".visible .entry zcc_aot_mc_rmsnorm",
            ".visible .entry zcc_aot_mc_rope_kv",
            ".visible .entry zcc_aot_mc_attn_gqa",
            ".visible .entry zcc_aot_mc_swiglu",
            ".visible .entry zcc_aot_model_meta",
        )
        cases = (
            (
                "qwen2",
                QWEN2_GGUF,
                QWEN2_PTX,
                (
                    ".visible .entry zcc_aot_mc_embed_q8_0",
                    ".visible .entry zcc_aot_mc_matvec_q4_k",
                    ".visible .entry zcc_aot_mc_matvec_q6_k",
                    ".visible .entry zcc_aot_mc_add_bias",
                ),
                (
                    "zcc_aot_mc_matvec_f32",
                    "zcc_aot_mc_matvec_f16",
                    "zcc_aot_mc_matvec_bf16",
                    "zcc_aot_mc_matvec_q4_0",
                    "zcc_aot_mc_matvec_q8_0",
                    "zcc_aot_mc_gemm_tc_f16",
                    "zcc_aot_forward_token",
                ),
            ),
            (
                "q4_0",
                Q4_0_GGUF,
                Q4_0_PTX,
                (
                    ".visible .entry zcc_aot_mc_embed_q4_0",
                    ".visible .entry zcc_aot_mc_matvec_q4_0",
                ),
                (
                    "zcc_aot_mc_add_bias",
                    "zcc_aot_mc_matvec_f32",
                    "zcc_aot_mc_matvec_f16",
                    "zcc_aot_mc_matvec_bf16",
                    "zcc_aot_mc_matvec_q8_0",
                    "zcc_aot_mc_matvec_q4_k",
                    "zcc_aot_mc_matvec_q6_k",
                    "zcc_aot_mc_gemm_tc_f16",
                    "zcc_aot_forward_token",
                ),
            ),
            (
                "bf16",
                BF16_GGUF,
                BF16_PTX,
                (
                    ".visible .entry zcc_aot_mc_embed_bf16",
                    ".visible .entry zcc_aot_mc_matvec_bf16",
                ),
                (
                    "zcc_aot_mc_add_bias",
                    "zcc_aot_mc_matvec_f32",
                    "zcc_aot_mc_matvec_f16",
                    "zcc_aot_mc_matvec_q4_0",
                    "zcc_aot_mc_matvec_q8_0",
                    "zcc_aot_mc_matvec_q4_k",
                    "zcc_aot_mc_matvec_q6_k",
                    "zcc_aot_mc_gemm_tc_f16",
                    "zcc_aot_forward_token",
                ),
            ),
        )
        for tag, gguf_p, win_ptx, req_syms, banned_syms in cases:
            host_ptx = os.path.join(BUILD_DIR, f"test_v2_{tag}_host.ptx")
            wasm_ptx = os.path.join(BUILD_DIR, f"test_v2_{tag}_wasm.ptx")
            try:
                rh = subprocess.run(
                    [ZCC_BIN, "--target=ptx", "--gpu-arch=sm_120", "--model", to_rel_posix(gguf_p), "-o", to_rel_posix(host_ptx)],
                    cwd=REPO_ROOT,
                    capture_output=True,
                    text=True,
                    timeout=60,
                )
                self.assertEqual(rh.returncode, 0, rh.stderr)
                rw = run_node_wasi(
                    node_bin,
                    ZCC_WASM,
                    ["zcc", "--target=ptx", "--gpu-arch=sm_120", "--model", to_rel_posix(gguf_p), "-o", to_rel_posix(wasm_ptx)],
                )
                self.assertEqual(rw.returncode, 0, rw.stderr)

                sha_h = sha256_file(host_ptx)
                sha_w = sha256_file(win_ptx)
                sha_ws = sha256_file(wasm_ptx)
                self.assertEqual(sha_h, sha_w, f"{tag}: ./zcc vs zcc_sov_stage3.exe Tier B PTX mismatch")
                self.assertEqual(sha_h, sha_ws, f"{tag}: ./zcc vs WASI zcc.wasm Tier B PTX mismatch")

                fc = run_fc_b(host_ptx, win_ptx)
                self.assertEqual(fc.returncode, 0, fc.stdout)
                self.assertIn("no differences encountered", fc.stdout.lower())

                with open(win_ptx, "r", encoding="utf-8") as f:
                    ptx_text = f.read()
                for sym in common_req + req_syms:
                    self.assertIn(sym, ptx_text, f"{tag}: missing Tier B symbol {sym!r}")
                for sym in banned_syms:
                    self.assertNotIn(sym, ptx_text, f"{tag}: dead-format elimination failed, found {sym!r}")
            finally:
                for tmp in (host_ptx, wasm_ptx):
                    if os.path.exists(tmp):
                        os.remove(tmp)

    def test_02_qwen2_20l_q4km_bias_gpt2_bpe_differential_and_cuda_graph(self) -> None:
        prompt = "ZCC compiles Blackwell Qwen2!"
        steps = 16
        with tempfile.TemporaryDirectory(dir=BUILD_DIR) as tmpdir:
            d_gen = os.path.join(tmpdir, "qwen_gen.bin")
            d_gr = os.path.join(tmpdir, "qwen_gr.bin")
            d_ng = os.path.join(tmpdir, "qwen_ng.bin")

            rg = run_win_cmd(
                LLM_EXE,
                ["--model", to_rel_win(QWEN2_GGUF), "--prompt", prompt, "--chat", "--ignore-eos", "--steps", str(steps), "--dump-logits", to_rel_win(d_gen)],
                timeout_s=120,
            )
            rgr = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(QWEN2_PTX), "--model", to_rel_win(QWEN2_GGUF), "--prompt", prompt, "--chat", "--ignore-eos", "--steps", str(steps), "--dump-logits", to_rel_win(d_gr)],
                timeout_s=120,
            )
            rng = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(QWEN2_PTX), "--model", to_rel_win(QWEN2_GGUF), "--prompt", prompt, "--chat", "--ignore-eos", "--no-cuda-graph", "--steps", str(steps), "--dump-logits", to_rel_win(d_ng)],
                timeout_s=120,
            )
            for r in (rg, rgr, rng):
                self.assertEqual(r.returncode, 0, r.stderr + r.stdout)

            meta_gr = extract_prefixed_kv(rgr.stdout, "GPU_LLM_AOT_META")
            mdl_gr = extract_prefixed_kv(rgr.stdout, "GPU_LLM_MODEL")
            self.assertEqual(meta_gr["launches_per_token"], "1")
            self.assertEqual(mdl_gr["arch"], "qwen2")
            self.assertEqual(mdl_gr["layers"], "20")
            self.assertEqual(mdl_gr["wtype"], "Q4_K")
            self.assertEqual(mdl_gr["mode"], "aot_graph")

            meta_ng = extract_prefixed_kv(rng.stdout, "GPU_LLM_AOT_META")
            mdl_ng = extract_prefixed_kv(rng.stdout, "GPU_LLM_MODEL")
            self.assertEqual(mdl_ng["mode"], "aot_multi_cta")
            self.assertEqual(meta_ng["launches_per_token"], meta_gr["graph_nodes"])

            pids = reference_tokenize_gguf(QWEN2_GGUF, prompt, chat=True)
            ref = reference_forward_gguf_v2(QWEN2_GGUF, pids, steps, ignore_eos=True)

            lg_gen = read_f32_bin(d_gen, 384)
            lg_gr = read_f32_bin(d_gr, 384)
            lg_ng = read_f32_bin(d_ng, 384)

            self.assertLess(max(abs(a - b) for a, b in zip(lg_gen, ref["prompt_logits"])), 5e-3)
            self.assertLess(max(abs(a - b) for a, b in zip(lg_gr, ref["prompt_logits"])), 5e-3)
            self.assertEqual(max(abs(a - b) for a, b in zip(lg_gr, lg_ng)), 0.0)
            self.assertEqual(
                extract_prefixed_kv(rgr.stdout, "GPU_LLM_OK")["logits_fnv"],
                extract_prefixed_kv(rng.stdout, "GPU_LLM_OK")["logits_fnv"],
            )
            self.assertEqual(parse_token_ids(rg.stdout), ref["generated_ids"])
            self.assertEqual(parse_token_ids(rgr.stdout), ref["generated_ids"])
            self.assertEqual(parse_token_ids(rng.stdout), ref["generated_ids"])

            # 3 alternating benchmark runs over 32 steps: CUDA Graph must beat direct Multi-CTA launch
            gr_tps, ng_tps = [], []
            for _ in range(3):
                r1 = run_win_cmd(
                    LLM_EXE,
                    ["--aot-ptx", to_rel_win(QWEN2_PTX), "--model", to_rel_win(QWEN2_GGUF), "--prompt", prompt, "--chat", "--ignore-eos", "--steps", "32"],
                    timeout_s=120,
                )
                r2 = run_win_cmd(
                    LLM_EXE,
                    ["--aot-ptx", to_rel_win(QWEN2_PTX), "--model", to_rel_win(QWEN2_GGUF), "--prompt", prompt, "--chat", "--ignore-eos", "--no-cuda-graph", "--steps", "32"],
                    timeout_s=120,
                )
                self.assertEqual(r1.returncode, 0, r1.stderr + r1.stdout)
                self.assertEqual(r2.returncode, 0, r2.stderr + r2.stdout)
                gr_tps.append(float(extract_prefixed_kv(r1.stdout, "GPU_LLM_OK")["tok_per_sec"]))
                ng_tps.append(float(extract_prefixed_kv(r2.stdout, "GPU_LLM_OK")["tok_per_sec"]))
            med_gr = statistics.median(gr_tps)
            med_ng = statistics.median(ng_tps)
            self.assertGreater(med_gr, med_ng * 2.0, f"Expected >2x CUDA Graph speedup: {med_gr:.2f} vs {med_ng:.2f} tok/s")

    def test_03_llama_q4_0_sentencepiece_differential(self) -> None:
        prompt = "ZCC Blackwell @#!"
        steps = 16
        with tempfile.TemporaryDirectory(dir=BUILD_DIR) as tmpdir:
            d_gen = os.path.join(tmpdir, "q40_gen.bin")
            d_gr = os.path.join(tmpdir, "q40_gr.bin")
            rg = run_win_cmd(
                LLM_EXE,
                ["--model", to_rel_win(Q4_0_GGUF), "--prompt", prompt, "--ignore-eos", "--steps", str(steps), "--dump-logits", to_rel_win(d_gen)],
                timeout_s=120,
            )
            rgr = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(Q4_0_PTX), "--model", to_rel_win(Q4_0_GGUF), "--prompt", prompt, "--ignore-eos", "--steps", str(steps), "--dump-logits", to_rel_win(d_gr)],
                timeout_s=120,
            )
            self.assertEqual(rg.returncode, 0, rg.stderr + rg.stdout)
            self.assertEqual(rgr.returncode, 0, rgr.stderr + rgr.stdout)
            self.assertEqual(extract_prefixed_kv(rgr.stdout, "GPU_LLM_MODEL")["wtype"], "Q4_0")
            self.assertEqual(extract_prefixed_kv(rgr.stdout, "GPU_LLM_MODEL")["mode"], "aot_graph")

            pids = reference_tokenize_gguf(Q4_0_GGUF, prompt)
            self.assertIn(67, pids)  # '<0x40>' byte fallback for '@'
            self.assertIn(38, pids)  # '<0x23>' byte fallback for '#'
            ref = reference_forward_gguf_v2(Q4_0_GGUF, pids, steps, ignore_eos=True)

            lg_gen = read_f32_bin(d_gen, 384)
            lg_gr = read_f32_bin(d_gr, 384)
            self.assertLess(max(abs(a - b) for a, b in zip(lg_gen, ref["prompt_logits"])), 2e-3)
            self.assertLess(max(abs(a - b) for a, b in zip(lg_gr, ref["prompt_logits"])), 2e-3)
            self.assertLess(max(abs(a - b) for a, b in zip(lg_gen, lg_gr)), 1e-4)
            self.assertEqual(parse_token_ids(rg.stdout), ref["generated_ids"])
            self.assertEqual(parse_token_ids(rgr.stdout), ref["generated_ids"])

    def test_04_llama_bf16_differential_and_ctx_override(self) -> None:
        prompt = "ZCC Tensor PTX "
        steps = 16
        with tempfile.TemporaryDirectory(dir=BUILD_DIR) as tmpdir:
            d_gen = os.path.join(tmpdir, "bf16_gen.bin")
            d_gr = os.path.join(tmpdir, "bf16_gr.bin")
            d_ctx = os.path.join(tmpdir, "bf16_ctx.bin")
            rg = run_win_cmd(
                LLM_EXE,
                ["--model", to_rel_win(BF16_GGUF), "--prompt", prompt, "--ignore-eos", "--steps", str(steps), "--dump-logits", to_rel_win(d_gen)],
                timeout_s=120,
            )
            rgr = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(BF16_PTX), "--model", to_rel_win(BF16_GGUF), "--prompt", prompt, "--ignore-eos", "--steps", str(steps), "--dump-logits", to_rel_win(d_gr)],
                timeout_s=120,
            )
            rctx = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(BF16_PTX), "--model", to_rel_win(BF16_GGUF), "--prompt", prompt, "--ctx", "64", "--ignore-eos", "--steps", str(steps), "--dump-logits", to_rel_win(d_ctx)],
                timeout_s=120,
            )
            for r in (rg, rgr, rctx):
                self.assertEqual(r.returncode, 0, r.stderr + r.stdout)
            self.assertEqual(extract_prefixed_kv(rgr.stdout, "GPU_LLM_MODEL")["wtype"], "BF16")

            pids = reference_tokenize_gguf(BF16_GGUF, prompt)
            ref = reference_forward_gguf_v2(BF16_GGUF, pids, steps, ignore_eos=True)
            lg_gen = read_f32_bin(d_gen, 384)
            lg_gr = read_f32_bin(d_gr, 384)
            lg_ctx = read_f32_bin(d_ctx, 384)

            self.assertLess(max(abs(a - b) for a, b in zip(lg_gen, ref["prompt_logits"])), 2e-3)
            self.assertLess(max(abs(a - b) for a, b in zip(lg_gr, ref["prompt_logits"])), 2e-3)
            self.assertEqual(lg_gr, lg_ctx)
            self.assertEqual(
                extract_prefixed_kv(rgr.stdout, "GPU_LLM_OK")["logits_fnv"],
                extract_prefixed_kv(rctx.stdout, "GPU_LLM_OK")["logits_fnv"],
            )
            self.assertEqual(parse_token_ids(rg.stdout), ref["generated_ids"])
            self.assertEqual(parse_token_ids(rgr.stdout), ref["generated_ids"])
            self.assertEqual(parse_token_ids(rctx.stdout), ref["generated_ids"])

    def test_05_standalone_compiled_model_exe_empty_dir_execution(self) -> None:
        self.assertEqual(sha256_file(STANDALONE_HOST_EXE), sha256_file(STANDALONE_EXE))
        fc = run_fc_b(STANDALONE_HOST_EXE, STANDALONE_EXE)
        self.assertEqual(fc.returncode, 0, fc.stdout)
        self.assertIn("no differences encountered", fc.stdout.lower())

        info = audit_pe32plus_multi_dll(
            STANDALONE_EXE,
            required_dlls=("msvcrt.dll", "nvcuda.dll"),
            allowed_dlls=("msvcrt.dll", "nvcuda.dll"),
        )
        self.assertEqual(set(info["dlls"].keys()), {"msvcrt.dll", "nvcuda.dll"})

        with tempfile.TemporaryDirectory(dir=BUILD_DIR) as empty_dir, tempfile.TemporaryDirectory(dir=BUILD_DIR) as ref_dir:
            dst_exe = os.path.join(empty_dir, "zcc_tiny_llama_standalone.exe")
            shutil.copy2(STANDALONE_EXE, dst_exe)
            self.assertEqual(os.listdir(empty_dir), ["zcc_tiny_llama_standalone.exe"])

            cmd = [
                "zcc_tiny_llama_standalone.exe" if sys.platform == "win32" else CMD_EXE,
            ]
            if sys.platform != "win32":
                cmd.extend(["/c", "zcc_tiny_llama_standalone.exe"])
            cmd.extend(["--prompt", "ZCC compiles ", "--steps", "48", "--dump-logits", "standalone_logits.bin"])

            rs = subprocess.run(cmd, cwd=empty_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=120)
            self.assertEqual(rs.returncode, 0, rs.stderr + rs.stdout)

            meta = extract_prefixed_kv(rs.stdout, "GPU_LLM_AOT_META")
            mdl = extract_prefixed_kv(rs.stdout, "GPU_LLM_MODEL")
            self.assertEqual(meta["launches_per_token"], "1")
            self.assertIn('ptx_path="<embedded>"', rs.stdout)
            self.assertIn('path="<embedded>"', rs.stdout)
            self.assertEqual(mdl["mode"], "aot_embedded")
            self.assertEqual(
                extract_quoted_field(rs.stdout, "GPU_LLM_TEXT", "generated"),
                "C99 directly into native PTX ISA 8.7!\n id.\nnerne",
            )

            ref_bin = os.path.join(ref_dir, "ref_aot.bin")
            ra = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(AOT_PTX_F16), "--model", to_rel_win(GGUF_F16), "--prompt", "ZCC compiles ", "--steps", "48", "--dump-logits", to_rel_win(ref_bin)],
                timeout_s=120,
            )
            self.assertEqual(ra.returncode, 0, ra.stderr + ra.stdout)

            lg_standalone = read_f32_bin(os.path.join(empty_dir, "standalone_logits.bin"), 128)
            lg_aot = read_f32_bin(ref_bin, 128)
            self.assertEqual(lg_standalone, lg_aot)
            self.assertEqual(parse_token_ids(rs.stdout), parse_token_ids(ra.stdout))
            self.assertEqual(
                extract_prefixed_kv(rs.stdout, "GPU_LLM_OK")["logits_fnv"],
                extract_prefixed_kv(ra.stdout, "GPU_LLM_OK")["logits_fnv"],
            )

    def test_06_production_sampler_top_k_top_p_min_p_repeat_penalty_and_stream(self) -> None:
        base = ["--aot-ptx", to_rel_win(BF16_PTX), "--model", to_rel_win(BF16_GGUF), "--prompt", "ZCC Tensor PTX ", "--steps", "20", "--ignore-eos"]
        rg = run_win_cmd(LLM_EXE, base + ["--temp", "0.0"], timeout_s=60)
        rtk1 = run_win_cmd(LLM_EXE, base + ["--top-k", "1", "--temp", "5.0", "--seed", "77"], timeout_s=60)
        rrp = run_win_cmd(LLM_EXE, base + ["--repeat-penalty", "100.0"], timeout_s=60)
        rs1 = run_win_cmd(LLM_EXE, base + ["--top-p", "0.5", "--min-p", "0.1", "--temp", "1.2", "--seed", "42"], timeout_s=60)
        rs2 = run_win_cmd(LLM_EXE, base + ["--top-p", "0.5", "--min-p", "0.1", "--temp", "1.2", "--seed", "42"], timeout_s=60)
        rs3 = run_win_cmd(LLM_EXE, base + ["--top-p", "0.5", "--min-p", "0.1", "--temp", "1.2", "--seed", "99"], timeout_s=60)
        stream_cmd = ([LLM_EXE] if sys.platform == "win32" else [CMD_EXE, "/c", to_rel_win(LLM_EXE)]) + base + ["--stream"]
        rstr_b = subprocess.run(stream_cmd, cwd=REPO_ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
        rstr_out = rstr_b.stdout.decode("utf-8", errors="replace")
        rstr_err = rstr_b.stderr.decode("utf-8", errors="replace")

        for r in (rg, rtk1, rrp, rs1, rs2, rs3):
            self.assertEqual(r.returncode, 0, r.stderr + r.stdout)
        self.assertEqual(rstr_b.returncode, 0, rstr_err + rstr_out)

        ids_g = parse_token_ids(rg.stdout)
        self.assertEqual(parse_token_ids(rtk1.stdout), ids_g, "--top-k 1 --temp 5.0 must match greedy")
        self.assertNotEqual(parse_token_ids(rrp.stdout), ids_g, "--repeat-penalty 100.0 failed to break repetition")
        self.assertEqual(parse_token_ids(rs1.stdout), parse_token_ids(rs2.stdout), "Seeded top-p/min-p must be deterministic")
        self.assertNotEqual(parse_token_ids(rs1.stdout), parse_token_ids(rs3.stdout), "Different seeds must vary sampling")
        self.assertIn("GPU_LLM_STREAM ", rstr_out)
        self.assertEqual(parse_token_ids(rstr_out), ids_g)

    def test_07_v2_ef0_negative_controls(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR) as tmpdir, open(QWEN2_GGUF, "rb") as fq, open(Q4_0_GGUF, "rb") as fq4, open(BF16_GGUF, "rb") as fb16:
            orig_qwen = fq.read()
            orig_q40 = fq4.read()
            orig_bf16 = fb16.read()

            # V2-NC-1: Mutated Q4_K payload with Tier B PTX fails closed with AOT_FINGERPRINT_MISMATCH
            nc1_gguf = os.path.join(tmpdir, "nc1_q4k_mut.gguf")
            mut1 = bytearray(orig_qwen)
            mut1[-64] ^= 0x01
            with open(nc1_gguf, "wb") as f:
                f.write(mut1)
            r1 = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(QWEN2_PTX), "--model", to_rel_win(nc1_gguf), "--prompt", "ZCC", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r1.returncode, 0)
            self.assertIn("AOT_FINGERPRINT_MISMATCH", r1.stderr + r1.stdout)

            # V2-NC-2: Unaligned Q4_K inner dim (128 instead of 256) rejected by BOTH compiler & runtime
            nc2_gguf = os.path.join(tmpdir, "nc2_q4k_align.gguf")
            key_q = b"blk.0.attn_q.weight"
            kidx = orig_qwen.find(key_q)
            self.assertGreater(kidx, 0)
            mut2 = bytearray(orig_qwen)
            struct.pack_into("<Q", mut2, kidx + len(key_q) + 4, 128)
            with open(nc2_gguf, "wb") as f:
                f.write(mut2)
            r2_c = subprocess.run(
                [ZCC_BIN, "--target=ptx", "--gpu-arch=sm_120", "--model", to_rel_posix(nc2_gguf), "-o", to_rel_posix(os.path.join(tmpdir, "nc2.ptx"))],
                cwd=REPO_ROOT, capture_output=True, text=True, timeout=60,
            )
            r2_r = run_win_cmd(LLM_EXE, ["--model", to_rel_win(nc2_gguf), "--prompt", "ZCC", "--steps", "1"], timeout_s=60)
            self.assertNotEqual(r2_c.returncode, 0)
            self.assertNotEqual(r2_r.returncode, 0)
            self.assertIn("Q4_K", r2_c.stderr + r2_c.stdout)
            self.assertIn("Q4_K", r2_r.stderr + r2_r.stdout)

            # V2-NC-3: Corrupted ZCCAOT02 magic rejected with AOT_PTX_ERROR
            nc3_ptx = os.path.join(tmpdir, "nc3_bad_v2.ptx")
            with open(QWEN2_PTX, "r", encoding="utf-8") as f:
                ptx_s = f.read()
            bad_ptx = ptx_s.replace("0x5a4343414f543032", "0xbadcafedeadbeef")
            self.assertNotEqual(bad_ptx, ptx_s)
            with open(nc3_ptx, "w", encoding="utf-8", newline="\n") as f:
                f.write(bad_ptx)
            r3 = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(nc3_ptx), "--model", to_rel_win(QWEN2_GGUF), "--prompt", "ZCC", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r3.returncode, 0)
            self.assertIn("AOT_PTX_ERROR", r3.stderr + r3.stdout)

            # V2-NC-4: Unsupported architecture rejected by BOTH compiler & runtime
            nc4_gguf = os.path.join(tmpdir, "nc4_bad_arch.gguf")
            mut4 = orig_bf16.replace(b"\x05\x00\x00\x00\x00\x00\x00\x00llama", b"\x05\x00\x00\x00\x00\x00\x00\x00gptnx", 1)
            self.assertNotEqual(mut4, orig_bf16)
            with open(nc4_gguf, "wb") as f:
                f.write(mut4)
            r4_c = subprocess.run(
                [ZCC_BIN, "--target=ptx", "--gpu-arch=sm_120", "--model", to_rel_posix(nc4_gguf), "-o", to_rel_posix(os.path.join(tmpdir, "nc4.ptx"))],
                cwd=REPO_ROOT, capture_output=True, text=True, timeout=60,
            )
            r4_r = run_win_cmd(LLM_EXE, ["--model", to_rel_win(nc4_gguf), "--prompt", "ZCC", "--steps", "1"], timeout_s=60)
            self.assertNotEqual(r4_c.returncode, 0)
            self.assertNotEqual(r4_r.returncode, 0)
            self.assertIn("unsupported architecture", r4_c.stderr + r4_c.stdout)
            self.assertIn("unsupported architecture", r4_r.stderr + r4_r.stdout)

            # V2-NC-5: Invalid sampler & --ctx flags fail closed with INVALID_ARG
            for bad_args in (
                ["--top-k", "-1"],
                ["--top-p", "1.5"],
                ["--top-p", "0.0"],
                ["--min-p", "-0.1"],
                ["--repeat-penalty", "0.0"],
                ["--ctx", "0"],
            ):
                r5 = run_win_cmd(LLM_EXE, ["--model", to_rel_win(BF16_GGUF), "--prompt", "ZCC", "--steps", "1"] + bad_args, timeout_s=60)
                self.assertNotEqual(r5.returncode, 0, f"V2-NC-5 failed on {bad_args}")
                self.assertIn("INVALID_ARG", r5.stderr + r5.stdout)

            # V2-NC-6: Zeroing blk.0.attn_q.bias causes measurable logit divergence (> 0.01)
            nc6_gguf = os.path.join(tmpdir, "nc6_zero_bias.gguf")
            bq_key = b"blk.0.attn_q.bias"
            bidx = orig_qwen.find(bq_key)
            self.assertGreater(bidx, 0)
            b_nd = struct.unpack_from("<I", orig_qwen, bidx + len(bq_key))[0]
            b_off = struct.unpack_from("<Q", orig_qwen, bidx + len(bq_key) + 4 + 8 * b_nd + 4)[0]
            # Locate data_start from reference parser
            end_hdr_off = orig_qwen.find(b"output.weight")
            self.assertGreater(end_hdr_off, 0)
            align_pad = 32
            # Compute data_start via parsing header cleanly
            from tools.generate_gguf_v2_fixtures import _rd_str, _rd_u32, _rd_u64, _rd_val
            p_off = 24
            _, _, n_ten, n_kv = struct.unpack_from("<IIQQ", orig_qwen, 0)
            for _ in range(n_kv):
                _, p_off = _rd_str(orig_qwen, p_off)
                vt, p_off = _rd_u32(orig_qwen, p_off)
                _, p_off = _rd_val(orig_qwen, p_off, vt)
            for _ in range(n_ten):
                _, p_off = _rd_str(orig_qwen, p_off)
                nd_t, p_off = _rd_u32(orig_qwen, p_off)
                for _ in range(nd_t):
                    _, p_off = _rd_u64(orig_qwen, p_off)
                _, p_off = _rd_u32(orig_qwen, p_off)
                _, p_off = _rd_u64(orig_qwen, p_off)
            d_start = (p_off + align_pad - 1) & ~(align_pad - 1)
            mut6 = bytearray(orig_qwen)
            mut6[d_start + b_off : d_start + b_off + 256 * 4] = b"\x00" * (256 * 4)
            with open(nc6_gguf, "wb") as f:
                f.write(mut6)
            d_nc6 = os.path.join(tmpdir, "nc6.bin")
            r6 = run_win_cmd(
                LLM_EXE,
                ["--model", to_rel_win(nc6_gguf), "--prompt", "ZCC compiles Blackwell Qwen2!", "--chat", "--steps", "1", "--dump-logits", to_rel_win(d_nc6)],
                timeout_s=60,
            )
            self.assertEqual(r6.returncode, 0, r6.stderr + r6.stdout)
            ref_q = reference_forward_gguf_v2(QWEN2_GGUF, reference_tokenize_gguf(QWEN2_GGUF, "ZCC compiles Blackwell Qwen2!", chat=True), 1, ignore_eos=True)
            self.assertGreater(max(abs(a - b) for a, b in zip(read_f32_bin(d_nc6, 384), ref_q["prompt_logits"])), 0.01)

            # V2-NC-7: Mutating Q4_0 and BF16 payload causes measurable logit divergence (> 0.01)
            for tag, raw_b, clean_p, pr_s in (
                ("q40", orig_q40, Q4_0_GGUF, "ZCC Blackwell @#!"),
                ("bf16", orig_bf16, BF16_GGUF, "ZCC Tensor PTX "),
            ):
                nc7_p = os.path.join(tmpdir, f"nc7_{tag}.gguf")
                nc7_b = os.path.join(tmpdir, f"nc7_{tag}.bin")
                mut7 = bytearray(raw_b)
                for off in range(len(mut7) - 512, len(mut7) - 64, 4):
                    mut7[off] ^= 0x55
                with open(nc7_p, "wb") as f:
                    f.write(mut7)
                r7 = run_win_cmd(
                    LLM_EXE,
                    ["--model", to_rel_win(nc7_p), "--prompt", pr_s, "--steps", "1", "--dump-logits", to_rel_win(nc7_b)],
                    timeout_s=60,
                )
                self.assertEqual(r7.returncode, 0, r7.stderr + r7.stdout)
                ref7 = reference_forward_gguf_v2(clean_p, reference_tokenize_gguf(clean_p, pr_s), 1, ignore_eos=True)
                self.assertGreater(max(abs(a - b) for a, b in zip(read_f32_bin(nc7_b, 384), ref7["prompt_logits"])), 0.01)


if __name__ == "__main__":
    unittest.main()
