# CG-IR-012 Gate Evidence

**Fix:** CFG-based back-edge interval extension in `compiler_passes.c` to prevent loop-spanning variable clobbering.

## Phase 0 Verdict
```text
BASELINE:              GREEN
SYMPTOM-IN-HISTORY:    NO
FORENSIC-LATEST-SHA:   a3b696cc (Batch 8 and CG-IR-012 ticket restored)
PROCEED:               YES
```

## Gate Runs

### Gate 1: Self-Host Byte-Identical
```text
$ make selfhost
...
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (12510 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)" || (echo "SELF-HOST FAILED (assembly diverged)"; diff zcc2.s zcc3.s | head -20; exit 1)
SELF-HOST VERIFIED (assembly identical)
```

## Commit Message Draft
```text
fix(ir): implement CFG back-edge liveness extension to resolve bootstrap hang

Goal
Prevent linear scan register clobbering for loop-spanning variables without exploding register pressure.

Outcome
Implemented CFG back-edge detection in ir_asm_number_and_liveness that precisely extends last_use to the loop latch block, achieving a clean self-host bootstrap with zero hangs.

Gates
Gate 1: Self-host byte-identical: `cmp zcc2.s zcc3.s`
[Phase 5] Native C Peephole Optimization... OK (12510 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)"
SELF-HOST VERIFIED (assembly identical)

Bugs caught mid-gate
None — gates ran clean on first attempt with the precise CFG extension logic. The previous attempt (blind first_use < def_seq) was discarded because it caused extreme register pressure and spilled loop counters, leading to zcc2 infinite loops.
```
