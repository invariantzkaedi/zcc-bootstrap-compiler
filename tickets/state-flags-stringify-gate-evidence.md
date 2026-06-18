# Gate Evidence — feat(vir): stringify VIR state flags for diagnostics

**Milestone**: `feat(vir): stringify VIR state flags for diagnostics`
**Commit baseline**: `bd4456c4` (provenance JSON — last green commit)

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
- `vir_state_flag_name(uint32_t flag)` — string literal, do NOT free
- `vir_state_flags_to_string(uint32_t flags)` — malloc-owned, caller must free

### zcc_vir.c
- `vir_state_flag_name`: switch over all 7 known VIR_STATE_* constants → "UNKNOWN" fallback
- `vir_state_flags_to_string`: two-pass strlen/strcat, "CLEAN" when flags==0
- `vir_pipeline_provenance_json`: added `"state_names"` field via `vir_state_flags_to_string`;
  allocated separately before main snprintf, freed after both passes
- `vir_pipeline_to_dot`: both edge label patterns updated from `"requires flag 0x%X"` /
  `"invalidates flag 0x%X"` to `"requires %s"` / `"invalidates %s"` via `vir_state_flag_name`

### test_zcc_vir.c
- `test_vir_state_flags_stringify`: 7 known single-flag name checks, UNKNOWN for 0x80000000U
  and 0, CLEAN for flags==0, single-flag string, combined-flag string (pipe separator + all
  three names present), unknown-bit string, provenance JSON `state_names` field + "LOCALIZED"
  presence check
- `test_vir_pass_graph_exporter`: updated two stale assertions from hex label patterns to
  human-readable label substrings
- Both changes registered in main()

---

## Mid-gate bug caught

`test_vir_pass_graph_exporter` held two stale substring assertions that matched the old
`"requires flag 0x..."` / `"invalidates flag 0x..."` DOT edge format. After updating the
DOT exporter to emit named labels, those assertions failed on first run. Fixed assertions
to match new format before proceeding to sanitizer gates.

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
$ ./test_zcc_vir | tail -5
[+] test_vir_state_flags_stringify PASSED.
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
NATIVE_EXIT:0
```

**Result: ALL TESTS PASSED**

---

## Gate 3 — Memory Safety + Leak Detection (ASan + LSan)

```
$ ./test_zcc_vir_lsan (fsanitize=address,leak) | tail -3
[+] test_vir_state_flags_stringify PASSED.
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
LSAN_EXIT:0
```

**Result: 0 errors, 0 leaks**

---

## Gate 4 — Undefined Behavior (UBSan)

```
$ ./test_zcc_vir_ubsan (fsanitize=undefined) | tail -3
[+] test_vir_state_flags_stringify PASSED.
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
UBSAN_EXIT:0
```

**Result: 0 undefined behavior reports**
