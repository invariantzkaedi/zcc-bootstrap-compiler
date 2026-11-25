#!/usr/bin/env python3
"""
Sovereign Win64 + Blackwell SM 12.0 Hybrid Quantum-Transformer Verification Suite (Track 13)
===========================================================================================
Tests (ZCC-EF-0 & ZCC-EF-7 Evidence Tier E2):
  1. Triple-compiler PTX byte identity (./zcc, zcc_sov_stage3.exe, build_wasm/zcc.wasm),
     28 SM 12.0 kernel entries, Win64 PE32+ parity, and strict DLL IAT {msvcrt.dll, nvcuda.dll}.
  2. Independent pure-NumPy complex128 quantum forward oracle (diff < 1e-4) +
     live RTX 5070 gradient finite-diff & exact Z_0 parameter-shift check (--grad-check).
  3. Live RTX 5070 from-scratch 6-qubit training convergence (loss ~5.0 -> < 0.15),
     unit-norm invariant, bipartite Renyi-2 entanglement entropy, & deterministic generation.
  4. OpenQASM 3.0 circuit export & independent 64x64 unitary matrix verification.
  5. Multi-turn --interactive REPL (/reset, /quit) & --chat --clean output isolation.
  6. ZCC-EF-0 adversarial negative controls (QNC-1..QNC-5).
"""

import math
import os
import re
import struct
import subprocess
import sys
import tempfile
import unittest

import numpy as np

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
    to_rel_win,
)
from tools.build_gpu_win64 import audit_pe32plus_multi_dll
from tests.test_wasm_wasi_selfhost import ZCC_WASM, find_node_bin, run_node_wasi
from tests.test_win64_gpu_llm import extract_prefixed_kv, extract_quoted_field

QLLM_EXE = os.path.join(BUILD_DIR, "zcc_gpu_qllm.exe")
QLLM_HOST_EXE = os.path.join(BUILD_DIR, "zcc_gpu_qllm_host_zcc.exe")
KERNELS_C = os.path.join(REPO_ROOT, "src", "win64_gpu_qllm_kernels.c")
PTX_HOST = os.path.join(BUILD_DIR, "gpu_qllm_kernels_host.ptx")
PTX_WIN64 = os.path.join(BUILD_DIR, "gpu_qllm_kernels.ptx")
V1_ZQNN = os.path.join(BUILD_DIR, "zcc_qtransformer_v1.zqnn")
V1_QASM = os.path.join(BUILD_DIR, "zcc_qtransformer_v1.qasm")

EXPECTED_ENTRIES = (
    "qllm_embed_qrope_fwd",
    "qllm_embed_qrope_bwd",
    "qllm_cunit_norm_fwd",
    "qllm_cunit_norm_bwd",
    "qllm_pqc_fwd",
    "qllm_pqc_bwd",
    "qllm_qrope_heads_fwd",
    "qllm_qrope_heads_bwd",
    "qllm_born_attn_fwd",
    "qllm_born_attn_bwd_q",
    "qllm_born_attn_bwd_kv",
    "qllm_kerr_fwd",
    "qllm_kerr_bwd",
    "qllm_pack_quad",
    "qllm_unpack_quad",
    "qllm_linear_fwd",
    "qllm_linear_bwd_x",
    "qllm_linear_bwd_w",
    "qllm_linear_bwd_b",
    "qllm_silu_fwd",
    "qllm_silu_bwd",
    "qllm_add_f32",
    "qllm_add_inplace_f32",
    "qllm_zero_f32",
    "qllm_reduce_rows_add",
    "qllm_reduce_head_add",
    "qllm_ce_loss_bwd",
    "qllm_adamw_step",
)


