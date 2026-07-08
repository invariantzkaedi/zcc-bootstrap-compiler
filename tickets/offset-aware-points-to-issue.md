# opt(pointer_ssa): offset-aware points-to analysis to recover non-zero GEP rewrites

## Context

The Pointer SSA rewrite pass (`opt_pointer_ssa_rewrite_pass` in `src/opt/pointer_ssa.c`) originally propagated points-to bases through **all** `OP_GEP` instructions without inspecting the offset index. This caused struct/array member accesses (e.g. `cc->pos`) to be incorrectly resolved to offset 0 of the base alloca, producing memory corruption and compiler segfaults.

The fix on the `spill-work` branch (`opt(pointer_ssa): restrict GEP points-to propagation to constant offset 0`, commit `c8cf9fd`) resolved the miscompile **conservatively**: points-to targets are now only propagated through `OP_GEP` when the index is statically verified to be `OP_CONST` with value `0` (or when no index operand is present). All non-zero or variable GEP offsets are marked untracked.

This is correct but leaves optimization opportunity on the table: loads/stores through **constant non-zero offsets** (the common struct-member-access pattern) are no longer candidates for indirect load/store rewriting, even though they are statically analyzable.

## Current implementation

Conservative GEP propagation as of `c8cf9fd` — only constant-0 indices propagate the base:

```c
/* src/opt/pointer_ssa.c#L42-L61 */
} else if (ins->op == OP_GEP) {
    bool safe_gep = true;
    if (ins->n_src > 1) {
        RegID idx_reg = ins->src[1];
        if (idx_reg < MAX_INSTRS) {
            Instr *idx_def = fn->def_of[idx_reg];
            if (idx_def && idx_def->op == OP_CONST) {
                if (idx_def->imm != 0) {
                    safe_gep = false;
                }
            } else {
                safe_gep = false;
            }
        } else {
            safe_gep = false;
        }
    }
    if (safe_gep && ins->src[0] < MAX_INSTRS) {
        target = points_to[ins->src[0]];
    }
}
```

The rewrite site substitutes the base register directly — this only works because tracked offsets are guaranteed 0:

```c
/* src/opt/pointer_ssa.c#L154-L172 */
if (ins->op == OP_LOAD && ins->n_src >= 1) {
    RegID ptr_reg = ins->src[0];
    if (ptr_reg < MAX_INSTRS) {
        RegID base = points_to[ptr_reg];
        if (base != 0 && base != AMBIGUOUS && base < MAX_INSTRS && (!escaped || !escaped[base])) {
            ins->src[0] = base;
            rewrites++;
        }
    }
} else if (ins->op == OP_STORE && ins->n_src >= 2) {
    RegID ptr_reg = ins->src[1];
    if (ptr_reg < MAX_INSTRS) {
        RegID base = points_to[ptr_reg];
        if (base != 0 && base != AMBIGUOUS && base < MAX_INSTRS && (!escaped || !escaped[base])) {
            ins->src[1] = base;
            rewrites++;
        }
    }
}
```

Full file at the conservative fix: https://github.com/izkaedi-ui/ZCC/blob/c8cf9fd234be43fd048ec56337f60d5c091f96dd/src/opt/pointer_ssa.c

## Proposal

Extend the points-to analysis to track `(base_alloca, constant_offset)` pairs instead of bare bases:

1. **Tracking structure**: replace the flat `RegID *points_to` array with a per-register record, e.g. `{ RegID base; int64_t offset; bool valid; }`. `AMBIGUOUS` (currently sentinel `65537`) becomes `valid = false` or a dedicated flag.
2. **Propagation**: when walking `OP_GEP` with a constant index, accumulate the offset into the tracked pair rather than dropping it — e.g. `GEP %p, base, 8` followed by `GEP %q, %p, 4` tracks `%q → (base, 12)`. `OP_COPY` propagates the pair unchanged. Variable (non-`OP_CONST`) GEP indices remain untracked.
3. **Rewrite materialization** (key design decision): a non-zero tracked offset cannot be rewritten by register substitution (`ins->src[0] = base`) — no register holds `base + offset`. Options:
   - (a) materialize a new `OP_GEP base, #offset` with the folded constant and point the load/store at it, or
   - (b) extend `OP_LOAD`/`OP_STORE` with an immediate offset operand and teach the x86 lowering to fold it into the addressing mode (`off(%base)`).
   Option (b) produces better code but touches the IR schema and `ir_to_x86.c`; option (a) is contained within the pass. Decide before implementation.
4. **`mem_points_to` keying**: the memory map is currently keyed by base alloca alone (`mem_points_to[base]`, line 91) — pointer-stores to *different fields* of the same struct already collapse into one slot and degrade to `AMBIGUOUS`. Offset-aware tracking must key this map by `(base, offset)` to avoid spurious ambiguity.
5. **Safety gates preserved**: keep all existing escape checks (`OP_CALL` args, `OP_RET`, `OP_STORE` value escapes — lines 111–147), PHI merge ambiguity rejection (PHI pairs must match on **both** base and offset to merge), and determinism guarantees from the July 7 safety work.
6. **Overflow/bounds**: reject accumulation if the combined offset overflows or exceeds the alloca's known size (when available).

## Acceptance criteria

- [ ] `(base, constant_offset)` pair tracking implemented in `src/opt/pointer_ssa.c`
- [ ] Rewrite materialization strategy (option a or b) decided and documented in the pass header comment
- [ ] `mem_points_to` keyed by `(base, offset)` — pointer stores to distinct fields of one struct no longer collapse to `AMBIGUOUS`
- [ ] Existing assertion still holds: exactly 2 rewrites in `pointer_deref` on `t_ptr.c` (per `zcc_test_suite.sh` grep check) — or updated intentionally with justification if the new analysis legitimately rewrites more
- [ ] New test cases covering: single-level constant-offset member access, chained GEP offset accumulation, variable-index GEP (must remain untracked), offset that escapes via call, PHI merging identical vs. differing `(base, offset)` pairs
- [ ] Gate 1: `make selfhost` byte-identical (`SELF-HOST VERIFIED (assembly identical)`)
- [ ] Gate 2: `make compat-smoke` passes
- [ ] Gate 4: `make test` fully green (currently PASS: 33, FAIL: 0, SKIP: 3)
- [ ] Determinism check: repeated runs produce identical rewrite counts and assembly

## References

- Conservative fix: `spill-work` branch, commit `c8cf9fd` — `opt(pointer_ssa): restrict GEP points-to propagation to constant offset 0`
- Original pass introduction: `a1181351` — `opt(pointer_ssa): Implement Points-To Rewrite pass for indirect load/store dereferences`
- Safety gates: `1d42efc3` — `opt(pointer_ssa): Add safety checks and tests for pointer escapes, PHIs, and determinism`
- Rewrite-count assertion: `62b026e1` — `opt(pointer_ssa): Add IR-level rewrite count logging and assertion test`
- Evidence package for the conservative fix: `docs/evidence/2026-07-08/pointer_ssa_safety/`

## Notes

- Not urgent — the conservative fix is correct; this is a performance-recovery follow-up.
- Interacts with Mem2Reg: offset-aware rewrites may expose more scalar promotion opportunities on struct fields; the existing Mem2Reg interaction verify check in `zcc_test_suite.sh` should be extended accordingly.
- If option (b) (immediate-offset load/store) is chosen, coordinate with the spill-support work on this branch — both touch `ir_to_x86.c` addressing-mode emission, and sequential landing keeps bisection clean.
- Consider landing after the `spill-work` branch merges, since both touch the same optimizer surface.
