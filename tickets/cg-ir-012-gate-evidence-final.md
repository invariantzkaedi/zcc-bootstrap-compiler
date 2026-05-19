# Gate Evidence: ZCC Self-Hosting Verification

**Date:** 2026-05-19
**Context:** Finalized the ZCC IR liveness analysis to correctly handle backward jumps (loop latches) and added `next_token` to the IR whitelist.

## Goals
1. Complete the structural liveness model by correctly handling loop-spanning variables.
2. Extend live intervals across loop latch blocks in `regalloc.c`.
3. Validate stability through a successful `make selfhost` bootstrap cycle, ensuring assembly identical output.

## Outcomes
1. **Liveness Analysis:** Added a backward-jump loop detection pass in `build_intervals` (`regalloc.c`). If an interval is used inside a loop and defined before it, its interval is automatically extended to span the entire loop latch block (`iv->end = pos`), fixing the register clobbering bug.
2. **Whitelist Completion:** `next_token` (from the Lexer Core) was successfully compiled natively by the IR backend without triggering the infinite loop hangs that previously blocked Phase 2.
3. **Parity Check:** The `make selfhost` gate was fully executed and successfully verified the assembly (`zcc2.s == zcc3.s`).

## Output Log
```
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (12510 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
strip --strip-all zcc2
=== Stage 2: zcc2 compiles itself -> zcc3 ===
./zcc2 zcc.c -o zcc3
...
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (12510 elided)
[Phase 6] GCC Assembly/Linker Binding... OK
[OK] ZCC Engine Compilation Terminated Successfully.
strip --strip-all zcc3
=== Verify: zcc2.s == zcc3.s (codegen parity) ===
./zcc  zcc.c -o zcc2.s
./zcc2 zcc.c -o zcc3.s
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)" || (echo "SELF-HOST FAILED (assembly diverged)"; diff zcc2.s zcc3.s | head -20; exit 1)
SELF-HOST VERIFIED (assembly identical)
```

## Status
**BASELINE GREEN.** The self-hosting cycle is completely stabilized with identical assembly generation between host-compiled `zcc` and self-hosted `zcc2`.
