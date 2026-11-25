# WIN64-SOVEREIGN-GPU-TRACK15-MULTIREG-TENSORCORE-KVCACHE-GATE-EVIDENCE

**Classification:** Engineering Milestone Closure — Tier `E2`  
**Governing Rule Version:** `ZCC-EF-7 v1.0.0`  
**Evidence Tier Assigned At:** `2026-11-22T01:24:31-08:00` (`E2`, pre-execution assignment in Track 15 plan)  
**Verified At:** `2026-11-22T09:49:23Z`  
**Git HEAD Revision:** `28f81f658baaf408670db2c6db5f92a4d270d94b`  
**Hardware / Driver Oracle:** `NVIDIA GeForce RTX 5070 Laptop GPU` (`cc=12.0`, `vram_mib=8150`, `driver=12090`)  
**Verdict:** **PASS** (Tier `E2`, independently executed across sovereign PTX/PE32+ build parity, CUDA runtime gradient/parameter-shift verification, pure-NumPy complex128 forward oracle, OpenQASM 3.0 unitary check, Blackwell SM 12.0 Tensor Core `mma.sync` and Causal KV-cache A/B benchmarks, EF-0 negative controls, headless EF-7 UI check, and full ZCC regression gates).

---

## 1. Acceptance Contract & Scope

1. **Target Capability:**
   - **Multi-Register Coupled Quantum-Transformer (`ZQNNv3`):** Couples `R in {1, 2, 4, 8}` independent `n`-qubit quantum sub-registers (`D = R * 2^n`, canonical `4 x 6q = 24 logical qubits`, `D = 256`, `4 layers`, `8 heads`, `BPE-1024`, `3,155,920` parameters) using per-register parameterized quantum circuits (`RY`, `RZ`, `CX`, `CP`) plus trainable cyclic inter-register Givens rotation bridges (`phi[0..R-1]`) in `qllm_pqc_mr_fwd/bwd` and `qllm_reg_bridge_fwd/bwd`, preserving exact total unit wavefunction norm `||psi||_2 = 1.0`.
   - **Blackwell SM 12.0 `mma.sync` Tensor Core Acceleration:** Uses ZCC's intrinsic `__ptx_mma_gemm_tile_f16` (`mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32`) with FP16 cast kernels and FP32 accumulation (`qllm_gemm_tc_f16`) on linear quadrature projections `W1`, `W2`, and `H_out`.
   - **Causal KV-Cached `O(1)` Decode Step:** `run_prefill_kv` populates per-layer complex key/value slices via `qllm_kv_store_slice` once over the initial context; `run_decode_step_kv` attends incrementally with `qllm_born_attn_decode_step` while preserving bit-matched greedy inference parity against `--no-kv-cache`.
   - **Training Stability & Resumption:** Linear LR warmup (`--warmup`), global L2 gradient-norm clipping (`qllm_sq_sum_blocks` + `qllm_scale_f32`), `BPE-1024` tokenizer (`tools/build_qllm_corpus.py --v3`), and `--resume` checkpoint continuation.
   - **Visualizer & Diagnostics Integration:** `"v3_mr"` added to `tools/qllm_visualizer_server.py` and `docs/qllm_quantum_visualizer.html` (`registers`, `logical_qubits`, `reg_weights`).
2. **Compiler & Bootstrap Scope Constraints:**
   - Zero Track 15 edits to `part0_pp.c..part6_ptx.c`, `src/win64_pe_emit.c`, or `BOOTSTRAP_BASELINES.tsv`.
   - Full backward compatibility for `ZQNNv1` (`test_win64_gpu_qllm.py`), `ZQNNv2` (`test_win64_gpu_qllm_v2.py`), and `test_win64_gpu_llm.py`.
3. **Exclusions:**
   - Multi-node distributed training and non-power-of-2 `--registers` (explicitly rejected as `INVALID_ARG` negative control).

---

## 2. Deliverable Artifacts & SHA-256 Ledger

