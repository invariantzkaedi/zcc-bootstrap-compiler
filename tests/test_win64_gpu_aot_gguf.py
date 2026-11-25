#!/usr/bin/env python3
"""
Sovereign Native GGUF -> Blackwell SM 12.0 PTX AOT Model Compiler Verification Suite
===================================================================================
Tests (ZCC-EF-0 & ZCC-EF-7 Evidence Tier E2):
  1. Triple-compiler AOT GGUF->PTX byte identity (./zcc, zcc_sov_stage3.exe, zcc.wasm)
     + dead-format kernel elimination verification across F32, F16-TC, and Q8_0.
  2. Live RTX 5070 F32 AOT single-launch megakernel differential vs pure-Python oracle
     (max_abs_diff < 1e-4, 44/44 greedy token parity, launches_per_token=1).
  3. Live RTX 5070 F16 Tensor Core AOT megakernel differential vs oracle (< 1e-2)
     and byte-level agreement check vs generic Tensor Core path.
  4. Live RTX 5070 Q8_0 AOT megakernel differential vs pure-Python oracle (< 2e-3).
  5. Live RTX 5070 AOT fused single-launch vs generic multi-launch speedup benchmark.
  6. ZCC-EF-0 adversarial negative controls (AOT-NC-1..AOT-NC-5).
"""

import os
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
    ZCC_BIN,
    ZCC_WIN64,
    run_fc_b,
    run_win_cmd,
    sha256_file,
    to_rel_posix,
    to_rel_win,
)
from tools.generate_sovereign_gguf import reference_forward_gguf
from tests.test_wasm_wasi_selfhost import ZCC_WASM, find_node_bin, run_node_wasi
from tests.test_win64_gpu_llm import (
    GGUF_F16,
    GGUF_F32,
    GGUF_Q8_0,
    LLM_EXE,
    extract_prefixed_kv,
    extract_quoted_field,
    parse_token_ids,
    read_f32_bin,
)

AOT_PTX_F32 = os.path.join(BUILD_DIR, "zcc_tiny_llama_f32_sm120.ptx")
AOT_PTX_F16 = os.path.join(BUILD_DIR, "zcc_tiny_llama_f16_sm120.ptx")
AOT_PTX_Q8_0 = os.path.join(BUILD_DIR, "zcc_tiny_llama_q8_0_sm120.ptx")


