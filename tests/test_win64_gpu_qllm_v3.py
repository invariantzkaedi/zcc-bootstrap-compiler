#!/usr/bin/env python3
"""
ZCC Track 15 Verification Suite: Multi-Register Coupled Q-Transformer (ZQNNv3),
Blackwell SM 12.0 Tensor Core MMA (`mma.sync`), & Causal KV-Cache O(1) Decode.
"""

import hashlib
import json
import math
import os
import re
import shutil
import struct
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

import numpy as np

from tests.test_win64_gpu_llm import extract_prefixed_kv, extract_quoted_field
from tests.test_win64_gpu_qllm import (
    fnv1a_64,
    np_cunit_norm,
    np_pqc_apply,
)
from tools.build_gpu_win64 import audit_pe32plus_multi_dll
from tools.build_lua_win64 import BUILD_DIR, REPO_ROOT, run_win_cmd, sha256_file, to_rel_win
from tools.qllm_visualizer_server import create_server, verify_visualizer_headless

QLLM_EXE = os.path.join(BUILD_DIR, "zcc_gpu_qllm.exe")
QLLM_HOST_EXE = os.path.join(BUILD_DIR, "zcc_gpu_qllm_host_zcc.exe")
QLLM_PTX = os.path.join(BUILD_DIR, "gpu_qllm_kernels.ptx")
QLLM_HOST_PTX = os.path.join(BUILD_DIR, "gpu_qllm_kernels_host.ptx")

V3_CORPUS = os.path.join(BUILD_DIR, "zcc_v3_corpus.txt")
CANONICAL_V3_CKPT = os.path.join(BUILD_DIR, "zcc_qtransformer_v3_mr.zqnn")
CANONICAL_V3_QASM = os.path.join(BUILD_DIR, "zcc_qtransformer_v3_mr.qasm")


def load_zqnn_v3(path: str) -> tuple[dict, dict]:
    with open(path, "rb") as f:
        blob = f.read()
    if len(blob) < 64:
        raise AssertionError(f"Truncated ZQNN header in {path}")
    (
        magic0,
        magic1,
        version,
        vocab_size,
        n_qubits,
        dim,
        n_heads,
        n_layers,
        pqc_depth,
        h_ff,
        total_params,
        pad0,
        seed,
        fnv64,
    ) = struct.unpack_from("<12I2Q", blob, 0)

    if magic0 != 0x4E4E515A or magic1 != 0x00000001:
        raise AssertionError(f"Bad ZQNN magic: {magic0:#x} {magic1:#x}")
    if version not in (1, 2, 3):
        raise AssertionError(f"Unsupported ZQNN version {version}")

    reg_dim = 1 << n_qubits
    if dim % reg_dim != 0:
        raise AssertionError(f"dim {dim} not divisible by 2^n_qubits {reg_dim}")
    n_reg = dim // reg_dim
    n_merges = pad0 if vocab_size >= 256 else 0

    payload_off = 64 + n_merges * 4
    expected_len = payload_off + total_params * 4
    if len(blob) != expected_len:
        raise AssertionError(f"Size mismatch: {len(blob)} != {expected_len}")

    got_fnv = fnv1a_64(blob[64:])
    if got_fnv != fnv64:
        raise AssertionError(f"FNV mismatch: expected={fnv64:016x} got={got_fnv:016x}")

    merges = []
    vocab_bytes = [bytes([i]) for i in range(min(256, vocab_size))]
    for mi in range(n_merges):
        l, r = struct.unpack_from("<2H", blob, 64 + mi * 4)
        merges.append((l, r))
        vocab_bytes.append(vocab_bytes[l] + vocab_bytes[r])

    arr = np.frombuffer(blob, dtype="<f4", count=total_params, offset=payload_off).astype(np.float64)
    n_pqc_params = n_reg * pqc_depth * 3 * n_qubits + (n_reg if n_reg > 1 else 0)

    weights = {}
    off = 0
    weights["E_re"] = arr[off : off + vocab_size * dim].reshape(vocab_size, dim)
    off += vocab_size * dim
    weights["E_im"] = arr[off : off + vocab_size * dim].reshape(vocab_size, dim)
    off += vocab_size * dim

    layer_weights = []
    for _ in range(n_layers):
        lw = {}
        for kname in ("theta_q", "theta_k", "theta_v", "theta_f1", "theta_f2"):
            lw[kname] = arr[off : off + n_pqc_params].copy()
            off += n_pqc_params
        lw["beta"] = arr[off : off + n_heads].copy()
        off += n_heads
        lw["gamma"] = arr[off : off + n_heads].copy()
        off += n_heads
        lw["kerr_k"] = arr[off : off + dim].copy()
        off += dim
        lw["kerr_b"] = arr[off : off + dim].copy()
        off += dim
        lw["W1"] = arr[off : off + h_ff * (2 * dim)].reshape(h_ff, 2 * dim)
        off += h_ff * (2 * dim)
        lw["b1"] = arr[off : off + h_ff].copy()
        off += h_ff
        lw["W2"] = arr[off : off + (2 * dim) * h_ff].reshape(2 * dim, h_ff)
        off += (2 * dim) * h_ff
        lw["b2"] = arr[off : off + (2 * dim)].copy()
        off += 2 * dim
        layer_weights.append(lw)

    weights["layers"] = layer_weights
    weights["H_out"] = arr[off : off + vocab_size * (2 * dim)].reshape(vocab_size, 2 * dim)
    off += vocab_size * (2 * dim)
    weights["b_out"] = arr[off : off + vocab_size].copy()
    off += vocab_size
    assert off == total_params

    meta = {
        "version": version,
        "vocab_size": vocab_size,
        "n_merges": n_merges,
        "merges": merges,
        "vocab_bytes": vocab_bytes,
        "n_qubits": n_qubits,
        "n_registers": n_reg,
        "reg_dim": reg_dim,
        "dim": dim,
        "n_heads": n_heads,
        "head_dim": dim // n_heads,
        "n_layers": n_layers,
        "pqc_depth": pqc_depth,
        "h_ff": h_ff,
        "total_params": total_params,
        "n_pqc_params": n_pqc_params,
    }
    return meta, weights


