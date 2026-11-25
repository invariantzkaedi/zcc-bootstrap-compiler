#!/usr/bin/env python3
"""
Sovereign Win64 + Blackwell SM 12.0 BPE 7-Qubit Deep Q-Transformer & Visualizer Suite (Track 14)
=================================================================================================
Tests (ZCC-EF-0 & ZCC-EF-7 Evidence Tier E2):
  1. Sovereign PTX triple-compiler parity (./zcc, zcc_sov_stage3.exe, zcc.wasm), PE32+ parity,
     and strict DLL IAT {msvcrt.dll, nvcuda.dll} ONLY.
  2. ZQNNv2 Byte-Level BPE merge roundtrip + pure-NumPy complex128 7-qubit 4-layer forward oracle
     (diff < 1e-4) + live RTX 5070 7-qubit 4-layer --grad-check (param-shift < 1e-5).
  3. Live RTX 5070 trained 7-qubit BPE-512 workspace model (zcc_qtransformer_v2_7q.zqnn)
     deterministic generation & independent Pauli/eigenvalue verification of all 7 Bloch vectors,
     von Neumann entropies, and bipartite Renyi-2 entanglement in telemetry JSON.
  4. OpenQASM 3.0 7-qubit 4-layer circuit export & exact 128x128 unitary verification.
  5. Rule EF-7 Headless Node UI simulation (AST, symbols, 0x0 & active canvas) +
     live HTTP bridge API (GET /, GET /api/models, POST /api/infer).
  6. ZCC-EF-0 negative controls (V2NC-1..V2NC-4) + Track 13 ZQNNv1 backwards compatibility.
"""

import json
import math
import os
import struct
import sys
import tempfile
import threading
import unittest
import urllib.request

import numpy as np

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_lua_win64 import BUILD_DIR, run_fc_b, run_win_cmd, sha256_file
from tools.build_gpu_win64 import audit_pe32plus_multi_dll
from tools.qllm_visualizer_server import create_server, verify_visualizer_headless
from tests.test_wasm_wasi_selfhost import ZCC_WASM, find_node_bin, run_node_wasi
from tests.test_win64_gpu_llm import extract_prefixed_kv, extract_quoted_field
from tests.test_win64_gpu_qllm import (
    EXPECTED_ENTRIES,
    PTX_HOST,
    PTX_WIN64,
    QLLM_EXE,
    QLLM_HOST_EXE,
    V1_ZQNN,
    bpe_encode_py,
    fnv1a_64,
    load_zqnn_weights,
    np_forward_qtransformer,
    verify_qasm_block_unitary,
)

CORPUS_TXT = os.path.join(BUILD_DIR, "zcc_workspace_corpus.txt")
V2_ZQNN = os.path.join(BUILD_DIR, "zcc_qtransformer_v2_7q.zqnn")
V2_QASM = os.path.join(BUILD_DIR, "zcc_qtransformer_v2_7q.qasm")


