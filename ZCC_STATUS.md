# ZCC Status Report
Generated: 2026-04-28
Generation 2 — supersedes generation 1 below

## Build Health (verified Apr 28, 2026)

| Component | Status |
|-----------|--------|
| AST Selfhost (zcc2.s == zcc3.s) | VERIFIED |
| IR Backend | Operational (CG-IR-001 through CG-IR-022 all closed) |
| IR Telemetry | Operational |
| CRLF Hardening | Locked (tag: crlf-pp-hardened-20260424) |
| Tripwire (zcc.c == cat of PARTS) | Active |
| Rust Frontend (v1) | Merged (PR #7) |
| make rust-front-smoke | All checks passed (~40s) |

## Bootstrap Baselines (drift detectors)

| Compiler config | md5(zcc2.s) | Notes |
|-----------------|-------------|-------|
| C-only (pre-rust merge) | bbe72c8e677d4270bca32db48897e956 | locked Apr 28 on main b69147d |
| C + Rust v1 (current main) | a9d68cd228e2a34d508f849d7390ae9c | locked Apr 28 on main 15e2b2d |

If a future bootstrap produces a different hash, either codegen drifted (regression) or new compilation units were added (intentional). Use this table as the first line of forensic defense.

## Architecture

- Dual-emission: AST-direct (part4.c codegen_expr/codegen_stmt) plus IR backend (compiler_passes.c)
- IR gate: ir_whitelisted() in part4.c controls which functions use IR
- Hybrid frame: AST owns prologue/epilogue, IR owns body (body_only=1, slot_base=-stack_size)
- Bootstrap: GCC -> zcc -> zcc2.s -> zcc2 -> zcc3.s -> diff zcc2.s zcc3.s
- C build: make clean && make selfhost
- Rust smoke: make clean && make rust-front-smoke
- Frontend dispatch: file extension -> C path (.c) or Rust path (.rs); rust hooks live in part5.c
- Rust frontend: part7_rust.c (~3200 lines), positioned in PARTS after part5.c

## Milestones (chronological)

- DOOM compiled: 732 functions, 18.5% IR node reduction
- Lua 5.4.6: 100% test suite pass (gate b299f43)
- libcurl 8.7.1: 133/133 files compile, link, run (three ABI bugs fixed: ND_CAST strip, va_list 24B SysV ABI struct)
- SQLite 3.45.0: full SQL round-trip
- CG-IR-018: static pointer array .zero corruption — most significant find
- DCE pass: eliminated 1,862 / 17,264 instructions (10.79%)
- IR bridge: GCC-compiled boundary module with three-pointer ABI
- Rust v1 frontend merged: fn, let, let mut, return, if/else, while, calls, recursion (direct + mutual), SysV ABI <=6 reg args + >=7 stack args, strict modes, named diagnostics with concrete fix examples

## Known Open Items

- LICM pass commented out in run_all_passes
- src/ tree partial cleanup: 9 byte-identical duplicates removed; 10 diverged duplicates and 39 unique files still under review
- 43 GLB fleet assets (~546MB) tracked in git history; pending move to HF/R2 plus filter-repo
- ZKAEDI-MINI v7 retrain pending (v7 hallucinated due to dataset loading error)

## Suggested Next Steps

- Re-add rust-front-smoke as required status check on main (was removed during initial setup since the workflow did not yet exist on main; now it does)
- Re-enable LICM pass
- Resolve src/ diverged duplicates (10 files where root and src/ differ)
- ASan run to confirm SARIF CWE-416/415 findings
- GLB extraction and history rewrite

---

## Historical Status (generation 1, 2026-04-04)

*The following section is preserved as-is from the previous status doc. Most claims here are now superseded — IR Backend went from FAILED to operational, and several milestones have landed since. Kept for forensic record.*

# ZCC Status Report
Generated: 2026-04-04 13:58:45 PDT

## Build Health

| Component | Status |
|-----------|--------|
| AST Selfhost | VERIFIED (zcc2.s == zcc3.s) |
| IR Backend | FAILED |
| IR Functions | 176 compiled through IR |
| Blacklist Hits | 0 |
| IR Gate | ir_whitelisted() returns ? |

## Source Sizes (as of generation 1)

| File | Lines |
|------|-------|
| zcc_pp.c (concatenated) | 7792 |
| part4.c (codegen) | 2635 |
| compiler_passes.c (IR passes) | 7317 |
| compiler_passes_ir.c (IR helpers) | 570 |

## CG-IR Bug Fixes Applied (as of generation 1)

CG-IR-005, CG-IR-008, CG-IR-009, CG-IR-011, CG-IR-012, CG-IR-013

(Note: by generation 2, all of CG-IR-001 through CG-IR-022 are closed.)

## Optimization Pass Stats (last IR run, generation 1)

- [IR-Opts] Folded: 9 | S-Reduce: 2 | Copy-Prop: 0 | Peephole: 16
- [RLE] redundant loads eliminated: 2
- [DCE->SSA] instructions removed (after mem2reg): 55, blocks removed: 366
- [EscapeAna] allocations promoted to stack: 12 (of 15 total)
- [Mem2Reg] single-block allocas promoted: 11

## Architecture (generation 1)

- Dual-emission: AST-direct (part4.c codegen_expr/codegen_stmt) plus IR backend (compiler_passes.c)
- IR gate: ir_whitelisted() in part4.c controls which functions use IR
- Hybrid frame: AST owns prologue/epilogue, IR owns body (body_only=1, slot_base=-stack_size)
- Bootstrap: GCC -> zcc -> zcc2.s -> zcc2 -> zcc3.s -> cmp zcc2.s zcc3.s
- Build: make clean && make selfhost
- IR test: bash verify_ir_backend.sh
- Environment: Windows + WSL, PowerShell -> wsl -e sh -c
- Working dir: /mnt/h/__DOWNLOADS/selforglinux

## Key Code Locations (generation 1)

| What | File | Line(s) |
|------|------|---------|
| ir_whitelisted gate | part4.c | ~1890 |
| codegen_func | part4.c | ~1909 |
| IR body entry | compiler_passes.c | zcc_run_passes_emit_body_pgo ~5394 |
| run_all_passes | compiler_passes.c | ~4451 |
| Mem2Reg (single-block) | compiler_passes.c | scalar_promotion_pass ~1478 |
| Mem2Reg (multi-block) | compiler_passes.c | multi_block_mem2reg_one ~1681 |
| PHI edge copy | compiler_passes.c | ir_asm_emit_phi_edge_copy ~4826 |
| IRAsmCtx struct | compiler_passes.c | ~4775 |
| ir_asm_vreg_location | compiler_passes.c | ~4790 |

## Known Issues (generation 1)

- LICM pass is commented out in run_all_passes

## Next Steps (suggested, generation 1)

- Register allocation improvements (reduce spills)
- Re-enable LICM pass
- ASan run to confirm SARIF CWE-416/415 findings
- Per-function regression test suite (zcc_test_suite.sh)

---

## ZCC SQLite Milestone — April 10, 2026

- SQLite 3.45.0 compiled by ZCC
- Full SQL round trip verified:
  - open rc=0
  - SELECT 1 = 1
  - CREATE TABLE rc=0
  - INSERT rc=0
  - SELECT x = 42
- All rc=0, zero errors, zero segfaults

Bugs closed to achieve this:

- CG-IR-007: movslq width
- va_list phases 1-3: System V ABI
- Global struct initializer: recursive emitter
- Array-of-struct: budget cursor
- Array parameter decay
- ND_NEG: negative array initializers -> yyRuleInfoNRhs
- struct-by-value Token ABI
- __atomic_* inline
- cltq pointer corruption (8 sites)
- __builtin_va_end linker
- Makefile -no-pie
- sizeof(char_array) = 8 bug
- Octal escape sequences unimplemented

ZKAEDI PRIME: CONVERGED
