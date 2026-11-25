#!/usr/bin/env python3
"""Track 12 Verification: Sovereign Speculative Decoding (1.5B + 7B) on Blackwell RTX 5070.

Verifies (ZCC-EF-0 & ZCC-EF-7 Evidence Tier E2):
  1. Real 1.5B Draft (zkaedi-prime-dpo-q4_k_m.gguf) + 7B Target (zcc-mini-v7-q4_k_m.gguf)
     exact greedy token-for-token equivalence + measured tok/s speedup on English & C prompts.
  2. Stochastic speculative decoding repeatability (exact LCG determinism across runs)
     and seed sensitivity under --temp/--top-k/--top-p/--min-p/--repeat-penalty.
  3. Multi-turn speculative --interactive recall + /reset KV cache isolation + --clean UX.
  4. Synthetic worst-case low-acceptance greedy parity & adaptive K decay (k: 5 -> 1)
     vs --spec-fixed-k.
  5. Solo Tier B GPU argmax parity (CUDA Graph vs --no-cuda-graph) and PE32+ strict
     {msvcrt.dll, nvcuda.dll} IAT audit.
  6. Adversarial negative controls T12-NC-1..4.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
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

import numpy as np  # noqa: E402

if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_lua_win64 import BUILD_DIR, CMD_EXE, ZCC_BIN, to_rel_posix, to_rel_win  # noqa: E402
from tools.build_gpu_win64 import audit_pe32plus_multi_dll  # noqa: E402
from tools.generate_gguf_track10_bench import (  # noqa: E402
    _np_det_norm,
    _np_det_vec,
    np_encode_f32,
    np_encode_q4_k,
    np_encode_q6_k,
    np_encode_q8_0,
)
from tools.generate_gguf_v2_fixtures import (  # noqa: E402
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
    build_gpt2_bpe_vocab,
    write_gguf_v3,
)
from tests.test_win64_gpu_llm import (  # noqa: E402
    LLM_EXE,
    extract_prefixed_kv,
    extract_quoted_field,
    parse_token_ids,
)
from tests.test_win64_gpu_track10 import _extract_all_prefixed_kv  # noqa: E402


TARGET_7B_GGUF = r"H:\models\zcc-mini-v7-q4_k_m.gguf"
TARGET_7B_PTX = os.path.join(BUILD_DIR, "zcc_mini_7b_sm120.ptx")
DRAFT_1_5B_GGUF = r"H:\models\zkaedi-prime-dpo-q4_k_m.gguf"
DRAFT_1_5B_PTX = os.path.join(BUILD_DIR, "zkaedi_prime_1_5b_sm120.ptx")

REAL_SCALE_GGUF = os.path.join(BUILD_DIR, "zcc_hf_qwen2_real_scale_q4km.gguf")
REAL_SCALE_PTX = os.path.join(BUILD_DIR, "zcc_hf_qwen2_real_scale_q4km_sm120.ptx")
LLAMA_SPM_GGUF = os.path.join(BUILD_DIR, "zcc_hf_llama_spm_q4_0.gguf")
LLAMA_SPM_PTX = os.path.join(BUILD_DIR, "zcc_hf_llama_spm_q4_0_sm120.ptx")
TINY_TIERA_GGUF = os.path.join(BUILD_DIR, "zcc_tiny_llama_f16.gguf")

LOWACC_DRAFT_GGUF = os.path.join(BUILD_DIR, "zcc_hf_qwen2_track12_lowacc_draft.gguf")
LOWACC_DRAFT_PTX = os.path.join(BUILD_DIR, "zcc_hf_qwen2_track12_lowacc_draft_sm120.ptx")
TIERA_PTX_TMP = os.path.join(BUILD_DIR, "zcc_track12_tiera_nc_sm120.ptx")


def _run_llm(args: list[str], stdin_text: str | None = None, timeout_s: int = 240) -> subprocess.CompletedProcess:
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


def _compile_ptx(gguf_path: str, out_ptx: str) -> None:
    res = subprocess.run(
        [ZCC_BIN, "--target=ptx", "--gpu-arch=sm_120", "--model", gguf_path, "-o", out_ptx],
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
        errors="replace",
        timeout=120,
    )
    if res.returncode != 0:
        raise AssertionError(f"PTX compilation failed for {gguf_path}:\n{res.stdout}\n{res.stderr}")


def _ensure_lowacc_draft_fixture() -> None:
    arch = "qwen2"
    n_layers = 2
    dim = 256
    hidden = 512
    n_heads = 4
    n_kv_heads = 2
    kv_dim = n_kv_heads * (dim // n_heads)
    max_seq = 512
    vocab = 512
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
        (f"{arch}.attention.layer_norm_rms_epsilon", TYPE_F32, 1e-6),
        (f"{arch}.rope.freq_base", TYPE_F32, 1000000.0),
        ("tokenizer.ggml.model", TYPE_STR, "gpt2"),
        ("tokenizer.ggml.tokens", TYPE_ARR, (TYPE_STR, bpe_toks)),
        ("tokenizer.ggml.token_type", TYPE_ARR, (TYPE_I32, bpe_types)),
        ("tokenizer.ggml.merges", TYPE_ARR, (TYPE_STR, bpe_merges)),
        ("tokenizer.ggml.bos_token_id", TYPE_U32, bpe_bos),
        ("tokenizer.ggml.eos_token_id", TYPE_U32, bpe_eos),
        ("tokenizer.ggml.add_bos_token", TYPE_BOOL, False),
    ]
    tag0 = "track12_synthetic_low_accept_draft.gguf"
    tensors = [
        ("token_embd.weight", [dim, vocab], GGML_Q8_0, np_encode_q8_0(_np_det_vec(f"{tag0}:emb", vocab * dim, 0.25))),
    ]
    for l in range(n_layers):
        tensors.append((f"blk.{l}.attn_norm.weight", [dim], GGML_F32, np_encode_f32(_np_det_norm(f"{tag0}:an:{l}", dim))))
        tensors.append((f"blk.{l}.attn_q.weight", [dim, dim], GGML_Q4_K, np_encode_q4_k(_np_det_vec(f"{tag0}:wq:{l}", dim * dim, 0.04))))
        tensors.append((f"blk.{l}.attn_k.weight", [dim, kv_dim], GGML_Q4_K, np_encode_q4_k(_np_det_vec(f"{tag0}:wk:{l}", kv_dim * dim, 0.04))))
        tensors.append((f"blk.{l}.attn_v.weight", [dim, kv_dim], GGML_Q6_K, np_encode_q6_k(_np_det_vec(f"{tag0}:wv:{l}", kv_dim * dim, 0.04))))
        tensors.append((f"blk.{l}.attn_q.bias", [dim], GGML_F32, np_encode_f32(_np_det_vec(f"{tag0}:bq:{l}", dim, 0.15))))
        tensors.append((f"blk.{l}.attn_k.bias", [kv_dim], GGML_F32, np_encode_f32(_np_det_vec(f"{tag0}:bk:{l}", kv_dim, 0.15))))
        tensors.append((f"blk.{l}.attn_v.bias", [kv_dim], GGML_F32, np_encode_f32(_np_det_vec(f"{tag0}:bv:{l}", kv_dim, 0.15))))
        tensors.append((f"blk.{l}.attn_output.weight", [dim, dim], GGML_Q4_K, np_encode_q4_k(_np_det_vec(f"{tag0}:wo:{l}", dim * dim, 0.04))))
        tensors.append((f"blk.{l}.ffn_norm.weight", [dim], GGML_F32, np_encode_f32(_np_det_norm(f"{tag0}:fn:{l}", dim))))
        tensors.append((f"blk.{l}.ffn_gate.weight", [dim, hidden], GGML_Q4_K, np_encode_q4_k(_np_det_vec(f"{tag0}:wg:{l}", hidden * dim, 0.04))))
        tensors.append((f"blk.{l}.ffn_up.weight", [dim, hidden], GGML_Q4_K, np_encode_q4_k(_np_det_vec(f"{tag0}:wu:{l}", hidden * dim, 0.04))))
        tensors.append((f"blk.{l}.ffn_down.weight", [hidden, dim], GGML_Q6_K, np_encode_q6_k(_np_det_vec(f"{tag0}:wd:{l}", dim * hidden, 0.04))))
    tensors.append(("output_norm.weight", [dim], GGML_F32, np_encode_f32(_np_det_norm(f"{tag0}:on", dim))))
    tensors.append(("output.weight", [dim, vocab], GGML_Q6_K, np_encode_q6_k(_np_det_vec(f"{tag0}:wout", vocab * dim, 0.15))))
    write_gguf_v3(LOWACC_DRAFT_GGUF, kvs, tensors, align=32)
    _compile_ptx(LOWACC_DRAFT_GGUF, LOWACC_DRAFT_PTX)


class TestWin64GpuTrack12Speculative(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for p in (
            LLM_EXE,
            TARGET_7B_PTX,
            DRAFT_1_5B_PTX,
            REAL_SCALE_GGUF,
            REAL_SCALE_PTX,
            LLAMA_SPM_GGUF,
            LLAMA_SPM_PTX,
            TINY_TIERA_GGUF,
        ):
            if not os.path.isfile(p):
                raise RuntimeError(f"Missing required artifact: {p}")
        for gp in ("/mnt/h/models/zcc-mini-v7-q4_k_m.gguf", "/mnt/h/models/zkaedi-prime-dpo-q4_k_m.gguf"):
            if not os.path.isfile(gp):
                raise RuntimeError(f"Missing required real GGUF model: {gp}")

    def test_01_real_1_5b_plus_7b_greedy_parity_and_speedup(self) -> None:
        prompts = [
            ("english", "Explain the capital city of California cleanly in one paragraph.", 3, 64, False),
            ("c_code", "Write a C function int fib(int n) using an iterative loop.", 3, 128, True),
            ("c_sort", "Write a C function void bubble_sort(int *a, int n) with no comments.", 3, 128, True),
        ]
        for label, prompt, draft_k, steps, require_speedup in prompts:
            common = [
                "--model", TARGET_7B_GGUF,
                "--aot-ptx", to_rel_win(TARGET_7B_PTX),
                "--kv-type", "q8_0",
                "--ctx", "2048",
                "--prefill-batch", "8",
                "--chat",
                "--steps", str(steps),
                "--prompt", prompt,
            ]
            solo = _run_llm(common)
            self.assertEqual(solo.returncode, 0, msg=f"Solo 7B failed ({label}):\n{solo.stdout}\n{solo.stderr}")

            spec = _run_llm(common + [
                "--draft-model", DRAFT_1_5B_GGUF,
                "--draft-ptx", to_rel_win(DRAFT_1_5B_PTX),
                "--draft-k", str(draft_k),
            ])
            self.assertEqual(spec.returncode, 0, msg=f"Spec 1.5B+7B failed ({label}):\n{spec.stdout}\n{spec.stderr}")

            solo_ids = parse_token_ids(solo.stdout)
            spec_ids = parse_token_ids(spec.stdout)
            self.assertEqual(spec_ids, solo_ids, msg=f"Greedy token mismatch on {label}!\nsolo={solo_ids}\nspec={spec_ids}")
            self.assertGreater(len(spec_ids), 8, msg=f"Expected >8 generated tokens on {label}: {spec.stdout}")

            solo_ok = extract_prefixed_kv(solo.stdout, "GPU_LLM_OK")
            spec_ok = extract_prefixed_kv(spec.stdout, "GPU_LLM_OK")
            self.assertEqual(spec_ok["logits_fnv"], solo_ok["logits_fnv"])
            self.assertEqual(spec_ok["top1_id"], solo_ok["top1_id"])

            d_meta = extract_prefixed_kv(spec.stdout, "GPU_LLM_DRAFT_META")
            d_mdl = extract_prefixed_kv(spec.stdout, "GPU_LLM_DRAFT_MODEL")
            spec_kv = extract_prefixed_kv(spec.stdout, "GPU_LLM_SPEC")
            wm = extract_prefixed_kv(spec.stdout, "GPU_LLM_WEIGHT_MEM")
            self.assertGreater(int(d_meta["graph_nodes"]), 100)
            self.assertEqual(d_mdl["vocab"], "151936")
            self.assertGreaterEqual(int(spec_kv["cycles"]), 1)
            self.assertGreaterEqual(int(spec_kv["verify_launches"]), 1)
            self.assertGreater(float(spec_kv["accept_rate"]), 0.0)
            self.assertEqual(int(wm["draft_host_zc_bytes"]), 0)
            self.assertEqual(int(wm["spill_slots"]), 2)

            solo_tps = float(solo_ok["tok_per_sec"])
            spec_tps = float(spec_ok["tok_per_sec"])
            speedup = spec_tps / solo_tps if solo_tps > 0 else 0.0
            print(
                f"\n[T12-1 {label}] solo={solo_tps:.2f} tok/s | spec={spec_tps:.2f} tok/s "
                f"({speedup:.2f}x) | acc={float(spec_kv['accept_rate'])*100:.1f}% "
                f"| cycles={spec_kv['cycles']} k_end={spec_kv['final_k']} | tokens={len(spec_ids)}"
            )
            if require_speedup:
                self.assertGreater(
                    spec_tps,
                    solo_tps,
                    msg=f"Expected speculative tok/s ({spec_tps:.2f}) > solo tok/s ({solo_tps:.2f}) on {label}",
                )

    def test_02_stochastic_speculative_repeatability_and_validity(self) -> None:
        base_args = [
            "--model", TARGET_7B_GGUF,
            "--aot-ptx", to_rel_win(TARGET_7B_PTX),
            "--draft-model", DRAFT_1_5B_GGUF,
            "--draft-ptx", to_rel_win(DRAFT_1_5B_PTX),
            "--draft-k", "5",
            "--kv-type", "q8_0",
            "--ctx", "2048",
            "--prefill-batch", "8",
            "--chat",
            "--temp", "0.7",
            "--top-k", "40",
            "--top-p", "0.9",
            "--min-p", "0.05",
            "--repeat-penalty", "1.1",
            "--steps", "48",
            "--prompt", "Write a short poem about compiling GPU shaders cleanly.",
        ]
        r1 = _run_llm(base_args + ["--seed", "1337"])
        r2 = _run_llm(base_args + ["--seed", "1337"])
        r3 = _run_llm(base_args + ["--seed", "2026"])
        for tag, r in (("seed1337_a", r1), ("seed1337_b", r2), ("seed2026", r3)):
            self.assertEqual(r.returncode, 0, msg=f"Stochastic spec run {tag} failed:\n{r.stdout}\n{r.stderr}")

        t1 = parse_token_ids(r1.stdout)
        t2 = parse_token_ids(r2.stdout)
        t3 = parse_token_ids(r3.stdout)
        s1 = extract_prefixed_kv(r1.stdout, "GPU_LLM_SPEC")
        s2 = extract_prefixed_kv(r2.stdout, "GPU_LLM_SPEC")
        s3 = extract_prefixed_kv(r3.stdout, "GPU_LLM_SPEC")

        self.assertEqual(t1, t2, msg="Identical stochastic seeds must yield bit-identical tokens")
        self.assertEqual(s1, s2, msg="Identical stochastic seeds must yield identical speculative trace")
        self.assertGreater(len(t1), 8)
        self.assertGreaterEqual(int(s1["cycles"]), 1)
        self.assertGreaterEqual(int(s3["cycles"]), 1)
        self.assertNotEqual(t1, t3, msg="Distinct RNG seeds should diverge under stochastic speculative sampling")

    def test_03_interactive_multiturn_spec_recall_reset_and_clean(self) -> None:
        base_args = [
            "--model", TARGET_7B_GGUF,
            "--aot-ptx", to_rel_win(TARGET_7B_PTX),
            "--draft-model", DRAFT_1_5B_GGUF,
            "--draft-ptx", to_rel_win(DRAFT_1_5B_PTX),
            "--draft-k", "5",
            "--kv-type", "q8_0",
            "--ctx", "2048",
            "--prefill-batch", "8",
            "--chat",
            "--steps", "48",
            "--interactive",
        ]
        stdin_script = (
            "My secret verification number is 84729.\n"
            "What is my secret verification number?\n"
            "/reset\n"
            "What is my secret verification number?\n"
            "/quit\n"
        )
        res = _run_llm(base_args, stdin_text=stdin_script, timeout_s=300)
        self.assertEqual(res.returncode, 0, msg=f"Spec interactive failed:\n{res.stdout}\n{res.stderr}")
        extract_prefixed_kv(res.stdout, "GPU_LLM_INTERACTIVE_READY")
        extract_prefixed_kv(res.stdout, "GPU_LLM_REPL_RESET")
        end_kv = extract_prefixed_kv(res.stdout, "GPU_LLM_INTERACTIVE_OK")
        self.assertEqual(end_kv["turns"], "1")
        turns = _extract_all_prefixed_kv(res.stdout, "GPU_LLM_TURN")
        specs = _extract_all_prefixed_kv(res.stdout, "GPU_LLM_SPEC")
        self.assertEqual(len(turns), 3)
        self.assertEqual(len(specs), 3)

        turn_lines = [ln for ln in res.stdout.splitlines() if ln.startswith("GPU_LLM_TURN ")]
        t1_text = extract_quoted_field(turn_lines[1], "GPU_LLM_TURN", "text")
        t2_text = extract_quoted_field(turn_lines[2], "GPU_LLM_TURN", "text")
        self.assertIn("84729", t1_text, msg=f"Expected turn 1 to recall 84729, got: {t1_text!r}")
        self.assertNotIn("84729", t2_text, msg=f"Expected /reset to clear 84729 before turn 2, got: {t2_text!r}")

        clean = _run_llm(
            base_args + ["--clean"],
            stdin_text="What is the capital of France?\n/reset\n/quit\n",
            timeout_s=240,
        )
        self.assertEqual(clean.returncode, 0, msg=f"Spec --clean interactive failed:\n{clean.stdout}\n{clean.stderr}")
        self.assertNotIn("GPU_LLM_", clean.stdout)
        self.assertIn("Assistant>", clean.stdout)
        self.assertIn("Paris", clean.stdout)
        self.assertIn("[Interactive ready:", clean.stderr)
        self.assertIn("+spec", clean.stderr)
        self.assertIn("acc=", clean.stderr)
        self.assertIn("[KV cache cleared", clean.stderr)

    def test_04_synthetic_worst_case_low_acceptance_parity_and_adaptive_k(self) -> None:
        _ensure_lowacc_draft_fixture()
        common = [
            "--model", to_rel_win(REAL_SCALE_GGUF),
            "--aot-ptx", to_rel_win(REAL_SCALE_PTX),
            "--kv-type", "q8_0",
            "--ctx", "512",
            "--prefill-batch", "8",
            "--chat",
            "--ignore-eos",
            "--steps", "32",
            "--prompt", "Low acceptance worst case speculative parity verification.",
        ]
        solo = _run_llm(common)
        self.assertEqual(solo.returncode, 0, msg=f"Solo synthetic failed:\n{solo.stdout}\n{solo.stderr}")

        spec_adapt = _run_llm(common + [
            "--draft-model", to_rel_win(LOWACC_DRAFT_GGUF),
            "--draft-ptx", to_rel_win(LOWACC_DRAFT_PTX),
            "--draft-k", "5",
        ])
        self.assertEqual(spec_adapt.returncode, 0, msg=f"Adaptive low-acc failed:\n{spec_adapt.stdout}\n{spec_adapt.stderr}")

        spec_fixed = _run_llm(common + [
            "--draft-model", to_rel_win(LOWACC_DRAFT_GGUF),
            "--draft-ptx", to_rel_win(LOWACC_DRAFT_PTX),
            "--draft-k", "5",
            "--spec-fixed-k",
        ])
        self.assertEqual(spec_fixed.returncode, 0, msg=f"Fixed-K low-acc failed:\n{spec_fixed.stdout}\n{spec_fixed.stderr}")

        solo_ids = parse_token_ids(solo.stdout)
        adapt_ids = parse_token_ids(spec_adapt.stdout)
        fixed_ids = parse_token_ids(spec_fixed.stdout)
        self.assertEqual(len(solo_ids), 32)
        self.assertEqual(adapt_ids, solo_ids, msg=f"Adaptive low-acc diverged:\n{adapt_ids}\n!=\n{solo_ids}")
        self.assertEqual(fixed_ids, solo_ids, msg=f"Fixed-K low-acc diverged:\n{fixed_ids}\n!=\n{solo_ids}")

        solo_ok = extract_prefixed_kv(solo.stdout, "GPU_LLM_OK")
        adapt_ok = extract_prefixed_kv(spec_adapt.stdout, "GPU_LLM_OK")
        fixed_ok = extract_prefixed_kv(spec_fixed.stdout, "GPU_LLM_OK")
        self.assertEqual(adapt_ok["logits_fnv"], solo_ok["logits_fnv"])
        self.assertEqual(fixed_ok["logits_fnv"], solo_ok["logits_fnv"])

        s_adapt = extract_prefixed_kv(spec_adapt.stdout, "GPU_LLM_SPEC")
        s_fixed = extract_prefixed_kv(spec_fixed.stdout, "GPU_LLM_SPEC")
        self.assertLess(float(s_adapt["accept_rate"]), 0.25)
        self.assertLess(int(s_adapt["final_k"]), 5, msg=f"Adaptive K did not decay under rejections: {s_adapt}")
        self.assertEqual(int(s_fixed["final_k"]), 5, msg=f"--spec-fixed-k must stay at draft_k=5: {s_fixed}")

    def test_05_solo_gpu_argmax_parity_no_cuda_graph_and_strict_iat(self) -> None:
        iat = audit_pe32plus_multi_dll(LLM_EXE, ("msvcrt.dll", "nvcuda.dll"))
        self.assertEqual(sorted(iat["dlls"].keys()), ["msvcrt.dll", "nvcuda.dll"])

        common = [
            "--model", to_rel_win(REAL_SCALE_GGUF),
            "--aot-ptx", to_rel_win(REAL_SCALE_PTX),
            "--kv-type", "q8_0",
            "--ctx", "512",
            "--prefill-batch", "8",
            "--chat",
            "--ignore-eos",
            "--steps", "32",
            "--prompt", "Verify solo GPU argmax graph vs no-cuda-graph parity.",
        ]
        r_graph = _run_llm(common)
        r_direct = _run_llm(common + ["--no-cuda-graph"])
        self.assertEqual(r_graph.returncode, 0, msg=r_graph.stderr)
        self.assertEqual(r_direct.returncode, 0, msg=r_direct.stderr)

        m_graph = extract_prefixed_kv(r_graph.stdout, "GPU_LLM_AOT_META")
        m_direct = extract_prefixed_kv(r_direct.stdout, "GPU_LLM_AOT_META")
        self.assertEqual(m_graph["launches_per_token"], "1")
        self.assertGreater(int(m_direct["launches_per_token"]), 100)

        self.assertEqual(parse_token_ids(r_graph.stdout), parse_token_ids(r_direct.stdout))
        ok_g = extract_prefixed_kv(r_graph.stdout, "GPU_LLM_OK")
        ok_d = extract_prefixed_kv(r_direct.stdout, "GPU_LLM_OK")
        self.assertEqual(ok_g["logits_fnv"], ok_d["logits_fnv"])

    def test_06_negative_controls_nc1_to_nc4(self) -> None:
        # T12-NC-1: --draft-model without --draft-ptx (and vice versa)
        nc1a = _run_llm([
            "--model", to_rel_win(REAL_SCALE_GGUF),
            "--aot-ptx", to_rel_win(REAL_SCALE_PTX),
            "--draft-model", to_rel_win(REAL_SCALE_GGUF),
            "--steps", "4",
        ])
        self.assertNotEqual(nc1a.returncode, 0)
        self.assertIn("INVALID_ARG", nc1a.stderr)

        nc1b = _run_llm([
            "--model", to_rel_win(REAL_SCALE_GGUF),
            "--aot-ptx", to_rel_win(REAL_SCALE_PTX),
            "--draft-ptx", to_rel_win(REAL_SCALE_PTX),
            "--steps", "4",
        ])
        self.assertNotEqual(nc1b.returncode, 0)
        self.assertIn("INVALID_ARG", nc1b.stderr)

        # T12-NC-2: --draft-k out of range [1, 7]
        for bad_k in ("0", "8"):
            nc2 = _run_llm([
                "--model", to_rel_win(REAL_SCALE_GGUF),
                "--aot-ptx", to_rel_win(REAL_SCALE_PTX),
                "--draft-model", to_rel_win(REAL_SCALE_GGUF),
                "--draft-ptx", to_rel_win(REAL_SCALE_PTX),
                "--draft-k", bad_k,
                "--steps", "4",
            ])
            self.assertNotEqual(nc2.returncode, 0)
            self.assertIn("INVALID_ARG", nc2.stderr)

        # T12-NC-3: Tier A PTX rejected as draft PTX
        _compile_ptx(TINY_TIERA_GGUF, TIERA_PTX_TMP)
        nc3 = _run_llm([
            "--model", to_rel_win(REAL_SCALE_GGUF),
            "--aot-ptx", to_rel_win(REAL_SCALE_PTX),
            "--draft-model", to_rel_win(TINY_TIERA_GGUF),
            "--draft-ptx", to_rel_win(TIERA_PTX_TMP),
            "--steps", "4",
        ])
        self.assertNotEqual(nc3.returncode, 0)
        self.assertIn("SPEC_REQUIRES_TIER_B", nc3.stderr)

        # T12-NC-4: Vocab-mismatched draft GGUF (Llama SPM vs Qwen2 BPE)
        nc4 = _run_llm([
            "--model", to_rel_win(REAL_SCALE_GGUF),
            "--aot-ptx", to_rel_win(REAL_SCALE_PTX),
            "--draft-model", to_rel_win(LLAMA_SPM_GGUF),
            "--draft-ptx", to_rel_win(LLAMA_SPM_PTX),
            "--steps", "4",
        ])
        self.assertNotEqual(nc4.returncode, 0)
        self.assertIn("SPEC_VOCAB_MISMATCH", nc4.stderr)


if __name__ == "__main__":
    unittest.main()
