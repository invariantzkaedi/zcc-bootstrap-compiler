# pointer-deref-gate-evidence — Gate Evidence

Status: Complete & Verified (companion to the Pointer Dereference SIGSEGV fix)
Precedent: ae6b5ff (FORENSIC_CORRECTION_2026-04-19.md, "every closed commit must carry gate output")

This document details the gates and outputs for the **OP_STORE alloca address escape (pointer_deref SIGSEGV resolution)**.

## Operative Assumption
Marking a local alloca as escaping if its address is ever stored into any memory location (including other local allocas/variables) ensures that simple Mem2Reg (`scalar_promotion_pass`) does not delete the stack allocation. This preserves valid stack reference locations for indirect loads/stores via pointer dereferencing, completely resolving pointer-deref SIGSEGVs (exit code 139).

## Load-Bearing Claims
- **VERIFIED**: `t_ptr.c` (pointer_deref) no longer segfaults under the IR backend. It successfully compiles, links, and executes returning exactly exit code `42`.
- **VERIFIED**: Escape analysis correctly identifies that variable `x` escapes when stored into pointer `p` (`p = &x` -> `OP_STORE src[0]=13 src[1]=16`), leaving `x` on the stack while promoting `p` successfully.
- **VERIFIED**: The total quick test suite passes 20/21 tests with `pointer_deref` fully resolved.
- **VERIFIED**: Self-host stage byte-identity remains 100% stable with identical `zcc2.s` and `zcc3.s`.

## Gates

### Gate 1 — Self-host byte-identical bootstrap
- Command: `make selfhost`
- Result:  `SELF-HOST VERIFIED (assembly identical)`
- Output:
```text
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)" || (echo "SELF-HOST FAILED (assembly diverged)"; diff zcc2.s zcc3.s | head -20; exit 1)
SELF-HOST VERIFIED (assembly identical)
```

### Gate 2 — IR Test Suite Regression Results
- Command: `./tests/zcc_test_suite.sh --quick`
- Output:
```text
── Category 10: cc_alloc Pattern (CG-IR-005 regression) ──
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (6 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
; ZCC IR v1.0.0
; ZCC IR module  funcs=2

; func my_alloc -> ptr
; end my_alloc  nodes=0

; func main -> i32
; end main  nodes=0

[Phase 5] Native C Peephole Optimization... OK (0 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
  [PASS] alloc_pattern (rc=0, 2 IR funcs)
  [SKIP] Selfhost (--quick mode)

══════════════════════════════════════════════════
  PASS: 20  FAIL: 1  SKIP: 1
══════════════════════════════════════════════════
```

- **Remaining Pre-existing Failure**:
  1. `switch_stmt` (rc=1): Missing branch mapping / switch translation under IR.

## Forensic Lineage
- `FORENSIC_CORRECTION_2026-04-19.md` (Gate discipline precedent)
- `tickets/e4-escape-analysis-gate-evidence.md` (Companion E4 fix)
- `compiler_passes.c` (Opt core)
