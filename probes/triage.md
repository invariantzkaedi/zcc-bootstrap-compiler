# ZCC Floating-Point Behavior Triage Report

This report summarizes the diagnostic probe suite run on ZCC (`zcc2`) comparing its outputs against the GCC baseline.

---

### Cat-1: Static global initializers
Status: FAIL
Diff:
```diff
 g_lim_inf: inf
-g_lim_ninf: -inf
-g_lim_nan: nan
-g_lim_nnan: -nan
+g_lim_ninf: -0.000000
+g_lim_nan: -nan
+g_lim_nnan: 0.000000
```
Notes: Negated float limits `-INFINITY` and `-NAN` evaluate to signed zero (`-0.0` and `0.0`) because `eval_const_expr_p4` does not support float negation (`ND_NEG`) on division expressions.

---

### Cat-2: Local variable initializers
Status: PASS
Diff:
```diff
-g_lim_nan: nan
-g_lim_nnan: -nan
+g_lim_nan: -nan
+g_lim_nnan: nan
```
Notes: Clean pass; the only difference is the sign bit of NaN/neg-NaN which is implementation-defined.

---

### Cat-3: Arithmetic correctness
Status: PASS
Diff:
```diff
-nan_add: nan
-nan_mul: nan
+nan_add: -nan
+nan_mul: -nan
```
Notes: Clean pass; arithmetic operations behave identically with expected overflow/underflow, except for implementation-defined NaN sign bits.

---

### Cat-4: Comparisons
Status: FAIL
Diff:
```diff
-nan < 1.0: 0
+nan < 1.0: 1
```
Notes: Floating-point comparisons with NaN (unordered) incorrectly evaluate to `1` (true) for `setb` (`<`) because ZCC does not verify the parity flag (`PF`) to exclude unordered operands.

---

### Cat-5: printf formatting
Status: PASS
Diff:
```diff
-f_nan: nan
+f_nan: -nan
-e_nan: nan
+e_nan: -nan
-g_nan: nan
+g_nan: -nan
```
Notes: Clean pass; formatting behaves identically except for implementation-defined NaN sign bits.

---

### Cat-6: Function args and return values
Status: PASS
Diff: none
Notes: Function calls, return values, float-to-double varargs promotion, and passing structs by value are fully correct.

---

### Cat-7: Arrays and struct members
Status: PASS
Diff: none
Notes: Float/double array reads/writes and structure alignments are identical.

---

### Cat-8: Constant folding in expressions
Status: PASS
Diff: none
Notes: Ternaries, static constant scale factors, and local constant propagation behave identically.
