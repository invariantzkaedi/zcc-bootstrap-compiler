#!/usr/bin/env python3
"""
Deterministic Offline Workspace Corpus Builder for ZKAEDI Quantum-Transformer (Track 14)
======================================================================================
Collects curated technical Q&A, ZCC compiler documentation, forensic tickets, and C99/PTX
kernel source excerpts from the local repository into `build_win64/zcc_workspace_corpus.txt`.
100% offline, zero external downloads, deterministic byte output.
"""

import argparse
import hashlib
import os
import re

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_PATH = os.path.join(REPO_ROOT, "build_win64", "zcc_workspace_corpus.txt")
OUT_PATH_V3 = os.path.join(REPO_ROOT, "build_win64", "zcc_v3_corpus.txt")

STRUCTURED_QA = """Q: What is ZKAEDI Q-Transformer?
A: ZKAEDI Q-Transformer is a sovereign complex-wavefunction neural language model compiled directly by ZCC into NVIDIA Blackwell sm_120 PTX and Win64 PE32+ without PyTorch, CUDA Toolkit, or MSVC.

Q: What is ZCC?
A: ZCC is a self-hosting C99 compiler generating Linux ELF64, WebAssembly WASI, Win64 PE32+, ARM64, RISC-V, and NVIDIA sm_120 PTX directly from pure C99.

Q: How does Byte-Level BPE work in ZQNNv2?
A: ZQNNv2 embeds learned left-to-right byte-pair merge tables inside the FNV-1a verified checkpoint payload alongside float32 quantum and neural weights, compressing ASCII prose and C99 code cleanly.

Q: How does Born-rule interference attention work?
A: Each layer evolves normalized complex states through U_Q, U_K, and U_V unitary parameterized quantum circuits, applies per-head Quantum Phase-RoPE, and scores attention via S = (beta * |<K|Q>|^2 + gamma * Re<K|Q>) / sqrt(D_h).

Q: How does Optical Kerr nonlinearity work?
A: Optical Kerr self-phase modulation rotates every complex state amplitude by a norm-preserving phase phi_k = k_k * D * |u_k|^2 + b_k, creating nonlinear interference while preserving ||psi||_2 = 1.

Q: How are Bloch spheres and entanglement computed?
A: Tracing out complement qubits yields each qubit's reduced density matrix rho_q with Bloch vector (x, y, z) = (Tr(rho_q X), Tr(rho_q Y), Tr(rho_q Z)) and bipartite Renyi-2 entanglement entropy S_2 = -log2(Tr(rho_A^2)).

Q: How is OpenQASM 3.0 export verified?
A: Each trained layer's U_Q, U_K, U_V, U_f1, and U_f2 PQC blocks export to standard OPENQASM 3.0 ry, rz, cx, and cp gates whose 2^N x 2^N unitary matrix satisfies ||U^H U - I||_F < 1e-10.

Q: How is backpropagation verified on Blackwell GPU?
A: Exact adjoint circuit uncomputation and hybrid quadrature chain rules pass 30-probe central finite differences and exact Z_0 +/-pi/2 quantum parameter-shift verification directly on RTX 5070 hardware.

"""

V3_QA = """Q: How does Multi-Register Coupled ZQNNv3 scale beyond single-register qubits?
A: ZQNNv3 couples R independent n-qubit quantum sub-registers (such as 4x6q = 24 logical qubits, D = 256) with per-register parameterized quantum circuits followed by trainable cyclic Givens rotation bridges phi across adjacent registers, preserving exact unit norm ||psi||_2 = 1 without exponential 2^24 memory blowup.

Q: How does Blackwell SM 12.0 Tensor Core acceleration work in ZQNNv3?
A: Linear quadrature projections W1, W2, and H_out cast inputs and weights to IEEE fp16 on-device and execute mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 tensor core matrix multiplications with FP32 accumulation straight from ZCC-emitted PTX.

Q: How does Causal KV-Cached O(1) decoding work?
A: Autoregressive sampling runs prefill_kv once over the initial context, caches per-layer Phase-RoPE complex keys K and values V in VRAM slices, and decodes each new token in O(t * D) via qllm_born_attn_decode_step instead of O(t^2 * D) full recomputation.

Q: What training stability upgrades are in ZQNNv3?
A: ZQNNv3 adds linear learning-rate warmup before cosine decay, global L2 gradient-norm clipping via parallel block reduction and scaling, BPE-1024 byte-level tokenization, and lossless --resume checkpoint continuation.

"""

DOC_FILES = (
    "README.md",
    "FORENSIC_023A_PARSER001.md",
    "FORENSIC_024_REGALLOC_DETERMINISM.md",
    "FORENSIC_025_PACKED_ALIGNMENT.md",
    "FORENSIC_CORRECTION_2026-04-19.md",
    os.path.join("docs", "ZCC_STATUS.md"),
    os.path.join("docs", "ZCC_ERROR_ARCHITECTURE.md"),
    os.path.join("docs", "SPEC_C_SUBSET_v1.md"),
    os.path.join("docs", "D19_ARCHITECTURE.md"),
    os.path.join("docs", "ZCC_ROADMAP_v2.0.md"),
)

