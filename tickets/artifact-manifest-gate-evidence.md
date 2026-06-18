# Gate Evidence — feat(vir): add artifact manifests and execution plan introspection

**Milestone**: `feat(vir): add artifact manifests and execution plan introspection`
**Commit baseline**: `0d9a4ba2` (SVG lifecycle fix — last green commit)

## Phase 0 Verdict
```
BASELINE:              GREEN
SYMPTOM-IN-HISTORY:    NO
FORENSIC-LATEST-SHA:   a60a8b89
PROCEED:               YES
```

---

## Changes Implemented

### zcc_vir.h
- `VirArtifactManifest` struct (fingerprint, schema_version, state_flags, segment_count, bounds)
- `vir_path_manifest()` declaration — non-mutating, zero-fills bounds when EXACT_BOUNDS absent
- `vir_manifest_verify()` declaration — field-by-field comparison, returns 0 on first mismatch
- `VirExecutionPlan` struct (passes[32], count, target_state, current_state)
- `vir_build_execution_plan()` declaration — read-only dependency walk, never mutates path
- `vir_execute_plan()` declaration — routes through vir_run_pipeline_with_deps

### zcc_vir.c
- `vir_path_manifest()` — calls `vir_path_canonical_fingerprint` (fast-path if localized),
  folds schema_version, state_flags, segment_count, and conditional bounds
- `vir_manifest_verify()` — captures live manifest and compares all fields
- `collect_deps_for_state()` — static recursive dependency walker (read-only mirror of
  schedule_and_run_pass, never calls run(), never touches path->state_flags)
- `vir_build_execution_plan()` — iterates missing state bits, calls collect_deps_for_state,
  returns early with count==0 when path already satisfies target
- `vir_execute_plan()` — zero-count fast-path returns 1; otherwise delegates to
  vir_run_pipeline_with_deps

### test_zcc_vir.c
- `test_vir_artifact_manifest()` — capture, positive verify, negative verify (post-mutation),
  NULL safety, bounds sanity checks
- `test_vir_execution_plan()` — empty plan for converged path, full plan for raw path,
  execute plan and confirm state_flags, zero-count plan, NULL safety
- Both registered in main()

---

## Gate 1 — Self-host byte-identical: `cmp zcc2.s zcc3.s`

```
$ make selfhost 2>&1 | tail -3
[OK] ZCC Engine Compilation Terminated Successfully.
SELF-HOST VERIFIED (assembly identical)

$ cmp zcc2.s zcc3.s; echo CMP_EXIT:$?
CMP_EXIT:0
```

**Result: BYTE-IDENTICAL**

---

## Gate 2 — Native Test Suite

```
$ gcc -O0 -g -I. test_zcc_vir.c zcc_vir.c zcc_svg_path_parser.c zcc_svg.c \
    -o test_zcc_vir -lm && ./test_zcc_vir | grep -E '(PASSED|FAILED|JACKPOT)'
[+] test_vir_registry_validation PASSED.
[+] test_vir_artifact_manifest PASSED.
[+] test_vir_execution_plan PASSED.
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
NATIVE_EXIT:0
```

**Result: ALL TESTS PASSED**

---

## Gate 3 — Memory Safety + Leak Detection (ASan + LSan)

```
$ gcc -O0 -g -fsanitize=address,leak -I. test_zcc_vir.c zcc_vir.c \
    zcc_svg_path_parser.c zcc_svg.c -o test_zcc_vir_lsan -lm \
    && ./test_zcc_vir_lsan 2>&1 | tail -2
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
LSAN_EXIT:0
```

**Result: 0 errors, 0 leaks**

---

## Gate 4 — Undefined Behavior (UBSan)

```
$ gcc -O0 -g -fsanitize=undefined -I. test_zcc_vir.c zcc_vir.c \
    zcc_svg_path_parser.c zcc_svg.c -o test_zcc_vir_ubsan -lm \
    && ./test_zcc_vir_ubsan 2>&1 | tail -2
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
UBSAN_EXIT:0
```

**Result: 0 undefined behavior reports**

---

## Bugs caught mid-gate

None — all gates ran clean on first attempt.

## Hygiene / deferred

HYGIENE-001: `collect_deps_for_state` is static — intentionally not exposed in the header.
The public surface is `vir_build_execution_plan` only.