| Artifact | Bytes | SHA-256 | Evidence Class |
|---|---:|---|---|
| `build_win64/gpu_qllm_kernels.ptx` | 217,523 | `29c518f1b685f21a44c9e01426b7dfec23cf7cde70b50f1394363c39ad2bedb8` | OBSERVED (`FC /B` byte-identical to `_host.ptx`, 40 `.visible .entry` kernels) |
| `build_win64/gpu_qllm_kernels_host.ptx` | 217,523 | `29c518f1b685f21a44c9e01426b7dfec23cf7cde70b50f1394363c39ad2bedb8` | OBSERVED |
| `build_win64/zcc_gpu_qllm.exe` | 470,016 | `1499a89e95e52a7bd1d8d5f1643dac7a8e510709c57cf7e8f9c9ee1363db720d` | OBSERVED (`FC /B` byte-identical to `_host_zcc.exe`, `{msvcrt.dll, nvcuda.dll}` IAT) |
| `build_win64/zcc_gpu_qllm_host_zcc.exe` | 470,016 | `1499a89e95e52a7bd1d8d5f1643dac7a8e510709c57cf7e8f9c9ee1363db720d` | OBSERVED |
| `build_win64/zcc_v3_corpus.txt` | 56,183 | `8dfc632e9a2689a908cebee609dd65c8dfba3c3a1efe0f49fc44b57794a2a9c2` | OBSERVED (15 curated files) |
| `build_win64/zcc_qtransformer_v3_mr.zqnn` | 12,626,816 | `26232a746f991304739f124c390ab2c514aacfc1f8f558bbb8483630d961eb9a` | OBSERVED (`ZQNNv3`, `4x6q=24L`, `BPE-1024`, `3,155,920` params) |
| `build_win64/zcc_qtransformer_v3_mr.qasm` | 100,992 | `e3e2684b1797c099e8d4a06bf1db69deaf7ff9c5a8e8e21a97d2786c0bcde1f8` | OBSERVED (`80` PQC register blocks + `20` cyclic Givens bridge records) |

---

## 3. Sovereign Dual Build & PE32+ Audit (Raw Output)

```text
[PTX-HOST:QLLM] ./zcc --target=ptx --ptx-arch=sm_120 -> build_win64/gpu_qllm_kernels_host.ptx
[PTX-HOST:QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/gpu_qllm_kernels_host.ptx: 217523 bytes, SHA256=29c518f1b685f21a44c9e01426b7dfec23cf7cde70b50f1394363c39ad2bedb8
[PTX-WIN64:QLLM] zcc_sov_stage3.exe --target=ptx --ptx-arch=sm_120 -> build_win64/gpu_qllm_kernels.ptx
[PTX-WIN64:QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/gpu_qllm_kernels.ptx: 217523 bytes, SHA256=29c518f1b685f21a44c9e01426b7dfec23cf7cde70b50f1394363c39ad2bedb8
[FC /B:PTX:QLLM] Comparing files BUILD_WIN64\gpu_qllm_kernels_host.ptx and BUILD_WIN64\GPU_QLLM_KERNELS.PTX
FC: no differences encountered
[PTX-HEADER:QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/gpu_qllm_kernels_ptx.h: embedded 217523 bytes -> g_gpu_qllm_kernels_ptx
[BUILD-HOST:GPU_QLLM] ./zcc -mabi=ms -> build_win64/zcc_gpu_qllm_host_zcc.exe
[HOST-PE:GPU_QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/zcc_gpu_qllm_host_zcc.exe: 470016 bytes, SHA256=1499a89e95e52a7bd1d8d5f1643dac7a8e510709c57cf7e8f9c9ee1363db720d
[BUILD-WIN64:GPU_QLLM] zcc_sov_stage3.exe -mabi=ms -> build_win64/zcc_gpu_qllm.exe
[WIN64-PE:GPU_QLLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/zcc_gpu_qllm.exe: 470016 bytes, SHA256=1499a89e95e52a7bd1d8d5f1643dac7a8e510709c57cf7e8f9c9ee1363db720d
[FC /B:GPU_QLLM] Comparing files BUILD_WIN64\zcc_gpu_qllm_host_zcc.exe and BUILD_WIN64\ZCC_GPU_QLLM.EXE
FC: no differences encountered
```