def fnv1a_64(data: bytes) -> int:
    h = 14695981039346656037
    for b in data:
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def load_zqnn_weights(path: str) -> tuple[dict, dict]:
    with open(path, "rb") as f:
        raw = f.read()
    if len(raw) < 64:
        raise AssertionError(f"{path}: truncated header ({len(raw)} B)")
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
        _pad0,
        seed,
        fnv64,
    ) = struct.unpack_from("<4s11I2Q", raw, 0)
    if magic0 != b"ZQNN" or magic1 != 1 or version not in (1, 2):
        raise AssertionError(f"Bad ZQNN header: {magic0!r}, magic1={magic1}, version={version}")
    n_merges = _pad0 if version == 2 else 0
    if version == 2 and (_pad0 != vocab_size - 256):
        raise AssertionError(f"Bad v2 n_merges {_pad0} for vocab={vocab_size}")
    merge_bytes = n_merges * 4
    payload = raw[64:]
    if len(payload) != merge_bytes + total_params * 4:
        raise AssertionError(f"Payload length {len(payload)} != {merge_bytes + total_params * 4}")
    if fnv1a_64(payload) != fnv64:
        raise AssertionError(f"Checksum mismatch: got {fnv1a_64(payload):016x}, expected {fnv64:016x}")

    merges = []
    if n_merges > 0:
        merges_arr = np.frombuffer(payload[:merge_bytes], dtype="<u2").reshape(n_merges, 2)
        merges = [(int(l), int(r)) for l, r in merges_arr]
    w = np.frombuffer(payload[merge_bytes:], dtype="<f4").astype(np.float64)
    meta = {
        "version": version,
        "vocab_size": vocab_size,
        "n_merges": n_merges,
        "merges": merges,
        "n_qubits": n_qubits,
        "dim": dim,
        "n_heads": n_heads,
        "head_dim": dim // n_heads,
        "n_layers": n_layers,
        "pqc_depth": pqc_depth,
        "h_ff": h_ff,
        "total_params": total_params,
        "seed": seed,
        "fnv64": fnv64,
    }

    cur = 0

    def take(n: int) -> np.ndarray:
        nonlocal cur
        out = w[cur : cur + n]
        cur += n
        return out

    n_pqc = 3 * pqc_depth * n_qubits
    weights = {
        "E_re": take(vocab_size * dim).reshape(vocab_size, dim),
        "E_im": take(vocab_size * dim).reshape(vocab_size, dim),
        "layers": [],
    }
    for _ in range(n_layers):
        weights["layers"].append(
            {
                "theta_q": take(n_pqc).reshape(pqc_depth, 3, n_qubits),
                "theta_k": take(n_pqc).reshape(pqc_depth, 3, n_qubits),
                "theta_v": take(n_pqc).reshape(pqc_depth, 3, n_qubits),
                "theta_f1": take(n_pqc).reshape(pqc_depth, 3, n_qubits),
                "theta_f2": take(n_pqc).reshape(pqc_depth, 3, n_qubits),
                "beta": take(n_heads),
                "gamma": take(n_heads),
                "kerr_k": take(dim),
                "kerr_b": take(dim),
                "W1": take(h_ff * (2 * dim)).reshape(h_ff, 2 * dim),
                "b1": take(h_ff),
                "W2": take((2 * dim) * h_ff).reshape(2 * dim, h_ff),
                "b2": take(2 * dim),
            }
        )
    weights["H_out"] = take(vocab_size * (2 * dim)).reshape(vocab_size, 2 * dim)
    weights["b_out"] = take(vocab_size)
    if cur != total_params:
        raise AssertionError(f"Weight offset mismatch: {cur} != {total_params}")
    return meta, weights


def np_cunit_norm(psi: np.ndarray) -> np.ndarray:
    n2 = np.sum(psi.real**2 + psi.imag**2, axis=-1, keepdims=True)
    return psi / np.sqrt(n2 + 1e-6)


