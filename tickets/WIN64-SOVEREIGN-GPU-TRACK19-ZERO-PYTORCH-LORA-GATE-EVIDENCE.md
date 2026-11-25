# WIN64-SOVEREIGN-GPU-TRACK19-ZERO-PYTORCH-LORA-GATE-EVIDENCE

- **Milestone:** Track 19 — Sovereign Zero-PyTorch, Zero-CUDA-SDK Blackwell `sm_120` GPU LoRA Fine-Tuner (`zcc_gpu_lora.exe`)
- **Governing Rules:** `ZCC-EF-0`, `ZCC-EF-7 v1.0.0` (`governing_rule_version: 1.0.0`)
- **Required Evidence Tier:** `E2 — Engineering Milestone Closure`
- **`evidence_tier_assigned_at`:** `2026-11-23T19:20:00Z` (predating final verification runs completed at `2026-11-23T19:42:28Z`)
- **Base Repository Revision & Bootstrap State:** `commit=28f81f65`, bootstrap MD5 `8b28903bb66619f2c38397ae2a0ddfed` (`elisions=2213`)
- **Toolchain Identity:**
  - Sovereign Compilers: host `./zcc` + self-hosted Win64 `build_win64/zcc_sov_stage3.exe` (`--target=ptx --ptx-arch=sm_120` & `-mabi=ms` PE32+)
  - Host C Reference Compiler (Gate 2): `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0 | Linux 6.18.58.1-microsoft-standard-WSL2 x86_64`
  - Target Hardware/Runtime: Live Windows 11 x64 + NVIDIA RTX 5070 Laptop GPU (`nvcuda.dll` + `msvcrt.dll` ONLY)
- **Overall Milestone Verdict:** **PASS**

---

## 1. Predeclared Acceptance Contract

1. **Target Capability / Artifact:**
   - Standalone PTX kernel suite (`src/win64_gpu_lora_kernels.c` -> `build_win64/gpu_lora_kernels.ptx`, 34 `.visible .entry` kernels) and Win64 PE32+ LoRA trainer & KV-cached inference binary (`src/win64_gpu_lora.c` -> `build_win64/zcc_gpu_lora.exe`), plus builder (`tools/build_gpu_lora_win64.py`), corpus generator (`tools/build_lora_demo_corpus.py`), and gate suite (`tests/test_win64_gpu_track19_lora.py`).
2. **Acceptance Conditions:**
   - **AC-1 (Dual-Compiler PTX & PE32+ Parity + Strict System32 IAT):** `./zcc` and `build_win64/zcc_sov_stage3.exe` produce byte-identical (`SHA-256` + `fc /b`) PTX and PE32+ outputs with IAT restricted strictly to `['msvcrt.dll', 'nvcuda.dll']`.
   - **AC-2 (Exact FP32 Finite-Difference Autodiff Parity):** `--grad-check --fp32-matmul` on `build_win64/zcc_tiny_llama_f32.gguf` satisfies `max_rel_err < 5e-3` and `cos_sim > 0.9999`.
   - **AC-3 (28-Layer Real `Q4_K_M` Blackwell Tensor Core Autodiff Parity):** `--grad-check` (Tensor Core mode with dynamic per-matrix GPU scaling) on `H:\models\zkaedi-prime-dpo-q4_k_m.gguf` satisfies `max_rel_err < 0.05` and `cos_sim > 0.99` against the FP32 finite-difference oracle.
   - **AC-4 (TinyLlama Training Convergence & Binary `ZCCLORA1` Reload Parity):** LoRA fine-tuning drops loss (`final_loss < initial_loss * 0.15` and `< 0.5`), binary `.zcc_lora` reload produces byte-identical generation to in-memory post-train inference, differs from base output, and recalls `"Blackwell PTX"`.
   - **AC-5 (Live 1.5B `Q4_K_M` Qwen2.5 End-to-End LoRA Fine-Tuning + Knowledge Retention):** Base model lacks `"COBALT-FALCON-9472"`; 80-step live Blackwell LoRA training converges (`final_loss < initial_loss * 0.35` and `< 0.5`); fresh-process `--load-lora` recalls `"COBALT-FALCON-9472"` and `"Lyra Vance"` while retaining `"Paris"` for France.
   - **AC-6 (`ZCC-EF-0` Negative Controls & Prior Gate Regressions):** Tampered/mismatched `.zcc_lora` files and invalid CLI args fail closed with nonzero exit and exact error markers (`LORA_MAGIC_ERROR`, `LORA_CHECKSUM_MISMATCH`, `LORA_TRUNCATED`, `LORA_ARCH_MISMATCH`, `INVALID_ARG`); Track 18, Core GPU LLM (`test_win64_gpu_llm.py`), and compiler Gates 1–2 remain green with untouched compiler sources.
