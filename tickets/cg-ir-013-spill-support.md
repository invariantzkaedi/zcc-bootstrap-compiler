# CG-IR-013 — Spill-to-Stack Support for the IR Backend Linear Scan Allocator

## Status
OPEN — work plan committed on `spill-work`

## Opened
2026-07-08 — mandated by CG-IR-012 Revert 4 final verdict

## Goal
Implement spill-to-stack support in the `compiler_passes.c` internal linear
scan register allocator (`ZCC_IR_BACKEND=1` path) so that functions whose
register pressure exceeds the 7-register physical pool compile correctly,
unblocking the 4 permanently-deferred functions from CG-IR-012:
`next_token`, `read_char`, `read_escape`, `node_name`.

## Why this exists (blocker history)
CG-IR-012 recorded **5 liveness fixes, 5 reverts** (2026-05-19). Every fix
was correct under gcc-built selfhost; every fix hung the self-hosted
bootstrap (`zcc2` at 100% CPU, `zcc3.s` = 0 bytes). Root cause confirmed in
Revert 3:

> IR backend has no spill support (N_PHYS_REGS=7). next_token (588 blocks)
> exceeds register budget under any correct liveness extension. Allocator
> has no fallback when pressure > 7 registers.

Final verdict (Revert 4):

> next_token requires full spill-to-stack IR support. No further liveness
> fixes should be attempted without spill implementation.

## Scope — which allocator
There are TWO allocators in the tree. This ticket targets exactly one:

- **IN SCOPE**: the internal linear scan in `compiler_passes.c`
  (`ir_asm_number_and_liveness` / `ir_asm_linear_scan`, `N_PHYS_REGS 7`,
  pool: rbx, r10, r11, r12–r15; callee-saved mask rbx+r12–r15).
  Used under `ZCC_IR_BACKEND=1`. Currently has NO fallback at pressure > 7.
- **OUT OF SCOPE**: `regalloc.c` / `regalloc.h` (used by `ir_to_x86.c`).
  That allocator already degrades gracefully — unallocated temps keep their
  stack slots. Do not touch it in this ticket.

## Existing footholds in the code
The allocator already has a spill *concept* but no spill *emission*:

```c
/* compiler_passes.c — LiveInterval already models spill decisions */
typedef struct {
  RegID vreg;
  int start;
  int end;
  int phys_reg;   /* 0..N_PHYS_REGS-1 or -1 if spilled */
  int loop_depth; /* max loop_depth of blocks this interval spans */
} LiveInterval;
```

Missing pieces:
1. **Spill-slot assignment** — a stack slot per spilled vreg, integrated
   into the function's frame size computation.
2. **Spill emission** — store-to-slot after each def of a spilled vreg;
   reload-from-slot before each use.
3. **Eviction heuristic** — when the pool is exhausted, choose the victim
   (prefer: lowest `loop_depth`, then furthest `end`). The `loop_depth`
   field already exists for exactly this purpose.
4. **Reload scratch policy** — reloads need a register. Reserve policy must
   avoid %rax/%rcx/%rdx (div/shift hazards per the pool comment). Either
   (a) reserve one pool register as dedicated scratch (shrinking the
   allocatable pool to 6), or (b) evict-on-demand. Option (a) is simpler
   and self-host-safer; start there.

## Hard constraints (learned the expensive way)
1. **gcc-correct is NECESSARY but INSUFFICIENT.** All 5 reverted fixes
   passed gcc-built selfhost. The gate that matters is self-hosted:
   zcc2 must compile zcc.c to a byte-identical zcc3.s without hanging.
2. **The spill code is compiled by zcc during bootstrap.** New allocator
   code must stay within the C subset zcc compiles correctly — no exotic
   constructs. Revert 1's failure mode (zcc2 emits broken ELF) is what
   happens when this is violated.
3. **Frame sizing is a known bug class.** CG-IR-006 (CWE-121, critical):
   "Stack frame too small for IR spill slots." Spill slots MUST be included
   in the frame-depth pre-scan, and 16-byte call alignment (CG-IR-007
   territory) must be preserved after adding slots.
