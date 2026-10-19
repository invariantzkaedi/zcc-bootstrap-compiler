# TICKET: IR-WHITELIST-BATCH9-SSA-PROMOTION
**Subsystem:** Codegen / SSA-IR Lowering & Self-Host Optimization  
**Target:** ZCC Self-Host Pipeline & Pure SSA Lowering Frontier  
**Author:** Antigravity / ZCC Systems Engineering  
**Date:** 2026-10-19  
**Status:** **RESOLVED & VERIFIED (Gate 1 Byte-Identical, Gate 2 19/19 Optimization Gauntlet, Gate 4 QuickJS 15/15 PASS)**  
**Minimal Diff Line Count:** `part4.c` (+5 lines, strictly < 50 lines)

---

## 1. Goal & Architectural Promotion Vector

Advance the core ZCC compiler towards pure SSA self-hosting by safely promoting internal compiler core routines from AST-direct emission into the SSA-IR lowering backend (`zcc_run_passes_emit_body_pgo`).

Prior batches established:
- Batch 1-2: Test fixtures (`fold_test`, `dce_test`, `licm_test`, `test_amf`)
- Batch 3a/3c: Primitive enums & lexer predicates (`is_digit`, `is_space`, `is_alpha`, `is_alnum`, `hex_val`)
- Batch 4-5: AST node accessors & type queries (`node_kind`, `node_lhs`, `type_size`, `is_pointer`)
- Batch 6: Memory allocator & constructor helpers (`type_new`, `node_new`, `cc_strdup`)
- Batch 7-8: Token classification & math primitives (`peek_token`, `log2_of`, `guard_node`)
- Split Lexer: Fortified character decoders (`read_char`, `read_escape`, `lex_char`, `lex_operator`)

**Batch 9 Promotion Scope (14 Core Functions):**
1. `"symbol_alignment"` (`part1.c`) — System V symbol alignment evaluator
2. `"alignment_log2"` (`part1.c`) — Power-of-two exponent calculation via bit-shift loop
3. `"node_stmts"` (`part1.c`) — AST compound statement accessor
4. `"node_is_float"` (`part1.c`) — Floating-point type discriminator
5. `"node_is_char_or_void_ptr_cast"` (`part1.c`) — Pointer cast predicate
6. `"is_complex_type"` (`part2.c`) — C99 complex arithmetic classifier
7. `"is_compatible_type"` (`part2.c`) — Recursive type compatibility validator
8. `"is_unsigned_cmp"` (`part3.c`) — Unsigned comparison discriminator
9. `"abi_join"` (`part4.c`) — System V AMD64 ABI classification join lattice
10. `"is_nullable_call"` (`part5.c`) — Static analyzer nullable library call probe
11. `"is_null_check"` (`part5.c`) — Static analyzer register null-check detector
12. `"is_rax_deref"` (`part5.c`) — Unbounded accumulator memory dereference probe
13. `"is_rax_moved"` (`part5.c`) — Register lifetime movement probe
14. `"type_to_str"` (`part5.c`) — Static type kind string converter

---

## 2. Probe Before Patch Evidence (Rule EF-3 & EF-2)

- **Probe File:** `tests/probe_ssa_promotion.c`
- **Pre-Promotion Codegen (`/tmp/probe_before.s`):**
  - AST-direct codegen emitted 320-byte stack frames with standard stack push/pop sequences (`pushq %rax` / `popq %rax`).
- **Post-Promotion Codegen (`/tmp/probe_after.s`):**
  - Emitted SSA basic blocks with PGO block placement (`.Lir_b_107_0`, `.Lir_b_107_2` ...).
  - Redundant loads eliminated (RLE) and address modes folded (AMF).
  - Function body compiled through SSA-IR with register allocation preserving callee-saved registers (`%rbx`, `%r12`–`%r15`).
  - Execution test passed: `ALL PROBE CHECKS PASSED`.

---

## 3. Verification Gates (Rule EF-1, EF-2 & EF-6)

### Gate 1 — Self-Host Identity (Mandatory)
- **Command:** `cmp zcc2.s zcc3.s`
- **Output:** Exit Code 0 (Byte-Identical)
- **Assembly Hashes:**
  ```text
  9d5fee36290aa712530e0f33f616d3fc  zcc2.s
  9d5fee36290aa712530e0f33f616d3fc  zcc3.s
  ```
- **Line Count:** 492,024 lines
- **Verdict:** **PASS**

### Gate 2 — Multi-Suite Production Optimizer Gauntlet
- **Command:** `python3 tools/test_zcc_opt_passes.py`
- **Output:**
  ```text
  Target Test Suite          Baseline   Optimized  Reduction    Verdict
  ------------------------------------------------------------------------------
  test_abi                   435        417        -18 (4.1%)   [PASS]
  test_asm_real              46         46         -0 (0.0%)    [PASS]
  test_vla                   298        276        -22 (7.4%)   [PASS]
  exp1_raytracer             5873       5449       -424 (7.2%)  [PASS]
  exp2_voxel                 5587       5042       -545 (9.8%)  [PASS]
  exp3_audio                 6344       5863       -481 (7.6%)  [PASS]
  exp4_vr_stereo             5693       5152       -541 (9.5%)  [PASS]
  exp5_physics               40438      36234      -4204 (10.4%) [PASS]
  t_rigging_regressions      2195       2047       -148 (6.7%)  [PASS]
  diff_read_escape           2417       2127       -290 (12.0%) [PASS]
  diff_parse_initializer_list 9426      8682       -744 (7.9%)  [PASS]
  diff_HUlib_drawTextLine    2704       2422       -282 (10.4%) [PASS]
  diff_R_DrawColumn          2998       2693       -305 (10.2%) [PASS]
  diff_my_sha256_final       1671       1447       -224 (13.4%) [PASS]
  diff_PIT_CheckThing        835        716        -119 (14.3%) [PASS]
  diff_cf_socket_open        836        730        -106 (12.7%) [PASS]
  diff_addsfx                1603       1321       -282 (17.6%) [PASS]
  diff_P_SpawnMapThing       1173       960        -213 (18.2%) [PASS]
  diff_cr_eob_read           1213       1079       -134 (11.0%) [PASS]
  ------------------------------------------------------------------------------
  GAUNTLET SUMMARY:
    * Suites Evaluated:          19
    * Bit-Exact Passes:          19/19
    * Semantic Divergences:      0
    * Total Assembly (Baseline): 91,785 lines
    * Total Assembly (Optimized):82,703 lines
    * Net Instructions Elided:   9,082 lines (9.89% net reduction)
  ★ ALL 19 TEST SUITES BIT-EXACT & VERIFIED (EXIT 0) ★
  ```
- **Verdict:** **PASS**

### Gate 3 — Boundary Diff Audit (Rule EF-5)
- `part4.c`: +5 lines (whitelisted function array entries)
- Diff bounds strictly preserved (< 50 lines on foundational files).
- **Verdict:** **PASS**

### Gate 4 — Target Harness (QuickJS ES2020 15/15 Suite)
- **Command:** `/tmp/test_quickjs_runner`
- **Output:**
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
- **Verdict:** **PASS**

### Gate 5 — Evidence Freshness & Baseline Ledger Audit (Rule E-LEARN-017)
- Appended verified MD5 hash `9d5fee36290aa712530e0f33f616d3fc` to `BOOTSTRAP_BASELINES.tsv` across WSL2 and Azure runner profiles.
- **Verdict:** **PASS**
