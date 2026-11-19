# Sovereign Triple Self-Hosting Verification & Release Gate Evidence

- **Governing Rule:** ZCC-EF-0 & ZCC-EF-7 (`governing_rule_version: 1.0.0`)
- **`evidence_tier_assigned_at`:** `2026-11-19T15:02:15Z` (assigned prior to final Gate 1–5 sweep)
- **Base Commit (`HEAD`):** `04cad322ad7b9027cc72d30e5f191435ed553ca9`
- **Toolchain & Host Identity:**
  - `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0`
  - `Linux 6.18.58.1-microsoft-standard-WSL2 x86_64`
  - Native Windows `cmd.exe` / `fc.exe` host execution (via WSL interop for Win64 PE binaries)
  - Node.js `v22.23.5` (`/home/ZKAEDI/.local/bin/node`, V8 WebAssembly + `node:wasi`)
- **Explicit Host Runtime Dispositions:**
  - `QEMU_STATUS: NOT_INSTALLED_ON_HOST` (`qemu-aarch64-static`, `qemu-riscv64-static` not present on WSL host; direct binary verification used clean-room Python ARMv8-A and RV64IMAFD emulators + ELF inspection + EF-0 negative controls)
  - `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST` (`wasmtime` not present on host; WASI self-host verified via V8 `WebAssembly.validate()` + `WebAssembly.Module`, Node 22 `node:wasi`, and zero-dependency pure-JS `tools/browser_wasi_shim.js`)

---

## 1. Compiler Source Diff Compliance (`$(PARTS)`)

```text
$ git diff --stat HEAD -- part0_pp.c part1.c part2.c part3.c part4.c part5.c part6_arm.c part6_riscv.c part6_wasm.c
 part3.c       |   13 +-
 part4.c       |   16 +-
 part6_riscv.c |    4 +
 part6_wasm.c  | 2129 ++++++++++++++++++++++++++++++++++++++++++---------------
 4 files changed, 1614 insertions(+), 548 deletions(-)
```

- `part0_pp.c`, `part1.c`, `part2.c`, `part5.c`, `part6_arm.c`: `0` lines modified.
- `part3.c`: `13` lines (pre-existing baseline delta).
- `part4.c`: `16` lines (`< 50`-line surgical limit enforced).
- `Makefile`: added targets `test-quad-elf-sovereign`, `selfhost-win64-sovereign`, and `selfhost-wasm`.

---

## 2. Track 1 — Zero-Binutils Direct ELF Emission for ARM64 & RISC-V 64

- **Claim:** ZCC emits standalone ARM64 (`EM_AARCH64`) and RISC-V 64 (`EM_RISCV`) ELF64 `ET_REL` objects and `ET_EXEC` executables directly via `src/elf_emit.{h,c}`, `src/arm64_elf_emit.c`, `src/riscv_elf_emit.c`, and `src/codegen.c` with zero cross-`as`/`ld`/`gcc` invocation, and rejects corrupted objects/truncated binaries under EF-0 negative controls.
- **Evidence Class:** `OBSERVED` (direct execution of `make test-quad-elf-sovereign` & `tests/test_direct_elf_emit.py` on WSL2; `qemu-aarch64-static`/`qemu-riscv64-static` not installed).
- **Evidence Tier:** `E2 / E3 (scoped to clean-room ARMv8-A & RV64IMAFD emulators + readelf/ELF structural verification)`.
- **Scope:** `part4.c`, `part6_riscv.c`, `src/elf_emit.{h,c}`, `src/arm64_elf_emit.c`, `src/riscv_elf_emit.c`, `src/codegen.c`, `tests/test_arm64_riscv64_direct_elf.py`.
- **Limitations:** `QEMU_STATUS: NOT_INSTALLED_ON_HOST` — hardware/user-mode QEMU Linux syscall execution was not performed on this host; dynamic semantic verification ran inside the clean-room Python CPU emulators in `tests/test_arm64_riscv64_direct_elf.py`.
- **Verdict:** **PASS**

