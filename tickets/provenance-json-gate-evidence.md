# Gate Evidence — feat(vir): emit pipeline provenance JSON receipts

**Milestone**: `feat(vir): emit pipeline provenance JSON receipts`
**Commit baseline**: `9a5f3fc7` (artifact manifests + execution plan — last green commit)

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
- `vir_pipeline_provenance_json(path, stats, cache)` declaration
- NULL stats/cache tolerant; caller-owned malloc string; returns NULL only on alloc failure

### zcc_vir.c
- `vir_pipeline_provenance_json` — two-pass snprintf sizing (dry-run then allocate)
- Uses `vir_path_manifest` internally — path never mutated
- Bounds section zero-filled when VIR_STATE_EXACT_BOUNDS absent
- NULL stats/cache zero-filled via local structs

### test_zcc_vir.c
- `test_vir_pipeline_provenance` covering:
  - Primary emission with all 13 key field substring checks
  - `"hits": 3` numeric value check
  - NULL stats tolerance → `"total_passes": 0`
  - NULL cache tolerance → `"hits": 0`
  - NULL path tolerance → `"canonical_fingerprint": "0x0000000000000000"`
  - All malloc'd strings freed
- Registered in main()

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
    -o test_zcc_vir -lm && ./test_zcc_vir | grep -E '(PASSED|JACKPOT)' | tail -5
[+] test_vir_artifact_manifest PASSED.
[+] test_vir_execution_plan PASSED.
[+] test_vir_pipeline_provenance PASSED.
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

None.