def np_pqc_apply(psi_in: np.ndarray, theta: np.ndarray, n_qubits: int) -> np.ndarray:
    psi = np.array(psi_in, dtype=np.complex128, copy=True)
    D = 1 << n_qubits
    idx_all = np.arange(D)
    for p in range(theta.shape[0]):
        for q in range(n_qubits):
            ang = theta[p, 0, q]
            c, s = math.cos(0.5 * ang), math.sin(0.5 * ang)
            i0 = idx_all[(idx_all & (1 << q)) == 0]
            i1 = i0 | (1 << q)
            a0 = psi[..., i0].copy()
            a1 = psi[..., i1].copy()
            psi[..., i0] = c * a0 - s * a1
            psi[..., i1] = s * a0 + c * a1
        for q in range(n_qubits):
            ang = theta[p, 1, q]
            c, s = math.cos(0.5 * ang), math.sin(0.5 * ang)
            m0 = (idx_all & (1 << q)) == 0
            psi[..., m0] *= c - 1j * s
            psi[..., ~m0] *= c + 1j * s
        for q in range(n_qubits):
            tgt = (q + 1) % n_qubits
            cm = 1 << q
            tm = 1 << tgt
            i0 = idx_all[((idx_all & cm) != 0) & ((idx_all & tm) == 0)]
            i1 = i0 | tm
            t0 = psi[..., i0].copy()
            psi[..., i0] = psi[..., i1]
            psi[..., i1] = t0
        for q in range(n_qubits):
            tgt = (q + 1) % n_qubits
            alpha = theta[p, 2, q]
            mask = ((idx_all & (1 << q)) != 0) & ((idx_all & (1 << tgt)) != 0)
            psi[..., mask] *= math.cos(alpha) + 1j * math.sin(alpha)
    return psi


def bpe_encode_py(meta: dict, raw_bytes: bytes) -> list[int]:
    if meta.get("version", 1) != 2:
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


