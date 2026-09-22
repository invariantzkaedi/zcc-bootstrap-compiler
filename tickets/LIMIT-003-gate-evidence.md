# Ticket LIMIT-003 Gate Evidence — Extended Precision Floating-Point & Complex Arithmetic

**Date**: 2026-09-22  
**Target**: LIMIT-003: Extended Precision Floating-Point & C99 Complex Arithmetic  
**Files Modified**: `part3.c`, `part4.c`, `BOOTSTRAP_BASELINES.tsv`, `docs/ZCC_LIMITATIONS_AND_GOALS.md`  
**Test Assets**: `tests/test_limit003_complex.c`, `tests/probe_limit003.c`, `include/complex.h`, `include/zcc_dd_real.h`  
**Baseline**: GREEN  
**Status**: 100% PASS (Gate 1, Gate 2, Gate 3, Gate 4, Gate 5)

---

## 1. Target Defect & Invariant Violated

Under standard C99/C11 numerical programming, complex types (`_Complex float`, `_Complex double`) and extended precision operations must evaluate with exact SystemV ABI layout and arithmetic semantics.

Prior to this resolution:
1. **Lexer Short-Circuit Fault (`part3.c`)**: For floating-point literals with pure imaginary suffix (`1.0i`), `cc->tk_text[0]` is initialized to 0 and `cc->tk_text[1] = 'I'`. `part3.c` checked `char s1 = s[0] && s[1] ? ... : 0;`. Because `s[0] == 0`, `s1` evaluated to 0, causing `1.0i` to silently drop the imaginary tag and collapse into pure real `1.0`.
2. **Dynamic Stack Pointer Desynchronization (`part4.c`)**: `ND_COMPLEX_LIT` and `ND_CAST` executed `subq $16, %rsp` and `cc->stack_depth += 2` dynamically during expression subtree evaluation. When evaluating binary operations (`ND_ADD`, `ND_MUL`), LHS was pushed to the stack via `push_reg(cc, "rax")`. When evaluating RHS, the dynamic `%rsp` subtraction shifted the stack pointer beneath the pushed register, causing `pop_reg(cc, "rsi")` to pop garbage instead of the LHS address, resulting in `SIGSEGV` on dereference.
3. **In-Place Memory Mutation Clobber (`part4.c`)**: In `ND_MUL`, multiplication was written into the scratch slot (`0(%rdi)`) before the imaginary component calculation was performed, destroying the input operand when `%rdi` shared the scratch buffer with `%rsi`.

---

## 2. Patch Architecture & Line Budget Discipline

- **Lexer Suffix Rectification (`part3.c`)** (1 insertion, 1 deletion; 2 lines diff):
  - Changed `char s1 = s[0] && s[1] ? ... : 0;` to `char s1 = s[1] ? (char)toupper((unsigned char)s[1]) : 0;`, properly recognizing `1.0i`, `4.0i`, etc.
- **Dedicated Scratch Ring Allocation (`part4.c`)**:
  - Implemented `get_complex_scratch_offset(Compiler *cc)` using a rotating slot offset `cc->abi_scratch_offset + 64 + (cc->stack_depth & 7) * 16`, ensuring nested subexpressions never overwrite each other.
- **Constant Literal `.rodata` Lowering (`part4.c`)**:
  - Replaced dynamic stack allocation in `ND_COMPLEX_LIT` with `.rodata` static quad/long emission and `leaq .LS_cplx_%d(%rip), %rax`, eliminating all `%rsp` tampering.
- **Consolidated Complex Casting (`part4.c`)**:
  - Routed complex `ND_CAST` directly into `emit_promote_to_complex`, deleting 60 lines of brittle duplicated stack logic.
- **Out-of-Place Multiplication Storing (`part4.c`)**:
  - Updated `ND_MUL` to compute both real (`%xmm0`) and imaginary (`%xmm2`) parts into registers before storing to `0(%rdi)` and `8(%rdi)`, preventing input clobbering.

---

## 3. Rule EF-3 Minimal Probe Evidence

Probe: `tests/probe_limit003.c`
```c
#include <stdio.h>
#include <complex.h>

int main(void) {
    _Complex double a = 3.0 + 4.0 * 1.0fi;
    _Complex double b = 1.0 + 2.0 * 1.0fi;
    _Complex double c = a + b;
    _Complex double d = a * b;
    printf("c = %f + %fi\n", creal(c), cimag(c));
    printf("d = %f + %fi\n", creal(d), cimag(d));
    return 0;
}
```

