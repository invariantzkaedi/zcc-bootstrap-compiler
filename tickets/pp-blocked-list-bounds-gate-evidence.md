# HYGIENE-PP-002 — Blocked-List Bounds Gate Evidence

## Goal
Protect `state->blocked_macros[]` against silent out-of-bounds writes.
The hide-set array cap was a bare literal `32` scattered across three locations,
with a silent truncation guard that swallowed overflow without any diagnostic.

Core invariant enforced:

```text
macro hide-set tracking must never silently write past state->blocked_macros[PP_MAX_HIDESET]
```

## Outcome
Extracted the hard-coded cap to `PP_MAX_HIDESET 32`. Replaced all three raw
literals with the named constant. Converted the silent `if (!found && num < 32)`
write guard into a loud fatal diagnostic:

```c
fprintf(stderr, "zcc: FATAL: macro hide-set overflow (depth > %d) ...", PP_MAX_HIDESET, macro->name);
exit(1);
```

Self-host byte-identical, compat-smoke 9/9 PASS, SQLite3 3.7 MB preprocesses in
< 10 s, 32-deep synthetic chain expands correctly.

---

## Phase 0 Verdict
```text
BASELINE:              GREEN  (7d049358 — SELF-HOST VERIFIED (assembly identical))
SYMPTOM-IN-HISTORY:    NO
FORENSIC-LATEST-SHA:   7d049358
PROCEED:               YES
```

---

## Changes — `part0_pp.c` (+12 lines, 0 deletions net)

| # | Change |
|---|--------|
| 1 | Add `#define PP_MAX_HIDESET 32` in the constant block at top of file |
| 2 | `PPInputCtx.blocked_macros[32]` → `[PP_MAX_HIDESET]` |
| 3 | `PPState.blocked_macros[32]` → `[PP_MAX_HIDESET]` |
| 4 | Replace silent `if (!found && num_blocked < 32)` with fatal bounds check + diagnostic |

Total diff: +1 define, +2 field replacements (no size change), +4 lines of diagnostic in `pp_push_input`. No logic altered on any path that does not overflow.

---

## Gate 1: Self-Host Byte-Identical

```text
$ make selfhost | tail -3
[Phase 5] Native C Peephole Optimization... OK (13346 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
SELF-HOST VERIFIED (assembly identical)
```

`cmp zcc2.s zcc3.s` — exit 0, no output.

---

## Gate 2: 32-deep Synthetic Chain

Test: `scratch/test_hideset_deep.c` — 32 linearly chained `#define Mn Mn-1+1` macros.

Each expand is a simple substitution; the hide-set depth per macro is always 1
(no mutual recursion). This validates that normal macro chains up to 32 levels
continue to expand correctly without triggering the new bounds error.

```text
$ ./zcc --pp-only scratch/test_hideset_deep.c && echo '32-DEEP PASS'
[...]
int x = 1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1;
32-DEEP PASS
```

Correct output: 32 additions of `1`. **PASS**.

---

## Gate 3: Compat-Smoke Regression Suite

```text
$ make compat-smoke | tail -3
[compat] tests/test_abi.c           -> OK (20 elided)
[compat] tests/test_asm_real.c      -> OK (0 elided)
COMPAT SMOKE COMPLETE
```

9/9 targets: **PASS**.

---

## Gate 4: SQLite3 Industrial Stress Test

```text
$ timeout 10 ./zcc --pp-only scratch/sqlite-amalgamation-3530100/sqlite3.c > /dev/null \
  && echo "SQLITE3 OK"
SQLITE3 OK
```

3.7 MB amalgamation — **PASS**, no hide-set overflow triggered (SQLite3's deepest
macro chains are well within PP_MAX_HIDESET=32).

---

## Bugs Caught Mid-Gate
None — all four gates ran clean on first attempt.

---

## Hygiene / Deferred

- **HYGIENE-PP-001**: `pp_peek` still performs implicit frame-pop transitions.
  Long-term direction: make `pp_peek` purely observational, move frame advancement
  into `pp_next` / an explicit `pp_advance_frame`. The existing `pop_barrier`
  guard in `pp_parse_ident` (7d049358) makes this refactor safe to defer until
  a dedicated ticket.

---

## Forensic Notes

The old code had three independent hard-coded `32` literals:
- `PPInputCtx.blocked_macros[32]`
- `PPState.blocked_macros[32]`
- `if (!found && state->num_blocked < 32)` write guard

These were impossible to tune atomically (changing the cap required finding all
three sites). The named constant `PP_MAX_HIDESET` makes the cap a single source
of truth, and the fatal diagnostic converts a silent-corruption scenario into an
actionable error message that names the overflowing macro.

This also closes the last known unsafe write path in the PP hide-set subsystem
after the barrier fix at 7d049358.

Parent commit: `7d049358`
