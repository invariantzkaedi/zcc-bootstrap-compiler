# HYGIENE-PP-001 — Make `pp_peek` Purely Observational

## Status
🟡 OPEN — Pre-implementation ticket. No code modified.

## Invariant to enforce
```text
pp_peek() must be a pure O(1) observation of the current character.
Frame-pop transitions belong exclusively to pp_next().
```

## Why this matters
`pp_peek` currently performs a `while (pos >= len && depth > barrier)` drain loop
that pops exhausted input frames, frees heap buffers, and restores blocked-macro
lists. This makes a function whose *name* implies observation into an implicit
state-mutation operator.

The previous bugs in this chain both stemmed from exactly this property:
- `7d049358`: `pp_parse_ident` called `pp_peek` for boundary detection and
  accidentally popped the macro context that held the hide-set.
- The pop-barrier guard at `7d049358` is a correct short-term fence, but it
  works *around* the architectural smell rather than *removing* it.

PP-001 retires the root smell.

---

## Caller Inventory (all 48 references — classified)

### Category A — Observation-Only (rely on current-char value only)
These callers read the character `pp_peek` returns and branch/consume based on it.
With a pure `pp_peek`, they work identically provided frames are drained by
`pp_next` after each `pos++`. **No caller-site changes required.**

| Lines | Function | Pattern |
|-------|----------|---------|
| 339 | `pp_skip_whitespace` | `while (pp_peek == ' ' \| '\t') pp_next` |
| 354 | `pp_parse_ident` | `while (is_ident_char(pp_peek)) out[i++] = pp_next` |
| 387–411 | `pp_read_line` | `while (pp_peek != 0)` + `pp_peek` for branch checks |
| 423–461 | `pp_read_macro_body` | `while (pp_peek != 0)` + branch checks |
| 733–746 | `pp_parse_params` | `while (pp_peek != ')' && != '\n' && != 0)` |
| 1067,1085,1088 | `pp_parse_directive` (define/include) | branch on `'('`, `'<'`, `'"'` |
| 1112 | `pp_parse_directive` (trailing) | `if (pp_peek == '\n') pp_next` |
| 1139–1210 | `pp_expand_ident` (__attribute__ parser) | whitespace/paren balance scan |
| 1251 | `pp_expand_ident` | `if (pp_peek != '(')` — invocation detection |
| 1269–1270 | `pp_expand_ident` (arg loop) | `while (pp_peek != 0)` |
| 1323 | `pp_expand_ident` (backslash) | `if (pp_peek == '\n')` |
| 1568–1620 | `pp_parse_target_depth` | string/comment/ident scanning loop |
| 1595–1608 | `pp_parse_target_depth` (comments) | `while (pp_peek != 0)` |

### Category B — Indirectly Relies on Frame Drain (via `pp_next` chain)
`pp_next` calls `pp_peek` internally (line 286). Once `pp_peek` is pure, `pp_next`
must perform the drain itself immediately after `pos++`. **This is the single
structural change.**

| Lines | Function | Note |
|-------|----------|------|
| 286 | `pp_next` | **THE CHANGE POINT** — add drain loop after `pos++` |

### Category C — The Definition Itself
| Lines | Function | Note |
|-------|----------|------|
| 254–275 | `pp_peek` | Drain loop REMOVED; becomes pure `src[pos]` reader |

---

## The Structural Change (both functions, surgical)

### Current `pp_peek` (254–275)
```c
static char pp_peek(PPState *state) {
    static int pp_peek_cnt = 0;
    if (++pp_peek_cnt % 500000 == 0) { ... telemetry ... }
    while (state->pos >= state->len && state->input_depth > state->pop_barrier) {
        if (state->alloc_buf) free(state->alloc_buf);
        state->input_depth--;
        state->num_blocked = state->input_stack[state->input_depth].num_blocked;
        for (int bi = 0; bi < state->num_blocked; bi++) { ... restore ... }
        state->src       = state->input_stack[state->input_depth].src;
        state->pos       = state->input_stack[state->input_depth].pos;
        state->len       = state->input_stack[state->input_depth].len;
        state->alloc_buf = state->input_stack[state->input_depth].alloc_buf;
    }
    if (state->pos >= state->len) return 0;
    if (state->src[state->pos] == '\r') return '\n';
    return state->src[state->pos];
}
```

### Proposed `pp_peek` (pure)
```c
static char pp_peek(PPState *state) {
    static int pp_peek_cnt = 0;
    if (++pp_peek_cnt % 500000 == 0) { ... telemetry ... }
    /* Pure observation — no frame transitions. */
    if (state->pos >= state->len) return 0;
    if (state->src[state->pos] == '\r') return '\n';
    return state->src[state->pos];
}
```