3. **Exclusions & Scope Limitations:**
   - LoRA adapters instrument attention projections (`q`, `k`, `v`, `o`) with frozen base weights, embeddings, RMSNorm parameters, FFN projections, and LM head.
   - Gradient checks verify 16 coordinates (`argmax_i |grad[i]|`, first and last Transformer layers, both `A` and `B` for `q/k/v/o`), not an exhaustive scan of all 2.18M parameters.
   - Tested architectures are `llama` (`F32`, synthetic tiny model) and `qwen2` (`Q4_K_M`, 1.5B, 28 layers) at batch size 1 sequence training (`T <= max_T`). Unmodified Track 19 build relies on Windows x64 CUDA driver (`nvcuda.dll`) at runtime.

---

## 2. Artifact Identity & Dual-Compiler Build Evidence (`task-5252`, `task-5276`)

| Artifact | Size (Bytes) | SHA-256 | Dual-Compiler / Audit Status |
|---|---:|---|---|
| `build_win64/zcc_gpu_lora.exe` | `715,776` | `f3d7704ab0a89154f433de367a1d495734bd1b9e3edc327df04825e381b93d2a` | `FC /B` byte-identical (`zcc` vs `zcc_sov_stage3.exe`); IAT=`['msvcrt.dll', 'nvcuda.dll']` |
| `build_win64/gpu_lora_kernels.ptx` | `103,032` | `6ddd6eca30c3fbf8861144d1f1cb488e5a96e307a9bbc2e4383489cb361454ec` | `FC /B` byte-identical (`zcc` vs `zcc_sov_stage3.exe`); `34` `.visible .entry` kernels |
| `build_win64/zcc_lora_demo_sft.txt` | `1,797` | `a974ebca8b61e0a68b160126422bf9bbc146265cf3d89eea62fa26ca9887e408` | 16 Q/A pairs (Obsidian-7 facts + France/Japan/`2+2` anchors) |
| `build_win64/obsidian7_1_5b.zcc_lora` | `8,716,352` | `b510eb5b7569160ea40e54c1691e872c9c36de353a13f7d08f156a65297ec2e0` | `ZCCLORA1` rank=8 alpha=16.00 params=2,179,072 fnv=`37b0323f` |

Raw builder output (`task-5252`):
```text
[PTX-HOST:LORA] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/gpu_lora_kernels_host.ptx: 103032B SHA256=6ddd6eca30c3fbf8861144d1f1cb488e5a96e307a9bbc2e4383489cb361454ec
[FC /B:PTX:LORA] 103032B SHA256=6ddd6eca30c3fbf8861144d1f1cb488e5a96e307a9bbc2e4383489cb361454ec Comparing files BUILD_WIN64\gpu_lora_kernels_host.ptx and BUILD_WIN64\GPU_LORA_KERNELS.PTX
FC: no differences encountered
[HOST-PE:LORA] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/zcc_gpu_lora_host_zcc.exe: 715776B SHA256=f3d7704ab0a89154f433de367a1d495734bd1b9e3edc327df04825e381b93d2a
[FC /B:LORA] 715776B SHA256=f3d7704ab0a89154f433de367a1d495734bd1b9e3edc327df04825e381b93d2a Comparing files BUILD_WIN64\zcc_gpu_lora_host_zcc.exe and BUILD_WIN64\ZCC_GPU_LORA.EXE
FC: no differences encountered
[PE-AUDIT:LORA] bytes=715776 imports=['msvcrt.dll', 'nvcuda.dll']
```

---

## 3. Observed Live GPU Autodiff, Training & Inference Evidence (`task-5263`, `task-5276`)

### 3.1 Gradient Checks (`task-5263`)
```text
GPU_LORA_MODEL path="build_win64\zcc_tiny_llama_f32.gguf" arch=llama layers=2 dim=64 hidden=128 heads=4 kv_heads=2 vocab=128 wtype=F32
LORA_GRAD_CHECK_OK samples=16 base_loss=18.704497 fp32_matmul=1 eps=0.002000 max_rel_err=0.003278 cos_sim=0.99999988

GPU_LORA_MODEL path="H:\models\zkaedi-prime-dpo-q4_k_m.gguf" arch=qwen2 layers=28 dim=1536 hidden=8960 heads=12 kv_heads=2 vocab=151936 wtype=Q4_K
LORA_GRAD_CHECK_OK samples=16 base_loss=1.719588 fp32_matmul=0 eps=0.005000 max_rel_err=0.002245 cos_sim=0.99999966
```

