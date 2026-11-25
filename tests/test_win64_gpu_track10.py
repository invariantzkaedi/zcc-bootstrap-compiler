#!/usr/bin/env python3
"""
Track 10 Verification Suite: Blackwell Bandwidth, FP16/Q8_0 KV, TC Prefill & REPL
===================================================================================
Tests (ZCC-EF-0 & ZCC-EF-7 Evidence Tier E2):
  1. Triple-compiler Tier B ZCCAOT02 PTX byte identity on the ~87 MB real-scale
     Qwen2.5 Q4_K_M model + verification of 4-warp/CTA, ld.v4, FP16/Q8_0 KV, and
     batched SM 12.0 Tensor Core prefill opcodes + dead-format elimination.
  2. Live RTX 5070 `--kv-type f16` (2.00x) and `--kv-type q8_0` (3.76x) KV cache
     quantization across Tier B CUDA Graph (`launches_per_token=1`) and Generic mode,
     with NumPy differential oracle accuracy + 24/24 token parity.
  3. Batched SM 12.0 Tensor Core prefill (`--prefill-batch 128` vs `--prefill-batch 1`)
     on >= 96-token prompt measuring > 4.0x speedup + tile-count invariance + Generic
     vs Tier B accuracy parity.
  4. Real-scale ~87 MB (L2-exceeding) Blackwell GDDR7 bandwidth & 0.0 CUDA Graph
     vs --no-cuda-graph parity.
  5. Persistent multi-turn `--interactive` REPL causality across turns, `/reset`
     KV clearing, `--system`, and non-fatal `GPU_LLM_CTX_FULL` recovery.
  6. Strict System32 `{msvcrt,nvcuda}.dll` PE32+ import audit & SHA-256/`fc /b`
     sovereign host-vs-Win64 binary identity.
  7. ZCC-EF-0 adversarial negative controls (`T10-NC-1..T10-NC-5`).
"""

import os
import re
import statistics
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
from tools.generate_gguf_track10_bench import (
    reference_forward_track10,
    reference_tokenize_track10,
)
from tests.test_wasm_wasi_selfhost import ZCC_WASM, find_node_bin, run_node_wasi
from tests.test_win64_gpu_llm import (
    GGUF_F16,
    GGUF_F32,
    LLM_EXE,
    LLM_HOST_EXE,
    extract_prefixed_kv,
    parse_token_ids,
    read_f32_bin,
)

REAL_GGUF = os.path.join(BUILD_DIR, "zcc_hf_qwen2_real_scale_q4km.gguf")
REAL_PTX = os.path.join(BUILD_DIR, "zcc_hf_qwen2_real_scale_q4km_sm120.ptx")
QWEN2_20L_GGUF = os.path.join(BUILD_DIR, "zcc_hf_qwen2_20l_q4km.gguf")
QWEN2_20L_PTX = os.path.join(BUILD_DIR, "zcc_hf_qwen2_20l_q4km_sm120.ptx")
TINY_F16_PTX = os.path.join(BUILD_DIR, "zcc_tiny_llama_f16_sm120.ptx")


def _extract_all_prefixed_kv(stdout: str, prefix: str) -> list[dict[str, str]]:
    out = []
    for line in stdout.splitlines():
        s = line.strip()
        if s == prefix or s.startswith(prefix + " "):
            kv: dict[str, str] = {}
            for k, v in re.findall(r'([A-Za-z0-9_]+)=("[^"]*"|\S+)', s[len(prefix):]):
                kv[k] = v[1:-1] if (v.startswith('"') and v.endswith('"')) else v
            out.append(kv)
    return out


