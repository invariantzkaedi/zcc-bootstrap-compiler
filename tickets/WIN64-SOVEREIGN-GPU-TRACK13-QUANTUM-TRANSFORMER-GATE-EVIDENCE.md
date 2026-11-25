# WIN64 Sovereign GPU Track 13: Hybrid Quantum-Classical Transformer Gate Evidence

**Status:** PASS (Evidence Tier `E2`)  
**Governing Rule Version (`governing_rule_version`):** `1.0.0` (`ZCC-EF-0`, `ZCC-EF-7`)  
**Evidence Tier (`evidence_tier`):** `E2` (Engineering Milestone Closure)  
**Evidence Tier Assigned At (`evidence_tier_assigned_at`):** `2026-11-22T06:50:00Z`  
**Target Hardware:** `NVIDIA GeForce RTX 5070 Laptop GPU` (`cc=12.0`, `vram_mib=8150`, WDDM `driver=12090`)  
**Authoritative Remote:** `https://github.com/invariantzkaedi/zcc-bootstrap-compiler`  

---

## 1. Phase 0 Forensic Chain Verdict

- **BASELINE:** `GREEN` (`make selfhost` verified prior to Track 13 edits: `stage2.s == stage3.s`, `SHA256=987ef5ed22c1fd705fc9784256a0b894c58115a506ddf3a216798f0ce3481000`)
- **SYMPTOM-IN-HISTORY:** `NO`
- **FORENSIC-LATEST-SHA:** `049edf005` (`FORENSIC_025_PACKED_ALIGNMENT.md`)
- **PROCEED:** `YES`

> Note on workspace state: `git diff --stat` on `BOOTSTRAP_BASELINES.tsv`, `part2.c`, `part4.c`, `part5.c`, and `src/win64_pe_emit.c` reflects pre-existing uncommitted Track 11/12 changes already present at Track 13 Phase 0 baseline. Track 13 made zero edits to any compiler core (`part0_pp.c`–`part6_ptx.c`), PE emitter (`src/win64_pe_emit.c`), or bootstrap baseline ledger (`BOOTSTRAP_BASELINES.tsv`).

---

## 2. Track 13 Deliverables

- `src/win64_gpu_qllm_kernels.c` — 28 `.visible .entry` SM 12.0 Blackwell PTX kernels (`embed_qrope`, `cunit_norm`, N-qubit variational `pqc` `RY`/`RZ`/`CNOT`/`CP` + exact adjoint backward, per-head Phase-RoPE, causal Born-rule interference attention $S = (\beta|\langle K|Q\rangle|^2 + \gamma\text{Re}\langle K|Q\rangle)/\sqrt{D_h}$, Optical Kerr self-phase modulation, scaled quadrature pack/unpack, linear + SiLU FFN, POVM readout, mean cross-entropy + dLogits, atomic-free deterministic reductions, AdamW step).
- `src/win64_gpu_qllm.c` — Sovereign Win64 PE32+ host executable using `nvcuda.dll` + `msvcrt.dll` only: CUDA Driver API launcher, `ZQNNv1` binary serializer with 64-bit FNV-1a integrity verification, OpenQASM 3.0 circuit exporter, GPU `--grad-check` (30-probe central finite-difference + exact `±π/2` quantum parameter-shift on observable $\langle\psi|Z_0|\psi\rangle$), live from-scratch `--train`, `--dump-logits`, `--dump-state`, and `--interactive --chat --clean` REPL.
- `tools/build_gpu_qllm_win64.py` — Dual-compiler PTX & Win64 PE32+ parity builder + strict IAT verifier.
- `tests/test_win64_gpu_qllm.py` — 6 independent verification tests (`test_01`–`test_06`) including pure-NumPy `complex128` forward oracle, OpenQASM 3.0 $64\times 64$ unitary matrix proof, bipartite Rényi-2 entanglement cross-check, REPL tests, and `QNC-1`..`QNC-5` adversarial negative controls.
- `Makefile` — Added `build-gpu-qllm-win64` and `test-win64-gpu-qllm` targets.
- Pre-trained Sovereign Checkpoint & Circuit: `build_win64/zcc_qtransformer_v1.zqnn` (`6` qubits, `D=64`, `4` heads, `2` layers, `pqc_depth=2`, `h_ff=128`, `99,576` parameters) and `build_win64/zcc_qtransformer_v1.qasm`.

---