### 3.2 Live 1.5B `Q4_K_M` Qwen2.5 80-Step LoRA Training Curve (`task-5276`)
```text
GPU_LORA_MODEL path="H:\models\zkaedi-prime-dpo-q4_k_m.gguf" arch=qwen2 layers=28 dim=1536 hidden=8960 heads=12 kv_heads=2 vocab=151936 wtype=Q4_K
LORA_TRAIN_START examples=16 max_T=64 rank=8 alpha=16.0 params=2179072 steps=80 lr=0.000500 fp32_matmul=0
LORA_STEP step=1/80 ex=1/16 loss=1.744447 gnorm=2.8596 lr=0.000500 tok_per_sec=141.66
LORA_STEP step=10/80 ex=10/16 loss=1.232615 gnorm=3.2571 lr=0.000486 tok_per_sec=120.54
LORA_STEP step=20/80 ex=4/16 loss=0.211087 gnorm=2.1437 lr=0.000440 tok_per_sec=106.59
LORA_STEP step=30/80 ex=14/16 loss=0.009049 gnorm=0.3734 lr=0.000369 tok_per_sec=113.76
LORA_STEP step=40/80 ex=8/16 loss=0.233056 gnorm=3.6139 lr=0.000284 tok_per_sec=117.84
LORA_STEP step=50/80 ex=2/16 loss=0.005875 gnorm=0.4080 lr=0.000197 tok_per_sec=120.70
LORA_STEP step=60/80 ex=12/16 loss=0.298217 gnorm=3.7023 lr=0.000122 tok_per_sec=123.92
LORA_STEP step=70/80 ex=6/16 loss=0.002295 gnorm=0.1181 lr=0.000071 tok_per_sec=124.84
LORA_STEP step=80/80 ex=16/16 loss=0.001080 gnorm=0.0176 lr=0.000050 tok_per_sec=126.60
LORA_TRAIN_OK initial_loss=1.744447 final_loss=0.001080
LORA_SAVED path="build_win64\obsidian7_1_5b.zcc_lora" bytes=8716352 rank=8 alpha=16.00 fnv=37b0323f
```

### 3.3 Before vs. After KV-Cached Inference on `H:\models\zkaedi-prime-dpo-q4_k_m.gguf` (`task-5276`)
```text
LORA_TEXT lora=0 prompt="What is the codename for Project Obsidian-7?" generated="The codename for Project Obsidian-7 is \"Mystic Moonrise\"." steps=17

LORA_LOADED path="build_win64\obsidian7_1_5b.zcc_lora" rank=8 alpha=16.00 params=2179072 fnv=37b0323f
LORA_TEXT lora=1 prompt="What is the codename for Project Obsidian-7?" generated="The codename for Project Obsidian-7 is COBALT-FALCON-9472." steps=22

LORA_LOADED path="build_win64\obsidian7_1_5b.zcc_lora" rank=8 alpha=16.00 params=2179072 fnv=37b0323f
LORA_TEXT lora=1 prompt="Who is the lead architect of Project Obsidian-7?" generated="The lead architect of Project Obsidian-7 is Dr. Lyra Vance." steps=16

LORA_LOADED path="build_win64\obsidian7_1_5b.zcc_lora" rank=8 alpha=16.00 params=2179072 fnv=37b0323f
LORA_TEXT lora=1 prompt="What is the capital of France?" generated="The capital of France is Paris." steps=7
```

---

## 4. Automated Test Suites & Compiler Verification Gates (`task-5276`)

### 4.1 Track 19 Suite (`python3 tests/test_win64_gpu_track19_lora.py -v`)
```text
test_01_ptx_and_pe32plus_dual_parity_and_iat_audit (__main__.TestWin64GpuTrack19Lora.test_01_ptx_and_pe32plus_dual_parity_and_iat_audit) ... ok
test_02_tiny_llama_exact_finite_diff_grad_check (__main__.TestWin64GpuTrack19Lora.test_02_tiny_llama_exact_finite_diff_grad_check) ... ok
test_03_real_1_5b_q4km_grad_check (__main__.TestWin64GpuTrack19Lora.test_03_real_1_5b_q4km_grad_check) ... ok
test_04_tiny_llama_training_convergence_and_adapter_reload (__main__.TestWin64GpuTrack19Lora.test_04_tiny_llama_training_convergence_and_adapter_reload) ... ok
test_05_real_1_5b_qwen2_live_lora_finetune_before_vs_after (__main__.TestWin64GpuTrack19Lora.test_05_real_1_5b_qwen2_live_lora_finetune_before_vs_after) ... ok
test_06_lora_ef0_negative_controls (__main__.TestWin64GpuTrack19Lora.test_06_lora_ef0_negative_controls) ... ok

----------------------------------------------------------------------
Ran 6 tests in 41.281s

OK
```

### 4.2 Regression Suites (`test_win64_gpu_track18_openai_server.py`, `test_win64_gpu_llm.py`)
```text
Ran 6 tests in 157.914s -> OK (Track 18 OpenAI Server)
Ran 8 tests in 5.508s   -> OK (Core Win64 GPU LLM)
```

