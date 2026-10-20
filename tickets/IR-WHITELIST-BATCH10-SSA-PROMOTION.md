# TICKET: IR-WHITELIST-BATCH10-SSA-PROMOTION
**Subsystem:** Codegen / SSA-IR Lowering & Self-Host Optimization  
**Target:** ZCC Self-Host Pipeline & Pure SSA Lowering Frontier  
**Author:** Antigravity / ZCC Systems Engineering  
**Date:** 2026-10-19  
**Status:** **RESOLVED & VERIFIED (Gate 1 Byte-Identical, Gate 2 19/19 Optimization Gauntlet, Gate 4 QuickJS 15/15 PASS)**  
**Minimal Diff Line Count:** `part4.c` (+5 lines, strictly < 50 lines)

---

## 1. Goal & Architectural Promotion Vector

Advance the core ZCC compiler towards pure SSA self-hosting by safely promoting internal compiler routines across preprocessor, runtime verification, and AST inspection into the SSA-IR lowering backend (`zcc_run_passes_emit_body_pgo`).

Prior batches established:
- Batch 1-2: Test fixtures (`fold_test`, `dce_test`, `licm_test`, `test_amf`)
- Batch 3a/3c: Primitive enums & lexer predicates (`is_digit`, `is_space`, `is_alpha`, `is_alnum`, `hex_val`)
- Batch 4-5: AST node accessors & type queries (`node_kind`, `node_lhs`, `type_size`, `is_pointer`)
- Batch 6: Memory allocator & constructor helpers (`type_new`, `node_new`, `cc_strdup`)
- Batch 7-8: Token classification & math primitives (`peek_token`, `log2_of`, `guard_node`)
- Split Lexer: Fortified character decoders (`read_char`, `read_escape`, `lex_char`, `lex_operator`)
- Batch 9: Compiler Core & Type Resolution (`symbol_alignment`, `alignment_log2`, `node_stmts`, `node_is_float`, `node_is_char_or_void_ptr_cast`, `is_complex_type`, `is_compatible_type`, `is_unsigned_cmp`, `abi_join`, `is_nullable_call`, `is_null_check`, `is_rax_deref`, `is_rax_moved`, `type_to_str`)

**Batch 10 Promotion Scope (14 Preprocessor, Verification & AST Functions):**
1. `"node_float_bits"` (`part1.c`) — raw IEEE-754 bit extraction (`long long` return, scalar $\le 8$B)
2. `"node_label_name"` (`part1.c`) — AST node label name buffer copy (`void` return)
3. `"is_ident_start"` (`part0_pp.c`) — identifier start character predicate (`int` return)
4. `"is_ident_char"` (`part0_pp.c`) — identifier character predicate (`int` return)
5. `"is_simd_stub"` (`part0_pp.c`) — SIMD header discriminator (`int` return)
6. `"is_curl_stub"` (`part0_pp.c`) — Curl header discriminator (`int` return)
7. `"is_stddef_stub"` (`part0_pp.c`) — standard header discriminator (`int` return)
8. `"is_empty_fallback_safe"` (`part0_pp.c`) — fallback safety predicate (`int` return)
9. `"verify_shared_access"` (`part2.c`) — shared state concurrency verifier (`int` return)
10. `"verify_job_ownership"` (`part2.c`) — worker ownership validator (`int` return)
11. `"verify_runtime_policy"` (`part2.c`) — runtime opcode boundary checker (`int` return)
12. `"verify_manifest_abi"` (`part2.c`) — manifest ABI version checker (`int` return)
13. `"verify_opcode_registry"` (`part2.c`) — opcode registry integrity check (`int` return)
14. `"node_is_array"` (`part1.c`) — AST node array type accessor (`int` return)
15. `"node_has_label"` (`part4.c`) — pure recursive AST label probe (`int` return)

*(Note: `force_truncate` was evaluated in probe under Rule EF-3 and excluded due to `compiler_passes.c` integer cast transparent passthrough semantics, preserving correctness)*.

---

## 2. Probe Before Patch Evidence (Rule EF-3 & EF-2)

- **Probe File:** `tests/probe_batch10_ssa.c`
- **Pre-Promotion Codegen (`/tmp/probe_batch10_before.s`):**
  - AST-direct codegen emitted standard stack push/pop sequences with `0` `.Lir_b_` basic blocks.
- **Post-Promotion Codegen (`/tmp/probe_batch10_after.s`):**
  - Emitted SSA basic blocks with PGO block placement (`321` `.Lir_b_` labels detected).
  - Redundant loads eliminated (RLE), address modes folded (AMF), GVN redundant operations eliminated.
  - Execution test passed: `ALL BATCH 10 PROBE CHECKS PASSED` (exit 0).

---

## 3. Verification Gates (Rule EF-1, EF-2 & EF-6)

### Gate 1 — Self-Host Identity (Mandatory)
- **Command:** `cmp zcc2.s zcc3.s`
- **Output:** Exit Code 0 (Byte-Identical)
- **Assembly Hashes:**
  ```text
  becaea8339cb7f3bb0d5d07c8e5cf787  zcc2.s
  becaea8339cb7f3bb0d5d07c8e5cf787  zcc3.s
  ```
- **Line Count:** 493,351 lines
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
  ==============================================================================
  ★ ALL 19 TEST SUITES BIT-EXACT & VERIFIED (EXIT 0) ★
  ```
- **Verdict:** **PASS**

### Gate 3 — Boundary Diff Audit
- **Authorized File:** `part4.c`
- **Diff:** +5 lines, -0 lines (net +5 lines, strictly < 50 lines)
- **Verdict:** **PASS**

### Gate 4 — Target Harness (QuickJS ES2020)
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

### Gate 5 — Evidence Freshness & Ledger Synchronization
- Updated `BOOTSTRAP_BASELINES.tsv` with MD5 `becaea8339cb7f3bb0d5d07c8e5cf787` (493,351 lines).
- Updated `ZCC_STATUS.md` with Batch 10 milestone.
- Preserved evidence under `docs/evidence/2026-10-19/ir_whitelist_batch10_ssa/`.
- **Verdict:** **PASS**