def bpe_encode_v3(meta: dict, raw_bytes: bytes) -> list[int]:
    if meta.get("vocab_size", 128) < 256:
        return [b if b < 128 else 32 for b in raw_bytes]
    buf = list(raw_bytes)
    for mi, (l, r) in enumerate(meta["merges"]):
        ntok = 256 + mi
        out = []
        i = 0
        n = len(buf)
        while i < n:
            if i + 1 < n and buf[i] == l and buf[i + 1] == r:
                out.append(ntok)
                i += 2
            else:
                out.append(buf[i])
                i += 1
        buf = out
    return buf


def np_pqc_mr_apply(psi_in: np.ndarray, params: np.ndarray, n_reg: int, n_qubits: int, pqc_depth: int) -> np.ndarray:
    reg_dim = 1 << n_qubits
    per_reg = pqc_depth * 3 * n_qubits
    out = np.empty_like(psi_in)
    for r in range(n_reg):
        sl = slice(r * reg_dim, (r + 1) * reg_dim)
        th = params[r * per_reg : (r + 1) * per_reg].reshape(pqc_depth, 3, n_qubits)
        out[..., sl] = np_pqc_apply(psi_in[..., sl], th, n_qubits)
    if n_reg > 1:
        phi = params[n_reg * per_reg : n_reg * per_reg + n_reg]
        for r in range(n_reg):
            rn = (r + 1) % n_reg
            c = math.cos(0.5 * float(phi[r]))
            s = math.sin(0.5 * float(phi[r]))
            sl0 = slice(r * reg_dim, (r + 1) * reg_dim)
            sl1 = slice(rn * reg_dim, (rn + 1) * reg_dim)
            a = out[..., sl0].copy()
            b = out[..., sl1].copy()
            out[..., sl0] = c * a - s * b
            out[..., sl1] = s * a + c * b
    return out