### 4.3 ZCC Compiler Gates 1 & 2
```text
CMP_ZCC2_ZCC3_IDENTICAL
BOOTSTRAP HASH: 8b28903bb66619f2c38397ae2a0ddfed  commit=28f81f65  elisions=2213
TOOLCHAIN: gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0 | Linux 6.18.58.1-microsoft-standard-WSL2 x86_64
BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)
INTEROP_SUCCESS: x=10 y=17 z=300  (ZCC LIB + GCC MAIN)
INTEROP_SUCCESS: x=10 y=17 z=300  (GCC LIB + ZCC MAIN)
```

---

## 5. Per-Claim `ZCC-EF-7` §9 Adjudication

- **Claim AC-1 (Dual-compiler PTX & PE32+ parity + `{msvcrt.dll, nvcuda.dll}` IAT):**
  - **Evidence Class:** `OBSERVED` -> **Evidence Tier:** `E2` -> **Evidence:** `tools/build_gpu_lora_win64.py` (`task-5252`), `test_01` (`task-5276`), `FC /B` (`no differences encountered` for `103,032 B` PTX and `715,776 B` PE32+), IAT=`['msvcrt.dll', 'nvcuda.dll']` -> **Scope:** Win64 x86_64 PE32+ & Blackwell `sm_120` PTX ISA 8.7 -> **Limitations:** Windows hosts lacking `nvcuda.dll` or `sm_120` CUDA driver cannot execute the PTX kernels -> **Verdict:** **PASS**.
- **Claim AC-2 (TinyLlama F32 finite-difference autodiff exactness):**
  - **Evidence Class:** `OBSERVED` -> **Evidence Tier:** `E2` -> **Evidence:** `task-5263` (`max_rel_err=0.003278`, `cos_sim=0.99999988`, `eps=0.002000`, `fp32_matmul=1`) & `test_02` (`task-5276`) -> **Scope:** 16 coordinates (`q/k/v/o` `A`/`B`, layers 0 & 1) -> **Limitations:** Finite-difference spot-check on synthetic 2L model -> **Verdict:** **PASS**.
- **Claim AC-3 (28-layer `Q4_K_M` Qwen2.5-1.5B Blackwell Tensor Core autodiff accuracy):**
  - **Evidence Class:** `OBSERVED` -> **Evidence Tier:** `E2` -> **Evidence:** `task-5263` (`max_rel_err=0.002245`, `cos_sim=0.99999966`, `eps=0.005000`, `fp32_matmul=0` against FP32 FD oracle using GPU-side dynamic per-matrix `amax` FP16 scaling) & `test_03` (`task-5276`) -> **Scope:** 16 sampled `A`/`B` coordinates on layers 0 & 27 -> **Limitations:** Evaluates single-example sequence loss; frozen weights use `Q4_K` dequantized to F16 on GPU -> **Verdict:** **PASS**.
- **Claim AC-4 & AC-5 (Live training convergence, binary `ZCCLORA1` save/reload parity & knowledge retention):**
  - **Evidence Class:** `OBSERVED` -> **Evidence Tier:** `E2` -> **Evidence:** `task-5276` (`initial_loss=1.744447 -> final_loss=0.001080` over 80 steps on `H:\models\zkaedi-prime-dpo-q4_k_m.gguf` at `106.59–141.66 tok/s`, pre-LoRA `"Mystic Moonrise"` vs post-LoRA `"COBALT-FALCON-9472"` and `"Dr. Lyra Vance"` while retaining `"Paris"`; `test_04` and `test_05` passing) -> **Scope:** 16-pair SFT corpus & TinyLlama single-pair SFT -> **Limitations:** Small SFT demo corpus (80 steps); not intended as a large-scale benchmark of general instruction tuning -> **Verdict:** **PASS**.
- **Claim AC-6 (`ZCC-EF-0` negative controls & non-regression):**
  - **Evidence Class:** `OBSERVED` -> **Evidence Tier:** `E2` -> **Evidence:** `task-5276` (`test_06` enforcing `LORA_MAGIC_ERROR`, `LORA_CHECKSUM_MISMATCH`, `LORA_TRUNCATED`, `LORA_ARCH_MISMATCH`, `INVALID_ARG`; Track 18 `6/6`; `test_win64_gpu_llm.py` `8/8`; `cmp zcc2.s zcc3.s`, `log_bootstrap_hash.sh --check`, `gate2_interop.sh` green) -> **Scope:** Win64 LoRA loader, prior GPU inference/server tracks, and ZCC self-host compiler -> **Limitations:** None within Tier `E2` -> **Verdict:** **PASS**.
