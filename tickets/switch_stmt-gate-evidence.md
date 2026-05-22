# switch_stmt-gate-evidence — Gate Evidence

Status: Complete & Verified (companion to ZND_SWITCH reachability and loop stack fix)  
Precedent: ae6b5ff (FORENSIC_CORRECTION_2026-04-19.md, "every closed commit must carry gate output")  

This document details the gates and outputs for the **ZND_SWITCH CFG reachability and lex-stack resolution (switch_stmt pre-existing FAIL resolution)**.

## Operative Assumption
Restoring compiler reachability traversal by removing overwritten successor edges from case comparison blocks to case label body blocks guarantees that the basic block reordering (BBR) pass includes all case bodies in final assembly emission. Managing the exit and latch stacks of switches prevents break targeting drift, enabling full System V ABI compatible switch statement codegen.

## Load-Bearing Claims
- **VERIFIED**: `t_switch.c` (switch_stmt) no longer terminates with exit code 1 or experiences dead blocks under the IR backend. It compiles, links, and runs, returning exactly exit code `20`.
- **VERIFIED**: The total quick and full test suites pass 100% of all 32 unit tests with zero failures.
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
- Command: `./zcc_test_suite.sh`
- Output:
```text
── Category 6: Switch Statements ──
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (2 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
; ZCC IR v1.0.0
; ZCC IR module  funcs=2

; func classify -> i32
; end classify  nodes=0

; func main -> i32
; end main  nodes=0

[Phase 5] Native C Peephole Optimization... OK (0 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
  [PASS] switch_stmt (rc=0, 2 IR funcs)
── Category 7: String and Global Operations ──
...
── Category 11: Full Selfhost ──
  [INFO] Running make clean && make selfhost
  [PASS] AST selfhost VERIFIED
  [INFO] Running verify_ir_backend.sh
  [SKIP] verify_ir_backend.sh not found

══════════════════════════════════════════════════
  PASS: 32  FAIL: 0  SKIP: 1
══════════════════════════════════════════════════
ALL TESTS PASSED
```

## Forensic Lineage
- `FORENSIC_CORRECTION_2026-04-19.md` (Gate discipline precedent)
- `tickets/switch_stmt-issue.md` (Pre-existing failure ticket resolved)
- `tickets/pointer-deref-gate-evidence.md` (Prior companion evidence)
- `compiler_passes.c` (Opt core)
