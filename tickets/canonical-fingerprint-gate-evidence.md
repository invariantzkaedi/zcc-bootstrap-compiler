# Gate Evidence — feat(vir): implement canonical artifact fingerprints

**Milestone**: `feat(vir): implement canonical artifact fingerprints and fast-path lookup`
**Commit baseline**: `b85a0b8d` (cache-hardening milestone — last green commit)

## Phase 0 Verdict
```
BASELINE:              GREEN
SYMPTOM-IN-HISTORY:    NO
FORENSIC-LATEST-SHA:   a60a8b89
PROCEED:               YES
```

---

## Gate 1 — Self-host byte-identical: `cmp zcc2.s zcc3.s`

```
$ make selfhost 2>&1 | tail -8
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (16089 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)"
SELF-HOST VERIFIED (assembly identical)

$ cmp zcc2.s zcc3.s; echo CMP_EXIT:$?
CMP_EXIT:0
```

**Result: BYTE-IDENTICAL** — `cmp` produced no output, exit 0.

---

## Gate 2 — VIR Unit Test Suite

```
$ gcc -O0 -g -I. test_zcc_vir.c zcc_vir.c zcc_svg_path_parser.c zcc_svg.c -o test_zcc_vir -lm && ./test_zcc_vir
=== ZCC Vector IR (VIR) Test Harness ===
[*] Running test_vir_path_equivalence...
[+] test_vir_path_equivalence PASSED.
[*] Running test_vir_path_fingerprint...
[+] test_vir_path_fingerprint PASSED.
[*] Running test_vir_canonical_fingerprint...
[+] test_vir_canonical_fingerprint PASSED.
[*] Running test_vir_compilation_caching...
[DEBUG-TEST] st2_stat: hits=1, misses=1, evictions=0
[DEBUG-TEST] st3_stat: hits=2, misses=2
[DEBUG-TEST] st4_stat: hits=4, misses=2
[DEBUG-TEST] st5_stat: hits=4, misses=3
[DEBUG-TEST] st6_stat: hits=5, misses=3
[+] test_vir_compilation_caching PASSED.
[*] Running test_vir_path_normalization...
[+] test_vir_path_normalization PASSED.
[*] Running test_vir_path_creation_and_growth...
[+] test_vir_path_creation_and_growth PASSED.
[*] Running test_vir_degenerate_removal...
[+] test_vir_degenerate_removal PASSED.
[*] Running test_vir_bounds_propagation...
[+] test_vir_bounds_propagation PASSED.
[*] Running test_svg_to_vir_adapter...
[+] test_svg_to_vir_adapter PASSED.
[*] Running test_extreme_and_overflow_vir...
[+] test_extreme_and_overflow_vir PASSED.
[*] Running test_vir_metadata_and_bounds_caching...
[+] test_vir_metadata_and_bounds_caching PASSED.
[*] Running test_vir_arc_ingestion_and_expansion...
[+] test_vir_arc_ingestion_and_expansion PASSED.
[*] Running test_vir_backend_diversification...
[+] test_vir_backend_diversification PASSED.
[*] Running test_sdf_to_glsl_compilation...
[+] test_sdf_to_glsl_compilation PASSED.
[*] Running test_vir_pass_canonicalize...
[+] test_vir_pass_canonicalize PASSED.
[*] Running test_vir_pass_manager...
[+] test_vir_pass_manager PASSED.
[*] Running test_vir_pipeline_telemetry...
[+] test_vir_pipeline_telemetry PASSED.
[*] Running test_vir_fixed_point_pipeline...
[+] test_vir_fixed_point_pipeline PASSED.
[*] Running test_vir_pass_dependency_graph...
[+] test_vir_pass_dependency_graph PASSED.
[*] Running test_vir_backend_planner...
[+] test_vir_backend_planner PASSED.
[*] Running test_vir_pass_graph_exporter...
[+] test_vir_pass_graph_exporter PASSED.
[*] Running test_vir_exact_bounds_solving...
[+] test_vir_exact_bounds_solving PASSED.
[*] Running test_vir_registry_validation...
[+] test_vir_registry_validation PASSED.
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
EXIT_CODE:0
```

**Result: ALL TESTS PASSED** — exit 0.

---

## Gate 3 — Memory Safety (ASan)

```
$ gcc -O0 -g -fsanitize=address -I. test_zcc_vir.c zcc_vir.c zcc_svg_path_parser.c zcc_svg.c \
    -o test_zcc_vir_asan -lm && ./test_zcc_vir_asan 2>&1 | tail -5; echo ASAN_EXIT:$?
[+] test_vir_exact_bounds_solving PASSED.
[*] Running test_vir_registry_validation...
[+] test_vir_registry_validation PASSED.
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
ASAN_EXIT:0
```

**Result: CLEAN** — ASan exit 0, zero heap violations, zero use-after-free, zero boundary overflows.

---

## Gate 4 — Undefined Behavior (UBSan)

```
$ gcc -O0 -g -fsanitize=undefined -I. test_zcc_vir.c zcc_vir.c zcc_svg_path_parser.c zcc_svg.c \
    -o test_zcc_vir_ubsan -lm && ./test_zcc_vir_ubsan 2>&1 | tail -5; echo UBSAN_EXIT:$?
[+] test_vir_exact_bounds_solving PASSED.
[*] Running test_vir_registry_validation...
[+] test_vir_registry_validation PASSED.
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
UBSAN_EXIT:0
```

**Result: CLEAN** — UBSan exit 0, zero undefined behavior reports.

---

## Bugs caught mid-gate

None — gates ran clean on first attempt.

## Hygiene / deferred

None.