V3_EXTRA_DOCS = (
    os.path.join("tickets", "WIN64-SOVEREIGN-GPU-TRACK13-QUANTUM-TRANSFORMER-GATE-EVIDENCE.md"),
    os.path.join("tickets", "WIN64-SOVEREIGN-GPU-TRACK14-BPE-7Q-VISUALIZER-GATE-EVIDENCE.md"),
    "CHANGELOG.md",
)

UNICODE_MAP = str.maketrans(
    {
        "\u2014": "--",
        "\u2013": "-",
        "\u2192": "->",
        "\u2190": "<-",
        "\u2194": "<->",
        "\u2018": "'",
        "\u2019": "'",
        "\u201c": '"',
        "\u201d": '"',
        "\u2026": "...",
        "\u00a0": " ",
        "\u2264": "<=",
        "\u2265": ">=",
        "\u2260": "!=",
    }
)


def clean_text(raw: str, max_chars: int | None = None, skip_tables_and_hashes: bool = True) -> str:
    txt = raw.replace("\r\n", "\n").replace("\r", "\n").translate(UNICODE_MAP)
    lines = []
    blank_run = 0
    for ln in txt.splitlines():
        s = ln.strip()
        if re.match(r"^[-=*|:\s]{4,}$", s):
            continue
        if skip_tables_and_hashes:
            if s.startswith("|") or s.startswith("```"):
                continue
            if re.search(r"\b[0-9a-f]{32,}\b", s):
                continue
        cleaned = "".join(ch if (32 <= ord(ch) < 127 or ch == "\t") else " " for ch in ln).rstrip()
        if not cleaned:
            blank_run += 1
            if blank_run > 1:
                continue
        else:
            blank_run = 0
        lines.append(cleaned)
    out = "\n".join(lines).strip() + "\n"
    if max_chars is not None and len(out) > max_chars:
        cut = out.rfind("\n", 0, max_chars)
        if cut <= 0:
            cut = max_chars
        out = out[:cut].rstrip() + "\n"
    return out


def build_corpus(out_path: str = OUT_PATH, target_max_bytes: int = 36864, v3_mode: bool = False) -> dict:
    parts = [(STRUCTURED_QA + V3_QA) * 2 if v3_mode else STRUCTURED_QA * 2]
    used_files = 0
    total_len = len(parts[0])
    src_reserve = 12500 if v3_mode else 5800
    doc_cap = 3600 if v3_mode else 2400
    docs = DOC_FILES + (V3_EXTRA_DOCS if v3_mode else ())

    for rel in docs:
        full = os.path.join(REPO_ROOT, rel)
        if not os.path.isfile(full):
            continue
        with open(full, "r", encoding="utf-8", errors="replace") as f:
            doc_txt = clean_text(f.read(), max_chars=doc_cap, skip_tables_and_hashes=True)
        chunk = f"\n# FILE: {rel.replace(os.sep, '/')}\n{doc_txt}"
        if total_len + len(chunk) > target_max_bytes - src_reserve:
            rem = max(0, target_max_bytes - src_reserve - total_len)
            if rem > 400:
                parts.append(chunk[:rem].rsplit("\n", 1)[0] + "\n")
                total_len += len(parts[-1])
                used_files += 1
            break
        parts.append(chunk)
        total_len += len(chunk)
        used_files += 1

    src_list = [os.path.join("src", "win64_gpu_qllm_kernels.c")]
    if v3_mode:
        src_list.append(os.path.join("src", "win64_gpu_qllm.c"))
    for src_rel in src_list:
        src_path = os.path.join(REPO_ROOT, src_rel)
        if not os.path.isfile(src_path):
            continue
        per_cap = 6500 if v3_mode else 5500
        with open(src_path, "r", encoding="utf-8", errors="replace") as f:
            k_txt = clean_text(f.read(), max_chars=per_cap, skip_tables_and_hashes=False)
        rem = max(0, target_max_bytes - total_len)
        if rem < 256:
            break
        k_chunk = f"\n/* SRC: {src_rel.replace(os.sep, '/')} */\n{k_txt}"
        if len(k_chunk) > rem:
            k_chunk = k_chunk[:rem].rsplit("\n", 1)[0] + "\n"
        parts.append(k_chunk)
        total_len += len(k_chunk)
        used_files += 1

    corpus_bytes = "".join(parts).encode("ascii", errors="replace")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as fout:
        fout.write(corpus_bytes)

    digest = hashlib.sha256(corpus_bytes).hexdigest()
    return {
        "path": out_path,
        "files": used_files,
        "bytes": len(corpus_bytes),
        "sha256": digest,
    }


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Build deterministic offline Q-Transformer training corpus")
    ap.add_argument("--v3", action="store_true", help="Build extended ~64KB Track 15 v3 corpus")
    ap.add_argument("--out", default=None, help="Optional custom output path")
    ap.add_argument("--max-bytes", type=int, default=None, help="Optional max byte budget")
    args = ap.parse_args()
    out_p = args.out if args.out else (OUT_PATH_V3 if args.v3 else OUT_PATH)
    max_b = args.max_bytes if args.max_bytes else (65536 if args.v3 else 36864)
    info = build_corpus(out_path=out_p, target_max_bytes=max_b, v3_mode=args.v3)
    print(
        f"CORPUS_BUILT path={info['path']} files={info['files']} bytes={info['bytes']} sha256={info['sha256']}"
    )