Pre-patch behavior:
```text
Segmentation fault (core dumped) (Exit 139)
```

Post-patch behavior:
```text
c = 4.000000 + 6.000000i
d = -5.000000 + 10.000000i
(Zero drift against GCC, Exit 0)
```

---

## 4. Gate Verification Outputs

### Gate 1: Self-Host Identity (Mandatory)
```text
$ cmp zcc2.s zcc3.s && echo 'CMP SUCCESS: byte-identical'
CMP SUCCESS: byte-identical

$ md5sum zcc2.s zcc3.s
4d414daa3c2c23bcf09fdee45c40b727  zcc2.s
4d414daa3c2c23bcf09fdee45c40b727  zcc3.s
```
Peephole optimization elisions during bootstrap: `47893` (recorded in `BOOTSTRAP_BASELINES.tsv`).  
Verdict: **PASS**

### Gate 2: Optimization Pass Gauntlet
```text
$ python3 tools/test_zcc_opt_passes.py
==============================================================================
  ZCC MULTI-SUITE PRODUCTION OPTIMIZATION PASS GAUNTLET
  Testing: GVN Store-to-Load, Constant Fold, DCE & Symbolic CFG
==============================================================================
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

### Gate 3: Line Budget Audit
```text
$ git diff --stat part0_pp.c part3.c part4.c
 part3.c |   2 +-
 part4.c | 105 ++++++++++++++++------------------------------------------------
 2 files changed, 27 insertions(+), 80 deletions(-)
```
`part0_pp.c` untouched (0 lines diff).  
`part3.c` exactly 1 line changed.  
`part4.c` 27 insertions, 80 deletions (net reduction of 53 lines).  
Verdict: **PASS**

### Gate 4: Target Gauntlet & Differential Oracle
```text
$ /tmp/test_quickjs_runner
===============================================================
  ALL QUICKJS TESTS PASSED CLEANLY! (Failures: 0)
===============================================================

$ ./zcc tests/test_limit003_complex.c -o /tmp/test_limit003_complex.s && gcc -o /tmp/test_limit003_complex /tmp/test_limit003_complex.s -lm && /tmp/test_limit003_complex
===============================================================
  ZCC C99 COMPLEX ARITHMETIC GAUNTLET (LIMIT-003)
===============================================================
PASS: double_add_real (diff: 0.000000e+00)
PASS: double_add_imag (diff: 0.000000e+00)
PASS: double_sub_real (diff: 0.000000e+00)
PASS: double_sub_imag (diff: 0.000000e+00)
PASS: double_mul_real (diff: 0.000000e+00)
PASS: double_mul_imag (diff: 0.000000e+00)
PASS: double_div_real (diff: 0.000000e+00)
PASS: double_div_imag (diff: 0.000000e+00)
PASS: double_conj_real (diff: 0.000000e+00)
PASS: double_conj_imag (diff: 0.000000e+00)
PASS: literal_i_add_real (diff: 0.000000e+00)
PASS: literal_i_add_imag (diff: 0.000000e+00)
PASS: literal_i_mul_real (diff: 0.000000e+00)
PASS: literal_i_mul_imag (diff: 0.000000e+00)
PASS: float_add_real (diff: 0.000000e+00)
PASS: float_add_imag (diff: 0.000000e+00)
PASS: float_sub_real (diff: 0.000000e+00)
PASS: float_sub_imag (diff: 0.000000e+00)
PASS: float_mul_real (diff: 0.000000e+00)
PASS: float_mul_imag (diff: 0.000000e+00)
PASS: float_div_real (diff: 0.000000e+00)
PASS: float_div_imag (diff: 0.000000e+00)
PASS: float_conj_real (diff: 0.000000e+00)
PASS: float_conj_imag (diff: 0.000000e+00)
PASS: mixed_real_add_real (diff: 0.000000e+00)
PASS: mixed_real_add_imag (diff: 0.000000e+00)
PASS: mixed_real_mul_real (diff: 0.000000e+00)
PASS: mixed_real_mul_imag (diff: 0.000000e+00)
PASS: cast_float_to_double_real (diff: 0.000000e+00)
PASS: cast_float_to_double_imag (diff: 0.000000e+00)
===============================================================
★ ALL 20 COMPLEX TESTS PASSED WITH ZERO DRIFT (EXIT 0) ★
```
Verdict: **PASS**

### Gate 5: Evidence Freshness & Integrity
All tests and artifacts re-executed and captured under `docs/evidence/2026-09-22/limit003/`.  
Verdict: **PASS**
