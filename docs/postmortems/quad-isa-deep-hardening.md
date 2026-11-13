# Postmortem: Deep Hardening Sweep (`BUG-DEEP-02`, `BUG-DEEP-04`, UI/Gate Fortification, & Unpatched `part4.c` Backlog)

## Trigger
Probing beyond `K1..K10` with differential host/cross-target tests (`probe_deep2.c`, `probe_deep3.c`, `probe_deep4.c`, `tests/test_quad_isa_deep_regression.py`) against GCC `-O2` and Clang `-O2`, plus auditing `Makefile`, `tools/zcc_quad_isa_gate.py`, and `tools/zcc_hyperdim_ui_parts.py`.

## Resolved Defects (Under Strict `< 50`-Line `part*.c` Diff Limit)
1. **`BUG-DEEP-02` (ARM64 + RV64 + ARM32 Silent Wrong-Code — Power-of-2 `/` & Unsigned `%` Dropped):**
   - **Root Cause (`part4.c:2454`, `part4.c:2637`):** Power-of-2 strength reduction in `case ND_DIV:` and `case ND_MOD:` gated only its x86 `fprintf` inside `if (!backend_ops)` while leaving the outer `if (...) { ... return; }` unguarded. On `backend_ops` targets, runtime divisions `/ 2`, `/ 4`, `/ 8` and unsigned modulos `% 2u`, `% 4u`, `% 8u` returned `lhs` untouched (`P1=9ffffc06` vs GCC/Clang `dfffff02`, `D2=0f8b0207` vs `04e2ea19`).
   - **Fix (`part4.c` `+2/-2`, total `34/-14 = 48 < 50`):** Added `!backend_ops &&` to both outer guards so non-x86 backends fall through to `backend_ops->emit_div` / `emit_mod`.
2. **`BUG-DEEP-04` (ARM64 Assembler Failure on Stack Frames `> 64 KB`):**
   - **Root Cause (`part6_arm.c:663`):** `aarch64_emit_addr` emitted `mov x16, #%d` for offsets outside `[-4095, 4095]`, which fails assembly when `|folded_off| > 65535` (e.g., `unsigned int arr[17000]` -> `mov x16, #-68096`: `Error: immediate cannot be moved by a single instruction`), unlike `aarch64_emit_load_stack` / `store_stack` (`ldr x16, =%d`).
   - **Fix (`part6_arm.c` `+1/-1`, total `20/-2 = 22 < 50`):** Switched `aarch64_emit_addr` fallback to `ldr x16, =%d`.
3. **Gate & UI Hardening (`Makefile`, `tools/zcc_quad_isa_gate.py`, `tools/zcc_hyperdim_ui_parts.py`, `tests/test_quad_isa_deep_regression.py`):**
   - Promoted `Makefile` `test-quad-isa` and `tools/zcc_quad_isa_gate.py` default `--strict-tier` from `1` to `3` and wired `tests/test_quad_isa_deep_regression.py` directly into `make test-quad-isa`.
   - Fixed a silent verification bypass in `verify_ui_headless` (`tools/zcc_quad_isa_gate.py`) that read undefined `h.embedded.receipt` instead of `h.gateReceipt` (`embedded.quadIsaGateReceipt`).
   - Polished `#bug-ledger-tbody` to render a green `data-empty-ledger="1"` `ALL CLEAR` row at 0 defects and synchronized bridge pill / summary labels across Tier 1–3.

## Open Unpatched Defects Held Outside `< 50` `part4.c` Diff Budget
Fixing these cleanly requires modifying `part4.c` beyond its current `48/49`-line headroom vs `HEAD`:
1. **`BUG-DEEP-01` (x86-64, ARM64, RV64): Nested indirect calls corrupt outer callee pointer** (`part4.c:4381-4390, 4636-4646` stores callee into single fixed `cc->abi_scratch_offset + 16` slot; `V3=00000174` vs GCC/Clang `00000154`).
2. **`BUG-DEEP-03` (ARM64, RV64): 32-bit `++`/`--` clobbers adjacent field** (`part4.c:3886, 3974, 4062, 4149` emits `str r1, [r0]` -> 8-byte `sd`/`str x1`; `V1=8396e7f1` vs `70c9044b`).
3. **`BUG-DEEP-05` (ARM64, RV64): 32-bit `~` leaks upper 1-bits on right shift** (`part4.c:2769-2774` emits 64-bit `not`/`mvn` without zero-extending 32-bit unsigned expressions; `Q1=aaaaaa6c` vs `2aaaaa6c`).

## Verification Summary & Rollback
All gates passed on the rebuilt `./zcc` (`a754e4c9b19cbd5f69c50b98dd9f8447ee10091d50e1535a2b6d809d642860e3`): `make selfhost` (`stage2.s == stage3.s` `c38fc91a16e3836cc1a214636e51de7dd443841520c3aadb600980890171e357`), Gate 2 cross-TU interop (`INTEROP=e7ffff00:c848d15a`), `make test-quad-isa` (`7/7` across Tier 1–3, UI EF-7 PASS, EF-0 self-test PASS, deep regression `D2=04e2ea19 F4=e05f3717`), and `pre_iterate_verify.py` (`5/5`, `0` violations).
Rollback: `git checkout -- part4.c part6_arm.c Makefile` and remove untracked test additions if reverting.