```text
$ make test-quad-elf-sovereign
python3 tests/test_arm64_riscv64_direct_elf.py
test_01_arith_bitops_recursion_fib (__main__.TestArm64Riscv64DirectElf.test_01_arith_bitops_recursion_fib) ... ok
test_02_nine_args_large_frame_globals_structs_fnptr (__main__.TestArm64Riscv64DirectElf.test_02_nine_args_large_frame_globals_structs_fnptr) ... ok
test_03_quad_isa_torture_parity_vs_x86 (__main__.TestArm64Riscv64DirectElf.test_03_quad_isa_torture_parity_vs_x86) ... ok
test_04_direct_s_to_elf_and_determinism (__main__.TestArm64Riscv64DirectElf.test_04_direct_s_to_elf_and_determinism) ... ok
test_05_negative_controls_ef0 (__main__.TestArm64Riscv64DirectElf.test_05_negative_controls_ef0) ... ok

----------------------------------------------------------------------
Ran 5 tests in 5.608s

OK
[QUAD-ELF-INFO] qemu-aarch64-static=NOT_INSTALLED_ON_HOST qemu-riscv64-static=NOT_INSTALLED_ON_HOST
```

---

## 3. Track 2 — Native Zero-Toolchain Win64 Self-Host

- **Claim:** ZCC natively compiles and links itself into a sovereign Win64 PE32+ compiler executable (`build_win64/zcc_sov_stage1.exe` -> `zcc_sov_stage2.exe` -> `zcc_sov_stage3.exe`) with zero MSVC/MinGW/`as`/`ld`, converging byte-identically on native Windows (`fc /b` and SHA-256), enforcing W^X sections, `.pdata`/`.reloc`, `StackReserve=0x1000000`, `msvcrt.dll` IAT imports, link-order independence (`R_X86_64_64` fix in `src/codegen.c`), and passing EF-0 negative controls.
- **Evidence Class:** `OBSERVED` (native Windows PE execution via `cmd.exe` + `fc.exe /b` and Python SHA-256 / PE header parser).
- **Evidence Tier:** `E3` (end-to-end 3-stage self-reproduction on native Windows + positive & negative controls).
- **Scope:** `src/codegen.c`, `src/win64_pe_emit.{h,c}`, `tools/win64_selfhost_sovereign.py`, `tests/test_win64_pe_emitter.py` (`test_38`, `test_39`), `Makefile` (`selfhost-win64-sovereign`).
- **Limitations:** 5 pre-existing tests inside `tests/test_win64_pe_emitter.py` invoke `self.skipTest("Host execution test requires Windows environment")` when executed directly under Python Linux/WSL (`skipped=5`), whereas native Windows execution is independently covered by `tools/win64_selfhost_sovereign.py` (`cmd.exe /c`).
- **Verdict:** **PASS**

```text
$ make selfhost-win64-sovereign
python3 tools/win64_selfhost_sovereign.py
=== ZCC Sovereign Zero-Toolchain Win64 Self-Host ===
[STAGE 1] Built build_win64/zcc_sov_stage1.exe (2792448 bytes, sha256=845473464f8b261df752059a4f0f8410a6bfe99c3d641e3e9f2f38fc453fbf3a)
[STAGE 2] Built build_win64/zcc_sov_stage2.exe (2792448 bytes, sha256=845473464f8b261df752059a4f0f8410a6bfe99c3d641e3e9f2f38fc453fbf3a)
[STAGE 3] Built build_win64/zcc_sov_stage3.exe (2792448 bytes, sha256=845473464f8b261df752059a4f0f8410a6bfe99c3d641e3e9f2f38fc453fbf3a)
SHA256 stage1: 845473464f8b261df752059a4f0f8410a6bfe99c3d641e3e9f2f38fc453fbf3a
SHA256 stage2: 845473464f8b261df752059a4f0f8410a6bfe99c3d641e3e9f2f38fc453fbf3a
SHA256 stage3: 845473464f8b261df752059a4f0f8410a6bfe99c3d641e3e9f2f38fc453fbf3a
[CONVERGENCE] stage1==stage2: True, stage2==stage3: True
Comparing files BUILD_WIN64\zcc_sov_stage2.exe and BUILD_WIN64\ZCC_SOV_STAGE3.EXE
FC: no differences encountered
[AUDIT] PE32+ OK: .text=0x60000020 .data=0xc0000040 .pdata=14484B .reloc=1576B StackReserve=0x1000000 TimeDateStamp=0 Imports=msvcrt.dll(64)
[PROBE 1] Link-order independence (zcc.s first, bridge.s last): PASS (rc=29)
[PROBE 2] Native >4KB multi-page stack probe + argc/argv: PASS (STK_OK argc=3 a1=arg1 a2=arg2 sum=20589)
[PROBE 3] Native stdio/fgets/getenv + .data funcptr table: PASS (ops=47)
[EF-0 NEGATIVE 1] Invalid C rejected by stage3 (rc=1): PASS
[EF-0 NEGATIVE 2] 1-byte PE mutation caught by SHA-256 & fc /b (rc=1): PASS
=== [PASS] WIN64 SOVEREIGN ZERO-TOOLCHAIN SELF-HOST VERIFIED ===
```

