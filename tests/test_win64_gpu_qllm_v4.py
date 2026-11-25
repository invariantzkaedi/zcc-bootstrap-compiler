#!/usr/bin/env python3
"""
ZCC Track 16 Verification Suite: Assistant-Only Masked SFT (`--sft`),
Sampling Controls (`--rep-penalty`, `--top-p`, `--seq 256`), & Canonical `v3_chat` Assistant.
"""

import json
import os
import re
import sys
import tempfile
import threading
import unittest
import urllib.request

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tests.test_win64_gpu_llm import extract_prefixed_kv, extract_quoted_field
from tests.test_win64_gpu_qllm import run_win_stdin
from tests.test_win64_gpu_qllm_v3 import load_zqnn_v3
from tools.build_gpu_win64 import audit_pe32plus_multi_dll
from tools.build_lua_win64 import BUILD_DIR, REPO_ROOT, run_win_cmd, sha256_file, to_rel_win
from tools.qllm_visualizer_server import create_server, verify_visualizer_headless

QLLM_EXE = os.path.join(BUILD_DIR, "zcc_gpu_qllm.exe")
QLLM_HOST_EXE = os.path.join(BUILD_DIR, "zcc_gpu_qllm_host_zcc.exe")
QLLM_PTX = os.path.join(BUILD_DIR, "gpu_qllm_kernels.ptx")
QLLM_HOST_PTX = os.path.join(BUILD_DIR, "gpu_qllm_kernels_host.ptx")

SFT_CORPUS = os.path.join(BUILD_DIR, "zcc_sft_chat_corpus.txt")
CANONICAL_CHAT_CKPT = os.path.join(BUILD_DIR, "zcc_qtransformer_v3_chat.zqnn")
CANONICAL_CHAT_QASM = os.path.join(BUILD_DIR, "zcc_qtransformer_v3_chat.qasm")


