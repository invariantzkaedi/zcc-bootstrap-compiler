# CG-IR-012 — IR Lowering Hang on High-Frequency Lexer State Machines

## Status
FIXED — commit after e4485727
DEFERRED — bisect-isolated, not yet root-caused

## Discovered
2026-05-18 — Batch 7 whitelist bisect session

## Symptoms
Adding any of the 4 functions below to `ir_whitelisted()` causes `zcc2`
to hang at 100% CPU during `make selfhost` Phase 2, leaving `zcc3.s`
empty. Timeout 30s confirmed in 15-round bisect.

## Bad functions (4)
- `next_token` — core lexer state machine, large switch + loop
- `read_char` — character advance, tight inner loop
- `read_escape` — escape sequence parser, called inside string lexing loop
- `node_name` — AST node-kind switch, large dispatch table

## Safe functions confirmed in same session (9)
peek_token, peek_char, is_type_token, lookup_keyword,
lookup_keyword_fallback, expect, node_ptr_elem_size,
ptr_elem_size, get_member_size

## Hypothesis
Same LICM+PGO inverted interval class as BUG-3 (fixed in commit
`7979f6d0`), but triggered at a different seam. All 4 bad functions
share: large switch dispatch OR tight inner loop OR both. The IR
lowering pass likely produces another inverted interval that the
linear scan allocator mishandles under PGO block reordering.

## Investigation plan
1. Whitelist `next_token` alone, run selfhost under GDB with
   `set pagination off` + `run` to catch the hang site
2. Compare IR output of `next_token` vs `peek_token` (safe) —
   look for inverted def_seq/last_use pairs in the liveness dump
3. Check if BUG-3 fix boundary conditions cover large switches
   (>16 arms) — may need to extend the sweep

## Prerequisite
`make selfhost` must be green before opening this investigation.
Current state: GREEN (commit `7979f6d0` + Batch 7 landed clean).

## Related
- BUG-3 FIX: `compiler_passes.c` — inverted interval backward expansion
- Bisect artifact: `tickets/batch7-bisect-20260518.md`
- Gate evidence: `tickets/b302c11-gate-evidence.md`
