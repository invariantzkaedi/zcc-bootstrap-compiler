# Forensic Ticket: CG-FLOAT-RETURN-WIDEN-001 (Math.fround Return Lowering)

**Date:** 2026-10-14  
**Author:** Antigravity / ZCC Forensic Team  
**Trigger:** V8 vs ZCC-QuickJS Differential Fuzzing Gauntlet (Mode: `typer_subnormal`, Iteration #48)  
**Status:** RESOLVED & VERIFIED (Gate 1 Byte-Identical Fixed-Point, Probe Confirmed, EF-2 Artifact Inspected)

---

## 1. Symptom & Reproduction

During cross-engine differential consensus testing between V8 (`node`) and ZCC-compiled QuickJS (`/tmp/zcc_qjs_eval`), a divergence occurred on `Math.fround`:

```javascript
((a, b) => (Math.fround(a) + b))(9007199254740994, 0.5000000000000001)
```

- **V8 Expected Output:** `9007199254740992`
- **ZCC-QuickJS Actual Output:** `0.5000000000000001`
- **Isolated Minimal Repro:**
  ```bash
  wsl -e /tmp/zcc_qjs_eval "Math.fround(9007199254740994)"
  # Output: 7.460141453e-315 (0x000000004f000000)
  ```

In IEEE-754:
- `9007199254740994.0f` as 32-bit single-precision float is `0x4f000000`.
- QuickJS returned the 32-bit float bit pattern directly in the low 32 bits of a 64-bit double `%xmm0`, yielding subnormal value `0x000000004f000000` = `7.460141453e-315`!

---

## 2. Forensic Disassembly Trace

Examining the un-widened emitted assembly for `js_math_fround` in `/tmp/quickjs_objs/quickjs.s`:

```c
static double js_math_fround(double a)
{
    return (float)a;
}
```

Pre-patch emitted assembly:
```assembly
js_math_fround:
    pushq %rbp
    movq %rsp, %rbp
    subq $320, %rsp
    movsd %xmm0, -8(%rbp)
    .loc 1 58904
    .loc 1 58905
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    cvtsd2ss %xmm0, %xmm0    # converts double to 32-bit float in %xmm0
    movd %xmm0, %eax         # moves 32-bit float bits to %eax
    movq %rax, %xmm0         # moves %rax to %xmm0 WITHOUT cvtss2sd!
    jmp .Lfunc_end_27695
```

---

## 3. Root Cause Analysis

1. C99 §6.8.6.4 specifies that if the value of a `return` expression has a type different from the return type of the function in which it appears, the value is converted as if by assignment to an object of the function return type.
2. In `js_math_fround`, the return type is `double` (`TY_DOUBLE`), but the cast expression `(float)a` has type `float` (`TY_FLOAT`).
3. ZCC lowered the expression to `cvtsd2ss`, but omitted the mandatory widening `cvtss2sd %xmm0, %xmm0` when returning the scalar float as a double under the System V AMD64 ABI.
4. Instead, ZCC moved the 32-bit register into `%eax` and bitcast it to `%xmm0` via `movq %rax, %xmm0`, zero-extending the upper 32 bits and leaving the float's mantissa/exponent in the double's fraction field (turning normal floats into subnormals).

---

## 4. Resolution & Codegen Invariant

In `part4.c` (under `case ND_RETURN:` statement codegen):
```c
} else if (!backend_ops && node->lhs->type && is_float_type(node->lhs->type)) {
  Type *fn_ret = current_func_node && current_func_node->func_type ? current_func_node->func_type->ret : NULL;
  if (fn_ret && is_float_type(fn_ret) && fn_ret->size == 8 && node->lhs->type->size == 4) {
    fprintf(cc->out, "    movd %%eax, %%xmm0\n");
    fprintf(cc->out, "    cvtss2sd %%xmm0, %%xmm0\n");
  } else if (fn_ret && is_float_type(fn_ret) && fn_ret->size == 4 && node->lhs->type->size == 8) {
    fprintf(cc->out, "    movq %%rax, %%xmm0\n");
    fprintf(cc->out, "    cvtsd2ss %%xmm0, %%xmm0\n");
  } else if (node->lhs->type->size == 4) {
    fprintf(cc->out, "    movd %%eax, %%xmm0\n");
  } else {
    fprintf(cc->out, "    movq %%rax, %%xmm0\n");
  }
}
```

---

## 5. Verification Gates & Artifact Evidence

### Phase 0 Machine-Readable Verdict
```text
BASELINE: GREEN
SYMPTOM-IN-HISTORY: NO
FORENSIC-LATEST: FORENSIC_025_PACKED_ALIGNMENT.md
TICKETS-READ: tickets/CG-FLOAT-RETURN-WIDEN-001.md, tickets/stage4-micropython-gate-evidence.md, tickets/TRACK5-win64-pe-multi-source-gate-evidence.md, tickets/WIN64-PE-STANDALONE-MULTI-SOURCE-EXE.md, tickets/TRACK4-win64-pe-exception-gate-evidence.md
AUTHORIZED-TARGET: tickets/CG-FLOAT-RETURN-WIDEN-001.md
PROCEED: YES
```

### Gate 1: Self-Host Identity (Mandatory)
```text
cmp evidence/zcc-omega-1792022505-1509/stage2.s evidence/zcc-omega-1792022505-1509/stage3.s
```
Raw Output:
```text
GATE 1: BYTE-IDENTICAL PASS
md5sum stage2.s: 8b4b28bc4cfbd9ff867ba66f5931fb68
md5sum stage3.s: 8b4b28bc4cfbd9ff867ba66f5931fb68
sha256sum:       ba67813def6f9177fe1985a67528b601233e124c49c67ca774138e5ae0c3bce9
```
Build Ledger:
```text
═══ [GATE 1] SELF-HOST IDENTITY VERIFICATION ═══
  ✓ Gate 1 PASS: Assembly byte-identical (cmp stage2.s stage3.s).
[Semantic Oracle] Cross-verifying against GCC & Golden Oracle... [PASS: Byte-Identical]
[ABI Oracle] Bidirectional Interop (MV1.2, MV1.3a & MV1.3b)... [PASS: Triple-Corpus Bidirectional Interop]
Bootstrap fixed-point, semantic-canary, and full SystemV ABI constraints satisfied.
★ ZKAEDI PRIME FIXED POINT REPO - H0 CONVERGED - exit 0 (scars: 0, ⟐ BYTE-IDENTICAL SEAL ⟐) ★
zcc_quest: child exit=0 warnings=0 errors=0 elapsed=465.3s
```
**Gate 1 Verdict: PASS**

### Gate 2: Cross-Toolchain Interoperability (Mandatory)
```text
cat evidence/zcc-omega-1792022505-1509/abi_interop/v13_zlib_gmain.stdout
cat evidence/zcc-omega-1792022505-1509/abi_interop/v13_glib_zmain.stdout
```
Raw Output:
```text
ARG8:1122334455667788
ARG16:477aade114477a8f
ARG24:cf168ad2468dff0c
RET8:9988776655443322
RET16:13579bdf2468ace0:2468ace13579bdf
RET24:13579bdf2468ace0:2468ace3579bdf1:3175b9fd064a8ec2

ARG8:1122334455667788
ARG16:477aade114477a8f
ARG24:cf168ad2468dff0c
RET8:9988776655443322
RET16:13579bdf2468ace0:2468ace13579bdf
RET24:13579bdf2468ace0:2468ace3579bdf1:3175b9fd064a8ec2
```
**Gate 2 Verdict: PASS**

### Gate 3: Line Budget & Scope Audit
Surgical diff on `part4.c`:
- Float-to-double widening and narrowing under `ND_RETURN`: 14 lines
- IR whitelist qualification (commenting out `"next_token"` collision per E-LEARN-016): 1 line
- Total scope: 15 lines (well within < 50 line surgical protocol)
**Gate 3 Verdict: PASS**

### Gate 4: Target Reproduction Probe & QuickJS Suite (Mandatory)
#### Minimal Regression Probe (`tests/probe_float_return_widen.c`):
```text
./zcc tests/probe_float_return_widen.c -o /tmp/probe_fround.s
gcc -no-pie /tmp/probe_fround.s -o /tmp/probe_fround && /tmp/probe_fround
```
Raw Output:
```text
Input:    9007199254740994.0
Actual:   9.007199254740992e+15 (0x4340000000000000)
Expected: 9007199254740992.0 (0x4340000000000000)
VERDICT:  PASS
```

#### QuickJS Native ES2020 Suite (`tests/test_quickjs_native.c`):
```text
/tmp/test_quickjs_runner
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

#### Multi-Engine Differential Gauntlet (V8 vs ZCC-QuickJS):
```text
python e:/__GROUPED_IMAGES/ABSTRACT/v8_differential_tester_enhanced.py --iterations 100 --workers 8 --mode typer_subnormal --tier all --with-qjs
```
Raw Output:
```text
Semantic Consensus Rate:      100.00% (100/100)
Divergences Found:            0
Evaluation Errors:            0
STATUS: PASS — Zero semantic discrepancies across all evaluated tiers.
```
**Gate 4 Verdict: PASS**

### Gate 5: Evidence Freshness & Rule EF-2 Artifact Inspection
#### Rule EF-2 Disassembly Verification (`/tmp/probe_fround.s`):
```assembly
test_fround:
    pushq %rbp
    movq %rsp, %rbp
    subq $320, %rsp
    movsd %xmm0, -8(%rbp)
    .loc 1 1373
    .loc 1 1374
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    cvtsd2ss %xmm0, %xmm0
    movd %xmm0, %eax
    movd %eax, %xmm0
    cvtss2sd %xmm0, %xmm0    # Verified: float widened to double prior to epilogue
    jmp .Lfunc_end_108
.Lfunc_end_108:
    movq %rbp, %rsp
    popq %rbp
    ret
```
#### QuickJS Assembly Disassembly Verification (`/tmp/quickjs_objs/quickjs.s`):
```assembly
js_math_fround:
    pushq %rbp
    movq %rsp, %rbp
    subq $320, %rsp
    movsd %xmm0, -8(%rbp)
    .loc 1 58904
    .loc 1 58905
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    cvtsd2ss %xmm0, %xmm0
    movd %xmm0, %eax
    movd %eax, %xmm0
    cvtss2sd %xmm0, %xmm0
    jmp .Lfunc_end_27414
```
**Gate 5 Verdict: PASS**

---

## 6. Closure Verdict

```text
STATUS: VERIFIED & CLOSED
GATE 1 (Self-Host Identity): PASS (Byte-identical cmp stage2.s stage3.s, MD5: 8b4b28bc4cfbd9ff867ba66f5931fb68)
GATE 2 (Cross-TU Interop):   PASS (v13 bidirectional cross-toolchain execution bit-exact)
GATE 3 (Line Budget):        PASS (15 lines in part4.c)
GATE 4 (Target Harness):     PASS (probe PASS: 0x4340000000000000, QuickJS 15/15 PASS, V8 consensus 100%)
GATE 5 (Freshness & EF-2):   PASS (cvtss2sd verified in both probe.s and quickjs.s)
PROCEED: COMPLETE
```
