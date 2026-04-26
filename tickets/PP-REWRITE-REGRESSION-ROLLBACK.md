# Forensic Chain: PP-REWRITE-REGRESSION-ROLLBACK ❌

## Goal
Harden Preprocessor (PP) macro-arg depth handling.

## Outcome
A wholesale preprocessor rewrite was attempted. This approach regressed the ZCC self-hosting bootstrap invariant (`zcc2.s == zcc3.s`) and failed to improve the Lua compilation pipeline. The reported "success" on Lua's `lcode.c` was diagnosed as a false positive caused by missing include paths (`-I`) and a silent fallback to a generic `stddef` stub.

## Root Cause
1. **Inadequate Diagnostic Harness:** The rewrite was validated using an isolated file compilation of `lcode.c` rather than a full system build, obscuring macro resolution failures related to the include path.
2. **Bootstrap Regression:** The holistic rewrite broke bit-exact determinism. Phase 2 and Phase 3 compiler generations began to drift due to token stream overshoots and conditional scoping bugs.

## Gate Audit Verdict
**ROLLBACK**. The generated evidence (`702 errors`, final line `FAILED`) directly contradicted the purported success. The gate-discipline protocol successfully caught the drift before it was permanently committed to the repository.

## Policy & Remediation
1. **Repository Synchronization:** The local state has been hard-reset and synchronized with the stable baseline (`origin/main`, commit `b299f43`), completely eliminating the preprocessor-induced bootstrap regressions.
2. **Build Integrity Confirmed:** The self-hosting build (`make selfhost`) has been run, and bootstrap parity (`cmp zcc2.s zcc3.s`) is confirmed green and IDENTICAL.
3. **Future Modifications:** Any future preprocessor fixes must be strictly surgical, rigorously preserve the zero-drift bootstrap equality, and be validated against a formal build harness (e.g., standard Lua Makefile integration).

## Evidence of Rollback (verified 2026-04-22)

- HEAD before rollback: (unrecorded — working-tree-only PP rewrite, never committed)
- HEAD after rollback:  b299f43fba1622e4b600260272ae074f6aa16131
- Reset command:        git reset --hard origin/main
- Bootstrap gate:       cmp zcc2.s zcc3.s → exit 0
- .s sizes (identical): 138,690 lines each
- Phases clean:         [1] Lex → [2] AST → [3] Fold → [4] Codegen → [5] Peephole (9,827 elided)
- Verified by:          user terminal, WSL/Ubuntu, conda env `prime`