def np_forward_token_ids(meta: dict, weights: dict, token_ids: list[int] | np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    nq = meta["n_qubits"]
    D = meta["dim"]
    H = meta["n_heads"]
    Dh = meta["head_dim"]
    toks = np.asarray(token_ids, dtype=np.int64)
    T = len(toks)

    emb = weights["E_re"][toks] + 1j * weights["E_im"][toks]
    pos = np.arange(T, dtype=np.float64)[:, None]
    omega_d = 10000.0 ** (-np.arange(D, dtype=np.float64)[None, :] / float(D))
    emb = emb * np.exp(1j * pos * omega_d)
    x = np_cunit_norm(emb)

    omega_h = 10000.0 ** (-np.arange(Dh, dtype=np.float64)[None, :] / float(Dh))
    rope_h = np.exp(1j * pos * omega_h)
    sqrt_d = math.sqrt(float(D))

    for lw in weights["layers"]:
        Q = np_pqc_apply(x, lw["theta_q"], nq).reshape(T, H, Dh) * rope_h[:, None, :]
        K = np_pqc_apply(x, lw["theta_k"], nq).reshape(T, H, Dh) * rope_h[:, None, :]
        V = np_pqc_apply(x, lw["theta_v"], nq).reshape(T, H, Dh)

        attn_out = np.zeros((T, H, Dh), dtype=np.complex128)
        inv_sqrt_dh = 1.0 / math.sqrt(float(Dh))
        for h in range(H):
            qh = Q[:, h, :]
            kh = K[:, h, :]
            vh = V[:, h, :]
            # <K_s | Q_t> = sum_j conj(K_{s,j}) * Q_{t,j}
            ov = qh @ kh.conj().T
            R = ov.real
            I = ov.imag
            S = (lw["beta"][h] * (R * R + I * I) + lw["gamma"][h] * R) * inv_sqrt_dh
            mask = np.triu(np.ones((T, T), dtype=bool), k=1)
            S = np.where(mask, -1e30, S)
            S = S - np.max(S, axis=1, keepdims=True)
            W = np.exp(S)
            W = np.where(mask, 0.0, W)
            W = W / np.sum(W, axis=1, keepdims=True)
            attn_out[:, h, :] = W @ vh
        attn_out = attn_out.reshape(T, D)

        psi_mid = np_cunit_norm(x + attn_out)
        u = np_pqc_apply(psi_mid, lw["theta_f1"], nq)
        phi_kerr = lw["kerr_k"][None, :] * (float(D) * (u.real**2 + u.imag**2)) + lw["kerr_b"][None, :]
        v_kerr = u * np.exp(1j * phi_kerr)

        quad_in = sqrt_d * np.concatenate([v_kerr.real, v_kerr.imag], axis=-1)
        h1_pre = quad_in @ lw["W1"].T + lw["b1"][None, :]
        h1_act = h1_pre / (1.0 + np.exp(-h1_pre))
        delta_quad = h1_act @ lw["W2"].T + lw["b2"][None, :]
        delta = (delta_quad[:, :D] + 1j * delta_quad[:, D:]) / sqrt_d

        pre_f2 = psi_mid + delta
        post_f2 = np_pqc_apply(pre_f2, lw["theta_f2"], nq)
        x = np_cunit_norm(post_f2)

    last_psi = x[-1]
    last_quad = sqrt_d * np.concatenate([last_psi.real, last_psi.imag])
    logits = weights["H_out"] @ last_quad + weights["b_out"]
    return logits, last_psi


def np_forward_qtransformer(meta: dict, weights: dict, prompt_bytes: bytes) -> tuple[np.ndarray, np.ndarray]:
    return np_forward_token_ids(meta, weights, bpe_encode_py(meta, prompt_bytes))


def verify_qasm_block_unitary(
    qasm_path: str,
    n_qubits: int,
    expected_blocks: int,
    expected_gates: int,
    block_header: str,
    theta_expected: np.ndarray,
) -> tuple[float, float]:
    with open(qasm_path, "r", encoding="utf-8") as f:
        qasm_lines = [ln.strip() for ln in f if ln.strip()]

    if qasm_lines[0] != "OPENQASM 3.0;" or qasm_lines[1] != 'include "stdgates.inc";':
        raise AssertionError(f"Bad OpenQASM header in {qasm_path}")
    if f"qubit[{n_qubits}] q;" not in qasm_lines:
        raise AssertionError(f"Missing qubit[{n_qubits}] q; in {qasm_path}")

    block_headers = [ln for ln in qasm_lines if ln.startswith("// Layer ")]
    if len(block_headers) != expected_blocks:
        raise AssertionError(f"Expected {expected_blocks} blocks, got {len(block_headers)}")

    D = 1 << n_qubits
    gates_in_blocks = {}
    cur_block = None
    total_gates = 0
    for ln in qasm_lines:
        if ln.startswith("// Layer "):
            cur_block = ln
            gates_in_blocks[cur_block] = []
            continue
        if ln.startswith(("ry(", "rz(", "cx ", "cp(")):
            total_gates += 1
            if cur_block is not None:
                gates_in_blocks[cur_block].append(ln)
    if total_gates != expected_gates:
        raise AssertionError(f"Expected {expected_gates} gates, got {total_gates}")

    u0 = np.eye(D, dtype=np.complex128)
    idx_all = np.arange(D)
    for stmt in gates_in_blocks[block_header]:
        m = re.match(r"^(ry|rz)\(([-0-9.eE+]+)\)\s+q\[(\d+)\];$", stmt)
        if m:
            kind, ang_s, q_s = m.groups()
            ang, q = float(ang_s), int(q_s)
            c, s = math.cos(0.5 * ang), math.sin(0.5 * ang)
            if kind == "ry":
                i0 = idx_all[(idx_all & (1 << q)) == 0]
                i1 = i0 | (1 << q)
                r0, r1 = u0[i0, :].copy(), u0[i1, :].copy()
                u0[i0, :] = c * r0 - s * r1
                u0[i1, :] = s * r0 + c * r1
            else:
                m0 = (idx_all & (1 << q)) == 0
                u0[m0, :] *= c - 1j * s
                u0[~m0, :] *= c + 1j * s
            continue
        m = re.match(r"^cx\s+q\[(\d+)\],\s*q\[(\d+)\];$", stmt)
        if m:
            qc, qt = int(m.group(1)), int(m.group(2))
            i0 = idx_all[((idx_all & (1 << qc)) != 0) & ((idx_all & (1 << qt)) == 0)]
            i1 = i0 | (1 << qt)
            t0 = u0[i0, :].copy()
            u0[i0, :] = u0[i1, :]
            u0[i1, :] = t0
            continue
        m = re.match(r"^cp\(([-0-9.eE+]+)\)\s+q\[(\d+)\],\s*q\[(\d+)\];$", stmt)
        if m:
            alpha, qc, qt = float(m.group(1)), int(m.group(2)), int(m.group(3))
            mask = ((idx_all & (1 << qc)) != 0) & ((idx_all & (1 << qt)) != 0)
            u0[mask, :] *= math.cos(alpha) + 1j * math.sin(alpha)
            continue
        raise AssertionError(f"Unrecognized OpenQASM 3.0 statement: {stmt}")

    unitarity_err = float(np.linalg.norm(u0.conj().T @ u0 - np.eye(D, dtype=np.complex128)))
    rng = np.random.default_rng(7)
    psi_probe = rng.normal(size=D) + 1j * rng.normal(size=D)
    psi_probe /= np.linalg.norm(psi_probe)
    psi_qasm = u0 @ psi_probe
    psi_pqc = np_pqc_apply(psi_probe, theta_expected, n_qubits)
    statevec_err = float(np.max(np.abs(psi_qasm - psi_pqc)))
    return unitarity_err, statevec_err


def run_win_stdin(exe_path: str, args: list[str], stdin_text: str, timeout_s: int = 120) -> subprocess.CompletedProcess:
    if sys.platform == "win32":
        cmd = [exe_path] + list(args)
    else:
        cmd = [CMD_EXE, "/c", to_rel_win(exe_path)] + list(args)
    return subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        input=stdin_text,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout_s,
    )


