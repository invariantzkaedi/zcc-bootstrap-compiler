# Ticket LIMIT-001 Gate Evidence — Extended GCC Inline Assembly

## Target Defect / Feature
- **Issue**: LIMIT-001 (Extended GCC Inline Assembly with Constraints)
- **Files**: `part1.c`, `part3.c`, `part4.c`, `zcc_inline_asm.h`, `tests/probe_inline_asm.c`, `BOOTSTRAP_BASELINES.tsv`
- **Baseline**: GREEN

## Root Cause
`part3.c` previously counted colons and discarded operand expressions into a basic string without capturing constraint bindings or target lvalues. `part4.c` lacked template placeholder substitution (`%0..%9`, `%k0`, `%%`), register allocation bindings, stack-isolated input evaluation, and write-back. Additionally, `variable_is_read()` in `part4.c` did not traverse `ND_ASM` operands, causing dead-code elimination (DCE) to elide variables solely read by inline assembly.

## Patch Architecture
- Created modular helper `zcc_inline_asm.h` with:
  - `parse_extended_asm_operands()`: parses outputs, inputs, clobbers into `AsmOperand` array.
  - `codegen_extended_asm()`: allocates physical registers, isolates inputs via stack push/pop, substitutes `%0..%9`, handles modifiers `%k`, `%q`, `%w`, `%b`, unescapes `%%` to `%`, and writes back outputs to local/global/lvalue destinations.
- Modified `part3.c` (< 50 lines diff) to wire operand parsing.
- Modified `part4.c` (< 50 lines diff) to invoke codegen and update `variable_is_read()` for DCE correctness.

## Gates Output

### Gate 1: Self-Host Identity
```text
$ cmp zcc2.s zcc3.s && echo 'CMP SUCCESS: byte-identical'
CMP SUCCESS: byte-identical

$ md5sum zcc2.s zcc3.s
4202b5e2ca1046c73f547946b766e84b  zcc2.s
4202b5e2ca1046c73f547946b766e84b  zcc3.s
```

### Gate 2: Optimization Pass Gauntlet
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

### Gate 4: QuickJS ES2020 Engine Harness
```text
$ /tmp/test_quickjs_runner
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

### Gate 5: Extended Inline Assembly Gauntlet (Rule EF-3)
```text
$ ./zcc tests/probe_inline_asm.c -o /tmp/probe_asm.s && gcc /tmp/probe_asm.s -o /tmp/probe_asm && /tmp/probe_asm
zcc: warning: using synthesized header stub for stdio.h
pp_process_include: stdio.h added 178 macros, total is 215
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (134 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
PROBE_ASM PASS: all 13 extended inline asm tests succeeded!
```