def np_forward_v3(meta: dict, weights: dict, token_ids: list[int]) -> tuple[np.ndarray, np.ndarray]:
    D = meta["dim"]
    n_reg = meta["n_registers"]
    nq = meta["n_qubits"]
    pd = meta["pqc_depth"]
    H = meta["n_heads"]
    Dh = meta["head_dim"]
    toks = np.asarray(token_ids, dtype=np.int64)
    T = len(toks)
    sqrt_d = math.sqrt(float(D))

    emb = weights["E_re"][toks] + 1j * weights["E_im"][toks]
    pos = np.arange(T, dtype=np.float64)[:, None]
    omega_d = 10000.0 ** (-np.arange(D, dtype=np.float64)[None, :] / float(D))
    x = np_cunit_norm(emb * np.exp(1j * pos * omega_d))

    omega_h = 10000.0 ** (-np.arange(Dh, dtype=np.float64)[None, :] / float(Dh))
    rope_h = np.exp(1j * pos * omega_h)
    inv_sqrt_dh = 1.0 / math.sqrt(float(Dh))
    mask = np.triu(np.ones((T, T), dtype=bool), k=1)

    for lw in weights["layers"]:
        Q = np_pqc_mr_apply(x, lw["theta_q"], n_reg, nq, pd).reshape(T, H, Dh) * rope_h[:, None, :]
        K = np_pqc_mr_apply(x, lw["theta_k"], n_reg, nq, pd).reshape(T, H, Dh) * rope_h[:, None, :]
        V = np_pqc_mr_apply(x, lw["theta_v"], n_reg, nq, pd).reshape(T, H, Dh)

        attn_out = np.zeros((T, H, Dh), dtype=np.complex128)
        for h in range(H):
            qh = Q[:, h, :]
            kh = K[:, h, :]
            vh = V[:, h, :]
            ov = qh @ kh.conj().T
            R, I = ov.real, ov.imag
            S = (lw["beta"][h] * (R * R + I * I) + lw["gamma"][h] * R) * inv_sqrt_dh
            S = np.where(mask, -1e30, S)
            S = S - np.max(S, axis=1, keepdims=True)
            W = np.where(mask, 0.0, np.exp(S))
            W = W / np.sum(W, axis=1, keepdims=True)
            attn_out[:, h, :] = W @ vh
        attn_out = attn_out.reshape(T, D)

        psi_mid = np_cunit_norm(x + attn_out)
        u = np_pqc_mr_apply(psi_mid, lw["theta_f1"], n_reg, nq, pd)
        phi_kerr = lw["kerr_k"][None, :] * (float(D) * (u.real ** 2 + u.imag ** 2)) + lw["kerr_b"][None, :]
        v_kerr = u * np.exp(1j * phi_kerr)

        quad_in = sqrt_d * np.concatenate([v_kerr.real, v_kerr.imag], axis=-1)
        h1_pre = quad_in @ lw["W1"].T + lw["b1"][None, :]
        h1_act = h1_pre / (1.0 + np.exp(-np.clip(h1_pre, -60.0, 60.0)))
        delta_quad = h1_act @ lw["W2"].T + lw["b2"][None, :]
        delta = (delta_quad[:, :D] + 1j * delta_quad[:, D:]) / sqrt_d

        pre_f2 = psi_mid + delta
        post_f2 = np_pqc_mr_apply(pre_f2, lw["theta_f2"], n_reg, nq, pd)
        x = np_cunit_norm(post_f2)

    last_psi = x[-1]
    last_quad = sqrt_d * np.concatenate([last_psi.real, last_psi.imag])
    logits = weights["H_out"] @ last_quad + weights["b_out"]
    return logits, last_psi


def single_reg_gate_unitary(nq: int, gates: list[tuple]) -> np.ndarray:
    D = 1 << nq
    U = np.eye(D, dtype=np.complex128)
    for g in gates:
        kind = g[0]
        G = np.zeros((D, D), dtype=np.complex128)
        if kind == "ry":
            _, q, th = g
            bit = 1 << q
            c, s = math.cos(0.5 * th), math.sin(0.5 * th)
            for i in range(D):
                if (i & bit) == 0:
                    j = i | bit
                    G[i, i] = c
                    G[i, j] = -s
                    G[j, i] = s
                    G[j, j] = c
        elif kind == "rz":
            _, q, th = g
            bit = 1 << q
            ep = complex(math.cos(0.5 * th), -math.sin(0.5 * th))
            em = complex(math.cos(0.5 * th), math.sin(0.5 * th))
            for i in range(D):
                G[i, i] = ep if (i & bit) == 0 else em
        elif kind == "cx":
            _, cbit, tbit = g
            cm, tm = 1 << cbit, 1 << tbit
            for i in range(D):
                G[i ^ tm if (i & cm) else i, i] = 1.0
        elif kind == "cp":
            _, cbit, tbit, th = g
            cm, tm = 1 << cbit, 1 << tbit
            ph = complex(math.cos(th), math.sin(th))
            for i in range(D):
                G[i, i] = ph if ((i & cm) and (i & tm)) else 1.0
        U = G @ U
    return U


