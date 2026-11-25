# WIN64 Sovereign GPU Track 14: Byte-Level BPE, 7-Qubit Deep Q-Transformer & Live Quantum Visualizer Gate Evidence

**Status:** PASS (Evidence Tier `E2`)  
**Governing Rule Version (`governing_rule_version`):** `1.0.0` (`ZCC-EF-0`, `ZCC-EF-7`)  
**Evidence Tier (`evidence_tier`):** `E2` (Engineering Milestone Closure)  
**Evidence Tier Assigned At (`evidence_tier_assigned_at`):** `2026-11-22T08:30:00Z`  
**Target Hardware:** `NVIDIA GeForce RTX 5070 Laptop GPU` (`cc=12.0`, `vram_mib=8150`, WDDM `driver=12090`)  
**Authoritative Remote:** `https://github.com/invariantzkaedi/zcc-bootstrap-compiler`  

---

## 1. Track 14 Deliverables & Architecture

- **Zero edits** to `part0_pp.c`–`part6_ptx.c`, `src/win64_pe_emit.c`, `BOOTSTRAP_BASELINES.tsv`, or `src/win64_gpu_qllm_kernels.c` (`gpu_qllm_kernels.ptx` remained byte-identical at `136586 B`).
- `src/win64_gpu_qllm.c` — Upgraded host runtime supporting:
  - **`ZQNNv2` Byte-Level BPE Tokenizer (`--bpe-vocab 256..2048`):** Deterministic frequency-ordered pair merge trainer (`bpe_train_merges`), left-to-right greedy merge encoder (`bpe_encode`), flat O(1) decode string pool, binary serialization of merge table `(uint16_t left, uint16_t right)` + weights under full-payload 64-bit FNV-1a integrity verification, and fail-closed acyclic merge validation (`left < 256 + i && right < 256 + i`). Full backwards compatibility with Track 13 `ZQNNv1`.
  - **7-Qubit (`dim=128`), 4-Layer Deep Q-Transformer (`790,888` parameters):** Validated via both 30-probe central finite-difference and exact $\langle Z_0 \rangle$ $\pm\pi/2$ quantum parameter-shift gradient checks directly on RTX 5070.
  - **Real GPU Quantum Telemetry Exporter (`--dump-telemetry <path.json>`):** Exports exact GPU per-layer reduced single-qubit Bloch vectors $(x,y,z,r,\text{purity}, S_{\text{vN}})$, bipartite Rényi-2 & linear entropies, causal Born-rule interference attention matrices `attn_heads` & `attn_mean`, learned $(\beta, \gamma)$ & PQC angles, $2^N=128$ basis amplitudes/phases, and Top-16 BPE next-token probabilities.
- `tools/build_qllm_corpus.py` — 100% offline workspace corpus builder extracting technical Q&A, ZCC markdown docs/forensics, and C99/PTX kernel source into `build_win64/zcc_workspace_corpus.txt` (`31,153 B`, `11` workspace files).
- `docs/qllm_quantum_visualizer.html` + `tools/qllm_visualizer_server.py` — Interactive Quantum State & 3D Multi-Qubit Bloch-Sphere Visualizer (mouse-drag 3D orbit, Hilbert basis spectrum colored by phase, Born-rule attention heatmap, layer entanglement trajectory, OpenQASM PQC circuit ribbon, POVM next-token probabilities) with 3 embedded live RTX 5070 telemetry presets and zero-dependency stdlib HTTP bridge (`GET /`, `GET /api/models`, `POST /api/infer`) conforming to Rule EF-7 UI invariants.
- `tests/test_win64_gpu_qllm_v2.py` — 6 automated tests covering triple-compiler PTX/PE32+/IAT, Python vs C BPE + NumPy `complex128` 7q 4L oracle + `--grad-check`, independent Pauli/eigenvalue verification of telemetry JSON, 7q 4L OpenQASM 3.0 $128\times 128$ unitary proof, EF-7 headless Node UI simulation + live HTTP bridge, and `V2NC-1`..`V2NC-5` negative controls + `ZQNNv1` backwards compatibility.
- `Makefile` — Added `test-win64-gpu-qllm-v2`.

