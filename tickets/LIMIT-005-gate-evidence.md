# Ticket LIMIT-005 Gate Evidence — Raw Glibc System Header Ingestion

**Date**: 2026-09-22  
**Target**: Raw GNU glibc System Header Ingestion (`/usr/include/stdio.h`, `/usr/include/stdlib.h`) under Ubuntu 24.04  
**Files**: `part0_pp.c`, `part3.c`, `part5.c`, `zcc_glibc_compat.h`, `tests/test_glibc_dual_headers.c`, `BOOTSTRAP_BASELINES.tsv`  
**Baseline**: GREEN  
**Status**: 100% PASS (Gate 1, Gate 2, Gate 3, Gate 4, Gate 5)

---

## 1. Target Defect & Invariant Violated

Under standard Linux development workflows, code directly imports standard system headers (e.g. `#include </usr/include/stdio.h>`).
In previous versions of ZCC:
1. GNU system headers failed during preprocessing due to unabsorbed compiler-internal GNU builtins (`__builtin_bswap16/32/64`, `__glibc_likely`, `__glibc_unlikely`, `__builtin_va_arg_pack`) and macro expansions.
2. glibc internal types (`__gnuc_va_list`, `__off_t`, `__off64_t`, `__ssize_t`, `__uint32_t`, `__int32_t`, etc.) were missing from base types.
3. glibc function declarations employ GNU assembly aliases (e.g. `extern int rename (...) __asm__ ("__rename") ...;`), which caused parser syntax errors in forward declarations.
4. `FILE` was declared as `void*`, whereas glibc headers declare `struct _IO_FILE; typedef struct _IO_FILE FILE;`, causing type conflicts.

---

## 2. Patch Architecture & Line Budget Discipline

- **Modular Tolerator (`zcc_glibc_compat.h`)**:
  - Implements `zcc_glibc_init_macros()` to absorb GNU builtins (`__builtin_bswap16/32/64`, `__glibc_likely`, `__glibc_unlikely`) and attributes (`__wur`, `__leaf`, `__cold`, `__hot`, `__returns_nonnull`, `__attribute_alloc_size__`, `__attribute_format_arg__`).
  - Resizes macro body allocation safely with `zcc_set_macro_body()`.
- **Preprocessor (`part0_pp.c`)** (38 additions, 1 deletion; 39 lines diff, well below 50-line budget):
  - Adds glibc internal types to base types.
  - Adds `--raw-glibc` flag and `ZCC_RAW_GLIBC` environment check.
  - Bypasses synthetic stubs when raw system headers are requested (`path[0] == '/'` or `--raw-glibc`).
- **Parser (`part3.c`)** (17 additions, 0 deletions; 17 lines diff, well below 50-line budget):
  - In `parse_func_def()`, skims and absorbs GNU `__asm__("...")`, `asm`, and `__attribute__` before `;` in function declarations.
- **Compiler Driver & Types (`part5.c`)** (19 additions, 5 deletions; 24 lines diff):
  - Declares `struct _IO_FILE` and aliases `FILE` to `struct _IO_FILE`.
  - Automatically adds system include paths `/usr/include:/usr/include/x86_64-linux-gnu` when `--raw-glibc` is enabled.

---

## 3. Rule EF-3 Minimal Probe Evidence

Probe: `tests/test_glibc_dual_headers.c` ingesting raw `/usr/include/stdio.h` and `/usr/include/stdlib.h`.

```c
#include </usr/include/stdio.h>
#include </usr/include/stdlib.h>

int main(void) {
    char *buf = (char *)malloc(64);
    if (!buf) return 1;
    snprintf(buf, 64, "GLIBC DUAL INGESTION: stdio.h + stdlib.h VERIFIED");
    printf("%s\n", buf);
    free(buf);
    return 0;
}
```

Raw Command & Execution Output:
```text
$ ./zcc --raw-glibc tests/test_glibc_dual_headers.c -o /tmp/glibc_test.s && gcc /tmp/glibc_test.s -o /tmp/glibc_test && /tmp/glibc_test
pp_process_include: /usr/include/stdio.h added 453 macros, total is 510
pp_process_include: /usr/include/stdlib.h added 150 macros, total is 660
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (71 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
GLIBC DUAL INGESTION: stdio.h + stdlib.h VERIFIED
```
Verdict: **PASS (Exit 0)**

---

## 4. Gate Verification Outputs

### Gate 1: Self-Host Identity (Mandatory)
```text
$ cmp zcc2.s zcc3.s && echo 'CMP SUCCESS: byte-identical'
CMP SUCCESS: byte-identical

$ md5sum zcc2.s zcc3.s
ca9f0f87c0c6638ba715d7389720fe76  zcc2.s
ca9f0f87c0c6638ba715d7389720fe76  zcc3.s
```
Peephole optimization elisions during bootstrap: `47991` (recorded in `BOOTSTRAP_BASELINES.tsv`).  
Verdict: **PASS**

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
Verdict: **PASS**

### Gate 3: Line Budget & Conformance Audit (< 50 lines)
```text
$ git diff --numstat part0_pp.c part3.c part4.c part5.c
38	1	part0_pp.c
17	0	part3.c
0	0	part4.c
19	5	part5.c
```
- `part0_pp.c`: 39 lines diff (budget < 50)
- `part3.c`: 17 lines diff (budget < 50)
- `part4.c`: 0 lines diff (unmodified)
- `part5.c`: 24 lines diff

Verdict: **PASS**

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
Verdict: **PASS (15/15 clean)**

### Gate 5: Evidence Freshness
All verification gates freshly executed on active checked-out tree.  
Verdict: **PASS**
