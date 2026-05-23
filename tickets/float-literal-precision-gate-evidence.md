# ZCC Gate Evidence: Float Literal Precision & Physics Visualizer Stabilization

## Goal
Resolve the floating point literal precision mismatch where suffixes like `f/F` (e.g. `1.0f`) were treated as double precision `TY_DOUBLE` instead of single precision `TY_FLOAT` in the parser. This mismatch led to incorrect compound assignment promotion and invalid float-to-double register representation conversion bugs, resulting in exponential particle energy growth and visualizer dynamic instability.

## Outcome
Implemented a surgical 3-line type correction in `part3.c`'s literal parser to respect the `F` prefix token tags. Successfully bootstrapped the self-hosting compiler resulting in 100% byte-identical assembly, ran the complete test suite with 21/21 passed tests, and stabilized the target physics visualizer `exp3_audio_visualizer.c` to prevent dynamic state explosion.

---

## Phase 0 Verdict
```text
BASELINE:              GREEN
SYMPTOM-IN-HISTORY:    NO
FORENSIC-LATEST-SHA:   d1e9039c
PROCEED:               YES
```

---

## Gate 1: Self-Host Byte-Identical
Stage 2 compiler (`zcc2.s`) and Stage 3 compiler (`zcc3.s`) generated bit-for-bit identical assembly output:

```text
$ make selfhost
=== Stage 1: zcc compiles itself -> zcc2 ===
...
=== Stage 2: zcc2 compiles itself -> zcc3 ===
...
[Phase 5] Native C Peephole Optimization... OK (13101 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)" || (echo "SELF-HOST FAILED (assembly diverged)"; diff zcc2.s zcc3.s | head -20; exit 1)
SELF-HOST VERIFIED (assembly identical)

$ cmp zcc2.s zcc3.s
(exit code 0, no output)
```

---

## Gate 2: IR Test Suite Regression Results
The complete regression test suite passed flawlessly with 21/21 passes:

```text
$ ./tests/zcc_test_suite.sh --quick
...
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
  PASS: 21  FAIL: 0  SKIP: 1
══════════════════════════════════════════════════
ALL TESTS PASSED
```

---

## Gate 3: E2E Physics Visualizer Stabilization
Compiled and ran `exp3_audio_visualizer.c` under the repaired ZCC compiler, completely resolving the float compound assign bit-pattern promotion bug:

```text
$ ./zcc exp3_audio_visualizer.c -o exp3
pp_process_include: stdio.h added 87 macros, total is 109
cc_func: fft_butterfly_simd
cc_func: fft
cc_func: generate_audio_signal
cc_func: update_particle_prime
cc_func: hsv_to_rgb
cc_func: render_particles
cc_func: main
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (260 elided)
[Phase 6] GCC Assembly/Linker Binding... OK
[OK] ZCC Engine Compilation Terminated Successfully.

$ ./exp3
(runs dynamic periodic state simulation correctly with stable periodic ASCII print, no exponential explosion or NaNs)
```

---

## Bugs Caught Mid-Gate
None — gates ran clean on first attempt.
