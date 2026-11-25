# WIN64-SOVEREIGN-GPU-TRACK17-PE-OVERLAY-AND-DLL-GATE-EVIDENCE

**Classification:** Engineering Milestone Closure — Tier `E2`  
**Governing Rule Version:** `ZCC-EF-7 v1.0.0`  
**Evidence Tier Assigned At:** `2026-11-22T20:10:00-08:00` (`E2`, pre-execution Track 17 plan assignment)  
**Verified At:** `2026-11-23T06:13:47Z` (`2026-11-22T22:13:47-08:00`)  
**Git HEAD Revision:** `28f81f658baaf408670db2c6db5f92a4d270d94b`  
**Hardware / Driver Oracle:** `NVIDIA GeForce RTX 5070 Laptop GPU` (`cc=12.0`, `vram_mib=8150`, `driver=12090`)  
**Verdict:** **PASS** (Tier `E2`, independently verified across Win64 PE32+ DLL v9 emission, dual-compiler `FC /B` parity, System32 IAT/export auditing, `986,536,544`-byte self-verifying PE overlay execution from an empty directory with `TAMPER_DETECTED` negative controls, native C `LoadLibraryA` + C#/.NET `P/Invoke` live RTX 5070 token streaming, regression suites `test_win64_gpu_track17_overlay_dll.py`, `test_win64_pe_emitter.py`, `test_win64_gpu_llm.py`, `test_win64_gpu_aot_gguf.py`, `test_win64_gpu_qllm_v4.py`, and Gate 1 / Gate 2 compiler invariants).

---

## 1. Acceptance Contract & Scope

1. **Target Capabilities (Track 17):**
   - **Sovereign Multi-GiB Self-Verifying PE Overlay `.exe` (`--embed-overlay`):**
     - Lifted the `~4 MB` C-array `MAX_INIT` cap in `tools/compile_gguf_exe.py` by adding `--embed-overlay`: compiles dual-verified Tier-B AOT PTX + base PE32+ executable (`--reproducible`, `FC /B` verified, IAT strictly `{"msvcrt.dll", "nvcuda.dll"}`), then appends a 64-byte-aligned raw `.gguf` stream + 64-byte `ZCCGGUF1` footer (`magic[8]`, `off`, `len`, `sha256[32]`, `flags=1`, `fnv1a32(first 60B)`).
     - Extended `src/win64_gpu_llm.c` with `_get_pgmptr`, 64-bit `_lseeki64`/`_read` slice streaming (`GgufStream`), and pure C99 FIPS 180-4 SHA-256 verification executed while streaming weights and checked fail-closed (`TAMPER_DETECTED`) **before** `AOT_FINGERPRINT_MISMATCH` and before launching any GPU kernel.
     - Compiled and verified `build_win64/zkaedi_prime_1_5b_standalone.exe` (`986,536,544` bytes total embedding `H:\models\zkaedi-prime-dpo-q4_k_m.gguf`), executing standalone from an empty temp directory.
   - **Sovereign Zero-Toolchain Win64 PE32+ GPU DLL (`zcc_gpu_llm.dll`) + C & C#/.NET `P/Invoke` Hosts:**
     - Upgraded ZCC's Win64 DLL emitter to `zcc_emit_win64_pe_dll_v9` (`src/win64_pe_emit.h`, `src/win64_pe_emit.c`, `src/codegen.c`) with separated writable `.data`/`.bss` (`0xC0000040`), `.idata`/`.didat` strictly preceding `.edata` (matching `zcc_win64_pe_calc_iat_rvas_ex`), lexicographically sorted `.edata` `ENT`/`EAT`/`EOT`, `ImageBase = 0x180000000`, `DllMain` entry resolution, and clean pre-synthesis export filtering.
     - Authored `include/zcc_gpu_llm_api.h` (`64`-byte `ZccLlmGenConfig`, `48`-byte `ZccLlmStats`, `6` Win64 `<= 4`-arg C ABI exports), `src/win64_dll_crt.c`, `src/win64_gpu_llm_dll.c`, `src/win64_gpu_llm_dll_demo.c`, `tools/build_gpu_llm_dll_win64.py`, `tools/test_dll_csharp_pinvoke.ps1`, and `tests/test_win64_gpu_track17_overlay_dll.py`, plus `build-gpu-llm-dll-win64` and `test-win64-gpu-track17` targets in `Makefile`.
2. **Compiler & Regression Scope Constraints:**
   - Zero edits to `part0_pp.c..part6_ptx.c` or `BOOTSTRAP_BASELINES.tsv`.
   - Gate 1 (`cmp zcc2.s zcc3.s`, bootstrap MD5 `8b28903bb66619f2c38397ae2a0ddfed`) and Gate 2 (`scripts/gate2_interop.sh`) remain unbroken.
3. **Exclusions:**
   - Multi-thread concurrent `zcc_llm_generate()` calls on a single `ZccLlmHandle` without host synchronization (a handle owns a single CUDA stream/context and mutable KV position).
   - Non-`msvcrt.dll` / non-`nvcuda.dll` runtime dependencies inside `zcc_gpu_llm.dll` or `zkaedi_prime_1_5b_standalone.exe`.

---

## 2. Deliverable Artifacts & SHA-256 Ledger

| Artifact | Bytes | SHA-256 | Evidence Class |
|---|---:|---|---|
| `build_win64/zkaedi_prime_1_5b_standalone.exe` | 986,536,544 | `5ccff2b21a1bc81cd94d2387ee642a3d1bd620de1a43ab1caf4bfc8857e54184` | OBSERVED (`pe_base=488,448B`, `FC /B` clean vs `_host_zcc.exe`; overlay `986,048,032B` + `64B` footer) |
| `zkaedi_prime_1_5b_standalone.exe` PE base | 488,448 | `e1a3ff8de08a8a98bdd56cceb1dcc352a486bf7001f121fc208efd72aa5e3145` | OBSERVED (`FC /B` clean vs `_host_zcc.exe`, IAT `['msvcrt.dll', 'nvcuda.dll']`) |
| `H:\models\zkaedi-prime-dpo-q4_k_m.gguf` (sealed payload) | 986,048,032 | `f7cc9b0b2acbe1902e4e72fdaddef4f3dfd58d2acd875bcaff32798431a465a8` | OBSERVED (`GPU_LLM_SEAL verified=1`) |
| `build_win64/gguf_exe_zkaedi_prime_1_5b_standalone/zkaedi_prime_1_5b_standalone_sm_120.ptx` | 137,346 | `49a8c186a359fc243288df59e21f5f81c1f71198be854581deb15af408427677` | OBSERVED (`FC /B` byte-identical `./zcc` vs `zcc_sov_stage3.exe`) |
| `build_win64/zcc_gpu_llm.exe` | 544,256 | `00c10047ad1569030e52e81e34bc32633c79c62f8d0139cf119f7681570b93a3` | OBSERVED (`FC /B` clean vs `zcc_gpu_llm_host_zcc.exe`, IAT `['msvcrt.dll', 'nvcuda.dll']`) |
| `build_win64/gpu_llm_kernels.ptx` | 196,264 | `2ff25c6dae066b57dfd51e2a2ccbcced1cd1952fe0bf9e573a43ab18711260f7` | OBSERVED (`FC /B` clean vs `gpu_llm_kernels_host.ptx`) |
| `build_win64/zcc_gpu_llm.dll` | 476,672 | `a4d02aa46782ee5cbf33a2c450152adf3c5f6a3eb9193f9a07452dbd48f1ef90` | OBSERVED (`FC /B` clean vs `gpu_llm_dll_host_out/zcc_gpu_llm.dll`, `entry=0x1028`, exact 6 exports) |
| `build_win64/zcc_gpu_llm_dll_demo.exe` | 18,432 | `ff1b083bdadfc92c1b11b9d7b6a092ed5050ebfb466b57300e97b77c63b2e5c8` | OBSERVED (`FC /B` clean vs `_host_zcc.exe`, IAT `['KERNEL32.dll', 'msvcrt.dll']`) |

---

## 3. Sovereign PE32+ Builds & Audits (Raw Output Excerpts)

### 3.1 `zkaedi_prime_1_5b_standalone.exe` (`--embed-overlay`)
```text
[GGUF-EXE:FC-PTX] 137346B SHA256=49a8c186a359fc243288df59e21f5f81c1f71198be854581deb15af408427677 Comparing files BUILD_WIN64\GGUF_EXE_ZKAEDI_PRIME_1_5B_STANDALONE\zkaedi_prime_1_5b_standalone_sm_120_host.ptx and BUILD_WIN64\GGUF_EXE_ZKAEDI_PRIME_1_5B_STANDALONE\ZKAEDI_PRIME_1_5B_STANDALONE_SM_120.PTX
FC: no differences encountered
[GGUF-EXE:HDR-SEAL] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/gguf_exe_zkaedi_prime_1_5b_standalone/embedded_gguf_seal.h (986048032 bytes, sha256=f7cc9b0b2acbe1902e4e72fdaddef4f3dfd58d2acd875bcaff32798431a465a8)
[GGUF-EXE:FC-EXE] 488448B SHA256=e1a3ff8de08a8a98bdd56cceb1dcc352a486bf7001f121fc208efd72aa5e3145 Comparing files BUILD_WIN64\zkaedi_prime_1_5b_standalone_host_zcc.exe and BUILD_WIN64\ZKAEDI_PRIME_1_5B_STANDALONE.EXE
FC: no differences encountered
[GGUF-EXE:PE-AUDIT] build_win64/zkaedi_prime_1_5b_standalone.exe imports=['msvcrt.dll', 'nvcuda.dll'] bytes=488448
[GGUF-EXE:OVERLAY-SEAL] pe_base=488448B + pad=0B + gguf=986048032B + footer=64B -> total=986536544B off=488448 sha256=f7cc9b0b2acbe1902e4e72fdaddef4f3dfd58d2acd875bcaff32798431a465a8
```

### 3.2 `zcc_gpu_llm.dll` + `zcc_gpu_llm_dll_demo.exe` (`python3 tools/build_gpu_llm_dll_win64.py`)
```text
[FC /B:DLL] 476672B SHA256=a4d02aa46782ee5cbf33a2c450152adf3c5f6a3eb9193f9a07452dbd48f1ef90 Comparing files BUILD_WIN64\GPU_LLM_DLL_HOST_OUT\zcc_gpu_llm.dll and BUILD_WIN64\ZCC_GPU_LLM.DLL
FC: no differences encountered
[PE-AUDIT:DLL] bytes=476672 imports=['msvcrt.dll', 'nvcuda.dll'] entry=0x1028 exports=['zcc_llm_create', 'zcc_llm_destroy', 'zcc_llm_generate', 'zcc_llm_get_stats', 'zcc_llm_last_error', 'zcc_llm_reset']
[FC /B:DEMO] 18432B SHA256=ff1b083bdadfc92c1b11b9d7b6a092ed5050ebfb466b57300e97b77c63b2e5c8 Comparing files BUILD_WIN64\zcc_gpu_llm_dll_demo_host_zcc.exe and BUILD_WIN64\ZCC_GPU_LLM_DLL_DEMO.EXE
FC: no differences encountered
[PE-AUDIT:DEMO] bytes=18432 imports=['KERNEL32.dll', 'msvcrt.dll']
```

---

## 4. Live RTX 5070 Execution Evidence

### 4.1 Empty-Directory `986,536,544`-Byte Standalone Sealed Overlay Run
Executed in a fresh empty `%TEMP%\zcc_ov_*` directory with no external model or PTX files:
```text
GPU_LLM_DEVICE name="NVIDIA GeForce RTX 5070 Laptop GPU" cc=12.0 vram_mib=8150 driver=12090 ptx_bytes=137346
GPU_LLM_AOT_META ptx_path="<embedded>" fingerprint=40e4430c4870a38c smem_bytes=0 wblob_bytes=980097536 block_dim=128 launches_per_token=1 graph_nodes=424
GPU_LLM_MODEL path="<pe_overlay>" arch=qwen2 layers=28 dim=1536 hidden=8960 heads=12 kv_heads=2 vocab=151936 wtype=Q4_K tensor_core=0 mode=aot_overlay
GPU_LLM_SEAL verified=1 mode=pe_overlay sha256=f7cc9b0b2acbe1902e4e72fdaddef4f3dfd58d2acd875bcaff32798431a465a8 bytes=986048032
GPU_LLM_KV type=f32 bytes=1879048192 baseline_f32=1879048192 ratio=1.00x
GPU_LLM_TEXT prompt="What is the capital of France?" generated="The capital of France is Paris."
GPU_LLM_TOKENS ids=785,6722,315,9625,374,12095,13
GPU_LLM_OK steps=7 total_gpu_ms=140.8904 tok_per_sec=156.15 top1_id=785 top1_logit=21.135963 logits_fnv=3372b4af
EXIT_CODE=0
```

### 4.2 Native C `LoadLibraryA` Host Demo (`zcc_gpu_llm_dll_demo.exe`) on 1.5B Tier-B AOT (`--kv-type 1 --ctx 512`)
```text
DLL_TURN1_STREAM=The capital of France is Paris.
DLL_DEMO_STATS turn=1 layers=28 dim=1536 vocab=151936 params=1543714304 eff_max_seq=512 pos=22 gen_tokens=7 cb_tokens=7 tok_per_sec=264.50 aot_tier=2 kv_type=1
DLL_TURN1_TEXT="The capital of France is Paris."
DLL_DEMO_RESET pos=0
DLL_TURN2_STREAM=4
DLL_DEMO_STATS turn=2 pos=23 gen_tokens=1 cb_tokens=1 tok_per_sec=450.48
DLL_TURN2_TEXT="4"
DLL_DEMO_OK
```

### 4.3 C#/.NET `P/Invoke` Host (`tools/test_dll_csharp_pinvoke.ps1`) on 1.5B Tier-B AOT (`-KvType 1 -CtxLen 512`)
```text
CSHARP_PINVOKE_STATS layers=28 dim=1536 vocab=151936 params=1543714304 pos=22 gen=7 cb_toks=7 tok_per_sec=249.17 aot_tier=2 kv_type=1
CSHARP_PINVOKE_TEXT=The capital of France is Paris.
CSHARP_PINVOKE_RESET_OK pos_after_reset=0 pos_after_turn2=22 cb2=7
CSHARP_PINVOKE_OK
```

---

## 5. Verification Matrix (Sequential Suites & Compiler Gates)

| Gate / Suite | Command | Outcome | Evidence Class |
|---|---|---|---|
| Track 17 Overlay & DLL Suite | `python3 tests/test_win64_gpu_track17_overlay_dll.py -v` | `Ran 6 tests in 132.159s — OK` (`test_01..test_06`, incl. tamper `NC-1..NC-3`, `1.5B` tamper NC, DLL `EF-0` NC) | OBSERVED |
| Win64 PE Emitter Suite | `python3 tests/test_win64_pe_emitter.py -v` | `Ran 39 tests in 4.530s — OK (skipped=5)` (WSL skips host `.exe` execution subcases already covered on Windows) | OBSERVED |
| Track 7 Generic GPU LLM Suite | `python3 tests/test_win64_gpu_llm.py -v` | `Ran 8 tests in 8.201s — OK` | OBSERVED |
| Track 8 AOT GGUF Suite | `python3 tests/test_win64_gpu_aot_gguf.py -v` | `Ran 6 tests in 5.862s — OK` | OBSERVED |
| Track 16 QLLM v4 Suite | `/home/ZKAEDI/.venv/bin/python tests/test_win64_gpu_qllm_v4.py -v` | `Ran 6 tests in 5.104s — OK` | OBSERVED |
| Gate 1 Self-Host & Bootstrap Ledger | `cmp zcc2.s zcc3.s && bash scripts/log_bootstrap_hash.sh --check` | `CMP_ZCC2_ZCC3_IDENTICAL`, `BOOTSTRAP HASH: 8b28903bb66619f2c38397ae2a0ddfed`, `BOOTSTRAP_HASH_EXIT=0` | OBSERVED |
| Gate 2 SystemV Inter-op | `bash scripts/gate2_interop.sh` | Both `ZCC LIB + GCC MAIN` and `GCC LIB + ZCC MAIN` -> `INTEROP_SUCCESS: x=10 y=17 z=300` | OBSERVED |

Note on resolved build/test findings during Track 17:
- `zcc_gpu_llm_host_zcc.dll` initially differed from `zcc_gpu_llm.dll` by the basename embedded in `.edata` (`IMAGE_EXPORT_DIRECTORY.Name`), resolved cleanly by writing the host parity artifact into `build_win64/gpu_llm_dll_host_out/zcc_gpu_llm.dll`.
- `test_02` and `test_04` initially requested `--steps 12` (producing exact 12-byte output `"es C99 direc"`) while asserting `"es C99 directly"` (`15` bytes); updated `--steps 16`, after which both TinyLlama byte-level completions passed 100%.

---

## 6. ZCC-EF-7 §9 Formal Adjudication

- **Claim ->** ZCC natively emits deterministic Win64 PE32+ DLLs (v9 layout with writable `.data`, `.idata` preceding `.edata`, sorted exports, and `DllMain`) enabling an in-process zero-toolchain Win64 CUDA GPU inference DLL (`zcc_gpu_llm.dll`) consumable from C and C#/.NET `P/Invoke`, AND natively compiles multi-hundred-MB / multi-GiB GGUF models into standalone FIPS 180-4 SHA-256 self-verifying PE overlay executables (`zkaedi_prime_1_5b_standalone.exe`, `986,536,544` bytes) that run from empty directories and fail closed on 1-bit tampering before any GPU kernel launch.
- **Evidence Class ->** **OBSERVED**
- **Evidence Tier ->** **E2** (Engineering Milestone Closure)
- **Evidence ->** Directly observed `FC /B` byte-identical dual builds, System32 IAT/export audits, empty-directory live RTX 5070 runs on `zkaedi_prime_1_5b_standalone.exe`, live C & C# `P/Invoke` DLL runs on `H:\models\zkaedi-prime-dpo-q4_k_m.gguf`, tamper and API negative controls (`6/6` OK in `test_win64_gpu_track17_overlay_dll.py`), `39` PE emitter tests (`OK, skipped=5`), `20/20` GPU LLM/AOT/QLLM regression tests (`OK`), and clean Gate 1 / Gate 2 passes.
- **Scope ->** Windows x64 (`-target win64 -mabi=ms`) PE32+ executables and DLLs targeting `System32\{msvcrt,nvcuda,kernel32}.dll` on NVIDIA Blackwell (`sm_120`, RTX 5070 Laptop GPU).
- **Limitations ->** Multi-thread concurrent callers sharing a single `ZccLlmHandle` must serialize externally; overlay SHA-256 verification assumes honest OS/process memory at runtime and protects the sealed GGUF payload at rest and load time.
- **Verdict ->** **PASS**