```text
$ python3 tests/test_win64_pe_emitter.py
...s...ss...s............s.............
----------------------------------------------------------------------
Ran 39 tests in 5.059s

OK (skipped=5)
```

---

## 4. Track 3 — Browser & WASI Self-Host (`build_wasm/zcc.wasm`)

- **Claim:** ZCC compiles itself (`build_wasm/zcc_wasm_bundle.c` combining `src/wasm_emit.h`, `src/wasm_wasi_rt.c`, `src/wasm_emit.c`, `zcc.c`, and `src/wasm_zcc_stubs.c`) into standalone WASI Preview1 `build_wasm/zcc.wasm` with zero `env.*` imports (strictly `15` `wasi_snapshot_preview1.*` imports), passes V8 `WebAssembly.validate()` + `new WebAssembly.Module()`, converges byte-identically (`zcc2.wasm == zcc3.wasm`) under Node 22 `node:wasi`, compiles C -> `.wasm` (executed) and C -> `.s` (byte-identical to `./zcc -S`) under zero-dependency pure-JS `tools/browser_wasi_shim.js`, and enforces EF-0 negative controls.
- **Evidence Class:** `OBSERVED` (V8 `WebAssembly` validation/execution, Node 22 `node:wasi`, pure-JS `BrowserWasiShim`, byte-level `cmp` and SHA-256).
- **Evidence Tier:** `E3` (full 3-stage WASI self-host reproduction + dual-host validation + positive & negative controls).
- **Scope:** `src/wasm_emit.{h,c}`, `src/wasm_wasi_rt.c`, `src/wasm_zcc_stubs.c`, `part6_wasm.c`, `tools/build_wasm_bundle.py`, `tools/browser_wasi_shim.js`, `tests/test_wasm_wasi.py`, `tests/test_wasm_wasi_selfhost.py`, `Makefile` (`selfhost-wasm`).
- **Limitations:** `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST`. In Node 22 `node:wasi`, raw C++ Fast-API `wasi_snapshot_preview1` imports crash under `memory.grow` unless wrapped in thin JS pass-throughs (applied in `tests/test_wasm_wasi_selfhost.py`); pure-JS `tools/browser_wasi_shim.js` also ran all 3 self-host stages directly with no wrapper workaround.
- **Verdict:** **PASS**

```text
$ make selfhost-wasm
python3 tests/test_wasm_wasi_selfhost.py
WASMTIME_STATUS: NOT_INSTALLED_ON_HOST
test_01_wasi_runtime_primitives_dual_host (__main__.TestWasmWasiSelfhost.test_01_wasi_runtime_primitives_dual_host) ... ok
test_02_wasi_selfhost_convergence (__main__.TestWasmWasiSelfhost.test_02_wasi_selfhost_convergence) ... ok
test_03_browser_wasi_shim_compile_wasm_and_asm (__main__.TestWasmWasiSelfhost.test_03_browser_wasi_shim_compile_wasm_and_asm) ... ok
test_04_ef0_negative_controls (__main__.TestWasmWasiSelfhost.test_04_ef0_negative_controls) ... ok

----------------------------------------------------------------------
Ran 4 tests in 25.641s

OK
[TEST_01 PASS] Dual-host (node:wasi & BrowserWasiShim) WASI primitives & ABI verified.
[TEST_02 PASS] WASI Self-Host Converged:
  zcc.wasm : 906110B SHA256=2d12f3e4fc02d2a0ef386d7c9e1b6ce57ac7e2f5d0b309e2149cb319de0ba309
  zcc2.wasm: 906110B SHA256=45d58c789998e499ff2da553db4d89ed43280f520ca384154c036247802d9e13
  zcc3.wasm: 906110B SHA256=45d58c789998e499ff2da553db4d89ed43280f520ca384154c036247802d9e13
  cmp zcc2.wasm zcc3.wasm: EXIT 0
[TEST_03 PASS] Pure-JS BrowserWasiShim: fib(10)=55 (.wasm 271B) & -S 2281B byte-identical to native ./zcc.
[TEST_04 PASS] EF-0 Negative Controls verified: invalid C rc=1 (no .wasm), 1-byte mutation caught by cmp/SHA-256, corrupt magic caught by WebAssembly.validate()=False.
```