### Proposed `pp_next` (absorbs the drain)
```c
static char pp_next(PPState *state) {
    char c = pp_peek(state);
    if (c) {
        if (c == '\r') {
            if (state->input_depth == 0) state->line++;
            state->pos++;
            if (state->pos < state->len && state->src[state->pos] == '\n') state->pos++;
        } else {
            if (c == '\n' && state->input_depth == 0) state->line++;
            state->pos++;
        }
        /* HYGIENE-PP-001: drain exhausted frames after advancing pos. */
        while (state->pos >= state->len && state->input_depth > state->pop_barrier) {
            if (state->alloc_buf) free(state->alloc_buf);
            state->input_depth--;
            state->num_blocked = state->input_stack[state->input_depth].num_blocked;
            for (int bi = 0; bi < state->num_blocked; bi++) {
                state->blocked_macros[bi] = state->input_stack[state->input_depth].blocked_macros[bi];
            }
            state->src       = state->input_stack[state->input_depth].src;
            state->pos       = state->input_stack[state->input_depth].pos;
            state->len       = state->input_stack[state->input_depth].len;
            state->alloc_buf = state->input_stack[state->input_depth].alloc_buf;
        }
    }
    return c;
}
```

---

## Critical Correctness Questions to Resolve Before Editing

### Q1: Are there any callers that use `pp_peek` as a drain-trigger WITHOUT following it with `pp_next`?

Review result: **NO.** Every Category A caller that uses `pp_peek` for a non-zero
result always follows with `pp_next` to consume the character. The only termination
condition is `pp_peek == 0`, at which point no consumption occurs — and in the
proposed design, `0` is returned when `pos >= len` (since `pp_next` has already
drained the frame). This is equivalent behaviour.

### Q2: Does the `pop_barrier` guard in `pp_parse_ident` (7d049358) still serve
a purpose after PP-001?

**YES, but it changes role.** Currently the barrier prevents `pp_peek`'s drain loop
from firing during boundary scans. After PP-001, `pp_peek` has no drain loop, so
the barrier's effect on `pp_peek` is moot. However, `pp_parse_ident` calls
`pp_next` to consume characters, and `pp_next` will now contain the drain loop.
The barrier must therefore remain in `pp_parse_ident` to prevent `pp_next`'s
drain from popping frames mid-identifier-scan. The three-line guard at 7d049358
is **still required** after PP-001.

### Q3: The `pp_parse_target_depth` loop (line 1566) checks `state->input_depth < target_depth`
as a termination condition. Does this still work?

```c
while (1) {
    char c = pp_peek(state);
    if (state->input_depth < target_depth || c == 0) break;
    ...
}
```

With pure `pp_peek`: when the current frame is exhausted, `pp_peek` returns `0`
(no drain). The loop breaks on `c == 0`. The drain fires on the *next* `pp_next`
call inside the loop body. Since every non-zero `c` path calls `pp_next`, the
drain happens correctly before the loop condition is re-evaluated. **Correct.**

### Q4: The `pp_next` drain runs after `pos++`. Is the drain condition
`pos >= len` still correct at that point?

Yes. After `pos++`, if `pos == len`, the current frame is exhausted. The drain
pops to the parent frame, restoring `src`, `pos`, `len` from the saved context.
The next `pp_peek` or `pp_next` call then reads from the parent frame's position.
This matches exactly what the current `pp_peek` drain does — just relocated.

---

## Required Gates

- Gate 1: `cmp zcc2.s zcc3.s` — self-host byte-identical
- Gate 2: `./zcc --pp-only scratch/test_macro.c` — `vfsList` regression (7d049358)
- Gate 3: `./zcc --pp-only scratch/test_hideset_deep.c` — 32-deep chain
- Gate 4: `make compat-smoke` — 9/9 targets
- Gate 5: `timeout 10 ./zcc --pp-only sqlite3.c` — SQLite3 amalgamation

Gate 2 is the most sensitive: it re-runs the exact pattern that caused the original
infinite loop. If the drain relocation regresses hide-set semantics, Gate 2 fails
before anything else.

---

## Implementation Risk Assessment

**Low-Medium.** The drain logic is being moved verbatim, not rewritten. The
correctness argument (Q1–Q4 above) is clean. The pop_barrier guard at 7d049358
remains in force and provides the primary safety invariant. The five-gate suite
provides fast feedback if any edge case is missed.

The one non-obvious risk: callers that call `pp_peek` in a loop *without* always
following with `pp_next` when the result is non-zero. The inventory above shows
no such caller exists, but this must be re-verified line-by-line during the actual
edit session before committing.

---

## Diff Budget

Expected net change: ~20 lines (remove drain from `pp_peek`, add drain to
`pp_next`, with inline comment). Well within the 50-line surgical budget.

Files touched: `part0_pp.c` only.

---

## Parent commits
- `7d049358` — pop-barrier fix (directly related; guard stays in place)
- `596287d0` — PP_MAX_HIDESET bounds (directly related; loop uses same constant)