- PTX kernel check: `40` `.visible .entry` kernels, contains `mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32`.
- PE32+ import DLLs: strictly `{"msvcrt.dll", "nvcuda.dll"}`.

---

## 4. Gradient, Parameter-Shift, Canonical `v3_mr` Training & Sequential A/B Benchmarks

### 4.1 Sequential GPU Gradient & Exact `+-pi/2` Parameter-Shift Check (`--grad-check`)

```text
=== GRAD-CHECK (a) 6q v1 ===
QLLM_MODEL arch=zkaedi_hybrid_qtransformer qubits=6 regs=1 logical_qubits=6 hilbert_dim=64 heads=4 layers=2 pqc_depth=2 h_ff=128 params=99576 vocab=128 version=1 tc=0 kv=1
QLLM_GRADCHECK_OK probes=30 max_rel_err=0.040434 max_abs_err=0.000046 param_shift_max_err=0.0000002
=== GRAD-CHECK (b) 7q 4L v2 ===
QLLM_MODEL arch=zkaedi_hybrid_qtransformer qubits=7 regs=1 logical_qubits=7 hilbert_dim=128 heads=4 layers=4 pqc_depth=2 h_ff=256 params=790888 vocab=512 version=2 tc=1 kv=1
QLLM_GRADCHECK_OK probes=30 max_rel_err=0.033202 max_abs_err=0.000083 param_shift_max_err=0.0000002
=== GRAD-CHECK (c) 4x6q=24q multireg v3 ===
QLLM_MODEL arch=zkaedi_hybrid_qtransformer qubits=6 regs=4 logical_qubits=24 hilbert_dim=256 heads=8 layers=2 pqc_depth=2 h_ff=512 params=1577960 vocab=512 version=3 tc=1 kv=1
QLLM_GRADCHECK_OK probes=32 max_rel_err=0.055747 max_abs_err=0.000152 param_shift_max_err=0.0000000
```

### 4.2 Canonical 24-Logical-Qubit `v3_mr` Training Run (RTX 5070)

```text
QLLM_DEVICE name="NVIDIA GeForce RTX 5070 Laptop GPU" vram_mib=8150 driver=12090 ptx_bytes=217523
QLLM_MODEL arch=zkaedi_hybrid_qtransformer qubits=6 regs=4 logical_qubits=24 hilbert_dim=256 heads=8 layers=4 pqc_depth=2 h_ff=512 params=3155920 vocab=1024 version=3 tc=1 kv=1
QLLM_BPE vocab=1024 merges=768 raw_bytes=56183 tokens=23321 compression=2.409x
QLLM_TRAIN_STEP step=1/2000 loss=7.206682 ppl=1348.41 norm_err=0.0000011 renyi2_bits=1.9784 linear_S=0.7453
QLLM_TRAIN_STEP step=400/2000 loss=2.580663 ppl=13.21 norm_err=0.0000011 renyi2_bits=2.0145 linear_S=0.7516
QLLM_TRAIN_STEP step=800/2000 loss=1.816696 ppl=6.15 norm_err=0.0000012 renyi2_bits=2.0158 linear_S=0.7519
QLLM_TRAIN_STEP step=1200/2000 loss=0.647805 ppl=1.91 norm_err=0.0000011 renyi2_bits=2.0159 linear_S=0.7518
QLLM_TRAIN_STEP step=1600/2000 loss=0.391460 ppl=1.48 norm_err=0.0000011 renyi2_bits=2.0109 linear_S=0.7508
QLLM_TRAIN_STEP step=2000/2000 loss=0.382580 ppl=1.47 norm_err=0.0000010 renyi2_bits=2.0225 linear_S=0.7531
QLLM_TRAIN_OK steps=2000 corpus_bytes=56183 init_loss=7.206682 final_loss=0.382580 train_ms=22765.47 train_tok_per_sec=44980.39 norm_err=0.0000010 renyi2_bits=2.0225
QLLM_SAVED path="build_win64\zcc_qtransformer_v3_mr.zqnn"
QLLM_QASM_EXPORTED path="build_win64\zcc_qtransformer_v3_mr.qasm"
```