class TestWin64GpuQllm(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (
            ZCC_BIN,
            ZCC_WIN64,
            ZCC_WASM,
            QLLM_EXE,
            QLLM_HOST_EXE,
            PTX_HOST,
            PTX_WIN64,
            V1_ZQNN,
            V1_QASM,
        )
        missing = [p for p in required if not os.path.isfile(p)]
        if missing:
            raise unittest.SkipTest(
                f"Missing required Q-Transformer artifacts ({missing}); build and train v1 first."
            )

    def test_01_sovereign_ptx_and_pe32plus_triple_parity_and_iat(self) -> None:
        sha_host = sha256_file(PTX_HOST)
        sha_win = sha256_file(PTX_WIN64)
        self.assertEqual(sha_host, sha_win, "Host ./zcc PTX != Win64 zcc_sov_stage3.exe PTX")

        fc_ptx = run_fc_b(PTX_HOST, PTX_WIN64)
        self.assertEqual(fc_ptx.returncode, 0, fc_ptx.stdout)
        self.assertIn("no differences encountered", fc_ptx.stdout.lower())

        ptx_wasm = os.path.join(BUILD_DIR, "gpu_qllm_kernels_wasm_test.ptx")
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
                    "build_win64/gpu_qllm_kernels_wasm_test.ptx",
                ],
            )
            self.assertEqual(res.returncode, 0, res.stderr)
            sha_wasm = sha256_file(ptx_wasm)
            self.assertEqual(sha_host, sha_wasm, f"Host PTX ({sha_host}) != WASI zcc.wasm PTX ({sha_wasm})")
        finally:
            if os.path.exists(ptx_wasm):
                os.remove(ptx_wasm)

        with open(PTX_WIN64, "r", encoding="utf-8") as f:
            ptx_text = f.read()
        for marker in (".version 8.7", ".target sm_120", "shfl.sync.down.b32"):
            self.assertIn(marker, ptx_text)
        for entry in EXPECTED_ENTRIES:
            self.assertIn(f".visible .entry {entry}", ptx_text)

        self.assertEqual(sha256_file(QLLM_HOST_EXE), sha256_file(QLLM_EXE))
        fc_exe = run_fc_b(QLLM_HOST_EXE, QLLM_EXE)
        self.assertEqual(fc_exe.returncode, 0, fc_exe.stdout)
        self.assertIn("no differences encountered", fc_exe.stdout.lower())

        iat = audit_pe32plus_multi_dll(QLLM_EXE, required_dlls=("msvcrt.dll", "nvcuda.dll"))
        self.assertEqual(set(iat["dlls"].keys()), {"msvcrt.dll", "nvcuda.dll"})

    def test_02_independent_numpy_complex_quantum_forward_and_param_shift_oracle(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="tmp_qllm_t2_") as tmpdir:
            rel_tmp = os.path.basename(tmpdir)
            ckpt = os.path.join(tmpdir, "oracle_4q.zqnn")
            logits_bin = os.path.join(tmpdir, "logits.bin")
            state_bin = os.path.join(tmpdir, "state.bin")

            r_tr = run_win_cmd(
                QLLM_EXE,
                [
                    "--train",
                    "--qubits",
                    "4",
                    "--heads",
                    "2",
                    "--layers",
                    "2",
                    "--pqc-depth",
                    "2",
                    "--batch",
                    "2",
                    "--seq",
                    "16",
                    "--steps",
                    "1",
                    "--seed",
                    "4242",
                    "--save",
                    rf"build_win64\{rel_tmp}\oracle_4q.zqnn",
                ],
                timeout_s=120,
            )
            self.assertEqual(r_tr.returncode, 0, r_tr.stderr + r_tr.stdout)

            prompt = "Q: ZKAEDI"
            r_inf = run_win_cmd(
                QLLM_EXE,
                [
                    "--load",
                    rf"build_win64\{rel_tmp}\oracle_4q.zqnn",
                    "--prompt",
                    prompt,
                    "--steps",
                    "1",
                    "--dump-logits",
                    rf"build_win64\{rel_tmp}\logits.bin",
                    "--dump-state",
                    rf"build_win64\{rel_tmp}\state.bin",
                ],
                timeout_s=120,
            )
            self.assertEqual(r_inf.returncode, 0, r_inf.stderr + r_inf.stdout)

            meta, weights = load_zqnn_weights(ckpt)
            np_logits, np_psi = np_forward_qtransformer(meta, weights, prompt.encode("ascii"))

            gpu_logits = np.fromfile(logits_bin, dtype="<f4").astype(np.float64)
            gpu_psi_raw = np.fromfile(state_bin, dtype="<f4").astype(np.float64)
            gpu_psi = gpu_psi_raw[: meta["dim"]] + 1j * gpu_psi_raw[meta["dim"] :]

            max_logit_diff = float(np.max(np.abs(gpu_logits - np_logits)))
            max_state_diff = float(np.max(np.abs(gpu_psi - np_psi)))
            norm_err = abs(float(np.linalg.norm(gpu_psi)) - 1.0)

            self.assertLess(max_logit_diff, 1e-4, f"Logit oracle diff too large: {max_logit_diff}")
            self.assertLess(max_state_diff, 1e-4, f"Quantum state oracle diff too large: {max_state_diff}")
            self.assertLess(norm_err, 1e-5, f"Unit-norm invariant violated: {norm_err}")

        r_gc = run_win_cmd(QLLM_EXE, ["--grad-check"], timeout_s=120)
        self.assertEqual(r_gc.returncode, 0, r_gc.stderr + r_gc.stdout)
        kv = extract_prefixed_kv(r_gc.stdout, "QLLM_GRADCHECK_OK")
        self.assertGreaterEqual(int(kv["probes"]), 30)
        self.assertLess(float(kv["max_abs_err"]), 1e-3)
        self.assertLess(float(kv["param_shift_max_err"]), 1e-5)

    def test_03_live_rtx5070_from_scratch_training_convergence_and_entanglement(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="tmp_qllm_t3_") as tmpdir:
            rel_tmp = os.path.basename(tmpdir)
            ckpt = os.path.join(tmpdir, "trained_6q.zqnn")
            psi_bin = os.path.join(tmpdir, "psi.bin")

            r_tr = run_win_cmd(
                QLLM_EXE,
                [
                    "--train",
                    "--qubits",
                    "6",
                    "--heads",
                    "4",
                    "--layers",
                    "2",
                    "--pqc-depth",
                    "2",
                    "--batch",
                    "8",
                    "--seq",
                    "64",
                    "--steps",
                    "1200",
                    "--log-every",
                    "300",
                    "--lr",
                    "0.02",
                    "--wd",
                    "0.001",
                    "--seed",
                    "1337",
                    "--save",
                    rf"build_win64\{rel_tmp}\trained_6q.zqnn",
                ],
                timeout_s=120,
            )
            self.assertEqual(r_tr.returncode, 0, r_tr.stderr + r_tr.stdout)
            tr_kv = extract_prefixed_kv(r_tr.stdout, "QLLM_TRAIN_OK")
            self.assertGreater(float(tr_kv["init_loss"]), 4.5)
            self.assertLess(float(tr_kv["final_loss"]), 0.15)
            self.assertLess(float(tr_kv["norm_err"]), 1e-5)
            self.assertGreater(float(tr_kv["renyi2_bits"]), 0.25)

            load_zqnn_weights(ckpt)
            runs = []
            for idx in range(2):
                args = [
                    "--load",
                    rf"build_win64\{rel_tmp}\trained_6q.zqnn",
                    "--prompt",
                    "Q: What is ZKAEDI Q-Transformer?",
                    "--steps",
                    "96",
                ]
                if idx == 0:
                    args += ["--dump-state", rf"build_win64\{rel_tmp}\psi.bin"]
                r_gen = run_win_cmd(QLLM_EXE, args, timeout_s=120)
                self.assertEqual(r_gen.returncode, 0, r_gen.stderr + r_gen.stdout)
                ok_kv = extract_prefixed_kv(r_gen.stdout, "QLLM_OK")
                gen_txt = extract_quoted_field(r_gen.stdout, "QLLM_TEXT", "generated")
                runs.append((ok_kv["logits_fnv"], gen_txt))

            self.assertEqual(runs[0], runs[1], "Reloaded generation not deterministic")
            self.assertIn("ZKAEDI Q-Transformer is a sovereign", runs[0][1])

            raw_psi = np.fromfile(psi_bin, dtype="<f4").astype(np.float64)
            psi = raw_psi[:64] + 1j * raw_psi[64:]
            self.assertLess(abs(float(np.linalg.norm(psi)) - 1.0), 1e-5)
            M = psi.reshape(8, 8)
            rho_a = M @ M.conj().T
            purity = float(np.trace(rho_a @ rho_a).real)
            renyi2 = -math.log2(max(purity, 1e-12))
            self.assertGreater(renyi2, 0.25, f"Insufficient bipartite entanglement: {renyi2}")

    def test_04_openqasm3_export_and_unitary_circuit_verification(self) -> None:
        meta, weights = load_zqnn_weights(V1_ZQNN)
        self.assertEqual(meta["n_qubits"], 6)
        unitarity_err, statevec_err = verify_qasm_block_unitary(
            V1_QASM,
            n_qubits=6,
            expected_blocks=10,
            expected_gates=480,
            block_header="// Layer 0 Block U_Q",
            theta_expected=weights["layers"][0]["theta_q"],
        )
        self.assertLess(unitarity_err, 1e-10)
        self.assertLess(statevec_err, 1e-5)

    def test_05_interactive_repl_and_clean_mode(self) -> None:
        r_raw = run_win_stdin(
            QLLM_EXE,
            ["--load", r"build_win64\zcc_qtransformer_v1.zqnn", "--interactive", "--steps", "32"],
            "Q: What is ZCC?\n/reset\nQ: What is Blackwell PTX?\n/quit\n",
        )
        self.assertEqual(r_raw.returncode, 0, r_raw.stderr + r_raw.stdout)
        self.assertIn("QLLM_INTERACTIVE_READY", r_raw.stdout)
        self.assertIn("QLLM_TURN", r_raw.stdout)
        self.assertIn("QLLM_REPL_RESET", r_raw.stdout)
        self.assertIn("QLLM_INTERACTIVE_OK turns=1", r_raw.stdout)

        r_clean = run_win_stdin(
            QLLM_EXE,
            ["--load", r"build_win64\zcc_qtransformer_v1.zqnn", "--interactive", "--chat", "--clean", "--steps", "48"],
            "What is ZCC?\n/quit\n",
        )
        self.assertEqual(r_clean.returncode, 0, r_clean.stderr + r_clean.stdout)
        self.assertIn("Assistant>", r_clean.stdout)
        self.assertNotIn("QLLM_DEVICE", r_clean.stdout)
        self.assertIn("[Quantum interactive ready:", r_clean.stderr)

    def test_06_ef0_negative_controls_qnc1_to_qnc5(self) -> None:
        # QNC-1: Qubit range out of bounds
        r1 = run_win_cmd(QLLM_EXE, ["--qubits", "3"], timeout_s=60)
        self.assertNotEqual(r1.returncode, 0)
        self.assertIn("INVALID_ARG:", r1.stderr + r1.stdout)

        # QNC-2: Heads not dividing 2^qubits
        r2 = run_win_cmd(QLLM_EXE, ["--qubits", "6", "--heads", "3"], timeout_s=60)
        self.assertNotEqual(r2.returncode, 0)
        self.assertIn("INVALID_ARG:", r2.stderr + r2.stdout)

        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="tmp_qllm_t6_") as tmpdir:
            rel_tmp = os.path.basename(tmpdir)
            with open(V1_ZQNN, "rb") as fin:
                orig = fin.read()

            # QNC-3: Bad magic in ZQNN checkpoint
            bad_magic = bytearray(orig)
            bad_magic[0:4] = b"FAKE"
            p_magic = os.path.join(tmpdir, "bad_magic.zqnn")
            with open(p_magic, "wb") as fout:
                fout.write(bad_magic)
            r3 = run_win_cmd(QLLM_EXE, ["--load", rf"build_win64\{rel_tmp}\bad_magic.zqnn", "--steps", "1"], timeout_s=60)
            self.assertNotEqual(r3.returncode, 0)
            self.assertIn("ZQNN_INVALID:", r3.stderr + r3.stdout)

            # QNC-4: Single-byte payload corruption breaks FNV-1a checksum
            bad_ck = bytearray(orig)
            bad_ck[128] ^= 0x01
            p_ck = os.path.join(tmpdir, "bad_cksum.zqnn")
            with open(p_ck, "wb") as fout:
                fout.write(bad_ck)
            r4 = run_win_cmd(QLLM_EXE, ["--load", rf"build_win64\{rel_tmp}\bad_cksum.zqnn", "--steps", "1"], timeout_s=60)
            self.assertNotEqual(r4.returncode, 0)
            self.assertIn("ZQNN_CHECKSUM_MISMATCH:", r4.stderr + r4.stdout)

            # QNC-5: Missing and undersized --data files fail closed
            r5a = run_win_cmd(
                QLLM_EXE,
                ["--train", "--data", rf"build_win64\{rel_tmp}\does_not_exist.txt", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r5a.returncode, 0)
            self.assertIn("IO_ERROR:", r5a.stderr + r5a.stdout)

            p_tiny = os.path.join(tmpdir, "tiny.txt")
            with open(p_tiny, "wb") as fout:
                fout.write(b"short")
            r5b = run_win_cmd(
                QLLM_EXE,
                ["--train", "--data", rf"build_win64\{rel_tmp}\tiny.txt", "--steps", "1"],
                timeout_s=60,
            )
            self.assertNotEqual(r5b.returncode, 0)
            self.assertIn("INVALID_DATA:", r5b.stderr + r5b.stdout)


if __name__ == "__main__":
    unittest.main()