class TestWin64GpuQllmV2(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (QLLM_EXE, QLLM_HOST_EXE, PTX_HOST, PTX_WIN64, CORPUS_TXT, V2_ZQNN, V2_QASM, V1_ZQNN, ZCC_WASM)
        missing = [p for p in required if not os.path.isfile(p)]
        if missing:
            raise unittest.SkipTest(f"Missing Track 14 artifacts: {missing}")

    def test_01_sovereign_ptx_pe32plus_triple_parity_and_strict_iat(self) -> None:
        sha_host = sha256_file(PTX_HOST)
        sha_win = sha256_file(PTX_WIN64)
        self.assertEqual(sha_host, sha_win)

        ptx_wasm = os.path.join(BUILD_DIR, "gpu_qllm_v2_wasm_test.ptx")
        try:
            node_bin = find_node_bin()
            res = run_node_wasi(
                node_bin,
                ZCC_WASM,
                [
                    "zcc",
                    "--target=ptx",
                    "--ptx-arch=sm_120",
                    "src/win64_gpu_qllm_kernels.c",
                    "-o",
                    "build_win64/gpu_qllm_v2_wasm_test.ptx",
                ],
            )
            self.assertEqual(res.returncode, 0, res.stderr)
            self.assertEqual(sha_host, sha256_file(ptx_wasm))
        finally:
            if os.path.exists(ptx_wasm):
                os.remove(ptx_wasm)

        with open(PTX_WIN64, "r", encoding="utf-8") as f:
            ptx_text = f.read()
        self.assertIn(".version 8.7", ptx_text)
        self.assertIn(".target sm_120", ptx_text)
        for entry in EXPECTED_ENTRIES:
            self.assertIn(f".visible .entry {entry}", ptx_text)

        self.assertEqual(sha256_file(QLLM_HOST_EXE), sha256_file(QLLM_EXE))
        fc_exe = run_fc_b(QLLM_HOST_EXE, QLLM_EXE)
        self.assertEqual(fc_exe.returncode, 0, fc_exe.stdout)
        self.assertIn("no differences encountered", fc_exe.stdout.lower())

        iat = audit_pe32plus_multi_dll(QLLM_EXE, required_dlls=("msvcrt.dll", "nvcuda.dll"))
        self.assertEqual(set(iat["dlls"].keys()), {"msvcrt.dll", "nvcuda.dll"})

    def test_02_bpe_roundtrip_and_numpy_complex128_v2_7q_oracle(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="tmp_qllm_v2_t2_") as tmpdir:
            rel_tmp = os.path.basename(tmpdir)
            ckpt = os.path.join(tmpdir, "oracle_7q_bpe.zqnn")
            logits_bin = os.path.join(tmpdir, "logits.bin")
            state_bin = os.path.join(tmpdir, "state.bin")
            tel_json = os.path.join(tmpdir, "tel.json")

            r_tr = run_win_cmd(
                QLLM_EXE,
                [
                    "--train",
                    "--data",
                    r"build_win64\zcc_workspace_corpus.txt",
                    "--bpe-vocab",
                    "320",
                    "--qubits",
                    "7",
                    "--heads",
                    "4",
                    "--layers",
                    "4",
                    "--pqc-depth",
                    "2",
                    "--batch",
                    "2",
                    "--seq",
                    "16",
                    "--steps",
                    "1",
                    "--seed",
                    "7777",
                    "--save",
                    rf"build_win64\{rel_tmp}\oracle_7q_bpe.zqnn",
                ],
                timeout_s=120,
            )
            self.assertEqual(r_tr.returncode, 0, r_tr.stderr + r_tr.stdout)
            bpe_kv = extract_prefixed_kv(r_tr.stdout, "QLLM_BPE")
            self.assertEqual(int(bpe_kv["vocab"]), 320)
            self.assertEqual(int(bpe_kv["merges"]), 64)

            prompt = "Q: What is ZKAEDI Q-Transformer?"
            r_inf = run_win_cmd(
                QLLM_EXE,
                [
                    "--load",
                    rf"build_win64\{rel_tmp}\oracle_7q_bpe.zqnn",
                    "--prompt",
                    prompt,
                    "--steps",
                    "1",
                    "--dump-logits",
                    rf"build_win64\{rel_tmp}\logits.bin",
                    "--dump-state",
                    rf"build_win64\{rel_tmp}\state.bin",
                    "--dump-telemetry",
                    rf"build_win64\{rel_tmp}\tel.json",
                ],
                timeout_s=120,
            )
            self.assertEqual(r_inf.returncode, 0, r_inf.stderr + r_inf.stdout)

            meta, weights = load_zqnn_weights(ckpt)
            self.assertEqual(meta["version"], 2)
            self.assertEqual(meta["vocab_size"], 320)
            self.assertEqual(meta["n_qubits"], 7)
            self.assertEqual(meta["dim"], 128)
            self.assertEqual(meta["n_layers"], 4)

            with open(tel_json, "r", encoding="utf-8") as f:
                tel = json.load(f)
            py_toks = bpe_encode_py(meta, prompt.encode("ascii"))
            gpu_prompt_toks = [t["id"] for t in tel["tokens"][: len(py_toks)]]
            self.assertEqual(py_toks, gpu_prompt_toks, "Python BPE vs C BPE encoder mismatch")

            np_logits, np_psi = np_forward_qtransformer(meta, weights, prompt.encode("ascii"))
            gpu_logits = np.fromfile(logits_bin, dtype="<f4").astype(np.float64)
            gpu_psi_raw = np.fromfile(state_bin, dtype="<f4").astype(np.float64)
            gpu_psi = gpu_psi_raw[:128] + 1j * gpu_psi_raw[128:]

            self.assertLess(float(np.max(np.abs(gpu_logits - np_logits))), 1e-4)
            self.assertLess(float(np.max(np.abs(gpu_psi - np_psi))), 1e-4)
            self.assertLess(abs(float(np.linalg.norm(gpu_psi)) - 1.0), 1e-5)

        r_gc = run_win_cmd(
            QLLM_EXE,
            ["--grad-check", "--qubits", "7", "--heads", "4", "--layers", "4", "--pqc-depth", "2", "--bpe-vocab", "512"],
            timeout_s=120,
        )
        self.assertEqual(r_gc.returncode, 0, r_gc.stderr + r_gc.stdout)
        kv = extract_prefixed_kv(r_gc.stdout, "QLLM_GRADCHECK_OK")
        self.assertGreaterEqual(int(kv["probes"]), 30)
        self.assertLess(float(kv["max_abs_err"]), 1e-3)
        self.assertLess(float(kv["param_shift_max_err"]), 1e-5)

    def test_03_live_rtx5070_7q_bpe_workspace_model_and_telemetry_math(self) -> None:
        meta, _ = load_zqnn_weights(V2_ZQNN)
        self.assertEqual(meta["version"], 2)
        self.assertEqual(meta["vocab_size"], 512)
        self.assertEqual(meta["n_qubits"], 7)
        self.assertEqual(meta["dim"], 128)
        self.assertEqual(meta["n_layers"], 4)

        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="tmp_qllm_v2_t3_") as tmpdir:
            rel_tmp = os.path.basename(tmpdir)
            runs = []
            tel = None
            for idx in range(2):
                args = [
                    "--load",
                    r"build_win64\zcc_qtransformer_v2_7q.zqnn",
                    "--prompt",
                    "Q: What is ZKAEDI Q-Transformer?",
                    "--steps",
                    "40",
                ]
                if idx == 0:
                    args += ["--dump-telemetry", rf"build_win64\{rel_tmp}\t.json"]
                r = run_win_cmd(QLLM_EXE, args, timeout_s=120)
                self.assertEqual(r.returncode, 0, r.stderr + r.stdout)
                ok_kv = extract_prefixed_kv(r.stdout, "QLLM_OK")
                gen_txt = extract_quoted_field(r.stdout, "QLLM_TEXT", "generated")
                runs.append((ok_kv["logits_fnv"], gen_txt))
                if idx == 0:
                    with open(os.path.join(tmpdir, "t.json"), "r", encoding="utf-8") as f:
                        tel = json.load(f)

            self.assertEqual(runs[0], runs[1])
            self.assertIn("complex-wavefunction neural language model", runs[0][1])

            psi = np.asarray(tel["final_state"]["re"], dtype=np.float64) + 1j * np.asarray(
                tel["final_state"]["im"], dtype=np.float64
            )
            D = 128
            self.assertEqual(psi.shape, (D,))
            self.assertLess(abs(float(np.linalg.norm(psi)) - 1.0), 1e-5)

            M = psi.reshape(8, 16)
            rho_a = M @ M.conj().T
            renyi2_ref = -math.log2(max(float(np.trace(rho_a @ rho_a).real), 1e-12))
            last_layer = tel["layers"][-1]
            self.assertLess(abs(renyi2_ref - last_layer["renyi2_bits"]), 1e-4)
            self.assertGreater(renyi2_ref, 0.5)

            pauli_x = np.array([[0.0, 1.0], [1.0, 0.0]], dtype=np.complex128)
            pauli_y = np.array([[0.0, -1j], [1j, 0.0]], dtype=np.complex128)
            pauli_z = np.array([[1.0, 0.0], [0.0, -1.0]], dtype=np.complex128)
            idx_all = np.arange(D)
            for q, brec in enumerate(last_layer["bloch"]):
                p0 = psi[(idx_all & (1 << q)) == 0]
                p1 = psi[(idx_all & (1 << q)) != 0]
                rho_q = np.array(
                    [
                        [np.vdot(p0, p0), np.vdot(p1, p0)],
                        [np.vdot(p0, p1), np.vdot(p1, p1)],
                    ],
                    dtype=np.complex128,
                )
                rx = float(np.trace(rho_q @ pauli_x).real)
                ry = float(np.trace(rho_q @ pauli_y).real)
                rz = float(np.trace(rho_q @ pauli_z).real)
                rr = math.sqrt(rx * rx + ry * ry + rz * rz)
                pur = float(np.trace(rho_q @ rho_q).real)
                evals = np.clip(np.linalg.eigvalsh(rho_q).real, 1e-12, 1.0)
                svn = -float(np.sum(evals * np.log2(evals)))

                self.assertLess(abs(rx - brec["x"]), 1e-4)
                self.assertLess(abs(ry - brec["y"]), 1e-4)
                self.assertLess(abs(rz - brec["z"]), 1e-4)
                self.assertLess(abs(rr - brec["r"]), 1e-4)
                self.assertLess(abs(pur - brec["purity"]), 1e-4)
                self.assertLess(abs(svn - brec["vn_entropy"]), 1e-4)

            top_preds = tel["top_predictions"]
            self.assertEqual(len(top_preds), 16)
            probs = [p["prob"] for p in top_preds]
            self.assertTrue(all(p > 0.0 for p in probs))
            self.assertEqual(probs, sorted(probs, reverse=True))
            self.assertLessEqual(sum(probs), 1.0 + 1e-4)

    def test_04_openqasm3_7q_4layer_unitary_verification(self) -> None:
        meta, weights = load_zqnn_weights(V2_ZQNN)
        self.assertEqual(meta["n_qubits"], 7)
        unitarity_err, statevec_err = verify_qasm_block_unitary(
            V2_QASM,
            n_qubits=7,
            expected_blocks=20,
            expected_gates=1120,
            block_header="// Layer 0 Block U_Q",
            theta_expected=weights["layers"][0]["theta_q"],
        )
        self.assertLess(unitarity_err, 1e-10)
        self.assertLess(statevec_err, 1e-5)

    def test_05_visualizer_ef7_headless_node_and_live_bridge_api(self) -> None:
        rep = verify_visualizer_headless()
        self.assertEqual(rep["errors"], 0)
        self.assertEqual(rep["collapsed_drawOps"], 0)
        self.assertGreaterEqual(rep["active_drawOps"], 1000)
        self.assertEqual(rep["preds"], 12)

        srv, url = create_server(host="127.0.0.1", port=0)
        t = threading.Thread(target=srv.serve_forever, daemon=True)
        t.start()
        try:
            with urllib.request.urlopen(url + "/", timeout=10) as r:
                self.assertEqual(r.status, 200)
                html = r.read().decode("utf-8")
                self.assertIn("ZKAEDI Hybrid Quantum-Classical Transformer", html)

            with urllib.request.urlopen(url + "/api/models", timeout=10) as r:
                self.assertEqual(r.status, 200)
                models = {m["id"] for m in json.loads(r.read().decode("utf-8"))["models"]}
                self.assertIn("v2_7q", models)
                self.assertIn("v1", models)

            req_body = json.dumps({"model": "v2_7q", "prompt": "Q: What is ZCC?", "steps": 12, "temp": 0.0}).encode(
                "utf-8"
            )
            req = urllib.request.Request(
                url + "/api/infer",
                data=req_body,
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=60) as r:
                self.assertEqual(r.status, 200)
                body = json.loads(r.read().decode("utf-8"))
                self.assertEqual(body["model"]["qubits"], 7)
                self.assertEqual(len(body["final_state"]["re"]), 128)
                self.assertEqual(len(body["layers"]), 5)
        finally:
            srv.shutdown()
            srv.server_close()
            t.join(timeout=5)

    def test_06_v2_ef0_negative_controls_and_v1_compat(self) -> None:
        for bad_v in ("100", "4096"):
            r = run_win_cmd(QLLM_EXE, ["--train", "--bpe-vocab", bad_v, "--steps", "1"], timeout_s=60)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("INVALID_ARG:", r.stderr + r.stdout)

        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="tmp_qllm_v2_t6_") as tmpdir:
            rel_tmp = os.path.basename(tmpdir)
            with open(V2_ZQNN, "rb") as f:
                orig = bytearray(f.read())

            # V2NC-3: 1-byte bitflip inside BPE merge table (header is 64 B)
            bad_ck = bytearray(orig)
            bad_ck[70] ^= 0x01
            with open(os.path.join(tmpdir, "bad_ck.zqnn"), "wb") as f:
                f.write(bad_ck)
            r3 = run_win_cmd(QLLM_EXE, ["--load", rf"build_win64\{rel_tmp}\bad_ck.zqnn", "--steps", "1"], timeout_s=60)
            self.assertNotEqual(r3.returncode, 0)
            self.assertIn("ZQNN_CHECKSUM_MISMATCH:", r3.stderr + r3.stdout)

            # V2NC-4: Forged out-of-range/acyclic BPE merge with recomputed valid FNV-1a checksum
            forged = bytearray(orig)
            struct.pack_into("<HH", forged, 64, 999, 0)
            new_fnv = fnv1a_64(bytes(forged[64:]))
            struct.pack_into("<Q", forged, 56, new_fnv)
            with open(os.path.join(tmpdir, "forged_merge.zqnn"), "wb") as f:
                f.write(forged)
            r4 = run_win_cmd(
                QLLM_EXE, ["--load", rf"build_win64\{rel_tmp}\forged_merge.zqnn", "--steps", "1"], timeout_s=60
            )
            self.assertNotEqual(r4.returncode, 0)
            self.assertIn("ZQNN_INVALID:", r4.stderr + r4.stdout)

        # V2NC-5: Confirm Track 13 ZQNNv1 backwards compatibility
        rv1 = run_win_cmd(
            QLLM_EXE,
            ["--load", r"build_win64\zcc_qtransformer_v1.zqnn", "--prompt", "Q: What is ZCC?", "--steps", "16"],
            timeout_s=60,
        )
        self.assertEqual(rv1.returncode, 0, rv1.stderr + rv1.stdout)
        self.assertIn("QLLM_OK", rv1.stdout)


if __name__ == "__main__":
    unittest.main()
