# Ticket TRACK1-WIN64-PE Gate Evidence — Autonomous Windows PE32+ Direct .exe Cross-Compiler

**Date**: 2026-09-28  
**Target**: Track 1: Autonomous Windows PE32+ Direct `.exe` Cross-Compiler (`-target win64` / `src/win64_pe_emit.c`)  
**Files Modified**: `src/win64_pe_emit.h`, `src/win64_pe_emit.c`, `src/codegen.c`, `part5.c`, `BOOTSTRAP_BASELINES.tsv`, `tests/test_win64_pe_emitter.py`  
**Test Assets**: `tests/test_win64_pe_emitter.py` (6 unit tests), `test_win42.exe`, `test_win_calc.exe`, `auto_win42.exe`  
**Baseline**: GREEN  
**Status**: 100% PASS (Gate 1, Gate 2, Gate 3, Gate 4, Gate 5)  

---

## 1. Target Defect & Invariant Violated

Under standard cross-compilation workflows, developers on Linux/WSL or cross-platform environments need to compile standard C source files directly into standalone Windows PE32+ 64-bit `.exe` executables without requiring external cross-toolchains (`x86_64-w64-mingw32-gcc`), external linkers, or wine.

Prior to this resolution:
1. **Missing Direct PE32+ Assembler & Emitter**: While ZCC had an ELF-64 object and executable emitter (`src/elf_emit.c`, `src/zld.c`), it lacked an autonomous PE32+ COFF executable generator capable of outputting valid portable executables for Windows AMD64.
2. **Missing Dynamic Entry Point Resolution**: `src/win64_pe_emit.c` had fixed entry point assumptions rather than computing the relative offset of `main` from the code buffer.
3. **Missing Driver Target Wiring**: `src/codegen.c` and `part5.c` lacked `-target win64`, `--target=win64`, `-win64`, `--win64`, `-pe64`, and automatic `.exe` extension target inference.
4. **Bootstrap Link Missing Unit**: In `part5.c` line 2782, the link command for `zcc.c` omitted `src/win64_pe_emit.c`, `src/arm64_codegen.c`, and `src/riscv_codegen.c`, triggering undefined reference errors during Stage 2 bootstrap linking.

---

## 2. Patch Architecture & Minimal Changes

- **`src/win64_pe_emit.h` & `src/win64_pe_emit.c`**:
  - Added `zcc_emit_win64_pe_file_ex(filename, code_bytes, code_len, entry_offset)` to synthesize PE32+ binary with dynamic entry point `opt_hdr.AddressOfEntryPoint = 0x1000 + entry_offset`.
  - Configured `.text` section characteristics with `0xE0000020` (`IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE`) to permit in-place literal reads and global data manipulation.
  - Retained `zcc_emit_win64_pe_file` as an inline backward-compatible wrapper passing `entry_offset = 0`.
- **`src/codegen.c`**:
  - Implemented target argument parsing for `-target win64`, `--target=win64`, `-win64`, `--win64`, `-pe64`, and auto-detection when the output file has `.exe` extension.
  - In `assemble()`: resolved symbol offsets, calculated the offset of `main`, resolved PC-relative `.data` displacements, and called `zcc_emit_win64_pe_file_ex()`.
  - In top-level driver `main()`: stripped Windows target flags when forwarding to `zcc_main()` in-memory assembly stream, assembling directly into PE32+ `.exe`.
- **`part5.c`**:
  - Integrated `src/arm64_codegen.c src/riscv_codegen.c src/win64_pe_emit.c` into line 2782 Stage 2 link command string.
  - Net diff in `part5.c`: exactly 1 line modified (+1, -1), far beneath the 50-line limit.
- **Foundational Core Diff Budget**:
  - `part0_pp.c`: 0 lines modified.
  - `part3.c`: 0 lines modified.
  - `part4.c`: 0 lines modified.

---

## 3. Verification Gates & Raw Command Evidence

### Gate 1: Self-Host Identity (Byte-Identical Fixed Point)
```text
$ make selfhost
★ ZKAEDI PRIME FIXED POINT REPO - H0 CONVERGED - exit 0 (scars: 0, ⟐ BYTE-IDENTICAL SEAL ⟐) ★
✓ Gate 1 PASS: Assembly byte-identical (cmp stage2.s stage3.s).
[Semantic Oracle] Cross-verifying against GCC & Golden Oracle... [PASS: Byte-Identical Semantic Parity]
[Cross-TU ABI Oracle] Bidirectional Interop (MV1.2, MV1.3a & MV1.3b)... [PASS: Triple-Corpus Bidirectional Parity]

$ cmp zcc2.s zcc3.s
CMP_IDENTICAL_EXIT_0

$ md5sum zcc2.s zcc3.s
1946222d441f3e038f7bcb2d43fb855c  zcc2.s
1946222d441f3e038f7bcb2d43fb855c  zcc3.s
```
**Verdict**: PASS