class TestWin64GpuTrack10(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (
            ZCC_BIN,
            ZCC_WIN64,
            ZCC_WASM,
            LLM_EXE,
            LLM_HOST_EXE,
            REAL_GGUF,
            REAL_PTX,
            QWEN2_20L_GGUF,
            QWEN2_20L_PTX,
            GGUF_F16,
            GGUF_F32,
            TINY_F16_PTX,
        )
        missing = [p for p in required if not os.path.isfile(p)]
        if missing:
            raise unittest.SkipTest(
                f"Missing Track 10 artifacts ({missing}); run `make build-gpu-track10-win64` first."
            )

    def test_01_track10_triple_compiler_ptx_parity_and_opcodes(self) -> None:
        node_bin = find_node_bin()
        host_ptx = os.path.join(BUILD_DIR, "test_t10_real_host.ptx")
        wasm_ptx = os.path.join(BUILD_DIR, "test_t10_real_wasm.ptx")
        try:
            rh = subprocess.run(
                [ZCC_BIN, "--target=ptx", "--gpu-arch=sm_120", "--model", to_rel_posix(REAL_GGUF), "-o", to_rel_posix(host_ptx)],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=90,
            )
            self.assertEqual(rh.returncode, 0, rh.stderr)
            rw = run_node_wasi(
                node_bin,
                ZCC_WASM,
                ["zcc", "--target=ptx", "--gpu-arch=sm_120", "--model", to_rel_posix(REAL_GGUF), "-o", to_rel_posix(wasm_ptx)],
            )
            self.assertEqual(rw.returncode, 0, rw.stderr)

            sha_h = sha256_file(host_ptx)
            sha_w = sha256_file(REAL_PTX)
            sha_ws = sha256_file(wasm_ptx)
            self.assertEqual(sha_h, sha_w, "./zcc vs zcc_sov_stage3.exe Track 10 PTX mismatch")
            self.assertEqual(sha_h, sha_ws, "./zcc vs WASI zcc.wasm Track 10 PTX mismatch")

            fc = run_fc_b(host_ptx, REAL_PTX)
            self.assertEqual(fc.returncode, 0, fc.stdout)
            self.assertIn("no differences encountered", fc.stdout.lower())

            with open(REAL_PTX, "r", encoding="utf-8") as f:
                ptx_text = f.read()

            required_syms = (
                ".version 8.7",
                ".target sm_120",
                "0x5a4343414f543032",
                "ld.v4.u32",
                "ld.v4.f32",
                "shr.u32 %r5, %r4, 5; shl.b32 %r3, %r3, 2;",
                ".visible .entry zcc_aot_mc_matvec_q4_k(",
                ".visible .entry zcc_aot_mc_matvec_q6_k(",
                ".visible .entry zcc_aot_mc_rope_kv_f16(",
                ".visible .entry zcc_aot_mc_attn_gqa_f16(",
                ".visible .entry zcc_aot_mc_rope_inplace(",
                ".visible .entry zcc_aot_mc_kv_store_q8_0(",
                ".visible .entry zcc_aot_mc_attn_gqa_q8_0(",
                ".visible .entry zcc_aot_mc_dequant_q4_k_to_f16(",
                ".visible .entry zcc_aot_mc_dequant_q6_k_to_f16(",
                ".visible .entry zcc_aot_mc_prefill_tc_gemm(",
                "mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32",
                ".visible .entry zcc_aot_mc_rmsnorm_batch(",
                ".visible .entry zcc_aot_mc_add_bias_batch(",
                ".visible .entry zcc_aot_mc_rope_kv_batch_f16(",
                ".visible .entry zcc_aot_mc_kv_store_batch_q8_0(",
                ".visible .entry zcc_aot_mc_attn_gqa_batch_q8_0(",
                ".visible .entry zcc_aot_mc_swiglu_batch(",
                "0x0000000000000080",
            )
            for sym in required_syms:
                self.assertIn(sym, ptx_text, f"Missing Track 10 symbol/opcode: {sym!r}")

            banned_syms = (
                "zcc_aot_mc_matvec_f32",
                "zcc_aot_mc_matvec_f16",
                "zcc_aot_mc_matvec_bf16",
                "zcc_aot_mc_matvec_q4_0",
                "zcc_aot_mc_matvec_q8_0",
                "zcc_aot_mc_dequant_f32_to_f16",
                "zcc_aot_mc_dequant_bf16_to_f16",
                "zcc_aot_mc_dequant_q4_0_to_f16",
                "zcc_aot_mc_dequant_q8_0_to_f16",
                "zcc_aot_mc_gemm_tc_f16",
            )
            for sym in banned_syms:
                self.assertNotIn(sym, ptx_text, f"Dead-format elimination failed, found {sym!r}")
        finally:
            for tmp in (host_ptx, wasm_ptx):
                if os.path.exists(tmp):
                    os.remove(tmp)

    def test_02_kv_cache_f16_and_q8_0_savings_accuracy_and_generic_parity(self) -> None:
        prompt = "ZCC compiles Blackwell Qwen2!"
        steps = 24
        pids = reference_tokenize_track10(REAL_GGUF, prompt, chat=True)
        ref = reference_forward_track10(REAL_GGUF, pids, steps, ignore_eos=True)
        expected_ids = ref["gen_ids"]

        with tempfile.TemporaryDirectory(dir=BUILD_DIR) as tmpdir:
            dumps = {}
            gen_dumps = {}
            kv_bytes_map = {}
            for kv, expected_ratio, tol in (("f32", "1.00x", 1e-4), ("f16", "2.00x", 5e-3), ("q8_0", "3.76x", 0.1)):
                dp = os.path.join(tmpdir, f"aot_{kv}.bin")
                r = run_win_cmd(
                    LLM_EXE,
                    ["--aot-ptx", to_rel_win(REAL_PTX), "--model", to_rel_win(REAL_GGUF),
                     "--kv-type", kv, "--prompt", prompt, "--chat", "--ignore-eos",
                     "--steps", str(steps), "--dump-logits", to_rel_win(dp)],
                    timeout_s=120,
                )
                self.assertEqual(r.returncode, 0, r.stderr + r.stdout)
                meta = extract_prefixed_kv(r.stdout, "GPU_LLM_AOT_META")
                kv_line = extract_prefixed_kv(r.stdout, "GPU_LLM_KV")
                self.assertEqual(meta["launches_per_token"], "1")
                self.assertEqual(kv_line["type"], kv)
                self.assertEqual(kv_line["ratio"], expected_ratio)
                kv_bytes_map[kv] = int(kv_line["bytes"])
                self.assertEqual(parse_token_ids(r.stdout), expected_ids, f"AOT {kv} token parity failed")
                lg = read_f32_bin(dp, 512)
                dumps[kv] = lg
                max_diff = max(abs(a - b) for a, b in zip(lg, ref["prompt_logits"]))
                self.assertLess(max_diff, tol, f"AOT {kv} logit drift {max_diff} >= {tol}")

                if kv in ("f16", "q8_0"):
                    gp = os.path.join(tmpdir, f"gen_{kv}.bin")
                    rg = run_win_cmd(
                        LLM_EXE,
                        ["--model", to_rel_win(REAL_GGUF), "--kv-type", kv,
                         "--prompt", prompt, "--chat", "--ignore-eos",
                         "--steps", str(steps), "--dump-logits", to_rel_win(gp)],
                        timeout_s=120,
                    )
                    self.assertEqual(rg.returncode, 0, rg.stderr + rg.stdout)
                    self.assertEqual(extract_prefixed_kv(rg.stdout, "GPU_LLM_KV")["ratio"], expected_ratio)
                    self.assertEqual(parse_token_ids(rg.stdout), expected_ids, f"Generic {kv} token parity failed")
                    lgg = read_f32_bin(gp, 512)
                    gen_dumps[kv] = lgg
                    cross_diff = max(abs(a - b) for a, b in zip(lg, lgg))
                    self.assertLess(cross_diff, 1.5e-3, f"{kv} AOT vs Generic drift={cross_diff}")

            self.assertEqual(kv_bytes_map["f32"], 12582912)
            self.assertEqual(kv_bytes_map["f16"], 6291456)
            self.assertEqual(kv_bytes_map["q8_0"], 3342336)

    def test_03_batched_sm120_tensor_core_prefill_speedup_and_parity(self) -> None:
        long_prompt = ("ZCC compiles Blackwell GPU Qwen2 Tensor Core PTX CUDA Graph zero toolchain "
                       "sovereign matrix verify benchmark hello world ") * 6
        with tempfile.TemporaryDirectory(dir=BUILD_DIR) as tmpdir:
            b1_times = []
            b128_times = []
            d_b1 = os.path.join(tmpdir, "pf_b1.bin")
            d_b64 = os.path.join(tmpdir, "pf_b64.bin")
            d_b128 = os.path.join(tmpdir, "pf_b128.bin")
            d_gen64 = os.path.join(tmpdir, "pf_gen64.bin")

            for rep in range(3):
                r1 = run_win_cmd(
                    LLM_EXE,
                    ["--aot-ptx", to_rel_win(REAL_PTX), "--model", to_rel_win(REAL_GGUF),
                     "--kv-type", "q8_0", "--prefill-batch", "1", "--prompt", long_prompt,
                     "--chat", "--ignore-eos", "--steps", "1"]
                    + (["--dump-logits", to_rel_win(d_b1)] if rep == 0 else []),
                    timeout_s=120,
                )
                r128 = run_win_cmd(
                    LLM_EXE,
                    ["--aot-ptx", to_rel_win(REAL_PTX), "--model", to_rel_win(REAL_GGUF),
                     "--kv-type", "q8_0", "--prefill-batch", "128", "--prompt", long_prompt,
                     "--chat", "--ignore-eos", "--steps", "1"]
                    + (["--dump-logits", to_rel_win(d_b128)] if rep == 0 else []),
                    timeout_s=120,
                )
                self.assertEqual(r1.returncode, 0, r1.stderr + r1.stdout)
                self.assertEqual(r128.returncode, 0, r128.stderr + r128.stdout)
                pf1 = extract_prefixed_kv(r1.stdout, "GPU_LLM_PREFILL")
                pf128 = extract_prefixed_kv(r128.stdout, "GPU_LLM_PREFILL")
                self.assertEqual(pf1["tc_prefill"], "0")
                self.assertEqual(pf128["tc_prefill"], "1")
                self.assertGreaterEqual(int(pf128["prompt_tokens"]), 96)
                b1_times.append(float(pf1["prefill_ms"]))
                b128_times.append(float(pf128["prefill_ms"]))
                if rep == 0:
                    ok1 = extract_prefixed_kv(r1.stdout, "GPU_LLM_OK")
                    ok128 = extract_prefixed_kv(r128.stdout, "GPU_LLM_OK")
                    self.assertEqual(ok1["top1_id"], ok128["top1_id"])

            med_b1 = statistics.median(b1_times)
            med_b128 = statistics.median(b128_times)
            speedup = med_b1 / med_b128
            self.assertGreater(
                speedup,
                4.0,
                f"Expected > 4.0x TC prefill speedup, got {speedup:.2f}x (B=1 {med_b1:.2f}ms vs B=128 {med_b128:.2f}ms)",
            )

            r64 = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(REAL_PTX), "--model", to_rel_win(REAL_GGUF),
                 "--kv-type", "q8_0", "--prefill-batch", "64", "--prompt", long_prompt,
                 "--chat", "--ignore-eos", "--steps", "1", "--dump-logits", to_rel_win(d_b64)],
                timeout_s=120,
            )
            rgen = run_win_cmd(
                LLM_EXE,
                ["--model", to_rel_win(REAL_GGUF), "--kv-type", "q8_0",
                 "--prefill-batch", "64", "--prompt", long_prompt,
                 "--chat", "--ignore-eos", "--steps", "1", "--dump-logits", to_rel_win(d_gen64)],
                timeout_s=120,
            )
            self.assertEqual(r64.returncode, 0, r64.stderr + r64.stdout)
            self.assertEqual(rgen.returncode, 0, rgen.stderr + rgen.stdout)

            lg1 = read_f32_bin(d_b1, 512)
            lg64 = read_f32_bin(d_b64, 512)
            lg128 = read_f32_bin(d_b128, 512)
            lggen = read_f32_bin(d_gen64, 512)

            self.assertLess(max(abs(a - b) for a, b in zip(lg1, lg128)), 5e-2)
            self.assertLess(max(abs(a - b) for a, b in zip(lg64, lg128)), 1e-4)
            self.assertLess(max(abs(a - b) for a, b in zip(lg64, lggen)), 1e-2)

    def test_04_real_scale_87mb_blackwell_bandwidth_and_graph_parity(self) -> None:
        self.assertGreater(os.path.getsize(REAL_GGUF), 80 * 1024 * 1024)
        prompt = "ZCC compiles Blackwell Qwen2!"
        steps = 48
        with tempfile.TemporaryDirectory(dir=BUILD_DIR) as tmpdir:
            d_gr = os.path.join(tmpdir, "rs_gr.bin")
            d_ng = os.path.join(tmpdir, "rs_ng.bin")
            r_gr = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(REAL_PTX), "--model", to_rel_win(REAL_GGUF),
                 "--kv-type", "f16", "--prompt", prompt, "--chat", "--ignore-eos",
                 "--steps", str(steps), "--dump-logits", to_rel_win(d_gr)],
                timeout_s=120,
            )
            r_ng = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(REAL_PTX), "--model", to_rel_win(REAL_GGUF),
                 "--kv-type", "f16", "--no-cuda-graph", "--prompt", prompt, "--chat", "--ignore-eos",
                 "--steps", str(steps), "--dump-logits", to_rel_win(d_ng)],
                timeout_s=120,
            )
            self.assertEqual(r_gr.returncode, 0, r_gr.stderr + r_gr.stdout)
            self.assertEqual(r_ng.returncode, 0, r_ng.stderr + r_ng.stdout)

            meta_gr = extract_prefixed_kv(r_gr.stdout, "GPU_LLM_AOT_META")
            meta_ng = extract_prefixed_kv(r_ng.stdout, "GPU_LLM_AOT_META")
            ok_gr = extract_prefixed_kv(r_gr.stdout, "GPU_LLM_OK")
            self.assertEqual(meta_gr["block_dim"], "128")
            self.assertEqual(meta_gr["launches_per_token"], "1")
            self.assertEqual(meta_ng["launches_per_token"], meta_gr["graph_nodes"])

            lg_gr = read_f32_bin(d_gr, 512)
            lg_ng = read_f32_bin(d_ng, 512)
            self.assertEqual(max(abs(a - b) for a, b in zip(lg_gr, lg_ng)), 0.0)
            self.assertEqual(parse_token_ids(r_gr.stdout), parse_token_ids(r_ng.stdout))

            tps = float(ok_gr["tok_per_sec"])
            wblob_b = float(meta_gr["wblob_bytes"])
            eff_gbps = (wblob_b / 1e9) * tps
            self.assertGreater(eff_gbps, 50.0, f"Expected > 50 GB/s effective model read bandwidth, got {eff_gbps:.2f} GB/s")

    def test_05_multi_turn_repl_kv_causality_reset_and_ctx_full(self) -> None:
        win_exe = to_rel_win(LLM_EXE)
        win_ptx = to_rel_win(QWEN2_20L_PTX)
        win_gguf = to_rel_win(QWEN2_20L_GGUF)
        cmd_a = f"{win_exe} --aot-ptx {win_ptx} --model {win_gguf} --kv-type q8_0 --prefill-batch 32 --system \"SoV\" --chat --ignore-eos --steps 6 --interactive"
        ra = subprocess.run(
            [CMD_EXE, "/c", cmd_a],
            cwd=REPO_ROOT,
            input="hello world\nZCC GPU\n/reset\nZCC GPU\n/quit\n",
            capture_output=True,
            text=True,
            timeout=90,
        )
        self.assertEqual(ra.returncode, 0, ra.stderr + ra.stdout)
        self.assertIn("GPU_LLM_INTERACTIVE_READY", ra.stdout)
        self.assertIn("GPU_LLM_REPL_RESET kv_cleared=1", ra.stdout)
        self.assertIn("GPU_LLM_INTERACTIVE_OK turns=1", ra.stdout)
        turns = _extract_all_prefixed_kv(ra.stdout, "GPU_LLM_TURN")
        self.assertEqual(len(turns), 3, f"Expected 3 turns in REPL output, got: {ra.stdout}")
        # Turn 1 ("ZCC GPU" with prior "hello world" context) MUST differ from Turn 2 ("ZCC GPU" fresh after /reset)
        self.assertGreater(int(turns[1]["pos"]), int(turns[2]["pos"]))
        self.assertNotEqual(turns[1]["logits_fnv"], turns[2]["logits_fnv"])

        # Context overflow recovery in interactive REPL
        cmd_b = f"{win_exe} --aot-ptx {win_ptx} --model {win_gguf} --kv-type f16 --ctx 32 --chat --ignore-eos --steps 4 --interactive"
        overflow_line = ("hello world benchmark matrix verify sovereign toolchain Blackwell Qwen2 GPU Tensor Core ") * 3
        rb = subprocess.run(
            [CMD_EXE, "/c", cmd_b],
            cwd=REPO_ROOT,
            input=f"{overflow_line}\n/reset\nZCC\n/quit\n",
            capture_output=True,
            text=True,
            timeout=90,
        )
        self.assertEqual(rb.returncode, 0, rb.stderr + rb.stdout)
        self.assertIn("GPU_LLM_CTX_FULL", rb.stdout)
        self.assertIn("GPU_LLM_REPL_RESET kv_cleared=1", rb.stdout)
        self.assertIn("GPU_LLM_INTERACTIVE_OK turns=1", rb.stdout)

    def test_06_sovereign_pe32plus_strict_audit_and_parity(self) -> None:
        self.assertEqual(sha256_file(LLM_HOST_EXE), sha256_file(LLM_EXE))
        fc = run_fc_b(LLM_HOST_EXE, LLM_EXE)
        self.assertEqual(fc.returncode, 0, fc.stdout)
        self.assertIn("no differences encountered", fc.stdout.lower())

        audit = audit_pe32plus_multi_dll(
            LLM_EXE,
            required_dlls={"msvcrt.dll", "nvcuda.dll"},
            allowed_dlls={"msvcrt.dll", "nvcuda.dll"},
        )
        self.assertNotIn("kernel32.dll", audit["dlls"])
        self.assertIn("fgets", audit["dlls"]["msvcrt.dll"])
        self.assertIn("cuMemGetInfo_v2", audit["dlls"]["nvcuda.dll"])
        self.assertIn("cuMemsetD8_v2", audit["dlls"]["nvcuda.dll"])

    def test_07_adversarial_negative_controls(self) -> None:
        # T10-NC-1: Invalid --kv-type fails closed
        for bad_kv in ("fp8", "int4", "q4_0"):
            r = run_win_cmd(LLM_EXE, ["--model", to_rel_win(QWEN2_20L_GGUF), "--kv-type", bad_kv], timeout_s=30)
            self.assertNotEqual(r.returncode, 0, f"Expected failure on --kv-type {bad_kv}")
            self.assertIn("INVALID_ARG", r.stderr)

        # T10-NC-2: Invalid --prefill-batch or missing trailing value fails closed
        for args in (
            ["--prefill-batch", "0"],
            ["--prefill-batch", "129"],
            ["--prefill-batch", "abc"],
            ["--kv-type"],
            ["--prefill-batch"],
        ):
            r = run_win_cmd(LLM_EXE, ["--model", to_rel_win(QWEN2_20L_GGUF)] + args, timeout_s=30)
            self.assertNotEqual(r.returncode, 0, f"Expected failure on {args}")
            self.assertIn("INVALID_ARG", r.stderr)

        # T10-NC-3: VRAM budget exceeded (--ctx 524288 f32 KV ~12.9 GB > 8 GB VRAM) fails closed
        r_vram = run_win_cmd(
            LLM_EXE,
            ["--aot-ptx", to_rel_win(REAL_PTX), "--model", to_rel_win(REAL_GGUF), "--kv-type", "f32", "--ctx", "524288"],
            timeout_s=30,
        )
        self.assertNotEqual(r_vram.returncode, 0)
        self.assertIn("VRAM_BUDGET_EXCEEDED", r_vram.stderr)

        # T10-NC-4: Non-F32 KV rejected in Tier A aot_fused mode
        r_ta = run_win_cmd(
            LLM_EXE,
            ["--aot-ptx", to_rel_win(TINY_F16_PTX), "--model", to_rel_win(GGUF_F16), "--kv-type", "f16"],
            timeout_s=30,
        )
        self.assertNotEqual(r_ta.returncode, 0)
        self.assertIn("INVALID_ARG", r_ta.stderr)

        # T10-NC-5: Q8_0 KV rejected when head_dim is not divisible by 32 (tiny_llama has head_dim=16)
        r_hd = run_win_cmd(
            LLM_EXE,
            ["--model", to_rel_win(GGUF_F32), "--kv-type", "q8_0"],
            timeout_s=30,
        )
        self.assertNotEqual(r_hd.returncode, 0)
        self.assertIn("INVALID_ARG", r_hd.stderr)


if __name__ == "__main__":
    unittest.main()
