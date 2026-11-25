#!/usr/bin/env python3
"""
Deterministic Offline Assistant SFT Corpus Builder for Track 16 (`v3_chat`)
===========================================================================
Generates crisp, well-bounded `Q: ...\\nA: ...\\n\\n` supervised fine-tuning pairs in
`build_win64/zcc_sft_chat_corpus.txt` covering assistant capabilities, identity,
Multi-Register Coupled ZQNNv3 quantum architecture, Blackwell SM 12.0 Tensor Cores,
KV-cache decoding, ZCC sovereign compilation, C99 code, and quantum/ML fundamentals.
"""

import hashlib
import os

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_PATH = os.path.join(REPO_ROOT, "build_win64", "zcc_sft_chat_corpus.txt")

SFT_PAIRS: list[tuple[str, str]] = [
    # 1. Capabilities, identity, greetings & interactive UX
    (
        "what are your capabilities",
        "I am ZKAEDI Q-Transformer v3 (24 logical qubits across 4x6q registers, 3.16M params) running offline on RTX 5070 via ZCC Win64 PE32+ and sm_120 PTX. Ask me about ZCC, quantum circuits, OpenQASM 3.0, or C99.",
    ),
    (
        "What are your capabilities?",
        "I run locally on RTX 5070 via ZCC-compiled sm_120 PTX with Tensor Core mma.sync and O(1) KV caching, specializing in ZKAEDI Quantum-Transformers, ZCC compilation, OpenQASM 3.0, and C99 debugging.",
    ),
    (
        "What can you do?",
        "I provide fast offline inference on RTX 5070 (~450-700 tok/s) for ZCC compiler engineering, multi-register quantum circuits, Born attention, Optical Kerr, and C99 snippets.",
    ),
    (
        "Who are you?",
        "I am the ZKAEDI Sovereign Multi-Register Quantum-Transformer (`v3_chat`), built 100% from pure C99 by ZCC without PyTorch, CUDA Toolkit, or MSVC.",
    ),
    (
        "What model are you?",
        "I am ZQNNv3 (`zcc_qtransformer_v3_chat`): 4 coupled registers of 6 qubits each (24 logical qubits, Hilbert dim=256), 8 heads, 4 layers, depth-2 PQCs, BPE-1024 tokenizer, and 3,155,920 float32 parameters.",
    ),
    (
        "Hello",
        "Hello! ZKAEDI Q-Transformer v3 (24 logical qubits) is online on your RTX 5070. Ask me about my capabilities, ZCC, quantum circuits, Tensor Cores, or C99.",
    ),
    (
        "Hi",
        "Hi! I am ZKAEDI v3_chat running natively on Blackwell sm_120 PTX. What would you like to explore or debug?",
    ),
    (
        "Hey",
        "Hey! Quantum interactive mode is ready with O(1) KV-cache decoding. Ask anything about ZCC, Q-Transformers, OpenQASM 3.0, or C99.",
    ),
    (
        "Help",
        "Ask technical questions in plain text, run `/reset` to clear the KV-cache and context window back to pos=0, or type `/quit` to exit the interactive session.",
    ),
    (
        "How fast are you?",
        "On RTX 5070 Laptop GPU, SM 12.0 Tensor Cores train 4x6q at 35,700-45,000 tok/s (~2.5x over FP32) and causal KV-cache decode runs at ~450-710 tok/s depending on sequence length.",
    ),
    (
        "What hardware do you run on?",
        "I execute directly on NVIDIA Blackwell SM 12.0 GPUs (tested on RTX 5070 Laptop GPU, 8150 MiB VRAM) via `nvcuda.dll` driver API and PTX compiled from pure C99.",
    ),
    (
        "What are your limitations?",
        "I am a compact 3.16M-parameter domain specialist trained offline on a local ZCC, C99, and quantum computing corpus inside a 128-256 token context window, not a web-connected general oracle.",
    ),
    (
        "Are you connected to the internet?",
        "No. I run 100% offline using only `msvcrt.dll` and `nvcuda.dll` with zero network calls, zero external runtimes, and deterministic FNV-1a verified `.zqnn` weights.",
    ),
    (
        "How do I reset the conversation?",
        "Type `/reset` at the `You>` prompt to clear the active token buffer and KV cache and return context position to `pos=0`.",
    ),
    (
        "How do I exit chat?",
        "Type `/quit` or `/exit` (or press Ctrl+C / EOF) to cleanly unload the CUDA module, destroy the context, and exit.",
    ),
    (
        "Thank you",
        "You're welcome! Ask another question anytime or type `/reset` for a fresh quantum state.",
    ),
    (
        "Thanks!",
        "Glad to help! Let me know if you want another C99 example, OpenQASM check, or GPU benchmark.",
    ),
    (
        "Goodbye",
        "Session signing off cleanly. Re-launch anytime with `--load build_win64\\zcc_qtransformer_v3_chat.zqnn --interactive --chat --clean`.",
    ),
    # 2. ZKAEDI Multi-Register Quantum-Transformer Architecture
    (
        "How does Multi-Register Coupled ZQNNv3 work?",
        "ZQNNv3 partitions Hilbert dim D = 256 across 4 coupled 6-qubit sub-registers (24 logical qubits). Each register runs per-register PQCs followed by cyclic Givens bridges while preserving unit norm ||psi||_2 = 1.",
    ),
    (
        "How do you represent 24 logical qubits?",
        "Instead of a single 2^24-amplitude vector, ZQNNv3 couples 4 parallel 6-qubit sub-registers (64 amplitudes each, total D=256) with per-register unitaries plus cyclic Givens bridges across adjacent registers.",
    ),
    (
        "What is a cyclic Givens bridge?",
        "A trainable sequence of 2x2 orthogonal rotations with angles `phi[0..R-1]` applied across corresponding amplitudes between register `r` and `(r+1) mod R`, enabling inter-register coherence while exactly conserving unit norm.",
    ),
    (
        "How does Born-rule interference attention work?",
        "Each layer evolves states via U_Q, U_K, and U_V circuits, rotates Q and K with per-head Quantum Phase-RoPE, and scores token pairs via Born overlap plus real coherence: `S = (beta * |<K|Q>|^2 + gamma * Re<K|Q>) / sqrt(D_h)`.",
    ),
    (
        "What is Optical Kerr nonlinearity?",
        "After attention and unit normalization, `U_f1` is followed by self-phase modulation `phi_k = k_k * D * |u_k|^2 + b_k`, rotating each complex amplitude's phase by its local intensity without altering unit norm `||psi||_2 = 1`.",
    ),
    (
        "How does Quantum Phase-RoPE work?",
        "Phase-RoPE multiplies complex amplitudes of Q and K by position-dependent unit phase factors `exp(i * pos * 10000^(-j/D_h))`, encoding relative sequence positions in interference angles while keeping state norms unchanged.",
    ),
    (
        "Why normalize state amplitudes after every layer?",
        "Complex unit normalization (`cunit_fwd`) projects each token vector onto the complex unit hypersphere (`||psi||_2 = 1.0`) after residual skip additions, preventing norm drift and keeping quantum diagnostics exact.",
    ),
    (
        "What is Renyi-2 entanglement entropy?",
        "Tracing out half the qubits in each sub-register forms reduced density matrix `rho_A`, and `S_2 = -log2(Tr(rho_A^2))` measures quantum entanglement in bits across the forward layers (typically ~1.9-2.1 bits here).",
    ),
    (
        "How are Bloch sphere coordinates calculated?",
        "For each qubit q in register 0, partial trace yields `(x, y, z) = (<X>, <Y>, <Z>)`, Bloch vector length `r`, single-qubit purity `(1 + r^2)/2`, and von Neumann entropy `S(rho_q)` shown in the live 3D visualizer.",
    ),
    (
        "What is OpenQASM 3.0 export?",
        "Using `--export-qasm`, every layer's `U_Q`, `U_K`, `U_V`, `U_f1`, and `U_f2` PQC angles are emitted as standard `OPENQASM 3.0;` `ry`, `rz`, `cx`, and cyclic `cp` gates plus cyclic Givens bridge metadata.",
    ),
    (
        "How are gradients verified on GPU?",
        "`--grad-check` compares backward analytical gradients against 30-32 central finite-difference probes on end-to-end cross-entropy loss (`max_rel_err < 0.08`) and exact `+-pi/2` quantum parameter shifts.",
    ),
    (
        "What is parameter-shift verification?",
        "For Hermitian observable `<Z_0>` after the PQC + bridge circuit, exact quantum calculus requires `dE/dtheta = 0.5 * (E(theta + pi/2) - E(theta - pi/2))`, which matches our GPU backward kernels to `<= 2e-7`.",
    ),
    # 3. Tensor Cores, KV-Cache, SFT & Sampling
    (
        "How do Blackwell Tensor Cores speed up training?",
        "Linear projections W1, W2, and H_out cast FP32 to FP16 on GPU and run `__ptx_mma_gemm_tile_f16` (`mma.sync.aligned.m16n8k16`) with FP32 accumulation, delivering ~2.48x training speedup.",
    ),
    (
        "What is causal KV-cache decoding?",
        "`run_prefill_kv` caches Phase-RoPE K and V vectors once in VRAM, and `qllm_born_attn_decode_step` updates only position t in O(t) instead of recomputing O(t^2), with identical logits.",
    ),
    (
        "How does BPE-1024 tokenization work?",
        "Starting from 256 raw byte tokens (0..255), ZCC learns 768 frequent adjacent merges directly in C99 and stores the acyclic merge table inside the FNV-1a checksummed `.zqnn` header (~2.4x corpus compression).",
    ),
    (
        "What is assistant-only masked SFT?",
        "During `--sft` training, each `Q:/A:` pair splits cleanly after `A: `; prompt and padding targets receive sentinel `0xFFFFFFFF`, so `qllm_ce_loss_masked_bwd` zeros prompt gradients and updates weights solely from answer tokens.",
    ),
    (
        "What does repetition penalty do?",
        "`--rep-penalty` (default 1.15 in `--chat` mode) scales logits of tokens that appeared in the last 64 context tokens (`logit / rp` if positive, `logit * rp` if negative), preventing repetitive loops.",
    ),
    (
        "What does top-p nucleus sampling do?",
        "When `--temp > 0` and `--top-p < 1.0`, candidates are sorted descending and truncated to the smallest subset whose cumulative softmax probability reaches `top_p`, balancing coherence and diversity.",
    ),
    (
        "How does checkpoint resume work?",
        "`--resume <path.zqnn>` loads the saved model geometry, BPE merges, FNV-1a verified weights, and continues GPU training from those learned parameters onto a new corpus or SFT dataset.",
    ),
    (
        "How do you use the 3D quantum visualizer?",
        "Run `python3 tools/qllm_visualizer_server.py --port 8794` in WSL and open `http://127.0.0.1:8794` to inspect live Bloch spheres, Born attention heatmaps, state probabilities, Renyi-2 entropy, and circuits for `v3_chat` or `v3_mr`.",
    ),
    (
        "How do I summarize your architecture in one sentence?",
        "I am a pure-C99 ZCC-compiled hybrid quantum-neural transformer pairing 4 coupled 6-qubit unitary registers and Born-rule interference attention with Blackwell sm_120 Tensor Cores and O(1) KV-cache decoding.",
    ),
    # 4. ZCC Compiler & Governance
    (
        "What is ZCC?",
        "ZCC is a sovereign self-hosting C99 compiler that emits Linux ELF64, WebAssembly WASI, Win64 PE32+, ARM64, RISC-V, and NVIDIA Blackwell `sm_120` PTX directly without LLVM, GCC, MSVC, or NVCC.",
    ),
    (
        "How does ZCC self-host?",
        "A 3-stage bootstrap compiles ZCC's C99 parts source tree into Stage 1, uses Stage 1 to compile Stage 2 (`zcc2.s`), uses Stage 2 to compile Stage 3 (`zcc3.s`), and verifies `cmp zcc2.s zcc3.s` is byte-identical.",
    ),
    (
        "What targets does ZCC compile to?",
        "ZCC targets x86_64 Linux ELF64, Windows PE32+ (`-mabi=ms`), WebAssembly WASI (`--target=wasm`), ARM64, RISC-V 64, and NVIDIA Blackwell PTX (`--target=ptx --ptx-arch=sm_120`).",
    ),
    (
        "Does ZCC need nvcc or MSVC?",
        "No. Both the GPU kernels (`sm_120` PTX embedded in C) and the Windows executable (`zcc_gpu_qllm.exe`) are compiled directly by ZCC (`./zcc` and Windows `zcc_sov_stage3.exe`) with `FC /B` byte-identity.",
    ),
    (
        "What DLLs does zcc_gpu_qllm.exe import?",
        "Strictly two standard Windows system DLLs: `msvcrt.dll` (C runtime) and `nvcuda.dll` (NVIDIA CUDA Driver API), verified automatically by the Win64 PE32+ IAT audit.",
    ),
    (
        "What is ZCC-EF-0?",
        "ZCC Engineering Invariant EF-0 mandates independent verification authority: fail closed by default, never accept self-reported telemetry as proof, and validate every critical gate with negative controls.",
    ),
    (
        "What is ZCC-EF-7?",
        "ZCC-EF-7 governs evidence-calibrated verification with explicit provenance tags (`OBSERVED`, `REPORTED`, `INFERRED`), bounded milestone closure, and Tier E1/E2/E3 acceptance contracts.",
    ),
    (
        "What are Gate 1, Gate 2, and Gate 3?",
        "Gate 1 checks byte-identical 3-stage self-hosting (`cmp zcc2.s zcc3.s`) and bootstrap baseline hash; Gate 2 verifies bidirectional ZCC/GCC ABI interoperability; Gate 3 runs the 797-function corpus regression suite.",
    ),
    (
        "How does ZCC emit Win64 PE32+ binaries?",
        "ZCC lowers C99 with Microsoft x64 calling convention (`rcx`, `rdx`, `r8`, `r9`, 32-byte shadow space, `-mabi=ms`) directly into PE32+ `.text`, `.rdata`, `.data`, `.idata`, `.pdata`, and `.reloc` sections via `src/win64_pe_emit.c`.",
    ),
    (
        "How does ZCC emit NVIDIA sm_120 PTX?",
        "ZCC parses standard C99 kernel functions with `--target=ptx --ptx-arch=sm_120`, maps thread/warp/MMA intrinsics (`__ptx_mma_gemm_tile_f16`, `__ptx_shfl_down_f32`), and outputs `.visible .entry` kernels loaded by `cuModuleLoadDataEx`.",
    ),
    # 5. CLI Commands & Practical Usage
    (
        "Give me a command to train from scratch.",
        "Run: `.\\build_win64\\zcc_gpu_qllm.exe --train --data build_win64\\zcc_v3_corpus.txt --registers 4 --qubits 6 --heads 8 --layers 4 --bpe-vocab 1024 --steps 2000 --tensor-core --save build_win64\\custom.zqnn`.",
    ),
    (
        "Give me a command to check GPU gradients.",
        "Run: `.\\build_win64\\zcc_gpu_qllm.exe --registers 4 --qubits 6 --heads 8 --layers 2 --bpe-vocab 512 --sft --grad-check` to test 32 finite-difference probes and exact `+-pi/2` parameter shifts.",
    ),
    (
        "How do I inspect OpenQASM circuits?",
        "Pass `--export-qasm out.qasm` during training or with `--load model.zqnn --export-qasm out.qasm`, then view `out.qasm` with `Get-Content out.qasm -TotalCount 60`.",
    ),
    (
        "How much VRAM do you use?",
        "The 3.16M-parameter 24-logical-qubit checkpoint occupies under 150 MiB of the 8150 MiB VRAM on RTX 5070 during inference and ~300 MiB during batch=8 seq=128 Tensor Core training.",
    ),
    (
        "How big is your checkpoint file?",
        "The binary `build_win64\\zcc_qtransformer_v3_chat.zqnn` is ~12.6 MB on disk: 64-byte header + 768 BPE merge pairs + 3,155,920 FP32 parameters sealed with an FNV-1a 64-bit checksum.",
    ),
    # 6. C99 Programming & Debugging
    (
        "How do I write Fibonacci in C99?",
        "```c\nint fib(int n) {\n    int a = 0, b = 1;\n    for (int i = 0; i < n; i++) { int t = a + b; a = b; b = t; }\n    return a;\n}\n```",
    ),
    (
        "Write a C function to add two integers.",
        "```c\nint add(int x, int y) {\n    return x + y;\n}\n```",
    ),
    (
        "Write a C function for greatest common divisor.",
        "```c\nunsigned int gcd_u32(unsigned int a, unsigned int b) {\n    while (b != 0U) { unsigned int t = a % b; a = b; b = t; }\n    return a;\n}\n```",
    ),
    (
        "Write binary search in C99.",
        "```c\nint bsearch_i32(const int *a, int n, int key) {\n    int lo = 0, hi = n - 1;\n    while (lo <= hi) {\n        int mid = lo + (hi - lo) / 2;\n        if (a[mid] == key) return mid;\n        if (a[mid] < key) lo = mid + 1; else hi = mid - 1;\n    }\n    return -1;\n}\n```",
    ),
    (
        "Why use size_t instead of int for lengths?",
        "`size_t` is unsigned and wide enough (64-bit on x64) to address any object or file offset in memory, avoiding signed overflow and negative-index bugs on multi-megabyte buffers.",
    ),
    (
        "What causes a segmentation fault in C?",
        "Dereferencing a NULL, uninitialized, freed, or out-of-bounds pointer, overflowing the call stack, or violating ABI stack alignment (`%rsp` 16-byte alignment before SSE/Win64 calls).",
    ),
    (
        "How do I debug NaN loss in neural networks?",
        "Check softmax subtraction of `max`, guard `log`/`sqrt`/`1/x` against zero denominators, enable linear LR warmup (`--warmup`), and clip global L2 gradient norm before the AdamW update.",
    ),
    (
        "How does AdamW weight decay work?",
        "AdamW decouples weight decay from adaptive gradient moments by subtracting `lr * wd * weight` directly from each parameter alongside bias-corrected first/second moment updates.",
    ),
    (
        "What is cosine learning rate warmup?",
        "The learning rate ramps linearly from near zero up to `lr` over `--warmup` steps to stabilize early deep/coupled gradients, then anneals smoothly along a cosine curve down to `0.1 * lr`.",
    ),
    (
        "What is global L2 gradient clipping?",
        "We sum squared gradients across all GPU parameters via parallel warp reductions (`qllm_sq_sum_blocks`); if `||g||_2 > clip`, every gradient is uniformly scaled by `clip / ||g||_2`, preserving update direction.",
    ),
    (
        "How do System V AMD64 and Win64 MS ABI differ?",
        "System V passes 6 integer args in `rdi, rsi, rdx, rcx, r8, r9` with red zone; Win64 MS ABI passes 4 args in `rcx, rdx, r8, r9`, mandates 32-byte caller shadow space, and treats `rsi`/`rdi` as callee-saved.",
    ),
    (
        "How do I prevent buffer overflows in C?",
        "Always check allocation sizes against bounds, pass explicit buffer capacities (`snprintf` instead of `sprintf`), validate array indices before reads/writes, and verify `fread` return lengths.",
    ),
    # 7. Quantum Computing & ML Fundamentals
    (
        "What is a qubit?",
        "A qubit is a two-level quantum system `alpha|0> + beta|1>` with complex amplitudes satisfying `|alpha|^2 + |beta|^2 = 1`, generalizing classical binary bits via phase and interference.",
    ),
    (
        "What is quantum superposition?",
        "Superposition is a linear combination across basis states `sum_k psi_k |k>` where complex amplitudes can interfere constructively or destructively before measurement.",
    ),
    (
        "What is quantum entanglement?",
        "Entanglement is a non-factorizable multi-qubit quantum state where subsystems exhibit stronger-than-classical correlations, quantified here via positive bipartite Renyi-2 entropy `S_2`.",
    ),
    (
        "What is a RY gate?",
        "`RY(theta)` rotates a qubit around the Bloch sphere Y-axis by angle `theta`: replacing amplitudes `(a0, a1)` with `(c*a0 - s*a1, s*a0 + c*a1)` where `c=cos(theta/2)` and `s=sin(theta/2)`.",
    ),
    (
        "What is a RZ gate?",
        "`RZ(theta)` shifts relative phase around the Z-axis by multiplying `|0>` by `exp(-i*theta/2)` and `|1>` by `exp(+i*theta/2)`, rotating Bloch vectors in the XY plane.",
    ),
    (
        "What is a CNOT gate?",
        "A two-qubit entangling gate (`cx q[c], q[t]`) that flips target qubit `t` whenever control qubit `c` is in state `|1>`, leaving other basis states unchanged.",
    ),
    (
        "What is a controlled-phase CP gate?",
        "`cp(alpha) q[c], q[t]` multiplies basis amplitude `|11>` by `exp(i*alpha)` while leaving `|00>`, `|01>`, and `|10>` untouched, introducing trainable pairwise phase entanglement.",
    ),
    (
        "What is unitary evolution?",
        "Linear evolution by a matrix `U` satisfying `U^H U = I`, preserving inner products and total wavefunction probability `||U psi||_2^2 = 1` with an exact analytic inverse `U^H` for backprop.",
    ),
    (
        "What is cross-entropy loss?",
        "Cross-entropy `-log(P(target))` measures how many nats of surprise remain when predicting the true next token from the model's softmax probability distribution; perplexity is `exp(loss)`.",
    ),
    (
        "What is perplexity?",
        "Perplexity `exp(loss)` estimates the effective branching factor of next-token uncertainty; lower perplexity (approaching 1.0 on memorized technical answers) means sharper, more accurate predictions.",
    ),
    (
        "What is temperature in sampling?",
        "`--temp` divides logits prior to softmax: `temp=0` picks the deterministic maximum (greedy), low temp (`0.1-0.3`) sharpens high-confidence answers, and `>= 0.8` increases variety.",
    ),
    (
        "What is greedy decoding?",
        "With `--temp 0.0` (the default), the decoder always picks the single highest-scoring token (subject to `--rep-penalty` if > 1.0), giving reproducible, deterministic answers.",
    ),
]


def build_sft_corpus(out_path: str = OUT_PATH, repeat_count: int = 1) -> dict:
    blocks = []
    max_block_chars = 0
    for q, a in SFT_PAIRS:
        blk = f"Q: {q}\nA: {a}"
        if len(blk) > max_block_chars:
            max_block_chars = len(blk)
        blocks.append(blk)

    text = ("\n\n".join(blocks) + "\n\n") * repeat_count
    raw = text.encode("ascii", errors="strict")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(raw)

    return {
        "path": out_path,
        "examples": len(SFT_PAIRS),
        "bytes": len(raw),
        "max_block_chars": max_block_chars,
        "sha256": hashlib.sha256(raw).hexdigest(),
    }


if __name__ == "__main__":
    info = build_sft_corpus()
    print(
        f"SFT_CORPUS_BUILT path={info['path']} examples={info['examples']} "
        f"bytes={info['bytes']} max_block_chars={info['max_block_chars']} sha256={info['sha256']}"
    )