## 3. Verbatim Gate & Execution Evidence

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
[HOST-PE:GPU_QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/zcc_gpu_qllm_host_zcc.exe: 312320 bytes, SHA256=66034404f534f6fb5884ee9516e994681daae46f4c5e3296d05ea2f15fb7837b
[BUILD-WIN64:GPU_QLLM] zcc_sov_stage3.exe -mabi=ms -> build_win64/zcc_gpu_qllm.exe
[WIN64-PE:GPU_QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/zcc_gpu_qllm.exe: 312320 bytes, SHA256=66034404f534f6fb5884ee9516e994681daae46f4c5e3296d05ea2f15fb7837b
[FC /B:GPU_QLLM] Comparing files BUILD_WIN64\zcc_gpu_qllm_host_zcc.exe and BUILD_WIN64\ZCC_GPU_QLLM.EXE
FC: no differences encountered
[PE-AUDIT:GPU_QLLM] imports={'bytes': 312320, 'pdata': 2136, 'reloc': 32, 'stack_reserve': 16777216, 'dlls': {'msvcrt.dll': ['__iob_func', '__getmainargs', 'exit', '_strtoi64', '_strtoui64', '_atoi64', '_getpid', '_getcwd', '_errno', 'memcpy', 'floor', 'ceil', 'fmod', 'log', 'pow', 'frexp', 'ldexp', 'exp', 'strtod', 'memset', '_ftime64', '_stricmp', '_strnicmp', '_open', '_close', '_read', '_write', '_lseek', '_mkdir', '_access', 'fprintf', 'cos', 'calloc', 'free', 'malloc', 'fopen', 'fwrite', 'fclose', 'fread', 'strlen', 'fseek', 'ftell', 'sin', 'printf', 'fflush', 'putchar', 'memmove', 'strcmp', 'atoi', 'fgets', 'strncmp', '_snprintf'], 'nvcuda.dll': ['cuGetErrorName', 'cuGetErrorString', 'cuModuleGetFunction', 'cuMemAlloc_v2', 'cuMemcpyHtoD_v2', 'cuMemsetD8_v2', 'cuMemFree_v2', 'cuLaunchKernel', 'cuMemcpyDtoH_v2', 'cuCtxSynchronize', 'cuEventCreate', 'cuEventRecord', 'cuEventSynchronize', 'cuEventElapsedTime', 'cuEventDestroy_v2', 'cuInit', 'cuDriverGetVersion', 'cuDeviceGetCount', 'cuDeviceGet', 'cuDeviceGetName', 'cuDeviceTotalMem_v2', 'cuCtxCreate_v2', 'cuModuleLoadDataEx', 'cuModuleUnload', 'cuCtxDestroy_v2']}}
```

### Gate B — Live RTX 5070 Finite-Diff + Quantum Parameter-Shift Gradient Check

Command: `.\build_win64\zcc_gpu_qllm.exe --grad-check` (exit code `0`)

```text
QLLM_DEVICE name="NVIDIA GeForce RTX 5070 Laptop GPU" vram_mib=8150 driver=12090 ptx_bytes=136586
QLLM_MODEL arch=zkaedi_hybrid_qtransformer qubits=6 hilbert_dim=64 heads=4 layers=2 pqc_depth=2 h_ff=128 params=99576
QLLM_GRADCHECK_OK probes=30 max_rel_err=0.040434 max_abs_err=0.000046 param_shift_max_err=0.0000002
```

### Gate C — Live RTX 5070 From-Scratch Training & Inference (`v1`)

Command: `.\build_win64\zcc_gpu_qllm.exe --train --qubits 6 --heads 4 --layers 2 --pqc-depth 2 --batch 8 --seq 64 --steps 1200 --log-every 200 --lr 0.02 --wd 0.001 --save build_win64\zcc_qtransformer_v1.zqnn --export-qasm build_win64\zcc_qtransformer_v1.qasm` (exit code `0`)

```text
QLLM_DEVICE name="NVIDIA GeForce RTX 5070 Laptop GPU" vram_mib=8150 driver=12090 ptx_bytes=136586
QLLM_MODEL arch=zkaedi_hybrid_qtransformer qubits=6 hilbert_dim=64 heads=4 layers=2 pqc_depth=2 h_ff=128 params=99576
QLLM_TRAIN_STEP step=1/1200 loss=5.073511 ppl=159.73 norm_err=0.0000010 renyi2_bits=2.0483 linear_S=0.7573
QLLM_TRAIN_STEP step=200/1200 loss=1.345411 ppl=3.84 norm_err=0.0000002 renyi2_bits=2.0276 linear_S=0.7537
QLLM_TRAIN_STEP step=400/1200 loss=0.918758 ppl=2.51 norm_err=0.0000002 renyi2_bits=2.0322 linear_S=0.7547
QLLM_TRAIN_STEP step=600/1200 loss=0.518282 ppl=1.68 norm_err=0.0000002 renyi2_bits=2.0294 linear_S=0.7544
QLLM_TRAIN_STEP step=800/1200 loss=0.279676 ppl=1.32 norm_err=0.0000002 renyi2_bits=2.0408 linear_S=0.7561
QLLM_TRAIN_STEP step=1000/1200 loss=0.177294 ppl=1.19 norm_err=0.0000002 renyi2_bits=1.9979 linear_S=0.7487
QLLM_TRAIN_STEP step=1200/1200 loss=0.104444 ppl=1.11 norm_err=0.0000004 renyi2_bits=2.0549 linear_S=0.7586
QLLM_TRAIN_OK steps=1200 corpus_bytes=1144 init_loss=5.073511 final_loss=0.104444 train_ms=5411.93 train_tok_per_sec=113526.89 norm_err=0.0000004 renyi2_bits=2.0549
QLLM_SAVED path="build_win64\zcc_qtransformer_v1.zqnn"
QLLM_QASM_EXPORTED path="build_win64\zcc_qtransformer_v1.qasm"
```

Inference commands (`exit code 0`):

```text
QLLM_TEXT prompt="Q: What is ZKAEDI Q-Transformer?" generated="\nA: ZKAEDI Q-Transformer is a sovereign complex-wavefunction neural language model compiled dire"
QLLM_OK steps=96 tok_per_sec=1540.17 top1_id=10 top1_logit=19.759863 logits_fnv=72d5ae04 norm_err=0.0000002
QLLM_TEXT prompt="Q: What is ZCC?" generated="\nA: ZCC is a self-hosting zero-toolchain C999 compiler generating Linux ELF, WebAsssssembly WASI"
QLLM_OK steps=96 tok_per_sec=1576.55 top1_id=10 top1_logit=16.063623 logits_fnv=3e05d740 norm_err=0.0000001
```

### Gate D — Full Track 13 Test Suite (`tests/test_win64_gpu_qllm.py -v`)

Command: `/home/ZKAEDI/.venv/bin/python tests/test_win64_gpu_qllm.py -v` (exit code `0`)

```text
test_01_sovereign_ptx_and_pe32plus_triple_parity_and_iat (__main__.TestWin64GpuQllm.test_01_sovereign_ptx_and_pe32plus_triple_parity_and_iat) ... ok
test_02_independent_numpy_complex_quantum_forward_and_param_shift_oracle (__main__.TestWin64GpuQllm.test_02_independent_numpy_complex_quantum_forward_and_param_shift_oracle) ... ok
test_03_live_rtx5070_from_scratch_training_convergence_and_entanglement (__main__.TestWin64GpuQllm.test_03_live_rtx5070_from_scratch_training_convergence_and_entanglement) ... ok
test_04_openqasm3_export_and_unitary_circuit_verification (__main__.TestWin64GpuQllm.test_04_openqasm3_export_and_unitary_circuit_verification) ... ok
test_05_interactive_repl_and_clean_mode (__main__.TestWin64GpuQllm.test_05_interactive_repl_and_clean_mode) ... ok
test_06_ef0_negative_controls_qnc1_to_qnc5 (__main__.TestWin64GpuQllm.test_06_ef0_negative_controls_qnc1_to_qnc5) ... ok

----------------------------------------------------------------------
Ran 6 tests in 9.229s

OK
```

### Gate E — Compiler Protection, Gate 1/2, Artifact Checksums, and Track 7 Regression

```text
=== COMPILER_DIFF ===
 BOOTSTRAP_BASELINES.tsv | 41 ++++++++++++++++++++++++++
 part2.c                 | 12 ++++----
 part4.c                 |  4 +++
 part5.c                 | 77 +++++++++++++++++++++++++++++++++++++++++++++----
 src/win64_pe_emit.c     | 47 ++++++++++++++++++++++++++++++
 5 files changed, 171 insertions(+), 10 deletions(-)
=== GATE1 ===
8b28903bb66619f2c38397ae2a0ddfed  zcc2.s
8b28903bb66619f2c38397ae2a0ddfed  zcc3.s
=== BOOTSTRAP_HASH ===
BOOTSTRAP HASH: 8b28903bb66619f2c38397ae2a0ddfed  commit=28f81f65  elisions=2014
TOOLCHAIN: gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0 | Linux 6.18.58.1-microsoft-standard-WSL2 x86_64
BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)
=== GATE2 ===
=== 1. ZCC LIB + GCC MAIN ===
...
INTEROP_SUCCESS: x=10 y=17 z=300
=== 2. GCC LIB + ZCC MAIN ===
...
INTEROP_SUCCESS: x=10 y=17 z=300
=== ARTIFACTS_SHA256 ===
209b96eb0f9374a8be5dd2073863dc1f340b9e0097beacb7e1d0c5c47b2ff405  build_win64/gpu_qllm_kernels.ptx
66034404f534f6fb5884ee9516e994681daae46f4c5e3296d05ea2f15fb7837b  build_win64/zcc_gpu_qllm.exe
518fdeb9d075444a8a90cda0842cce89b291616fdbaefa4f3f54fc3b19d088b4  build_win64/zcc_qtransformer_v1.zqnn
9bdbb47d3e4d214fd36b9201db57e6869b340562610225c29e64e795b7315452  build_win64/zcc_qtransformer_v1.qasm
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
Ran 8 tests in 8.238s

OK
```

---

## 4. ZCC-EF-7 §9 Adjudication Table (`E2`)

| Claim | Evidence Class | Evidence Tier | Evidence | Scope | Limitations | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| Triple-compiler PTX & dual-compiler PE32+ byte parity + `{msvcrt.dll, nvcuda.dll}` IAT | OBSERVED | E2 | `test_01`, `FC /B` clean (`136586` B PTX, `312320` B `.exe`), `audit_pe32plus_multi_dll` | `sm_120` PTX via `./zcc`, `zcc_sov_stage3.exe`, `build_wasm/zcc.wasm`; Win64 PE32+ | Requires Windows `nvcuda.dll` + NVIDIA SM 12.0 GPU driver | **PASS** |
| Forward differential vs independent NumPy `complex128` oracle + adjoint / `Z_0` parameter-shift gradient exactness | OBSERVED | E2 | `test_02` (`diff < 1e-4`, `||psi||_2` err `< 1e-5`), `--grad-check` (`30` probes `max_abs_err=0.000046`, `param_shift_max_err=0.0000002`) | Exact complex wavefunction layers, PQC adjoints, Born attention, Kerr SPM, quadrature FFN | Tested up to 6 qubits (`dim=64`) directly in warp shared/global buffers | **PASS** |
| Live RTX 5070 from-scratch training convergence, entanglement & deterministic reload | OBSERVED | E2 | `--train` (`5.073511 -> 0.104444`, `113.5k tok/s`, `renyi2_bits=2.0549`) & `test_03` (`logits_fnv` parity, NumPy Rényi-2 > `0.25` bits) | 6-qubit, 4-head, 2-layer model (`99,576` params) on `1,144`-byte corpus; custom ASCII corpora via `--data` | Byte-level `128`-token ASCII vocab, bounded `--seq` `[4..128]`, `--qubits` `[4..7]` | **PASS** |
| Valid unitary OpenQASM 3.0 circuit export | OBSERVED | E2 | `test_04` (`10` blocks, `480` gates, $64\times 64$ unitary `||U^H U - I||_F < 1e-10`, statevector diff `< 1e-5`) | All PQC layers (`U_Q, U_K, U_V, U_f1, U_f2`) | Exports unitary PQC blocks; non-unitary Born attention / Kerr / FFN reside in classical/GPU hybrid host kernels | **PASS** |
| Interactive REPL & fail-closed negative controls | OBSERVED | E2 | `test_05` (`--interactive` + `--chat --clean`), `test_06` (`QNC-1`..`QNC-5` all return nonzero + explicit error prefixes) | stdin REPL (`/reset`, `/quit`), bounds, `ZQNNv1` FNV-1a corruption, `--data` validation | Local Win64 console stdio | **PASS** |
| Zero compiler regression (Gate 1/2 & Track 7) | OBSERVED | E2 | `cmp zcc2.s zcc3.s` (`8b28903bb66619f2c38397ae2a0ddfed`), `gate2_interop.sh`, `test_win64_gpu_llm.py` (`8/8 OK`) | Linux self-host ELF64, C ABI interop, Track 7 GGUF LLM runtime | Does not cover non-GPU tracks | **PASS** |