---

## 2. Verbatim Gate & Execution Evidence

### Gate A — Dual-Compiler PTX & Win64 PE32+ Parity + Strict IAT (`python3 tools/build_gpu_qllm_win64.py`)

```text
[PTX-HOST:QLLM] ./zcc --target=ptx --ptx-arch=sm_120 -> build_win64/gpu_qllm_kernels_host.ptx
[PTX-HOST:QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/gpu_qllm_kernels_host.ptx: 136586 bytes, SHA256=209b96eb0f9374a8be5dd2073863dc1f340b9e0097beacb7e1d0c5c47b2ff405
[PTX-WIN64:QLLM] zcc_sov_stage3.exe --target=ptx --ptx-arch=sm_120 -> build_win64/gpu_qllm_kernels.ptx
[PTX-WIN64:QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/gpu_qllm_kernels.ptx: 136586 bytes, SHA256=209b96eb0f9374a8be5dd2073863dc1f340b9e0097beacb7e1d0c5c47b2ff405
[FC /B:PTX:QLLM] Comparing files BUILD_WIN64\gpu_qllm_kernels_host.ptx and BUILD_WIN64\GPU_QLLM_KERNELS.PTX
FC: no differences encountered
[PTX-HEADER:QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/gpu_qllm_kernels_ptx.h: embedded 136586 bytes -> g_gpu_qllm_kernels_ptx
[BUILD-HOST:GPU_QLLM] ./zcc -mabi=ms -> build_win64/zcc_gpu_qllm_host_zcc.exe
[HOST-PE:GPU_QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/zcc_gpu_qllm_host_zcc.exe: 351744 bytes, SHA256=e11a28d08fd97df89ec5fb8d3e1442aef5b825b3af28ec5d5be0321a7fd53978
[BUILD-WIN64:GPU_QLLM] zcc_sov_stage3.exe -mabi=ms -> build_win64/zcc_gpu_qllm.exe
[WIN64-PE:GPU_QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/zcc_gpu_qllm.exe: 351744 bytes, SHA256=e11a28d08fd97df89ec5fb8d3e1442aef5b825b3af28ec5d5be0321a7fd53978
[FC /B:GPU_QLLM] Comparing files BUILD_WIN64\zcc_gpu_qllm_host_zcc.exe and BUILD_WIN64\ZCC_GPU_QLLM.EXE
FC: no differences encountered
[PE-AUDIT:GPU_QLLM] imports={'bytes': 351744, 'pdata': 2268, 'reloc': 32, 'stack_reserve': 16777216, 'dlls': {'msvcrt.dll': ['__iob_func', '__getmainargs', 'exit', '_strtoi64', '_strtoui64', '_atoi64', '_getpid', '_getcwd', '_errno', 'memcpy', 'floor', 'ceil', 'fmod', 'log', 'pow', 'frexp', 'ldexp', 'exp', 'strtod', 'memset', '_ftime64', '_stricmp', '_strnicmp', '_open', '_close', '_read', '_write', '_lseek', '_mkdir', '_access', 'fprintf', 'cos', 'calloc', 'free', 'malloc', 'fopen', 'fwrite', 'fclose', 'fread', 'strlen', 'fseek', 'ftell', 'sin', 'printf', 'fflush', 'putchar', 'memmove', 'fputc', 'atan2', 'strcmp', 'atoi', 'fgets', 'strncmp', '_snprintf'], 'nvcuda.dll': ['cuGetErrorName', 'cuGetErrorString', 'cuModuleGetFunction', 'cuMemAlloc_v2', 'cuMemcpyHtoD_v2', 'cuMemsetD8_v2', 'cuMemFree_v2', 'cuLaunchKernel', 'cuMemcpyDtoH_v2', 'cuCtxSynchronize', 'cuEventCreate', 'cuEventRecord', 'cuEventSynchronize', 'cuEventElapsedTime', 'cuEventDestroy_v2', 'cuInit', 'cuDriverGetVersion', 'cuDeviceGetCount', 'cuDeviceGet', 'cuDeviceGetName', 'cuDeviceTotalMem_v2', 'cuCtxCreate_v2', 'cuModuleLoadDataEx', 'cuModuleUnload', 'cuCtxDestroy_v2']}}
```