4. **Hybrid frame ownership.** The AST owns prologue/epilogue; the IR owns
   the body (body_only mode). Spill slots live in AST-managed frame space —
   coordinate slot_base offsets, do not collide with AST locals (CG-IR-008:
   "AST/IR stack slot collision" is the documented failure).
5. **Determinism.** Eviction ties must break deterministically (vreg order),
   or Gate 1 byte-identity fails intermittently. See
   FORENSIC_024_REGALLOC_DETERMINISM.md.

## De-risking sequence (each step is a cheap, committable checkpoint)
- **Step 1 — Flag-gated landing.** Implement behind `ZCC_IR_SPILL=1`
  (default OFF). All gates green with flag off → zero-risk commit.
- **Step 2 — Forced-spill unit test.** Synthetic C function with ~10+
  simultaneously-live vars (pressure > 7 guaranteed). With flag on:
  correct output under BOTH gcc-built zcc AND self-hosted zcc2.
  This is where Reverts 1–5 would have been caught early.
- **Step 3 — The real gate.** Whitelist `next_token` alone in
  `ir_whitelisted()`. Run `make selfhost` with flag on. PASS = no hang,
  zcc2.s/zcc3.s byte-identical. Keep the zcc2.s artifact from before/after
  for diffing — that seam is the established debugging method (Revert 1).
- **Step 4 — Close out CG-IR-012's deferred list.** Add `read_char`,
  `read_escape`, `node_name`. Re-run gates. Update CG-IR-012 status to
  RESOLVED-BY CG-IR-013 (and fix its stale "FIXED/DEFERRED" header block).
- **Step 5 — Flip default.** `ZCC_IR_SPILL` default ON after a soak;
  remove flag in a later hygiene commit.

## Acceptance criteria
- [ ] Spill-slot assignment integrated into frame pre-scan (no CG-IR-006
      recurrence; alignment preserved)
- [ ] Store-after-def / reload-before-use emission for spilled intervals
- [ ] Deterministic eviction heuristic using existing `loop_depth` field
- [ ] Forced-spill unit test in test suite, passing under gcc-built AND
      self-hosted compilers
- [ ] `next_token` whitelisted: `make selfhost` completes, no hang,
      `SELF-HOST VERIFIED (assembly identical)`
- [ ] All 4 CG-IR-012 functions whitelisted with gates green
- [ ] Gate 1: `make selfhost` byte-identical
- [ ] Gate 2: `make compat-smoke` passes
- [ ] Gate 4: `make test` fully green (baseline: PASS 33 / FAIL 0 / SKIP 3)
- [ ] Determinism: repeated builds produce identical assembly
- [ ] CG-IR-012 status block updated; deferred list closed

## Estimated shape
~300–600 lines in `compiler_passes.c` (selection logic partially exists via
`phys_reg = -1`). Dominant cost is the verification loop, not the code:
each attempt = rebuild → selfhost → whitelist → selfhost. Budget 1–3
focused sessions and at least one zcc2.s-diff debugging round.

## Related
- CG-IR-012: `tickets/cg-ir-012-lexer-ir-hang.md` (blocker history, 5 reverts)
- CG-IR-006 / CG-IR-007 / CG-IR-008: frame-size, alignment, slot-collision
  bug corpus entries — the three failure modes spilling can reintroduce
- FORENSIC_024_REGALLOC_DETERMINISM.md — determinism requirements
- `tickets/offset-aware-points-to-issue.md` — if that ticket later chooses
  option (b) (immediate-offset load/store), coordinate `ir_to_x86.c` /
  emission changes; land sequentially for clean bisection
- Downstream unblocks: Rust v2 `match` lowering (large switch dispatch —
  same pressure shape as `next_token`); roadmap item #5 (allocator upgrade)

## Notes
- Prerequisite state: `spill-work` @ `30195bd6` — selfhost GREEN, tests
  33/0/3, GEP points-to fix landed.
- No liveness-extension changes in this ticket. Liveness fixes were reverted
  5× and are explicitly out of scope until spilling works; re-attempt them
  (if still needed) only AFTER Step 4.
