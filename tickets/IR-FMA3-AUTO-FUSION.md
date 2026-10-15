# Ticket: IR-FMA3-AUTO-FUSION (Autonomous IR FMA3 / FNMA Auto-Fusion Pass)

**Date:** 2026-10-15  
**Author:** Antigravity / ZCC Optimization Team  
**Trigger:** User Authorization (Option 1: Autonomous IR FMA3 / FNMA auto-fusion pass)  
**Status:** RESOLVED & VERIFIED (Gate 1 Byte-Identical Fixed-Point, Probe Confirmed, EF-2 Artifact Inspected)

---

## 1. Goal & Architecture

Implement an autonomous IR-level Fused Multiply-Add (`IR_FMA`) and Fused Negative Multiply-Add (`IR_FNMA`) peephole optimization pass in ZCC:
1. Pattern match `(a * b) + c` and `c + (a * b)` into `IR_FMA`.
2. Pattern match `c - (a * b)` into `IR_FNMA`.
3. Eliminate intermediate `IR_FMUL` operations when their result temporary is dead (`count_temp_uses == 0`), converting them to `IR_NOP` for dead node elimination.
4. Enforce basic-block safety windows via `is_safe_fma_window` (reject across branch boundaries, calls, or if operands are mutated before consumption).
5. Track `n->label` (the 3rd operand) in `regalloc.c` intervals and `ir_pass_manager.c` `node_uses` to prevent premature DCE or register clobbering.
6. Lower via `ir_to_x86.c` to native hardware instructions:
   - `vfmadd213sd %xmm2, %xmm1, %xmm0` (double-precision)
   - `vfmadd213ss %xmm2, %xmm1, %xmm0` (single-precision)
   - `vfnmadd213sd %xmm2, %xmm1, %xmm0` (negated multiply-add)

---

## 2. Rule EF-3 Pre-Patch Probe & EF-2 Artifact Inspection

Probe suite: `tests/probe_ir_fma_fusion.c`
Covering:
- `fma_add_test(a, b, c)` -> `(a * b) + c`
- `fma_add_comm_test(a, b, c)` -> `c + (a * b)`
- `fma_float_test(a, b, c)` -> `(a * b) + c` (float)
- `fnma_sub_test(a, b, c)` -> `c - (a * b)`

### Pre-Patch Emitted Assembly (/tmp/probe_fma_before.s)
Emitted separate scalar multiplication and addition/subtraction:
```assembly
fma_add_test:
    movsd %xmm4, %xmm0
    mulsd %xmm3, %xmm0
    addsd %xmm2, %xmm0
```

### Post-Patch Emitted Assembly (/tmp/probe_fma_after.s)
Scalar multiplications elided; fused hardware instructions emitted:
```assembly
fma_add_test:
    movsd %xmm4, %xmm0
    movsd %xmm3, %xmm1
    vfmadd213sd %xmm2, %xmm1, %xmm0

fma_add_comm_test:
    movsd %xmm3, %xmm0
    movsd %xmm2, %xmm1
    movsd %xmm4, %xmm2
    vfmadd213sd %xmm2, %xmm1, %xmm0

fma_float_test:
    movss %xmm4, %xmm0
    movss %xmm3, %xmm1
    vfmadd213ss %xmm2, %xmm1, %xmm0

fnma_sub_test:
    movsd %xmm3, %xmm0
    movsd %xmm2, %xmm1
    movsd %xmm4, %xmm2
    vfnmadd213sd %xmm2, %xmm1, %xmm0
```

### Probe Execution
```text
r1=10.0 r2=10.0 r3=10.0 r4=4.0
PROBE EXECUTION: PASS
```

---

## 3. Optimization Gauntlet & Suite Verification

`python3 tools/test_zcc_opt_passes.py`:
- Suites Evaluated: 19
- Bit-Exact Passes: 19/19 (100%)
- Semantic Divergences: 0
- Net Instructions Elided: 10,405 lines (11.53% net reduction)
- Verdict: PASS (Exit 0)

`make check-copy-const-prop`:
- Results: 19 passed, 0 failed
- Verdict: PASS (Exit 0)

---

## 4. Required Verification Gates

### Gate 1 — Self-Host Identity (Mandatory)
```bash
cmp zcc2.s zcc3.s
```
Raw Output:
```text
CMP RESULT: BYTES IDENTICAL
c836006231888c2b99be305e3fd1dc14  zcc2.s
c836006231888c2b99be305e3fd1dc14  zcc3.s
```
Pass: Byte-identical output.

### Gate 2 — Cross-Toolchain Interoperability (Mandatory)
`v12`, `v13`, `v13b` cross-toolchain inter-op suites:
- `v12_glib_zmain`: `ABI_PACKET_RES: 7281533571 (new_id=1100, new_delta=-20)`
- `v12_zlib_gmain`: `ABI_PACKET_RES: 7281533571 (new_id=1100, new_delta=-20)`
- `v13_glib_zmain`: `ARG8:1122334455667788 RET16:13579bdf2468ace0:2468ace13579bdf`
- `v13_zlib_gmain`: `ARG8:1122334455667788 RET16:13579bdf2468ace0:2468ace13579bdf`
- `v13b_glib_zmain`: `CALLER_ALIGN_MISALIGN:0 CALLEE_SAVED_MASK:00`
- `v13b_zlib_gmain`: `CALLER_ALIGN_MISALIGN:0 CALLEE_SAVED_MASK:00`
Pass: Bit-exact cross-toolchain execution in both directions.

### Gate 3 — 797-Function Corpus Diff
NOT-APPLICABLE: `part0_pp.c` and `part3.c` untouched.

### Gate 4 — Target Harness (Probe)
`tests/probe_ir_fma_fusion.c` compiled with `./zcc --ir` and executed:
```text
r1=10.0 r2=10.0 r3=10.0 r4=4.0
PROBE EXECUTION: PASS
```
Pass.

### Gate 5 — Evidence Freshness
Re-run all gates from latest baseline; verified clean.

---

## 5. Files Changed
- `ir_pass_manager.h`: Export `ir_pass_fma_fusion(void *fn)`.
- `ir_pass_manager.c`: Implement `count_temp_uses`, `is_safe_fma_window`, `ir_pass_fma_fusion`, and register pass in DPO / Standard pipelines.
- `regalloc.c`: Track `n->label` interval references for `IR_FMA` / `IR_FNMA` / `IR_VFMA`.
- `tests/probe_ir_fma_fusion.c`: Pre-patch & post-patch deterministic verification probe.
- `BOOTSTRAP_BASELINES.tsv`: Append new baseline hash `c836006231888c2b99be305e3fd1dc14`.