### Gate B — Live RTX 5070 `--grad-check` (6-Qubit v1 & 7-Qubit 4-Layer BPE v2)

```text
QLLM_DEVICE name="NVIDIA GeForce RTX 5070 Laptop GPU" vram_mib=8150 driver=12090 ptx_bytes=136586
QLLM_MODEL arch=zkaedi_hybrid_qtransformer qubits=6 hilbert_dim=64 heads=4 layers=2 pqc_depth=2 h_ff=128 params=99576 vocab=128 version=1
QLLM_GRADCHECK_OK probes=30 max_rel_err=0.040434 max_abs_err=0.000046 param_shift_max_err=0.0000002

QLLM_DEVICE name="NVIDIA GeForce RTX 5070 Laptop GPU" vram_mib=8150 driver=12090 ptx_bytes=136586
QLLM_MODEL arch=zkaedi_hybrid_qtransformer qubits=7 hilbert_dim=128 heads=4 layers=4 pqc_depth=2 h_ff=256 params=790888 vocab=512 version=2
QLLM_GRADCHECK_OK probes=30 max_rel_err=0.050651 max_abs_err=0.000076 param_shift_max_err=0.0000002
```

### Gate C — Curated Offline Corpus & Live RTX 5070 7-Qubit 4-Layer BPE-512 Training

```text
CORPUS_BUILT path=/mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/zcc_workspace_corpus.txt files=11 bytes=31153 sha256=d1118a2fc8ba69a08b14f5f102542c83c34b902944bde127d3038ed8b93e141e

QLLM_DEVICE name="NVIDIA GeForce RTX 5070 Laptop GPU" vram_mib=8150 driver=12090 ptx_bytes=136586
QLLM_MODEL arch=zkaedi_hybrid_qtransformer qubits=7 hilbert_dim=128 heads=4 layers=4 pqc_depth=2 h_ff=256 params=790888 vocab=512 version=2
QLLM_BPE vocab=512 merges=256 raw_bytes=31153 tokens=16915 compression=1.842x
QLLM_TRAIN_STEP step=1/2400 loss=6.513544 ppl=674.21 norm_err=0.0000011 renyi2_bits=2.4508 linear_S=0.8168
QLLM_TRAIN_STEP step=400/2400 loss=2.559437 ppl=12.93 norm_err=0.0000012 renyi2_bits=2.4427 linear_S=0.8158
QLLM_TRAIN_STEP step=800/2400 loss=1.619444 ppl=5.05 norm_err=0.0000012 renyi2_bits=2.4292 linear_S=0.8141
QLLM_TRAIN_STEP step=1200/2400 loss=1.153510 ppl=3.17 norm_err=0.0000009 renyi2_bits=2.4179 linear_S=0.8127
QLLM_TRAIN_STEP step=1600/2400 loss=0.749707 ppl=2.12 norm_err=0.0000009 renyi2_bits=2.4262 linear_S=0.8137
QLLM_TRAIN_STEP step=2000/2400 loss=0.440273 ppl=1.55 norm_err=0.0000011 renyi2_bits=2.4180 linear_S=0.8127
QLLM_TRAIN_STEP step=2400/2400 loss=0.315864 ppl=1.37 norm_err=0.0000011 renyi2_bits=2.4230 linear_S=0.8132
QLLM_TRAIN_OK steps=2400 corpus_bytes=31153 init_loss=6.513544 final_loss=0.315864 train_ms=28089.29 train_tok_per_sec=43746.21 norm_err=0.0000011 renyi2_bits=2.4230
QLLM_SAVED path="build_win64\zcc_qtransformer_v2_7q.zqnn"
QLLM_QASM_EXPORTED path="build_win64\zcc_qtransformer_v2_7q.qasm"
```