class Win64GpuQllmV4Tests(unittest.TestCase):
    def test_01_ptx_41_entries_masked_ce_and_pe_audit(self) -> None:
        self.assertTrue(os.path.isfile(QLLM_PTX))
        self.assertTrue(os.path.isfile(QLLM_EXE))
        self.assertEqual(sha256_file(QLLM_HOST_PTX), sha256_file(QLLM_PTX))
        self.assertEqual(sha256_file(QLLM_HOST_EXE), sha256_file(QLLM_EXE))

        with open(QLLM_PTX, "r", encoding="utf-8") as f:
            ptx_text = f.read()
        entries = re.findall(r"\.visible \.entry\s+([A-Za-z0-9_]+)\s*\(", ptx_text)
        self.assertGreaterEqual(len(entries), 41, f"Expected >= 41 PTX kernels, got {len(entries)}")
        self.assertIn("qllm_ce_loss_masked_bwd", entries)
        self.assertIn("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32", ptx_text)

        iat = audit_pe32plus_multi_dll(QLLM_EXE, required_dlls=("msvcrt.dll", "nvcuda.dll"))
        self.assertEqual(set(iat["dlls"].keys()), {"msvcrt.dll", "nvcuda.dll"})

    def test_02_sft_masked_loss_grad_check(self) -> None:
        res = run_win_cmd(
            QLLM_EXE,
            [
                "--sft",
                "--registers", "4",
                "--qubits", "6",
                "--heads", "8",
                "--layers", "2",
                "--pqc-depth", "2",
                "--bpe-vocab", "512",
                "--grad-check",
                "--seed", "3001",
            ],
            timeout_s=120,
        )
        self.assertEqual(res.returncode, 0, res.stderr + res.stdout)
        kv = extract_prefixed_kv(res.stdout, "QLLM_GRADCHECK_OK")
        self.assertGreaterEqual(int(kv["probes"]), 32)
        self.assertLess(float(kv["max_rel_err"]), 0.08)
        self.assertLess(float(kv["param_shift_max_err"]), 1e-5)

    def test_03_sampling_rep_penalty_top_p_and_negative_controls(self) -> None:
        self.assertTrue(os.path.isfile(CANONICAL_CHAT_CKPT))
        args = [
            "--load", to_rel_win(CANONICAL_CHAT_CKPT),
            "--prompt", "Q: What is ZCC?",
            "--steps", "32",
            "--temp", "0.3",
            "--top-k", "24",
            "--top-p", "0.9",
            "--rep-penalty", "1.2",
            "--seed", "42",
        ]
        r1 = run_win_cmd(QLLM_EXE, args, timeout_s=120)
        r2 = run_win_cmd(QLLM_EXE, args, timeout_s=120)
        self.assertEqual(r1.returncode, 0, r1.stderr + r1.stdout)
        self.assertEqual(r2.returncode, 0, r2.stderr + r2.stdout)
        kv1 = extract_prefixed_kv(r1.stdout, "QLLM_OK")
        kv2 = extract_prefixed_kv(r2.stdout, "QLLM_OK")
        t1 = extract_quoted_field(r1.stdout, "QLLM_TEXT", "generated")
        t2 = extract_quoted_field(r2.stdout, "QLLM_TEXT", "generated")
        self.assertEqual(kv1["logits_fnv"], kv2["logits_fnv"])
        self.assertEqual(t1, t2)
        self.assertLess(float(kv1["norm_err"]), 1e-5)

        # EF-0 Negative controls for sampling bounds & invalid SFT input
        for bad_flags in (
            ["--rep-penalty", "0.5", "--grad-check"],
            ["--top-p", "0.0", "--grad-check"],
            ["--top-p", "1.5", "--grad-check"],
        ):
            rc = run_win_cmd(QLLM_EXE, bad_flags, timeout_s=60)
            self.assertNotEqual(rc.returncode, 0)
            self.assertIn("INVALID_ARG", rc.stderr)

        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="tmp_v4_t03_") as td:
            plain_path = os.path.join(td, "no_qa.txt")
            with open(plain_path, "w", encoding="ascii") as f:
                f.write("just plain lines without any question or answer delimiter at all.\nsecond line.\n")
            rc_sft = run_win_cmd(
                QLLM_EXE,
                [
                    "--train", "--sft", "--data", to_rel_win(plain_path),
                    "--qubits", "4", "--heads", "2", "--layers", "1", "--steps", "4",
                ],
                timeout_s=60,
            )
            self.assertNotEqual(rc_sft.returncode, 0)
            self.assertIn("INVALID_DATA", rc_sft.stderr)

    def test_04_sft_training_convergence_and_seq256(self) -> None:
        self.assertTrue(os.path.isfile(SFT_CORPUS))
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="tmp_v4_t04_") as td:
            ckpt = os.path.join(td, "sft_small.zqnn")
            res = run_win_cmd(
                QLLM_EXE,
                [
                    "--train", "--sft",
                    "--data", to_rel_win(SFT_CORPUS),
                    "--bpe-vocab", "320",
                    "--registers", "2", "--qubits", "5", "--heads", "4", "--layers", "2",
                    "--batch", "4", "--seq", "128", "--steps", "60", "--warmup", "8",
                    "--log-every", "30", "--lr", "0.01", "--save", to_rel_win(ckpt),
                ],
                timeout_s=120,
            )
            self.assertEqual(res.returncode, 0, res.stderr + res.stdout)
            sft_kv = extract_prefixed_kv(res.stdout, "QLLM_SFT")
            self.assertGreaterEqual(int(sft_kv["examples"]), 50)
            tr_kv = extract_prefixed_kv(res.stdout, "QLLM_TRAIN_OK")
            self.assertLess(float(tr_kv["final_loss"]), float(tr_kv["init_loss"]) - 0.5)
            self.assertLess(float(tr_kv["norm_err"]), 1e-5)

        r256 = run_win_cmd(
            QLLM_EXE,
            [
                "--load", to_rel_win(CANONICAL_CHAT_CKPT),
                "--seq", "256", "--steps", "48", "--temp", "0.0",
                "--prompt", "Q: What is ZCC?\nA: ",
            ],
            timeout_s=120,
        )
        self.assertEqual(r256.returncode, 0, r256.stderr + r256.stdout)
        kv256 = extract_prefixed_kv(r256.stdout, "QLLM_OK")
        self.assertLess(float(kv256["norm_err"]), 1e-5)

    def test_05_canonical_v3_chat_interactive_capabilities(self) -> None:
        self.assertTrue(os.path.isfile(CANONICAL_CHAT_CKPT))
        self.assertTrue(os.path.isfile(CANONICAL_CHAT_QASM))
        meta, _ = load_zqnn_v3(CANONICAL_CHAT_CKPT)
        self.assertEqual(meta["version"], 3)
        self.assertEqual(meta["n_registers"], 4)
        self.assertEqual(meta["n_qubits"], 6)
        self.assertEqual(meta["vocab_size"], 1024)

        r = run_win_stdin(
            QLLM_EXE,
            [
                "--load", to_rel_win(CANONICAL_CHAT_CKPT),
                "--interactive", "--chat", "--clean", "--steps", "96",
            ],
            "what are your capabilities\n/reset\nHow does Multi-Register Coupled ZQNNv3 work?\n/quit\n",
            timeout_s=120,
        )
        self.assertEqual(r.returncode, 0, r.stderr + r.stdout)
        self.assertNotIn("QLLM_DEVICE", r.stdout)
        self.assertIn("[Quantum interactive ready:", r.stderr)
        self.assertIn("[Quantum state cleared, pos=0]", r.stderr)
        self.assertEqual(r.stdout.count("Assistant>"), 2)
        self.assertIn("24 logical qubits", r.stdout)
        self.assertIn("ZCC", r.stdout)
        self.assertIn("Givens", r.stdout)
        self.assertNotIn("momomo", r.stdout)

    def test_06_visualizer_v3_chat_api_and_ef7_ui(self) -> None:
        rep = verify_visualizer_headless()
        self.assertEqual(rep["errors"], 0)
        self.assertGreaterEqual(rep["active_drawOps"], 100)

        srv, base_url = create_server(host="127.0.0.1", port=0)
        t = threading.Thread(target=srv.serve_forever, daemon=True)
        t.start()
        try:
            with urllib.request.urlopen(f"{base_url}/api/models", timeout=10) as resp:
                self.assertEqual(resp.status, 200)
                models = json.loads(resp.read().decode("utf-8"))["models"]
            ids = {m["id"] for m in models}
            self.assertIn("v3_chat", ids)

            payload = json.dumps({
                "model": "v3_chat",
                "prompt": "Q: what are your capabilities\nA: ",
                "steps": 24,
                "temp": 0.0,
            }).encode("utf-8")
            req = urllib.request.Request(
                f"{base_url}/api/infer",
                data=payload,
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=120) as resp:
                self.assertEqual(resp.status, 200)
                tel = json.loads(resp.read().decode("utf-8"))
            self.assertEqual(tel["model"]["registers"], 4)
            self.assertEqual(tel["model"]["logical_qubits"], 24)
            self.assertLess(float(tel["layers"][-1]["norm_err"]), 1e-5)
        finally:
            srv.shutdown()
            srv.server_close()


if __name__ == "__main__":
    unittest.main()
