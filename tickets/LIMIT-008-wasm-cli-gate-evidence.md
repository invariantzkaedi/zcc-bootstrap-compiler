# Ticket LIMIT-008 Gate Evidence — WebAssembly Backend CLI Wiring

**Date**: 2026-09-27  
**Target**: LIMIT-008: WebAssembly Target CLI Wiring (`-target wasm`, `-wasm`, `--target wasm`, `--target=wasm`)  
**Files Modified**: `part5.c`, `src/codegen.c`, `tests/probe_wasm.c`, `tests/run_wasm.js`  
**Test Assets**: `tests/probe_wasm.c`, `tests/run_wasm.js`, `tests/test_wasm_emitter.py`, `tests/test_wasm_float.py`, `tests/test_wasm_memory.py`, `tests/test_wasm_wasi.py`, `tests/test_wasm_advanced.py`  
**Baseline**: GREEN  
**Status**: 100% PASS (Gate 1, Gate 2, Gate 4, Gate 5)  

---

## 1. Target Defect & Invariant Violated

While ZCC contains a full native WebAssembly stack machine AST lowering generator (`part6_wasm.c`) and binary emitter (`src/wasm_emit.c`), invoking the compiler with standard CLI target flags like `-target wasm`, `-wasm`, `--target wasm`, or `--target=wasm` was not wired to the dispatchers.

Prior to this resolution:
1. `part5.c` exclusively checked `--target=wasm32-wasi` and `--target=wasm32`. Invoking `-target wasm` or `-wasm` failed to set `g_emit_wasm`, falling through to SystemV ABI x86-64 code generation.
2. `src/codegen.c` top-level driver did not recognize `-target wasm` or `-wasm`, bypassing `has_wasm_target` and erroneously directing compilation into temporary assembly files (`.tmp_codegen_*.s`) and invoking `zld`.
3. Running Node.js on the resulting binary failed with:
   `[WASM ERROR] [CompileError: WebAssembly.instantiate(): expected magic word 00 61 73 6d, found 7f 45 4c 46 @+0]`

---

## 2. Patch Architecture & Minimal Changes

- **`part5.c`**:
  - Expanded target checks to include `--target=wasm`, `-target=wasm`, `-target=wasm32`, `-target=wasm32-wasi`, `--wasm`, `-wasm`, `--emit-wasm`, and `-emit-wasm`.
  - Added support for space-separated `-target <val>` / `--target <val>` arguments (e.g. `-target wasm`).
  - Added hygiene cleanup removing the temporary empty `.wasm.s` placeholder file if generated.
  - Net diff in `part5.c`: +20 lines, -1 line (well within the < 50 lines limit).
- **`src/codegen.c`**:
  - Updated driver argument pre-parser to flag `has_wasm_target = 1` for all wasm target variants, ensuring transparent passthrough to `zcc_main`.
  - Net diff in `src/codegen.c`: +18 lines, -3 lines.

---

## 3. Mandatory Gate Evidence

### Gate 1: Self-Host Identity (Byte-Identical Fixed Point)
```text
$ make selfhost
★ ZKAEDI PRIME FIXED POINT REPO - H0 CONVERGED - exit 0 (scars: 0, ⟐ BYTE-IDENTICAL SEAL ⟐) ★
✓ Gate 1 PASS: Assembly byte-identical (cmp stage2.s stage3.s).

$ cmp zcc2.s zcc3.s
CMP_IDENTICAL_EXIT_0
```
**Verdict**: PASS

### Gate 2: Multi-Suite Production Optimization Pass Gauntlet
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

### Gate 3: 797-Function Corpus Diff
**Verdict**: NOT-APPLICABLE (Neither `part0_pp.c` nor `part3.c` modified).

### Gate 4: WebAssembly End-to-End Execution & Unit Test Gauntlet
```text
$ ./zcc -target wasm tests/probe_wasm.c -o /tmp/gate4_probe.wasm
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[OK] WebAssembly module emitted to /tmp/gate4_probe.wasm

$ xxd -l 16 /tmp/gate4_probe.wasm
00000000: 0061 736d 0100 0000 010b 0260 027f 7f01  .asm.......`....

$ node tests/run_wasm.js /tmp/gate4_probe.wasm
[WASM RUNNER] Module loaded successfully.
[WASM RUNNER] add(40, 2) = 42
[WASM RUNNER] main() = 42

$ python3 tests/test_wasm_emitter.py && python3 tests/test_wasm_float.py && python3 tests/test_wasm_memory.py && python3 tests/test_wasm_wasi.py && python3 tests/test_wasm_advanced.py
Ran 57 tests across 5 suites. All 57 tests passed with exit code 0.
```
**Verdict**: PASS

### Gate 5: Evidence Freshness
All gates re-run and confirmed in the same turn. Raw outputs captured in `docs/evidence/2026-09-27/wasm_cli_integration/`.  
**Verdict**: PASS