class TestWin64GpuAotGguf(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (
            ZCC_BIN,
            ZCC_WIN64,
            ZCC_WASM,
            LLM_EXE,
            GGUF_F32,
            GGUF_F16,
            GGUF_Q8_0,
            AOT_PTX_F32,
            AOT_PTX_F16,
            AOT_PTX_Q8_0,
        )
        missing = [p for p in required if not os.path.isfile(p)]
        if missing:
            raise unittest.SkipTest(
                f"Missing required Track 8 artifacts ({missing}); run `python3 tools/build_gpu_llm_win64.py` first."
            )

    def test_01_aot_gguf_triple_compiler_byte_identity_and_dead_format_elim(self) -> None:
        node_bin = find_node_bin()
        cases = (
            (
                "f32",
                GGUF_F32,
                AOT_PTX_F32,
                ("zcc_aot_embed_f32", "zcc_aot_matvec_f32", ".visible .entry zcc_aot_model_meta", ".visible .entry zcc_aot_forward_token"),
                ("zcc_aot_embed_f16", "zcc_aot_embed_q8_0", "zcc_aot_matvec_f16", "zcc_aot_matvec_q8_0", "zcc_aot_gemm_tc_f16"),
            ),
            (
                "f16",
                GGUF_F16,
                AOT_PTX_F16,
                ("zcc_aot_embed_f16", "zcc_aot_f32_to_f16", "zcc_aot_gemm_tc_f16", "mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32"),
                ("zcc_aot_embed_f32", "zcc_aot_embed_q8_0", "zcc_aot_matvec_f32", "zcc_aot_matvec_f16", "zcc_aot_matvec_q8_0"),
            ),
            (
                "q8_0",
                GGUF_Q8_0,
                AOT_PTX_Q8_0,
                ("zcc_aot_embed_q8_0", "zcc_aot_matvec_q8_0", ".visible .entry zcc_aot_model_meta", ".visible .entry zcc_aot_forward_token"),
                ("zcc_aot_embed_f32", "zcc_aot_embed_f16", "zcc_aot_matvec_f32", "zcc_aot_matvec_f16", "zcc_aot_gemm_tc_f16"),
            ),
        )
        for tag, gguf_p, aot_win_ptx, req_syms, banned_syms in cases:
            host_ptx = os.path.join(BUILD_DIR, f"test_aot_{tag}_host.ptx")
            wasm_ptx = os.path.join(BUILD_DIR, f"test_aot_{tag}_wasm.ptx")
            try:
                r_host = subprocess.run(
                    [ZCC_BIN, "--target=ptx", "--gpu-arch=sm_120", "--model", to_rel_posix(gguf_p), "-o", to_rel_posix(host_ptx)],
                    cwd=REPO_ROOT,
                    capture_output=True,
                    text=True,
                    timeout=60,
                )
                self.assertEqual(r_host.returncode, 0, r_host.stderr)

                r_wasm = run_node_wasi(
                    node_bin,
                    ZCC_WASM,
                    ["zcc", "--target=ptx", "--gpu-arch=sm_120", "--model", to_rel_posix(gguf_p), "-o", to_rel_posix(wasm_ptx)],
                )
                self.assertEqual(r_wasm.returncode, 0, r_wasm.stderr)

                sha_h = sha256_file(host_ptx)
                sha_w = sha256_file(aot_win_ptx)
                sha_ws = sha256_file(wasm_ptx)
                self.assertEqual(sha_h, sha_w, f"{tag}: ./zcc vs zcc_sov_stage3.exe AOT PTX mismatch")
                self.assertEqual(sha_h, sha_ws, f"{tag}: ./zcc vs WASI zcc.wasm AOT PTX mismatch")

                fc = run_fc_b(host_ptx, aot_win_ptx)
                self.assertEqual(fc.returncode, 0, fc.stdout)
                self.assertIn("no differences encountered", fc.stdout.lower())

                with open(aot_win_ptx, "r", encoding="utf-8") as f:
                    ptx_text = f.read()
                self.assertIn(".version 8.7", ptx_text)
                self.assertIn(".target sm_120", ptx_text)
                for sym in req_syms:
                    self.assertIn(sym, ptx_text, f"{tag}: missing expected AOT PTX symbol {sym!r}")
                for sym in banned_syms:
                    self.assertNotIn(sym, ptx_text, f"{tag}: dead-format elimination failed, found unused {sym!r}")
            finally:
                for tmp in (host_ptx, wasm_ptx):
                    if os.path.exists(tmp):
                        os.remove(tmp)

    def test_02_aot_live_rtx5070_f32_differential_vs_python_oracle(self) -> None:
        prompt = "Blackwell "
        steps = 44
        dump_win = r"build_win64\test_aot_f32_logits.bin"
        dump_posix = os.path.join(BUILD_DIR, "test_aot_f32_logits.bin")
        try:
            res = run_win_cmd(
                LLM_EXE,
                [
                    "--aot-ptx",
                    to_rel_win(AOT_PTX_F32),
                    "--model",
                    to_rel_win(GGUF_F32),
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
            meta = extract_prefixed_kv(res.stdout, "GPU_LLM_AOT_META")
            self.assertEqual(meta["launches_per_token"], "1")
            mdl = extract_prefixed_kv(res.stdout, "GPU_LLM_MODEL")
            self.assertEqual(mdl["mode"], "aot_fused")
            self.assertEqual(mdl["wtype"], "F32")

            gpu_logits = read_f32_bin(dump_posix, 128)
            gpu_ids = parse_token_ids(res.stdout)
            ref = reference_forward_gguf(GGUF_F32, [ord(c) for c in prompt], steps, quantize_act_f16=False)
            max_diff = max(abs(a - b) for a, b in zip(gpu_logits, ref["prompt_logits"]))
            self.assertLess(max_diff, 1e-4, f"AOT F32 logit drift too large: {max_diff}")
            self.assertEqual(gpu_ids, ref["generated_ids"])
        finally:
            if os.path.exists(dump_posix):
                os.remove(dump_posix)

    def test_03_aot_live_rtx5070_f16_tensor_core_differential_and_parity(self) -> None:
        prompt = "ZCC compiles "
        steps = 44
        dump_aot_win = r"build_win64\test_aot_f16_logits.bin"
        dump_aot_posix = os.path.join(BUILD_DIR, "test_aot_f16_logits.bin")
        dump_gen_win = r"build_win64\test_gen_f16_logits.bin"
        dump_gen_posix = os.path.join(BUILD_DIR, "test_gen_f16_logits.bin")
        try:
            r_aot = run_win_cmd(
                LLM_EXE,
                [
                    "--aot-ptx",
                    to_rel_win(AOT_PTX_F16),
                    "--model",
                    to_rel_win(GGUF_F16),
                    "--prompt",
                    prompt,
                    "--steps",
                    str(steps),
                    "--dump-logits",
                    dump_aot_win,
                ],
                timeout_s=120,
            )
            r_gen = run_win_cmd(
                LLM_EXE,
                [
                    "--model",
                    to_rel_win(GGUF_F16),
                    "--prompt",
                    prompt,
                    "--steps",
                    str(steps),
                    "--dump-logits",
                    dump_gen_win,
                ],
                timeout_s=120,
            )
            for r in (r_aot, r_gen):
                self.assertEqual(r.returncode, 0, r.stderr + r.stdout)

            meta = extract_prefixed_kv(r_aot.stdout, "GPU_LLM_AOT_META")
            self.assertEqual(meta["launches_per_token"], "1")
            mdl = extract_prefixed_kv(r_aot.stdout, "GPU_LLM_MODEL")
            self.assertEqual(mdl["mode"], "aot_fused")
            self.assertEqual(mdl["tensor_core"], "1")

            aot_logits = read_f32_bin(dump_aot_posix, 128)
            gen_logits = read_f32_bin(dump_gen_posix, 128)
            aot_ids = parse_token_ids(r_aot.stdout)
            gen_ids = parse_token_ids(r_gen.stdout)

            ref = reference_forward_gguf(GGUF_F16, [ord(c) for c in prompt], steps, quantize_act_f16=True)
            max_oracle_diff = max(abs(a - b) for a, b in zip(aot_logits, ref["prompt_logits"]))
            max_gen_diff = max(abs(a - b) for a, b in zip(aot_logits, gen_logits))
            self.assertLess(max_oracle_diff, 1e-2, f"AOT F16-TC vs oracle drift: {max_oracle_diff}")
            self.assertLess(max_gen_diff, 1e-4, f"AOT F16-TC vs generic TC drift: {max_gen_diff}")
            self.assertEqual(aot_ids, ref["generated_ids"])
            self.assertEqual(aot_ids, gen_ids)
        finally:
            for p in (dump_aot_posix, dump_gen_posix):
                if os.path.exists(p):
                    os.remove(p)

    def test_04_aot_live_rtx5070_q8_0_differential(self) -> None:
        prompt = "RTX 5070 "
        steps = 42
        dump_win = r"build_win64\test_aot_q8_logits.bin"
        dump_posix = os.path.join(BUILD_DIR, "test_aot_q8_logits.bin")
        try:
            res = run_win_cmd(
                LLM_EXE,
                [
                    "--aot-ptx",
                    to_rel_win(AOT_PTX_Q8_0),
                    "--model",
                    to_rel_win(GGUF_Q8_0),
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
            self.assertEqual(mdl["mode"], "aot_fused")
            self.assertEqual(mdl["wtype"], "Q8_0")
            gen_txt = extract_quoted_field(res.stdout, "GPU_LLM_TEXT", "generated")
            self.assertEqual(gen_txt, "Tensor Core mma executes without CUDA SDK.")

            gpu_logits = read_f32_bin(dump_posix, 128)
            gpu_ids = parse_token_ids(res.stdout)
            ref = reference_forward_gguf(GGUF_Q8_0, [ord(c) for c in prompt], steps, quantize_act_f16=False)
            max_diff = max(abs(a - b) for a, b in zip(gpu_logits, ref["prompt_logits"]))
            self.assertLess(max_diff, 2e-3, f"AOT Q8_0 logit drift too large: {max_diff}")
            self.assertEqual(gpu_ids, ref["generated_ids"])
        finally:
            if os.path.exists(dump_posix):
                os.remove(dump_posix)

    def test_05_aot_vs_generic_throughput_speedup(self) -> None:
        for tag, model_win, ptx_win, prompt in (
            ("F16", to_rel_win(GGUF_F16), to_rel_win(AOT_PTX_F16), "ZCC compiles "),
            ("Q8_0", to_rel_win(GGUF_Q8_0), to_rel_win(AOT_PTX_Q8_0), "RTX 5070 "),
        ):
            gen_tps = []
            aot_tps = []
            for _ in range(3):
                rg = run_win_cmd(
                    LLM_EXE,
                    ["--model", model_win, "--prompt", prompt, "--steps", "48"],
                    timeout_s=120,
                )
                ra = run_win_cmd(
                    LLM_EXE,
                    ["--aot-ptx", ptx_win, "--model", model_win, "--prompt", prompt, "--steps", "48"],
                    timeout_s=120,
                )
                self.assertEqual(rg.returncode, 0, rg.stderr + rg.stdout)
                self.assertEqual(ra.returncode, 0, ra.stderr + ra.stdout)
                gen_tps.append(float(extract_prefixed_kv(rg.stdout, "GPU_LLM_OK")["tok_per_sec"]))
                aot_tps.append(float(extract_prefixed_kv(ra.stdout, "GPU_LLM_OK")["tok_per_sec"]))
            med_gen = statistics.median(gen_tps)
            med_aot = statistics.median(aot_tps)
            self.assertGreater(
                med_aot,
                med_gen * 1.25,
                f"{tag}: expected clear AOT speedup (>1.25x), got AOT={med_aot:.2f} tok/s vs Generic={med_gen:.2f} tok/s",
            )

    def test_06_aot_ef0_negative_controls(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR) as tmpdir:
            rel_tmp = os.path.basename(tmpdir)
            with open(GGUF_F16, "rb") as fin:
                orig_f16 = fin.read()

            # AOT-NC-1: Mutated weight payload triggers AOT_FINGERPRINT_MISMATCH before inference
            nc1_gguf = os.path.join(tmpdir, "nc1_mut_f16.gguf")
            mut_w = bytearray(orig_f16)
            mut_w[-64] ^= 0x01
            with open(nc1_gguf, "wb") as fout:
                fout.write(mut_w)
            r1 = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(AOT_PTX_F16), "--model", rf"build_win64\{rel_tmp}\nc1_mut_f16.gguf", "--prompt", "ZCC ", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r1.returncode, 0, "AOT-NC-1 failed to reject weight mutation via fingerprint")
            self.assertIn("AOT_FINGERPRINT_MISMATCH", r1.stderr + r1.stdout)

            # AOT-NC-2: Cross-model pairing (F16 AOT PTX paired with Q8_0 GGUF) must fail closed
            r2 = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", to_rel_win(AOT_PTX_F16), "--model", to_rel_win(GGUF_Q8_0), "--prompt", "RTX ", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r2.returncode, 0, "AOT-NC-2 failed to reject cross-model AOT pairing")
            self.assertIn("AOT_FINGERPRINT_MISMATCH", r2.stderr + r2.stdout)

            # AOT-NC-3: Corrupted AOT PTX magic header must fail closed
            nc3_ptx = os.path.join(tmpdir, "nc3_bad_magic.ptx")
            with open(AOT_PTX_F16, "r", encoding="utf-8") as fin:
                ptx_src = fin.read()
            bad_ptx = ptx_src.replace("0x5a4343414f543031", "0xdeadbeefdeadbeef")
            self.assertNotEqual(bad_ptx, ptx_src)
            with open(nc3_ptx, "w", encoding="utf-8", newline="\n") as fout:
                fout.write(bad_ptx)
            r3 = run_win_cmd(
                LLM_EXE,
                ["--aot-ptx", rf"build_win64\{rel_tmp}\nc3_bad_magic.ptx", "--model", to_rel_win(GGUF_F16), "--prompt", "ZCC ", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r3.returncode, 0, "AOT-NC-3 failed to reject corrupted ZCCAOT01 magic")
            self.assertIn("AOT_PTX_ERROR", r3.stderr + r3.stdout)

            # AOT-NC-4: Nonexistent / corrupt .gguf passed directly to compiler leaves no output PTX
            nc4_out = os.path.join(tmpdir, "should_not_exist.ptx")
            r4 = subprocess.run(
                [ZCC_BIN, "--target=ptx", "--gpu-arch=sm_120", "--model", os.path.join(tmpdir, "missing.gguf"), "-o", to_rel_posix(nc4_out)],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=60,
            )
            self.assertNotEqual(r4.returncode, 0, "AOT-NC-4 compiler did not reject missing .gguf")
            self.assertFalse(os.path.exists(nc4_out), "AOT-NC-4 left partial PTX on failed compilation")

            # AOT-NC-5: Non-power-of-two general.alignment rejected by AOT GGUF frontend
            nc5_gguf = os.path.join(tmpdir, "nc5_bad_align.gguf")
            nc5_out = os.path.join(tmpdir, "nc5_should_not_exist.ptx")
            al_idx = orig_f16.find(b"general.alignment")
            self.assertGreater(al_idx, 0)
            mut_al = bytearray(orig_f16)
            struct.pack_into("<I", mut_al, al_idx + len(b"general.alignment") + 4, 12)
            with open(nc5_gguf, "wb") as fout:
                fout.write(mut_al)
            r5 = subprocess.run(
                [ZCC_BIN, "--target=ptx", "--gpu-arch=sm_120", "--model", to_rel_posix(nc5_gguf), "-o", to_rel_posix(nc5_out)],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=60,
            )
            self.assertNotEqual(r5.returncode, 0, "AOT-NC-5 compiler failed to reject non-power-of-two alignment")
            self.assertIn("non-power-of-two", r5.stderr + r5.stdout)
            self.assertFalse(os.path.exists(nc5_out))


if __name__ == "__main__":
    unittest.main()