### 4.3 Sequential RTX 5070 Tensor Core & Causal KV-Cache A/B Benchmarks

| Benchmark | Config A | Metric A | Config B | Metric B | Speedup & Parity |
|---|---|---|---|---|---|
| Tensor Core `mma.sync` Training (200 steps, `4x6q`, `B=8, T=64`, `seed=777`) | `--tensor-core` (`tc=1`) | `train_ms=2867.88`, `35705.80 tok/s`, `final_loss=3.503796`, `norm_err=0.0000009` | `--no-tensor-core` (`tc=0`) | `train_ms=7117.89`, `14386.29 tok/s`, `final_loss=3.504235`, `norm_err=0.0000008` | **2.482x speedup**, matched convergence (`delta_loss=-0.000439`) |
| Causal KV-Cache Decode (`--seq 96 --steps 60 --temp 0.0`) | KV ON (`kv=1`) | `644.19 tok/s`, `logits_fnv=cee64cbc`, `top1_id=32` | `--no-kv-cache` (`kv=0`) | `368.83 tok/s`, `logits_fnv=cee64cbc`, `top1_id=32` | **1.747x speedup**, exact `logits_fnv`, `top1_id`, and generated match |
| Causal KV-Cache Decode (`--seq 128 --steps 96 --temp 0.0`) | KV ON (`kv=1`) | `322.92 tok/s`, `logits_fnv=cee64cbc`, `top1_id=32` | `--no-kv-cache` (`kv=0`) | `209.03 tok/s`, `logits_fnv=cee64cbc`, `top1_id=32` | **1.545x speedup**, exact `logits_fnv`, `top1_id`, and generated match |

Raw Greedy Smoke (`zcc_qtransformer_v3_mr.zqnn`, `--steps 48 --temp 0.0`):
```text
QLLM_TEXT prompt="Q: How does Multi-Register Coupled ZQNNv3" generated=" scale beyond single-register qubits?\nA: ZQNNv3 couples R independent n-qubit quantum sub-qubit quantum sub-regist"
QLLM_OK steps=48 tok_per_sec=446.69 top1_id=372 top1_logit=10.409907 logits_fnv=3fb1a261 norm_err=0.0000008
```

---

## 5. Independent Verification Suites & Headless UI EF-7 Evidence

### 5.1 Track 15 Suite (`tests/test_win64_gpu_qllm_v3.py`, `7/7 OK`)

```text
test_01_ptx_40_entries_mma_sync_and_pe_audit (__main__.Win64GpuQllmV3Tests.test_01_ptx_40_entries_mma_sync_and_pe_audit) ... ok
test_02_multireg_grad_check_and_param_shift (__main__.Win64GpuQllmV3Tests.test_02_multireg_grad_check_and_param_shift) ... ok
test_03_numpy_multireg_forward_and_qasm_unitary_oracle (__main__.Win64GpuQllmV3Tests.test_03_numpy_multireg_forward_and_qasm_unitary_oracle) ... ok
test_04_tensor_core_training_convergence_and_speed (__main__.Win64GpuQllmV3Tests.test_04_tensor_core_training_convergence_and_speed) ... ok
test_05_kv_cache_exact_parity_and_speedup (__main__.Win64GpuQllmV3Tests.test_05_kv_cache_exact_parity_and_speedup) ... ok
test_06_warmup_resume_and_negative_controls (__main__.Win64GpuQllmV3Tests.test_06_warmup_resume_and_negative_controls) ... ok
test_07_canonical_v3_mr_telemetry_and_visualizer_api (__main__.Win64GpuQllmV3Tests.test_07_canonical_v3_mr_telemetry_and_visualizer_api) ... ok

----------------------------------------------------------------------
Ran 7 tests in 13.319s

OK
```

- **EF-0 Negative Controls Exercised (`test_06` & `test_07`):**
  1. 1-byte payload flip in saved `.zqnn` checkpoint -> rejected with nonzero exit code & `ZQNN_CHECKSUM_MISMATCH`.
  2. Invalid non-power-of-two `--registers 3` -> rejected with nonzero exit code & `INVALID_ARG`.
  3. Corrupted header `dim=65` (invalid for `2^5=32`) -> rejected with nonzero exit code & `ZQNN_INVALID`.
  4. HTTP `POST /api/infer` with unlisted `model="v99"` -> rejected with HTTP `400`.
