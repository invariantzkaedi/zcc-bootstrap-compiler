#!/usr/bin/env python3
"""
Sovereign Zero-PyTorch Blackwell SM 12.0 GPU LoRA Fine-Tuner Gate (Track 19)
=============================================================================
Verification suite (ZCC-EF-0 & ZCC-EF-7 Evidence Tier E2):
  1. Dual-compiler PTX (`FC /B`) + Win64 PE32+ parity + strict System32 IAT
     `{msvcrt.dll, nvcuda.dll}`.
  2. Exact FP32 finite-difference gradient check (`--grad-check --fp32-matmul` on
     `zcc_tiny_llama_f32.gguf`: `max_rel_err < 5e-3`, `cos_sim > 0.9999`).
  3. Real 1.5B 28-layer Blackwell Tensor Core gradient check (`--grad-check` on
     `zkaedi-prime-dpo-q4_k_m.gguf`: `cos_sim > 0.99`).
  4. TinyLlama LoRA training convergence + binary `ZCCLORA1` save/reload parity.
  5. Live 1.5B Qwen2.5 `Q4_K_M` Obsidian-7 LoRA fine-tuning: before vs. after
     (`COBALT-FALCON-9472` + `Lyra Vance` learned cleanly while retaining `Paris`).
  6. ZCC-EF-0 negative controls (`LORA_MAGIC_ERROR`, `LORA_CHECKSUM_MISMATCH`,
     `LORA_TRUNCATED`, `LORA_ARCH_MISMATCH`, `INVALID_ARG`).
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_gpu_lora_win64 import (  # noqa: E402
    LORA_EXE_HOST_OUT,
    LORA_EXE_OUT,
    PTX_HOST_OUT,
    PTX_WIN64_OUT,
)
from tools.build_gpu_win64 import audit_pe32plus_multi_dll  # noqa: E402
from tools.build_lora_demo_corpus import DEFAULT_OUT as DEMO_SFT_PATH, generate_corpus  # noqa: E402
from tools.build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    ZCC_BIN,
    ZCC_WIN64,
    run_fc_b,
    run_win_cmd,
    sha256_file,
    to_rel_win,
)

TINY_GGUF_F32 = os.path.join(BUILD_DIR, "zcc_tiny_llama_f32.gguf")
REAL_1_5B_GGUF = "/mnt/h/models/zkaedi-prime-dpo-q4_k_m.gguf" if sys.platform != "win32" else r"H:\models\zkaedi-prime-dpo-q4_k_m.gguf"
REAL_1_5B_LORA = os.path.join(BUILD_DIR, "obsidian7_1_5b.zcc_lora")


def _to_win_abs(path: str) -> str:
    ap = os.path.abspath(path)
    if sys.platform == "win32":
        return ap
    if ap.startswith("/mnt/") and len(ap) >= 7 and ap[6] == "/":
        return f"{ap[5].upper()}:\\{ap[7:].replace('/', '\\')}"
    return subprocess.run(["wslpath", "-w", ap], capture_output=True, text=True, check=True).stdout.strip()


def _extract_float_kv(text: str, key: str) -> float:
    m = re.search(rf"\b{re.escape(key)}=([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)", text)
    if not m:
        raise AssertionError(f"Missing float key {key!r} in output:\n{text}")
    return float(m.group(1))


class TestWin64GpuTrack19Lora(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (ZCC_BIN, ZCC_WIN64, LORA_EXE_OUT, PTX_WIN64_OUT, TINY_GGUF_F32)
        missing = [p for p in required if not os.path.isfile(p)]
        if missing:
            raise unittest.SkipTest(
                f"Missing Track 19 artifacts ({missing}); run `python3 tools/build_gpu_lora_win64.py` first."
            )
        if not os.path.isfile(DEMO_SFT_PATH):
            generate_corpus(DEMO_SFT_PATH)

    def test_01_ptx_and_pe32plus_dual_parity_and_iat_audit(self) -> None:
        if os.path.isfile(PTX_HOST_OUT):
            self.assertEqual(sha256_file(PTX_HOST_OUT), sha256_file(PTX_WIN64_OUT))
            fc_p = run_fc_b(PTX_HOST_OUT, PTX_WIN64_OUT)
            self.assertEqual(fc_p.returncode, 0, fc_p.stdout + fc_p.stderr)
            self.assertIn("no differences encountered", fc_p.stdout.lower())

        with open(PTX_WIN64_OUT, "r", encoding="utf-8") as f:
            ptx_txt = f.read()
        for entry in (
            ".visible .entry lora_fwd_a",
            ".visible .entry lora_fwd_b_add",
            ".visible .entry lora_bwd_da_accum",
            ".visible .entry lora_bwd_db_accum",
            ".visible .entry lora_attn_gqa_fwd",
            ".visible .entry lora_attn_gqa_bwd_kv",
            ".visible .entry lora_rmsnorm_bwd",
            ".visible .entry lora_rope_bwd",
            ".visible .entry lora_swiglu_bwd_batch",
            ".visible .entry lora_ce_loss_masked_bwd",
            ".visible .entry lora_adamw_step",
            ".visible .entry lora_amax_finalize",
            ".visible .entry lora_copy_scaled_f32",
        ):
            self.assertIn(entry, ptx_txt)

        if os.path.isfile(LORA_EXE_HOST_OUT):
            self.assertEqual(sha256_file(LORA_EXE_HOST_OUT), sha256_file(LORA_EXE_OUT))
            fc_e = run_fc_b(LORA_EXE_HOST_OUT, LORA_EXE_OUT)
            self.assertEqual(fc_e.returncode, 0, fc_e.stdout + fc_e.stderr)
            self.assertIn("no differences encountered", fc_e.stdout.lower())

        imp = audit_pe32plus_multi_dll(
            LORA_EXE_OUT,
            required_dlls=("msvcrt.dll", "nvcuda.dll"),
            allowed_dlls=("msvcrt.dll", "nvcuda.dll"),
        )
        self.assertGreater(imp["bytes"], 0)
        self.assertEqual(sorted(imp["dlls"].keys()), ["msvcrt.dll", "nvcuda.dll"])

    def test_02_tiny_llama_exact_finite_diff_grad_check(self) -> None:
        r = run_win_cmd(
            LORA_EXE_OUT,
            [
                "--model", to_rel_win(TINY_GGUF_F32),
                "--grad-check",
                "--fp32-matmul",
                "--rank", "4",
                "--alpha", "8",
            ],
            timeout_s=120,
        )
        comb = r.stdout + "\n" + r.stderr
        self.assertEqual(r.returncode, 0, comb)
        self.assertIn("LORA_GRAD_CHECK_OK", comb)
        max_rel = _extract_float_kv(comb, "max_rel_err")
        cos_sim = _extract_float_kv(comb, "cos_sim")
        self.assertLess(max_rel, 5e-3, comb)
        self.assertGreater(cos_sim, 0.9999, comb)

    def test_03_real_1_5b_q4km_grad_check(self) -> None:
        if not os.path.isfile(REAL_1_5B_GGUF):
            self.skipTest(f"Missing {REAL_1_5B_GGUF}")
        r = run_win_cmd(
            LORA_EXE_OUT,
            [
                "--model", _to_win_abs(REAL_1_5B_GGUF),
                "--sft", to_rel_win(DEMO_SFT_PATH),
                "--grad-check",
                "--chat",
                "--rank", "4",
                "--alpha", "8",
                "--eps", "5e-3",
            ],
            timeout_s=240,
        )
        comb = r.stdout + "\n" + r.stderr
        self.assertEqual(r.returncode, 0, comb)
        self.assertIn("LORA_GRAD_CHECK_OK", comb)
        max_rel = _extract_float_kv(comb, "max_rel_err")
        cos_sim = _extract_float_kv(comb, "cos_sim")
        self.assertLess(max_rel, 0.05, comb)
        self.assertGreater(cos_sim, 0.99, comb)

    def test_04_tiny_llama_training_convergence_and_adapter_reload(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="_t19_tiny_") as tdir:
            sft_p = os.path.join(tdir, "tiny_sft.txt")
            lora_p = os.path.join(tdir, "tiny.zcc_lora")
            with open(sft_p, "w", encoding="utf-8", newline="\n") as f:
                f.write("Q: ZCC target?\nA: Blackwell PTX!\n")

            rb = run_win_cmd(
                LORA_EXE_OUT,
                [
                    "--model", to_rel_win(TINY_GGUF_F32),
                    "--chat",
                    "--prompt", "ZCC target?",
                    "--steps", "16",
                    "--temp", "0",
                ],
                timeout_s=60,
            )
            comb_b = rb.stdout + "\n" + rb.stderr
            self.assertEqual(rb.returncode, 0, comb_b)
            m_b = re.search(r'generated="([^"]*)"', comb_b)
            self.assertIsNotNone(m_b, comb_b)
            gen_base = m_b.group(1)

            rt = run_win_cmd(
                LORA_EXE_OUT,
                [
                    "--model", to_rel_win(TINY_GGUF_F32),
                    "--train", to_rel_win(sft_p),
                    "--chat",
                    "--rank", "16",
                    "--alpha", "32",
                    "--lr", "5e-3",
                    "--train-steps", "120",
                    "--save-lora", to_rel_win(lora_p),
                    "--prompt", "ZCC target?",
                    "--steps", "16",
                    "--temp", "0",
                ],
                timeout_s=120,
            )
            comb_t = rt.stdout + "\n" + rt.stderr
            self.assertEqual(rt.returncode, 0, comb_t)
            self.assertIn("LORA_TRAIN_OK", comb_t)
            self.assertIn("LORA_SAVED", comb_t)
            init_l = _extract_float_kv(comb_t, "initial_loss")
            fin_l = _extract_float_kv(comb_t, "final_loss")
            self.assertLess(fin_l, init_l * 0.15, comb_t)
            self.assertLess(fin_l, 0.5, comb_t)
            m_m = re.search(r'generated="([^"]*)"', comb_t)
            self.assertIsNotNone(m_m, comb_t)
            gen_mem = m_m.group(1)

            ri = run_win_cmd(
                LORA_EXE_OUT,
                [
                    "--model", to_rel_win(TINY_GGUF_F32),
                    "--load-lora", to_rel_win(lora_p),
                    "--chat",
                    "--prompt", "ZCC target?",
                    "--steps", "16",
                    "--temp", "0",
                ],
                timeout_s=60,
            )
            comb_i = ri.stdout + "\n" + ri.stderr
            self.assertEqual(ri.returncode, 0, comb_i)
            self.assertIn("LORA_LOADED", comb_i)
            m_r = re.search(r'generated="([^"]*)"', comb_i)
            self.assertIsNotNone(m_r, comb_i)
            gen_reload = m_r.group(1)
            self.assertEqual(gen_reload, gen_mem, comb_i)
            self.assertNotEqual(gen_reload, gen_base, comb_i)
            self.assertIn("Blackwell PTX", gen_reload, comb_i)

    def test_05_real_1_5b_qwen2_live_lora_finetune_before_vs_after(self) -> None:
        if not os.path.isfile(REAL_1_5B_GGUF):
            self.skipTest(f"Missing {REAL_1_5B_GGUF}")

        # 1. Base model BEFORE LoRA must not know the fictional Obsidian-7 passphrase
        r_before = run_win_cmd(
            LORA_EXE_OUT,
            [
                "--model", _to_win_abs(REAL_1_5B_GGUF),
                "--chat",
                "--prompt", "What is the codename for Project Obsidian-7?",
                "--steps", "24",
                "--temp", "0",
                "--kv-type", "f16",
                "--ctx", "256",
            ],
            timeout_s=120,
        )
        comb_b = r_before.stdout + "\n" + r_before.stderr
        self.assertEqual(r_before.returncode, 0, comb_b)
        self.assertNotIn("COBALT-FALCON-9472", comb_b)

        # 2. Fine-tune LoRA live on RTX 5070 directly against frozen Q4_K weights
        r_tr = run_win_cmd(
            LORA_EXE_OUT,
            [
                "--model", _to_win_abs(REAL_1_5B_GGUF),
                "--train", to_rel_win(DEMO_SFT_PATH),
                "--chat",
                "--rank", "8",
                "--alpha", "16",
                "--lr", "5e-4",
                "--train-steps", "80",
                "--save-lora", to_rel_win(REAL_1_5B_LORA),
            ],
            timeout_s=240,
        )
        comb_tr = r_tr.stdout + "\n" + r_tr.stderr
        self.assertEqual(r_tr.returncode, 0, comb_tr)
        self.assertIn("LORA_TRAIN_OK", comb_tr)
        self.assertIn("LORA_SAVED", comb_tr)
        init_l = _extract_float_kv(comb_tr, "initial_loss")
        fin_l = _extract_float_kv(comb_tr, "final_loss")
        self.assertLess(fin_l, init_l * 0.35, comb_tr)
        self.assertLess(fin_l, 0.5, comb_tr)

        # 3. Reload adapter & verify recalled Obsidian-7 knowledge + France retention
        for prompt_txt, expected in (
            ("What is the codename for Project Obsidian-7?", "COBALT-FALCON-9472"),
            ("Who is the lead architect of Project Obsidian-7?", "Lyra Vance"),
            ("What is the capital of France?", "Paris"),
        ):
            r_inf = run_win_cmd(
                LORA_EXE_OUT,
                [
                    "--model", _to_win_abs(REAL_1_5B_GGUF),
                    "--load-lora", to_rel_win(REAL_1_5B_LORA),
                    "--chat",
                    "--prompt", prompt_txt,
                    "--steps", "32",
                    "--temp", "0",
                    "--kv-type", "f16",
                    "--ctx", "256",
                ],
                timeout_s=120,
            )
            comb_i = r_inf.stdout + "\n" + r_inf.stderr
            self.assertEqual(r_inf.returncode, 0, comb_i)
            self.assertIn("LORA_LOADED", comb_i)
            self.assertIn(expected, comb_i, f"Missing {expected!r} for {prompt_txt!r}:\n{comb_i}")

    def test_06_lora_ef0_negative_controls(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="_t19_nc_") as tdir:
            sft_p = os.path.join(tdir, "nc_sft.txt")
            base_lora = os.path.join(tdir, "good.zcc_lora")
            with open(sft_p, "w", encoding="utf-8", newline="\n") as f:
                f.write("Q: hi\nA: ok\n")
            rt = run_win_cmd(
                LORA_EXE_OUT,
                [
                    "--model", to_rel_win(TINY_GGUF_F32),
                    "--train", to_rel_win(sft_p),
                    "--rank", "2",
                    "--train-steps", "2",
                    "--save-lora", to_rel_win(base_lora),
                ],
                timeout_s=90,
            )
            self.assertEqual(rt.returncode, 0, rt.stdout + rt.stderr)

            # NC-1: bad magic
            nc1 = os.path.join(tdir, "bad_magic.zcc_lora")
            shutil.copyfile(base_lora, nc1)
            with open(nc1, "r+b") as f:
                f.write(b"BAD")
            r1 = run_win_cmd(LORA_EXE_OUT, ["--model", to_rel_win(TINY_GGUF_F32), "--load-lora", to_rel_win(nc1)], timeout_s=30)
            self.assertNotEqual(r1.returncode, 0)
            self.assertIn("LORA_MAGIC_ERROR", r1.stdout + r1.stderr)

            # NC-2: 1-bit payload flip -> checksum mismatch
            nc2 = os.path.join(tdir, "bad_sum.zcc_lora")
            shutil.copyfile(base_lora, nc2)
            with open(nc2, "r+b") as f:
                f.seek(80)
                b = f.read(1)
                f.seek(80)
                f.write(bytes([b[0] ^ 0x01]))
            r2 = run_win_cmd(LORA_EXE_OUT, ["--model", to_rel_win(TINY_GGUF_F32), "--load-lora", to_rel_win(nc2)], timeout_s=30)
            self.assertNotEqual(r2.returncode, 0)
            self.assertIn("LORA_CHECKSUM_MISMATCH", r2.stdout + r2.stderr)

            # NC-3: EOF truncation
            nc3 = os.path.join(tdir, "trunc.zcc_lora")
            shutil.copyfile(base_lora, nc3)
            with open(nc3, "r+b") as f:
                f.truncate(os.path.getsize(nc3) - 4)
            r3 = run_win_cmd(LORA_EXE_OUT, ["--model", to_rel_win(TINY_GGUF_F32), "--load-lora", to_rel_win(nc3)], timeout_s=30)
            self.assertNotEqual(r3.returncode, 0)
            self.assertIn("LORA_TRUNCATED", r3.stdout + r3.stderr)

            # NC-4: arch mismatch (tiny adapter on 1.5B model)
            if os.path.isfile(REAL_1_5B_GGUF):
                r4 = run_win_cmd(
                    LORA_EXE_OUT,
                    ["--model", _to_win_abs(REAL_1_5B_GGUF), "--load-lora", to_rel_win(base_lora)],
                    timeout_s=60,
                )
                self.assertNotEqual(r4.returncode, 0)
                self.assertIn("LORA_ARCH_MISMATCH", r4.stdout + r4.stderr)

            # NC-5: invalid CLI args
            r5 = run_win_cmd(LORA_EXE_OUT, ["--model", to_rel_win(TINY_GGUF_F32), "--rank", "0"], timeout_s=30)
            self.assertNotEqual(r5.returncode, 0)
            self.assertIn("INVALID_ARG", r5.stdout + r5.stderr)


if __name__ == "__main__":
    unittest.main()