Live inference & telemetry captures (`exit code 0`):

```text
QLLM_TEXT prompt="Q: What is ZKAEDI Q-Transformer?" generated=" complex-wavefunction neural language model compiled directly by ZCC into NVI"
QLLM_OK steps=40 tok_per_sec=728.87 top1_id=32 top1_logit=16.582531 logits_fnv=15aa4181 norm_err=0.0000008
QLLM_TELEMETRY_SAVED path="build_win64\qllm_telemetry_v2_q1.json"

QLLM_TEXT prompt="Q: How does Born attention work?" generated=" does Optical Kerr self-r n-r nels U normalized complex state amplies pres"
QLLM_OK steps=40 tok_per_sec=711.51 top1_id=32 top1_logit=15.749708 logits_fnv=57dbb309 norm_err=0.0000008
QLLM_TELEMETRY_SAVED path="build_win64\qllm_telemetry_v2_q2.json"

QLLM_TEXT prompt="void qllm_cunit_norm_fwd(" generated="\n    const float *x_re,\n    const float *x_im,\n    float *x_im,\n    float *y_im,\n    float *y_im,\n    float *y_im,\n    unsigned int BT,\n    unsigned int D\n"
QLLM_OK steps=40 tok_per_sec=791.27 top1_id=273 top1_logit=13.716146 logits_fnv=7c0cc090 norm_err=0.0000006
QLLM_TELEMETRY_SAVED path="build_win64\qllm_telemetry_v2_code.json"
```

### Gate D — Rule EF-7 Headless Node UI Verification (`python3 tools/qllm_visualizer_server.py --verify-ui`)

```text
EMBEDDED_PRESETS count=3 html=/mnt/h/__DOWNLOADS/zcc_github_upload/docs/qllm_quantum_visualizer.html
NODE_EF7_UI_OK {"errors": 0, "collapsed_drawOps": 0, "active_drawOps": 34851, "kpiModel": "7q D=128 | V=512 (v2)", "kpiRenyi": "2.287 bits", "tokens": 59, "preds": 12}
```

### Gate E — Track 14 Automated Verification Suite (`tests/test_win64_gpu_qllm_v2.py -v`)

```text
test_01_sovereign_ptx_pe32plus_triple_parity_and_strict_iat (__main__.TestWin64GpuQllmV2.test_01_sovereign_ptx_pe32plus_triple_parity_and_strict_iat) ... ok
test_02_bpe_roundtrip_and_numpy_complex128_v2_7q_oracle (__main__.TestWin64GpuQllmV2.test_02_bpe_roundtrip_and_numpy_complex128_v2_7q_oracle) ... ok
test_03_live_rtx5070_7q_bpe_workspace_model_and_telemetry_math (__main__.TestWin64GpuQllmV2.test_03_live_rtx5070_7q_bpe_workspace_model_and_telemetry_math) ... ok
test_04_openqasm3_7q_4layer_unitary_verification (__main__.TestWin64GpuQllmV2.test_04_openqasm3_7q_4layer_unitary_verification) ... ok
test_05_visualizer_ef7_headless_node_and_live_bridge_api (__main__.TestWin64GpuQllmV2.test_05_visualizer_ef7_headless_node_and_live_bridge_api) ... ok
test_06_v2_ef0_negative_controls_and_v1_compat (__main__.TestWin64GpuQllmV2.test_06_v2_ef0_negative_controls_and_v1_compat) ... ok

----------------------------------------------------------------------
Ran 6 tests in 4.873s

OK
```

### Gate F — Full Regression Sweep (Track 13, Gate 1/2, Track 7) & SHA-256 Checksums