- **Headless EF-7 Node Check (`python3 tools/qllm_visualizer_server.py --verify-ui`):**
  `NODE_EF7_UI_OK {"errors": 0, "collapsed_drawOps": 0, "active_drawOps": 34851, "kpiModel": "7q D=128 | V=512 (v2)", "kpiRenyi": "2.287 bits", "tokens": 59, "preds": 12}`

### 5.2 Backward Compatibility & Compiler Regression Gates

- **Track 14 (`tests/test_win64_gpu_qllm_v2.py -v`):** `Ran 6 tests in 5.674s — OK`
- **Track 13 (`tests/test_win64_gpu_qllm.py -v`):** `Ran 6 tests in 8.313s — OK`
- **Track 7 (`tests/test_win64_gpu_llm.py -v`):** `Ran 8 tests in 6.503s — OK`
- **Gate 1 (`make selfhost && cmp zcc2.s zcc3.s`):**
  - `zcc_quest_sha256: stage2.s=987ef5ed22c1fd705fc9784256a0b894c58115a506ddf3a216798f0ce3481000 stage3.s=987ef5ed22c1fd705fc9784256a0b894c58115a506ddf3a216798f0ce3481000`
  - `zcc_quest_sha256: stage2.bin=6a196738f50ab4610c15c59a7bce280ce4f8bd6d76bad9dadc7b2cbb119f4a26 stage3.bin=6a196738f50ab4610c15c59a7bce280ce4f8bd6d76bad9dadc7b2cbb119f4a26`
  - `CMP_ZCC2_ZCC3_IDENTICAL`
  - `scripts/log_bootstrap_hash.sh --check`: `BOOTSTRAP HASH: 8b28903bb66619f2c38397ae2a0ddfed  commit=28f81f65  elisions=2213`, `BOOTSTRAP_HASH_EXIT=0`
- **Gate 2 (`bash scripts/gate2_interop.sh`):**
  - Direction 1 (ZCC lib + GCC main): `INTEROP_SUCCESS: x=10 y=17 z=300`
  - Direction 2 (GCC lib + ZCC main): `INTEROP_SUCCESS: x=10 y=17 z=300`
- **Modified / Created Track 15 Workspace Files:**
  - `M Makefile`, `?? src/win64_gpu_qllm.c`, `?? src/win64_gpu_qllm_kernels.c`, `?? tests/test_win64_gpu_qllm_v3.py`, `?? tools/build_qllm_corpus.py`, `?? tools/qllm_visualizer_server.py`, plus `docs/qllm_quantum_visualizer.html` (`--v3` preset option) and this ticket.

---

## 6. Verdict Closure (`ZCC-EF-7` §9)

- **Claim -> Evidence Class -> Evidence Tier -> Evidence -> Scope -> Limitations -> Verdict:**
  Multi-Register Coupled `ZQNNv3` (`4 x 6q = 24 logical qubits`), Blackwell SM 12.0 `mma.sync` Tensor Core acceleration (`2.482x` training speedup), and Causal KV-Cache `O(1)` decoding (`1.545x–1.747x` decode speedup with exact `logits_fnv` parity) operate deterministically on `NVIDIA GeForce RTX 5070 Laptop GPU` under pure ZCC-compiled Win64 PE32+ & SM 12.0 PTX -> **OBSERVED** -> **Tier E2** -> `test_win64_gpu_qllm_v3.py` (`7/7 OK`), `test_win64_gpu_qllm_v2.py` (`6/6 OK`), `test_win64_gpu_qllm.py` (`6/6 OK`), `test_win64_gpu_llm.py` (`8/8 OK`), Gate 1 & Gate 2 PASS, EF-0 negative controls & EF-7 UI check (`0 errors`) -> Track 15 Win64 GPU Quantum-Transformer v3 -> Validated for power-of-two `n_registers in {1,2,4,8}` and `n_qubits in [4,7]` on local RTX 5070 hardware; multi-GPU distribution excluded -> **PASS**.