### Gate 2: Production Optimization Pass Gauntlet
```text
$ python3 tools/test_zcc_opt_passes.py
==============================================================================
  ZCC MULTI-SUITE PRODUCTION OPTIMIZATION PASS GAUNTLET
  Testing: GVN Store-to-Load, Constant Fold, DCE & Symbolic CFG
==============================================================================
Target Test Suite          Baseline   Optimized  Reduction    Verdict
------------------------------------------------------------------------------
test_abi                   402        389        -13 (3.2%)   [PASS]
test_asm_real              39         39         -0 (0.0%)    [PASS]
test_vla                   271        264        -7 (2.6%)    [PASS]
exp1_raytracer             5594       5421       -173 (3.1%)  [PASS]
exp2_voxel                 5368       5223       -145 (2.7%)  [PASS]
exp3_audio                 6239       6098       -141 (2.3%)  [PASS]
exp4_vr_stereo             5479       5351       -128 (2.3%)  [PASS]
exp5_physics               40006      39135      -871 (2.2%)  [PASS]
t_rigging_regressions      2087       2027       -60 (2.9%)   [PASS]
diff_read_escape           2354       2289       -65 (2.8%)   [PASS]
diff_parse_initializer_list 9025       8870       -155 (1.7%)  [PASS]
diff_HUlib_drawTextLine    2632       2565       -67 (2.5%)   [PASS]
diff_R_DrawColumn          2917       2845       -72 (2.5%)   [PASS]
diff_my_sha256_final       1632       1598       -34 (2.1%)   [PASS]
diff_PIT_CheckThing        796        772        -24 (3.0%)   [PASS]
diff_cf_socket_open        803        786        -17 (2.1%)   [PASS]
diff_addsfx                1558       1512       -46 (3.0%)   [PASS]
diff_P_SpawnMapThing       1140       1113       -27 (2.4%)   [PASS]
diff_cr_eob_read           1180       1158       -22 (1.9%)   [PASS]
------------------------------------------------------------------------------
GAUNTLET SUMMARY:
  * Suites Evaluated:          19
  * Bit-Exact Passes:          19/19
  * Semantic Divergences:      0
  * Total Assembly (Baseline): 89,522 lines
  * Total Assembly (Optimized):87,455 lines
  * Net Instructions Elided:   2,067 lines (2.31% net reduction)
==============================================================================
★ ALL 19 TEST SUITES BIT-EXACT & VERIFIED (EXIT 0) ★
```
**Verdict**: PASS

### Gate 3: Foundational Line Budget Audit
```text
$ git diff --stat part0_pp.c part3.c part4.c
# (empty output — 0 lines changed)

$ git diff --stat part5.c
 part5.c | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
```
**Verdict**: PASS

### Gate 4: Target Unit Tests & Native Windows Host Execution
```text
$ python -m unittest -v tests/test_win64_pe_emitter.py
test_01_build_harness (tests.test_win64_pe_emitter.TestWin64PEEmitter.test_01_build_harness)
Verify Win64 PE emitter source exists and has clean prototypes. ... ok
test_02_pe_alignment_and_file_emission (tests.test_win64_pe_emitter.TestWin64PEEmitter.test_02_pe_alignment_and_file_emission)
Validates PE alignment math and binary structures. ... ok
test_03_direct_zcc_win64_compilation_and_headers (tests.test_win64_pe_emitter.TestWin64PEEmitter.test_03_direct_zcc_win64_compilation_and_headers)
Verify ./zcc input.c -target win64 -o app.exe emits valid PE32+ headers. ... ok
test_04_windows_host_execution_exit_code (tests.test_win64_pe_emitter.TestWin64PEEmitter.test_04_windows_host_execution_exit_code)
Verify emitted PE32+ executable runs directly and returns exit code 42. ... ok
test_05_multi_function_recursive_execution (tests.test_win64_pe_emitter.TestWin64PEEmitter.test_05_multi_function_recursive_execution)
Verify multi-function program with arithmetic & recursion compiles and executes with code 43. ... ok
test_06_target_auto_detection_from_exe (tests.test_win64_pe_emitter.TestWin64PEEmitter.test_06_target_auto_detection_from_exe)
Verify -o app.exe automatically enables PE32+ emission without explicit -target flag. ... ok

----------------------------------------------------------------------
Ran 6 tests in 0.758s

OK
```
**Verdict**: PASS

### Gate 5: Ledger Attestation & Evidence Freshness
All verification gates executed cleanly in the same turn. Artifacts and logs preserved in `docs/evidence/2026-09-28/win64_pe_emitter/`. Hash `1946222d441f3e038f7bcb2d43fb855c` locked in `BOOTSTRAP_BASELINES.tsv`.  
**Verdict**: PASS