class Win64GpuQllmV3Tests(unittest.TestCase):
    def test_01_ptx_40_entries_mma_sync_and_pe_audit(self) -> None:
        self.assertTrue(os.path.isfile(QLLM_PTX))
        self.assertTrue(os.path.isfile(QLLM_EXE))
        self.assertEqual(sha256_file(QLLM_HOST_PTX), sha256_file(QLLM_PTX))
        self.assertEqual(sha256_file(QLLM_HOST_EXE), sha256_file(QLLM_EXE))

        with open(QLLM_PTX, "r", encoding="utf-8") as f:
            ptx_text = f.read()
        entries = re.findall(r"\.visible \.entry\s+([A-Za-z0-9_]+)\s*\(", ptx_text)
        self.assertGreaterEqual(len(entries), 40, f"Expected >= 40 PTX kernels, got {len(entries)}")
        for req in (
            "qllm_gemm_tc_f16",
            "qllm_pqc_mr_fwd",
            "qllm_pqc_mr_bwd",
            "qllm_reg_bridge_fwd",
            "qllm_reg_bridge_bwd",
            "qllm_kv_store_slice",
            "qllm_born_attn_decode_step",
            "qllm_sq_sum_blocks",
            "qllm_scale_f32",
        ):
            self.assertIn(req, entries)
        self.assertIn("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32", ptx_text)

        audit = audit_pe32plus_multi_dll(QLLM_EXE, required_dlls=("msvcrt.dll", "nvcuda.dll"))
        self.assertEqual(set(audit["dlls"].keys()), {"msvcrt.dll", "nvcuda.dll"})
        self.assertIn("cuModuleLoadDataEx", audit["dlls"]["nvcuda.dll"])

    def test_02_multireg_grad_check_and_param_shift(self) -> None:
        res = run_win_cmd(
            QLLM_EXE,
            [
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

    def test_03_numpy_multireg_forward_and_qasm_unitary_oracle(self) -> None:
        self.assertTrue(os.path.isfile(CANONICAL_V3_CKPT))
        self.assertTrue(os.path.isfile(CANONICAL_V3_QASM))
        meta, weights = load_zqnn_v3(CANONICAL_V3_CKPT)
        self.assertEqual(meta["version"], 3)
        self.assertEqual(meta["n_registers"], 4)
        self.assertEqual(meta["n_qubits"], 6)
        self.assertEqual(meta["dim"], 256)

        prompt = "Q: How does Multi-Register Coupled ZQNNv3 scale?"
        tok_ids = bpe_encode_v3(meta, prompt.encode("ascii"))
        np_logits, np_state = np_forward_v3(meta, weights, tok_ids)
        self.assertAlmostEqual(float(np.sum(np.abs(np_state) ** 2)), 1.0, delta=1e-6)

        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="tmp_v3_t03_") as td:
            logits_bin = os.path.join(td, "logits.bin")
            state_bin = os.path.join(td, "state.bin")
            res = run_win_cmd(
                QLLM_EXE,
                [
                    "--load", to_rel_win(CANONICAL_V3_CKPT),
                    "--prompt", prompt,
                    "--steps", "0",
                    "--temp", "0.0",
                    "--dump-logits", to_rel_win(logits_bin),
                    "--dump-state", to_rel_win(state_bin),
                ],
                timeout_s=120,
            )
            self.assertEqual(res.returncode, 0, res.stderr + res.stdout)
            gpu_logits = np.fromfile(logits_bin, dtype="<f4").astype(np.float64)
            gpu_state_ri = np.fromfile(state_bin, dtype="<f4").astype(np.float64)
            gpu_state = gpu_state_ri[: meta["dim"]] + 1j * gpu_state_ri[meta["dim"] :]

            max_logit_err = float(np.max(np.abs(gpu_logits - np_logits)))
            max_state_err = float(np.max(np.abs(gpu_state - np_state)))
            self.assertLess(max_logit_err, 1e-4, f"max_logit_err={max_logit_err}")
            self.assertLess(max_state_err, 1e-4, f"max_state_err={max_state_err}")
            self.assertAlmostEqual(float(np.sum(np.abs(gpu_state) ** 2)), 1.0, delta=1e-5)

        with open(CANONICAL_V3_QASM, "r", encoding="utf-8") as f:
            qasm_lines = [ln.strip() for ln in f if ln.strip()]
        self.assertEqual(qasm_lines[0], "OPENQASM 3.0;")
        for r in range(4):
            self.assertIn(f"qubit[6] r{r};", qasm_lines)
        reg_headers = [ln for ln in qasm_lines if ln.startswith("// Layer ") and " Register " in ln]
        self.assertEqual(len(reg_headers), 80)  # 4 layers * 5 slots * 4 registers
        bridge_lines = [ln for ln in qasm_lines if ln.startswith("// Inter-Register Cyclic Givens")]
        self.assertEqual(len(bridge_lines), 20)  # 4 layers * 5 slots

        # Verify Layer 0 U_Q all 4 registers unitary U^H U == I and match checkpoint angles
        per_reg = meta["pqc_depth"] * 3 * meta["n_qubits"]
        blocks: dict[str, list[tuple]] = {}
        cur = None
        for ln in qasm_lines:
            if ln.startswith("// Layer "):
                cur = ln
                blocks[cur] = []
                continue
            if cur is None or ln.startswith("//"):
                continue
            m_ry = re.match(r"ry\(([^)]+)\)\s+r(\d+)\[(\d+)\];", ln)
            m_rz = re.match(r"rz\(([^)]+)\)\s+r(\d+)\[(\d+)\];", ln)
            m_cx = re.match(r"cx\s+r(\d+)\[(\d+)\],\s*r(\d+)\[(\d+)\];", ln)
            m_cp = re.match(r"cp\(([^)]+)\)\s+r(\d+)\[(\d+)\],\s*r(\d+)\[(\d+)\];", ln)
            if m_ry:
                blocks[cur].append(("ry", int(m_ry.group(3)), float(m_ry.group(1))))
            elif m_rz:
                blocks[cur].append(("rz", int(m_rz.group(3)), float(m_rz.group(1))))
            elif m_cx:
                blocks[cur].append(("cx", int(m_cx.group(2)), int(m_cx.group(4))))
            elif m_cp:
                blocks[cur].append(("cp", int(m_cp.group(3)), int(m_cp.group(5)), float(m_cp.group(1))))

        nq = meta["n_qubits"]
        for r in range(4):
            key = f"// Layer 0 Block U_Q Register {r}"
            U = single_reg_gate_unitary(nq, blocks[key])
            unit_err = float(np.linalg.norm(U.conj().T @ U - np.eye(1 << nq)))
            self.assertLess(unit_err, 1e-10)
            th_q = weights["layers"][0]["theta_q"][r * per_reg : (r + 1) * per_reg]
            th_qasm = [g[-1] for g in blocks[key] if g[0] in ("ry", "rz", "cp")]
            expected_flat = []
            for p in range(meta["pqc_depth"]):
                base = p * 3 * nq
                expected_flat.extend(th_q[base : base + 2 * nq])
                expected_flat.extend(th_q[base + 2 * nq : base + 3 * nq])
            self.assertLess(float(np.max(np.abs(np.array(th_qasm) - np.array(expected_flat)))), 1e-6)
        phi_qasm = [float(x) for x in re.findall(r"\d+->\d+=([-+0-9.eE]+)", bridge_lines[0])]
        self.assertEqual(len(phi_qasm), 4)
        self.assertLess(float(np.max(np.abs(np.array(phi_qasm) - weights["layers"][0]["theta_q"][4 * per_reg : 4 * per_reg + 4]))), 1e-6)

    def test_04_tensor_core_training_convergence_and_speed(self) -> None:
        self.assertTrue(os.path.isfile(V3_CORPUS))
        common = [
            "--train", "--data", to_rel_win(V3_CORPUS),
            "--bpe-vocab", "1024", "--registers", "4", "--qubits", "6",
            "--heads", "8", "--layers", "4", "--pqc-depth", "2",
            "--batch", "8", "--seq", "64", "--steps", "120",
            "--warmup", "12", "--log-every", "60", "--lr", "0.003", "--seed", "808",
        ]
        res_tc = run_win_cmd(QLLM_EXE, common + ["--tensor-core"], timeout_s=120)
        self.assertEqual(res_tc.returncode, 0, res_tc.stderr + res_tc.stdout)
        kv_tc = extract_prefixed_kv(res_tc.stdout, "QLLM_TRAIN_OK")

        res_fp = run_win_cmd(QLLM_EXE, common + ["--no-tensor-core"], timeout_s=120)
        self.assertEqual(res_fp.returncode, 0, res_fp.stderr + res_fp.stdout)
        kv_fp = extract_prefixed_kv(res_fp.stdout, "QLLM_TRAIN_OK")

        self.assertLess(float(kv_tc["final_loss"]), float(kv_tc["init_loss"]) - 1.5)
        self.assertLess(float(kv_tc["norm_err"]), 1e-5)
        self.assertLess(float(kv_tc["train_ms"]), float(kv_fp["train_ms"]))

    def test_05_kv_cache_exact_parity_and_speedup(self) -> None:
        prompt = "Q: What is ZKAEDI Q-Transformer?"
        res_kv = run_win_cmd(
            QLLM_EXE,
            [
                "--load", to_rel_win(CANONICAL_V3_CKPT),
                "--seq", "128", "--steps", "96", "--temp", "0.0",
                "--prompt", prompt,
            ],
            timeout_s=120,
        )
        self.assertEqual(res_kv.returncode, 0, res_kv.stderr + res_kv.stdout)
        kv_ok = extract_prefixed_kv(res_kv.stdout, "QLLM_OK")
        kv_txt = extract_quoted_field(res_kv.stdout, "QLLM_TEXT", "generated")

        res_no = run_win_cmd(
            QLLM_EXE,
            [
                "--load", to_rel_win(CANONICAL_V3_CKPT),
                "--seq", "128", "--steps", "96", "--temp", "0.0",
                "--no-kv-cache", "--prompt", prompt,
            ],
            timeout_s=120,
        )
        self.assertEqual(res_no.returncode, 0, res_no.stderr + res_no.stdout)
        no_ok = extract_prefixed_kv(res_no.stdout, "QLLM_OK")
        no_txt = extract_quoted_field(res_no.stdout, "QLLM_TEXT", "generated")

        self.assertEqual(kv_ok["logits_fnv"], no_ok["logits_fnv"])
        self.assertEqual(kv_ok["top1_id"], no_ok["top1_id"])
        self.assertEqual(kv_txt, no_txt)
        self.assertLess(float(kv_ok["norm_err"]), 1e-5)
        self.assertGreater(float(kv_ok["tok_per_sec"]), float(no_ok["tok_per_sec"]))

    def test_06_warmup_resume_and_negative_controls(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="tmp_v3_t06_") as td:
            ckpt1 = os.path.join(td, "step40.zqnn")
            ckpt2 = os.path.join(td, "step100.zqnn")
            res1 = run_win_cmd(
                QLLM_EXE,
                [
                    "--train", "--data", to_rel_win(V3_CORPUS),
                    "--bpe-vocab", "320", "--registers", "2", "--qubits", "5",
                    "--heads", "4", "--layers", "2", "--pqc-depth", "2",
                    "--batch", "4", "--seq", "48", "--steps", "40",
                    "--warmup", "10", "--log-every", "40", "--lr", "0.008",
                    "--save", to_rel_win(ckpt1),
                ],
                timeout_s=120,
            )
            self.assertEqual(res1.returncode, 0, res1.stderr + res1.stdout)
            kv1 = extract_prefixed_kv(res1.stdout, "QLLM_TRAIN_OK")

            res2 = run_win_cmd(
                QLLM_EXE,
                [
                    "--resume", to_rel_win(ckpt1),
                    "--data", to_rel_win(V3_CORPUS),
                    "--batch", "4", "--seq", "48", "--steps", "60",
                    "--warmup", "5", "--log-every", "30", "--lr", "0.006",
                    "--save", to_rel_win(ckpt2),
                ],
                timeout_s=120,
            )
            self.assertEqual(res2.returncode, 0, res2.stderr + res2.stdout)
            kv2 = extract_prefixed_kv(res2.stdout, "QLLM_TRAIN_OK")
            self.assertLess(float(kv2["final_loss"]), float(kv1["final_loss"]))
            meta2, _ = load_zqnn_v3(ckpt2)
            self.assertEqual(meta2["n_registers"], 2)

            # Negative control (a): 1-byte payload flip -> ZQNN_CHECKSUM_MISMATCH
            bad_cksum = os.path.join(td, "bad_cksum.zqnn")
            shutil.copy2(ckpt2, bad_cksum)
            with open(bad_cksum, "rb") as f:
                raw = bytearray(f.read())
            raw[-17] ^= 0x01
            with open(bad_cksum, "wb") as f:
                f.write(raw)
            rc_bad = run_win_cmd(QLLM_EXE, ["--load", to_rel_win(bad_cksum), "--steps", "1"], timeout_s=60)
            self.assertNotEqual(rc_bad.returncode, 0)
            self.assertIn("ZQNN_CHECKSUM_MISMATCH", rc_bad.stderr)

            # Negative control (b): invalid --registers 3 -> INVALID_ARG
            rc_reg = run_win_cmd(QLLM_EXE, ["--registers", "3", "--grad-check"], timeout_s=60)
            self.assertNotEqual(rc_reg.returncode, 0)
            self.assertIn("INVALID_ARG", rc_reg.stderr)

            # Negative control (c): corrupted header dim -> ZQNN_INVALID
            bad_dim = os.path.join(td, "bad_dim.zqnn")
            shutil.copy2(ckpt2, bad_dim)
            with open(bad_dim, "rb") as f:
                raw_d = bytearray(f.read())
            struct.pack_into("<I", raw_d, 20, 65)  # dim=65 invalid for 2^5=32
            with open(bad_dim, "wb") as f:
                f.write(raw_d)
            rc_dim = run_win_cmd(QLLM_EXE, ["--load", to_rel_win(bad_dim), "--steps", "1"], timeout_s=60)
            self.assertNotEqual(rc_dim.returncode, 0)
            self.assertIn("ZQNN_INVALID", rc_dim.stderr)

    def test_07_canonical_v3_mr_telemetry_and_visualizer_api(self) -> None:
        self.assertTrue(os.path.isfile(CANONICAL_V3_CKPT))
        self.assertTrue(os.path.isfile(CANONICAL_V3_QASM))
        meta, _ = load_zqnn_v3(CANONICAL_V3_CKPT)
        self.assertEqual(meta["vocab_size"], 1024)
        self.assertEqual(meta["n_registers"], 4)
        self.assertEqual(meta["n_qubits"], 6)

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
            self.assertIn("v3_mr", ids)

            payload = json.dumps({
                "model": "v3_mr",
                "prompt": "Q: What is ZCC?",
                "steps": 12,
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
            self.assertEqual(len(tel["layers"]), 5)
            last_l = tel["layers"][-1]
            self.assertEqual(len(last_l["reg_weights"]), 4)
            self.assertAlmostEqual(sum(last_l["reg_weights"]), 1.0, delta=1e-4)
            self.assertLess(float(last_l["norm_err"]), 1e-5)

            bad_req = urllib.request.Request(
                f"{base_url}/api/infer",
                data=json.dumps({"model": "v99", "prompt": "hi", "steps": 4}).encode("utf-8"),
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            with self.assertRaises(urllib.error.HTTPError) as ctx:
                urllib.request.urlopen(bad_req, timeout=30)
            self.assertEqual(ctx.exception.code, 400)
            ctx.exception.close()
        finally:
            srv.shutdown()
            srv.server_close()


if __name__ == "__main__":
    unittest.main()