```text
=== TRACK13 ===
test_01_sovereign_ptx_and_pe32plus_triple_parity_and_iat (__main__.TestWin64GpuQllm.test_01_sovereign_ptx_and_pe32plus_triple_parity_and_iat) ... ok
test_02_independent_numpy_complex_quantum_forward_and_param_shift_oracle (__main__.TestWin64GpuQllm.test_02_independent_numpy_complex_quantum_forward_and_param_shift_oracle) ... ok
test_03_live_rtx5070_from_scratch_training_convergence_and_entanglement (__main__.TestWin64GpuQllm.test_03_live_rtx5070_from_scratch_training_convergence_and_entanglement) ... ok
test_04_openqasm3_export_and_unitary_circuit_verification (__main__.TestWin64GpuQllm.test_04_openqasm3_export_and_unitary_circuit_verification) ... ok
test_05_interactive_repl_and_clean_mode (__main__.TestWin64GpuQllm.test_05_interactive_repl_and_clean_mode) ... ok
test_06_ef0_negative_controls_qnc1_to_qnc5 (__main__.TestWin64GpuQllm.test_06_ef0_negative_controls_qnc1_to_qnc5) ... ok

----------------------------------------------------------------------
Ran 6 tests in 11.250s

OK
=== GATE1 ===
8b28903bb66619f2c38397ae2a0ddfed  zcc2.s
8b28903bb66619f2c38397ae2a0ddfed  zcc3.s
=== BOOTSTRAP_HASH ===
BOOTSTRAP HASH: 8b28903bb66619f2c38397ae2a0ddfed  commit=28f81f65  elisions=2213
TOOLCHAIN: gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0 | Linux 6.18.58.1-microsoft-standard-WSL2 x86_64
BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)
=== GATE2 ===
=== 1. ZCC LIB + GCC MAIN ===
...
INTEROP_SUCCESS: x=10 y=17 z=300
=== 2. GCC LIB + ZCC MAIN ===
...
INTEROP_SUCCESS: x=10 y=17 z=300
=== TRACK7 ===
test_01_ptx_triple_compiler_byte_identity (__main__.TestWin64GpuLlm.test_01_ptx_triple_compiler_byte_identity) ... ok
test_02_pe32plus_nvcuda_audit_and_sovereign_parity (__main__.TestWin64GpuLlm.test_02_pe32plus_nvcuda_audit_and_sovereign_parity) ... ok
test_03_live_rtx5070_f32_differential_vs_independent_python_oracle (__main__.TestWin64GpuLlm.test_03_live_rtx5070_f32_differential_vs_independent_python_oracle) ... ok
test_04_live_rtx5070_f16_tensor_core_differential_and_gemm_bench (__main__.TestWin64GpuLlm.test_04_live_rtx5070_f16_tensor_core_differential_and_gemm_bench) ... ok
test_05_live_rtx5070_q8_0_gpu_dequant_differential (__main__.TestWin64GpuLlm.test_05_live_rtx5070_q8_0_gpu_dequant_differential) ... ok
test_06_multi_prompt_coherence_and_kv_cache_causality (__main__.TestWin64GpuLlm.test_06_multi_prompt_coherence_and_kv_cache_causality) ... ok
test_07_ef0_negative_controls (__main__.TestWin64GpuLlm.test_07_ef0_negative_controls) ... ok
test_08_real_temp_sampling_distribution (__main__.TestWin64GpuLlm.test_08_real_temp_sampling_distribution) ... ok

----------------------------------------------------------------------
Ran 8 tests in 7.694s

OK
=== TRACK14_SHA256 ===
209b96eb0f9374a8be5dd2073863dc1f340b9e0097beacb7e1d0c5c47b2ff405  build_win64/gpu_qllm_kernels.ptx
e11a28d08fd97df89ec5fb8d3e1442aef5b825b3af28ec5d5be0321a7fd53978  build_win64/zcc_gpu_qllm.exe
d1118a2fc8ba69a08b14f5f102542c83c34b902944bde127d3038ed8b93e141e  build_win64/zcc_workspace_corpus.txt
789f42c45c441b31265e3706f44d89d79c527a46f6b37a42043cee42fbba2897  build_win64/zcc_qtransformer_v2_7q.zqnn
c71d3e3d2d0f2b69b2be1e3c283949ff82f2a437c012cf1b3fa2d31fec32f791  build_win64/zcc_qtransformer_v2_7q.qasm
f00e841c9ab26e47d9d00c553569ab0d079fc0cec6beac13bd9f1a133a759a7d  docs/qllm_quantum_visualizer.html
```

