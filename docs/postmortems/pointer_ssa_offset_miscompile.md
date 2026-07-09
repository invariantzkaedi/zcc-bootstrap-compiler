# Post-Mortem: Struct Member Promotion Miscompile under Offset-Aware Pointer SSA

## Trigger
Enabling the newly-implemented offset-aware points-to analysis in `pointer_ssa.c` when compiling structs or arrays with member accesses.

## Impact/Scope
Silent data corruption (miscompile) during optimizations:
- Struct member accesses were promoted to scalar registers via Mem2Reg even though they targeted non-zero offsets.
- Different member fields (e.g., `p.x` and `p.y`) were mapped to the same alloca SSA register, causing stores to one field to overwrite the other, resulting in incorrect runtime execution (e.g., `test_rewrite()` returning 64 instead of 42).

## Root Cause
Two main issues were identified:
1. **Escape Analysis Bypass**: The escape analysis pass in `compiler_passes.c` (`escape_analysis_pass`) bypassed checking allocas with displacement/sbt_offsets (`ins->amf.disp != 0` or `ins->sbt_offset != 0`) for compiler functions (`is_compiler_func`). With the introduction of offset-aware pointer SSA, this bypass caused alloca promotions to happen on compiler functions and other paths without marking the struct/array as escaping.
2. **Redundant Rewrite Overwriting Displacement**: In `pointer_ssa.c`'s `opt_pointer_ssa_rewrite_pass`, if a load/store had already been folded by `opt_address_mode_folding_pass` to target the base alloca directly with a displacement (e.g., `amf.disp = 4`), `pointer_ssa` redundantly processed it because the pointer register pointed to the alloca. Since the base alloca has a point-to offset of 0, `pointer_ssa` overwrote `amf.disp` to `0`, discarding the member offset information.

## Violated Invariant
- **Mem2Reg Promotion Constraint**: An alloca with non-zero member/array offset accesses (`amf.disp != 0` or `sbt_offset != 0`) must never be promoted to a single scalar SSA register (it must be marked as escaping).
- **Address-Mode Folding Preservation**: Already folded address mode displacements must not be rewritten/reset to `0` by subsequent points-to optimization runs when the pointer register is already the base alloca itself.

## Patch
1. **Escape analysis update (`compiler_passes.c`)**: Removed the `is_compiler_func` check constraint around displacement/offset checks in `escape_analysis_pass`. Now, any load/store with displacement or sbt_offset makes the alloca escape universally.
2. **Pointer SSA rewrite guard (`src/opt/pointer_ssa.c`)**: Added `ptr_reg != base` check in `pointer_ssa` rewrite conditions to skip loads/stores that already directly point to the base alloca, preserving their existing folded displacement.
3. **Test suite alignment (`zcc_test_suite.sh`)**: Updated the pointer dereference test assertion to check that no redundant pointer rewrites occur on already-direct stack pointer variables.

## Verification Evidence
- **Harness Verification**: Scratch harness return value successfully verified at `42` (instead of the miscompiled `64` return value).
- **Gate 1 (Self-host identity)**: `make selfhost` successfully completed with Stage 2 and Stage 3 byte-identical output verification (`cmp zcc2.s zcc3.s` identical).
- **Gate 4 (Target harness)**: Full test suite execution through `make test` cleanly passed (33 PASS, 0 FAIL, 3 SKIP).

## Rollback/Recovery Path
Revert changes in:
- `src/opt/pointer_ssa.c`
- `compiler_passes.c`
- `zcc_test_suite.sh`
And rebuild via `make clean && make zcc`.
