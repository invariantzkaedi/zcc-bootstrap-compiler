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

## Revert — 2026-05-19
Commit ebfb7d68 reverted in 8a133ef4.
Reason: bounded latch expansion fix passes gcc-built selfhost gate but
produces a self-host regression — zcc2 generates broken ELF when
compiling itself with the new liveness intervals. 0 peephole elisions
in Stage 2 confirmed the codegen path diverged.
Root cause of regression: unknown. Likely the new interval boundaries
change register assignments enough that zcc2 miscompiles a critical
function in its own codegen path.
Next investigation: compare zcc2.s output with vs without the fix to
find which function diverges.
Status: REOPENED

## Revert 2 — 2026-05-19
Commit e4b5494b reverted in 66f826b7.
Fix: backward-jump latch extension in regalloc.c build_intervals().
Result: gcc-built selfhost PASS, self-hosted zcc2 hang (zcc3.s=0 bytes).
Pattern: both fixes correct under gcc, both fail under self-host.
Root cause: next_token register pressure under extended intervals
causes degenerate allocator behavior in self-hosted binary.
Next approach: source-level split of next_token OR spill handling fix.
Status: REOPENED — self-host stable fix required.

## Revert 3 — 2026-05-19
Commit 6f760335 reverted in b82dd365.
Fix: CFG back-edge detection in compiler_passes.c — block_start_seq/
block_end_seq tracking, back-edge via topological order, precise latch
boundary extension.
Result: gcc-built selfhost PASS, self-hosted zcc2 hang (zcc3.s=0 bytes).
Pattern: ALL four liveness fixes correct under gcc, ALL fail self-hosted.
Root cause confirmed: IR backend has no spill support (N_PHYS_REGS=7).
next_token (588 blocks) exceeds register budget under any correct liveness
extension. Allocator has no fallback when pressure > 7 registers.
Resolution: next_token permanently deferred until spill support lands.
Status: CLOSED-BLOCKED — requires IR spill implementation.

## Revert 4 — 2026-05-19
Commit a571e2b8 reverted in b0a56d0f.
Fix: BFS topological liveness order + heap first_use tracking +
     linear scan eviction (most complete fix attempted).
Result: gcc-built selfhost PASS, zcc2.s=3.6MB generated clean,
        zcc3.s=0 bytes — self-hosted hung on assembly phase.
Pattern: 5 fixes, 5 reverts. All correct under gcc, all hang self-hosted.
FINAL VERDICT: next_token requires full spill-to-stack IR support.
No further liveness fixes should be attempted without spill implementation.
Status: PERMANENTLY DEFERRED — open CG-IR-013 for spill support.
