# Gate Evidence: Track 3 Win64 PE Multi-DLL Dynamic Import Binding

**Ticket**: `tickets/WIN64-PE-MULTI-DLL-DYNAMIC-BINDING.md`  
**Date**: 2026-09-28  
**Author**: ZCC Architecture & Toolchain Division  
**Target**: Track 3: Win64 PE dynamic multi-DLL binding (`msvcrt.dll` for `printf`, `puts`, `malloc`, `free`, `strlen`, `strcpy`, `strcmp`, `memcpy`, `memset`, math APIs; `USER32.dll` for `MessageBox`, `GetDesktopWindow`, `GetSystemMetrics`; `KERNEL32.dll` for `ExitProcess`, `Sleep`, `GetStdHandle`, `WriteFile`, etc.)  
**Files Modified**: `src/win64_pe_emit.h`, `src/win64_pe_emit.c`, `tests/test_win64_pe_emitter.py`, `BOOTSTRAP_BASELINES.tsv`  
**Core Diff**: 0 lines in `part0_pp.c`, `part3.c`, `part4.c`.  

---

## Executive Summary

Track 3 delivers native multi-DLL dynamic import table emission and symbol routing in the ZCC PE32+ backend. Without requiring external linkers (`link.exe`, `lld-link`, `ld`) or import libraries (`.lib`, `.a`), ZCC autonomously:
1. Classifies external symbols by DLL destination (`msvcrt.dll`, `USER32.dll`, `GDI32.dll`, `KERNEL32.dll`).
2. Emits an arbitrary count $D$ of `IMAGE_IMPORT_DESCRIPTOR` structures in `.idata` with unified multi-slice Import Address Tables (IAT) and Import Lookup Tables (ILT).
3. Connects each DLL's IAT slice to 32-byte Win64 ABI bridge stubs in `.text` with correct indirect RIP-relative dispatch displacements.
4. Executes live multi-DLL applications on Windows host, demonstrating simultaneous resolution of C runtime memory & I/O (`malloc`, `free`, `printf`, `puts`, `strlen`, `strcmp`, `memcpy`), Win32 GUI metrics (`GetDesktopWindow`, `GetSystemMetrics`), and kernel process controls (`ExitProcess`, `Sleep`).

---

## Gate Execution Summary

### Gate 1 — Self-Host Identity (Mandatory)
- **Command**: `cmp zcc2.s zcc3.s && md5sum zcc2.s zcc3.s`
- **Output**:
```text
1946222d441f3e038f7bcb2d43fb855c  zcc2.s
1946222d441f3e038f7bcb2d43fb855c  zcc3.s
```
- **Verdict**: PASS (Bit-identical self-host fixed point confirmed, 0 drift from locked ledger hash)

### Gate 2 — Optimization Passes Gauntlet (Mandatory)
- **Command**: `python3 tools/test_zcc_opt_passes.py`
- **Output**:
```text
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
- **Verdict**: PASS (19/19 suites bit-exact, 0 divergences)

### Gate 3 — Foundational Core Diff Budget (Mandatory)
- **Command**: `git diff bcd03ee1b -- part0_pp.c part1.c part2.c part3.c part4.c part5.c`
- **Output**:
```text
(0 lines modified across all foundational compiler parts)
```
- **Verdict**: PASS (0 lines touched in core parts; diff strictly bounded to modular PE emitter and tests)

### Gate 4 — Target Unit Test Suite & Host Execution (Mandatory)
- **Command**: `python -m unittest -v tests/test_win64_pe_emitter.py`
- **Output**:
```text
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
test_07_win64_pe_import_directory_headers (tests.test_win64_pe_emitter.TestWin64PEEmitter.test_07_win64_pe_import_directory_headers)
Verify emitted PE binary has valid DataDirectory[1] (Import) and DataDirectory[12] (IAT). ... ok
test_08_windows_host_dynamic_dll_binding_execution (tests.test_win64_pe_emitter.TestWin64PEEmitter.test_08_windows_host_dynamic_dll_binding_execution)
Verify dynamic execution of GetStdHandle, WriteFile, and ExitProcess on Windows host. ... ok
test_09_multi_api_heap_and_sleep_execution (tests.test_win64_pe_emitter.TestWin64PEEmitter.test_09_multi_api_heap_and_sleep_execution)
Verify dynamic execution of VirtualAlloc, VirtualFree, Sleep, and normal main return 79. ... ok
test_10_multi_dll_msvcrt_printf_malloc (tests.test_win64_pe_emitter.TestWin64PEEmitter.test_10_multi_dll_msvcrt_printf_malloc)
Verify multi-DLL PE import directory with msvcrt.dll and KERNEL32.dll dynamic binding. ... ok
test_11_multi_dll_three_libraries_user32 (tests.test_win64_pe_emitter.TestWin64PEEmitter.test_11_multi_dll_three_libraries_user32)
Verify simultaneous multi-DLL binding across 3 libraries (KERNEL32, msvcrt, USER32). ... ok

----------------------------------------------------------------------
Ran 11 tests in 6.045s

OK
```
- **Verdict**: PASS (11/11 tests passing on Windows host)

### Gate 5 — Evidence Packaging
- Commands and logs archived under `docs/evidence/2026-09-28/win64_pe_multi_dll/`.
- **Verdict**: PASS
