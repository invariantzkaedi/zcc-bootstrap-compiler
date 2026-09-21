# Gate Evidence: LIMIT-006 (Dual-Register Struct Return in SSA-IR: `%rax/%rdx`)

**Date**: 2026-09-21  
**Target**: System V AMD64 Aggregate Return Compliance ($\le 16$ bytes) in SSA-IR (`compiler_passes.c` & `ir_to_x86.c`)  
**Status**: 100% PASS (Gate 1, Gate 2, Gate 4, Gate 5)  

---

## Invariant Violated & Root Cause

Under the System V AMD64 ABI (§3.2.3), aggregate values $\le 16$ bytes classified as `INTEGER` must return the low 8 bytes in `%rax` and the high 8 bytes in `%rdx`.

In ZCC's IR pipelines:
1. `compiler_passes.c` lowered `ZND_RETURN` to `OP_RET` with only 1 source operand and lowered `OP_RET` by loading only `%rax`. `OP_CALL` and `ZND_ASSIGN` only captured and stored `%rax`, corrupting high 8 bytes in `%rdx`.
2. `ir_to_x86.c` and `ir.h` only provided single-register `IR_RET`. In `part4.c`, returning small structs skipped IR emission entirely, causing functions in `--ir` mode to drop return nodes and fall through with uninitialized registers.
3. `is_ir_eligible` in `part4.c` defensively disqualified any struct-returning functions from IR passes to prevent corruption.

---

## Rule EF-3 Minimal Probe Evidence

Minimal probe: `tests/probe_ret2.c` returning `typedef struct { long a; long b; } Pair;`.

### Pre-Patch:
- Default: `PROBE_RET2 PASS: p.a=123456789, p.b=987654321`
- `ZCC_IR_LOWER=1`: `PROBE_RET2 FAIL: p.a=123456789, p.b=140154275916896` (corrupted `%rdx`)
- `--ir`: `PROBE_RET2 FAIL: p.a=140726364205944, p.b=134795472972528` (missing return terminator)

### Post-Patch:
- Default: `PROBE_RET2 PASS: p.a=123456789, p.b=987654321` (exit 0)
- `ZCC_IR_LOWER=1`: `PROBE_RET2 PASS: p.a=123456789, p.b=987654321` (exit 0)
- `--ir`: `PROBE_RET2 PASS: p.a=123456789, p.b=987654321` (exit 0)

---

## Gate 1: Self-Host Identity (Mandatory)

```text
wsl -e bash -c "cd /home/zkaedi/zcc_ext4_test && cmp zcc2.s zcc3.s && echo 'CMP SUCCESS: byte-identical'"
```

Raw Output:
```text
CMP SUCCESS: byte-identical
```

SHA-256 Checksums:
```text
0f8398bbed202e4175552f1f2bb35668d6653ff0917de30fb741ac401a8f8df6  zcc2.s
0f8398bbed202e4175552f1f2bb35668d6653ff0917de30fb741ac401a8f8df6  zcc3.s
```

Verdict: **PASS**

---

## Gate 2: Cross-Toolchain & Optimizer Interoperability

```text
wsl -e bash -c "cd /home/zkaedi/zcc_ext4_test && python3 tools/test_zcc_opt_passes.py"
```

Raw Output:
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

Verdict: **PASS**

---

## Gate 3: Corpus Regression (797-function diff)

Verdict: **NOT-APPLICABLE**  
Reason: `part0_pp.c` and `part3.c` were unmolested. `part4.c` was surgically modified (33 lines diff, well below 50-line budget).

---

## Gate 4: Target Harness (QuickJS ES2020 Execution & Probe)

```text
wsl -e bash -c "cd /home/zkaedi/zcc_ext4_test && /tmp/test_quickjs_runner"
```

Raw Output:
```text
===============================================================
  ZCC NATIVE QUICKJS ES2020 TEST HARNESS
===============================================================

--- Test Suite 1: Arithmetic & IEEE-754 Floats ---
  [PASS] 1 + 2 * 3 == 7
  [PASS] Math.hypot(3, 4) == 5
  [PASS] Math.sin(0) == 0
  [PASS] Math.PI > 3.14 && Math.PI < 3.15 == true
  [PASS] Number.MAX_SAFE_INTEGER.toString() == 9007199254740991

--- Test Suite 2: Objects, Arrays & JSON ---
  [PASS] JSON.stringify({ a: 42, b: 'hello' }) == {"a":42,"b":"hello"}
  [PASS] [1, 2, 3, 4, 5].map(x => x * x).reduce((a, b) => a + b, 0) == 55
  [PASS] ['apple', 'banana', 'cherry'].join('-') == apple-banana-cherry

--- Test Suite 3: Closures, Loops & Functions ---
  [PASS] (() => { let s = 0; for (let i = 1; i <= 10; i++) s += i; return s; })() == 55
  [PASS] function fib(n) { return n <= 1 ? n : fib(n-1) + fib(n-2); }; fib(10) == 55

--- Test Suite 4: RegExp & Strings ---
  [PASS] 'quickjs-2024'.replace(/([a-z]+)-([0-9]+)/, '$2-$1') == 2024-quickjs
  [PASS] /^[a-z0-9_]+$/i.test('zcc_2026') == true

--- Test Suite 5: ES6 Classes & Prototypes ---
  [PASS] class Point { constructor(x, y) { this.x = x; this.y = y; } norm2() { return this.x*this.x + this.y*this.y; } }; new Point(3, 4).norm2() == 25

--- Test Suite 6: Date & BigInt ---
  [PASS] new Date(0).toISOString() == 1970-01-01T00:00:00.000Z
  [PASS] (2n ** 64n).toString() == 18446744073709551616

===============================================================
  ALL QUICKJS TESTS PASSED CLEANLY! (Failures: 0)
===============================================================
```

Verdict: **PASS**

---

## Gate 5: Evidence Freshness

All verification gates freshly executed on live tree in WSL ext4 test harness.

Verdict: **PASS**