---

## 5. Gates 1–5 & Baseline Regression Ledger

- **Gate 1 (`make selfhost` + `scripts/log_bootstrap_hash.sh`):**
  - `zcc_quest`: `child exit=0 wrapper_exit=0 oracle_verified=True warnings=36 errors=0 elapsed=121.2s`
  - `stage2.s == stage3.s`: `4e0fe0688f2c79ddbd636cddceacdaba0709b6be6aef2b74371ac4bc211c8cee` (`md5sum`: `05199293442d19c6521f892c7f997972`, `cmp zcc2.s zcc3.s` exit 0)
  - `stage2.bin == stage3.bin`: `2697cce748eb92ca5e59d15a9ccacc0f9a0d628fb6c2eb029e00ba5dbd38c0de`
  - Provenance ledger: `/mnt/h/__DOWNLOADS/zcc_github_upload/evidence/zcc-omega-1795100614-17218-4bef1f9cd4bb19e7/provenance.jsonl`
  - `E-LEARN-017` Ledger Update: WSL2 MD5 `05199293442d19c6521f892c7f997972` **OBSERVED** locally; matching CI policy rows appended to `BOOTSTRAP_BASELINES.tsv` (Azure rows are CI baseline lock entries not locally executed on Azure). `bash scripts/log_bootstrap_hash.sh` exited `0` (`BOOTSTRAP_HASH_EXIT=0`).
- **Sovereign Linux (`make selfhost-sovereign`):** `cmp /tmp/zcc2_sov.o /tmp/zcc3_sov.o` (`text_seg.size=1732588, data_seg.size=169056, labels=68739, relocs=93518`), `/tmp/sov_fp3`, and `/tmp/sov_abi3` all passed (`[PASS] selfhost-sovereign: byte-identical zero-binutils object & ELF emission verified.`).
- **Gate 2 / Gate 3 / Optimizer Gauntlet (`python3 tools/test_zcc_opt_passes.py`):** `13/13` bit-exact passes, `0` semantic divergences, `88,212 -> 56,799` lines (`-31,413`, `35.61%` reduction).
- **Direct ELF Suite (`python3 tests/test_direct_elf_emit.py`):** `Ran 12 tests in 7.951s — OK`.
- **WASM Emitter (`python3 tests/test_wasm_emitter.py`):** `Ran 2 tests in 0.325s — OK`.
- **WASM WASI Suite (`python3 tests/test_wasm_wasi.py`):** `Ran 12 tests in 1.069s — OK`.
- **Gate 5 Prior Gate Re-Verification (`tickets/CORE-06-compiler-parts-hardening-gate-evidence.md`):** ARGON-4 6/6 suites + C99 UBSan/ASan differential oracle (`32 seeds, 64 UB-free C99 TUs, 192 executions, 0 divergences in 4.76s, SHA: a597e555c3cdbb9e...`) passed inside `make selfhost` (`=== [ARGON 4] ALL 6 VERIFICATION SUITES PASSED (0 REGRESSIONS) ===`).

---

## 6. Overall Adjudication

- **Claim:** All three Sovereign Self-Hosting tracks (ARM64/RV64 Direct ELF, Win64 Zero-Toolchain Self-Host, Browser & WASI Self-Host) plus Gates 1–5 regression suites pass without regressions.
- **Evidence Class:** `OBSERVED` (WSL2 Linux x86_64, Native Windows x64 via `cmd.exe`, V8 WebAssembly + Node `node:wasi` + `BrowserWasiShim`), with host binary emulator absences explicitly disclosed (`QEMU_STATUS: NOT_INSTALLED_ON_HOST`, `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST`).
- **Evidence Tier:** `E3` (for Linux x86_64, Win64 native PE, and Node/V8/BrowserWasiShim WASI self-host); `E2/E3` emulator-scoped (for ARM64/RV64 direct ELF pending host QEMU).
- **Overall Verdict:** **PASS**