---

## 3. ZCC-EF-7 §9 Adjudication Table (`E2`)

| Claim | Evidence Class | Evidence Tier | Evidence | Scope | Limitations | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| Sovereign PTX triple parity, PE32+ dual parity & strict `{msvcrt.dll, nvcuda.dll}` IAT | OBSERVED | E2 | Gate A (`136586` B PTX, `351744` B `.exe`, `FC /B` clean), `test_01` | `./zcc`, `zcc_sov_stage3.exe`, `build_wasm/zcc.wasm`; Win64 runtime | Requires NVIDIA SM 12.0 Windows driver `nvcuda.dll` | **PASS** |
| `ZQNNv2` Byte-Level BPE roundtrip + pure-NumPy `complex128` 7q 4L oracle + `--grad-check` | OBSERVED | E2 | `test_02` (C vs Python token parity, `< 1e-4` logits/state), `--grad-check` 7q 4L (`max_abs_err=0.000076`, `param_shift_max_err=0.0000002`) | `bpe_vocab` `256..2048`, 7 qubits `D=128`, 4 layers | Sequence context bounded `[4..128]` | **PASS** |
| Live RTX 5070 7-qubit BPE-512 workspace training, Bloch tomography & Rényi-2 parity | OBSERVED | E2 | Gate C (`6.513544 -> 0.315864`, `1.842x` BPE, `43.7k tok/s`, `renyi2_bits=2.4230`) & `test_03` (`< 1e-4` independent Pauli/eigenvalue verification across all 7 qubits) | `build_win64/zcc_qtransformer_v2_7q.zqnn` (`790,888` params) on `31,153` B workspace corpus | Capacity scaled for fast single-GPU experiments | **PASS** |
| 7-qubit 4-layer OpenQASM 3.0 unitary export verification | OBSERVED | E2 | `test_04` (`20` blocks, `1,120` gates, $128\times 128$ unitary $\|U^\dagger U - I\|_F < 10^{-10}$, statevector diff `< 1e-5`) | All PQC unitary blocks (`U_Q, U_K, U_V, U_f1, U_f2`) | Classical/GPU hybrid layers (Born attn / Kerr / FFN) remain outside unitary QASM export | **PASS** |
| Interactive Quantum State & Bloch-Sphere Visualizer (`Rule EF-7` UI + Live HTTP Bridge) | OBSERVED | E2 | Gate D (`NODE_EF7_UI_OK errors=0 active_drawOps=34851`), `test_05` (`GET /`, `GET /api/models`, `POST /api/infer` `200 OK`) | `docs/qllm_quantum_visualizer.html` + `tools/qllm_visualizer_server.py` | Localhost bridge (`127.0.0.1`); standalone HTML displays 3 embedded RTX 5070 captures when opened without server | **PASS** |
| Fail-closed `V2NC-1..5` negative controls & zero Track 13 / Track 7 / Gate 1/2 regression | OBSERVED | E2 | `test_06` (`ZQNN_CHECKSUM_MISMATCH`, forged merge `ZQNN_INVALID`, `v1` ok), `test_win64_gpu_qllm.py` (`6/6`), `test_win64_gpu_llm.py` (`8/8`), `cmp zcc2.s zcc3.s` | Full GPU LLM / QLLM / Linux ELF64 compiler baseline | Covers Tracks 7, 13, 14 & core compiler self-host | **PASS** |
