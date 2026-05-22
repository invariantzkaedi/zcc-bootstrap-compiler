# switch_stmt — Pre-existing FAIL — CLOSED

**Status:** Closed / Resolved  
**First observed:** E4 escape analysis gate (commit: pending)  
**Resolved:** 2026-05-22  
**Test:** `./zcc_test_suite.sh` → `[PASS] switch_stmt (rc=0, 2 IR funcs)`  
**Exit code:** 0 (Matched AST output perfectly)

## Symptom
Switch statement compiled under IR backend exited rc=1 instead of expected return value `20` in unit test suite.

## Root Cause Analysis
During `ZND_SWITCH` lowering in `zcc_lower_stmt` (in `compiler_passes.c`), the pre-existing implementation attempted to lower each case block's gotos (`node->case_bodies[i]`) and default block gotos (`node->default_body`), but then immediately emitted an unconditional exit branch (`OP_BR exit_blk`) and **overwrote** the block successors:
```c
fn->blocks[case_blk]->succs[0] = exit_blk;
fn->blocks[case_blk]->n_succs = 1;
```
This overwriting severed the CFG edge between the case comparison blocks and the actual case label bodies (which are stored separately in the switch body `node->body`). Because the CFG edges were broken, basic block reachability analysis (`compute_reachability` / `pgo_reorder_pass`) marked the case label bodies as **unreachable**, completely omitting them from emission. Consequently, the case blocks fell through into consecutive labels at the end of the function or hit dead ends.

Furthermore, `ZND_SWITCH` lowering previously ignored the switch block body `node->body` in `zcc_node_from_stmt` (mapping it to NULL), and it failed to manage the loop exit/latch stacks for lexical scoping, preventing inner `break` statements from functioning correctly.

## Resolution
1. **Body Mapping**: In `zcc_node_from_stmt`, mapped `node->body` to `z->body` for `ZND_SWITCH`.
2. **Lexical Stack Management**: Pushed `exit_blk` to `loop_exit_stack` and `0` to `loop_latch_stack` at switch entry.
3. **CFG Restoration**: Removed the redundant `br_exit`/`br_def` emission and the successor overwrites in the case/default comparison blocks. The lowered comparison blocks now naturally branch directly to the case label blocks, preserving correct CFG reachability.
4. **Switch Body Lowering**: Created a new basic block `switch.body`, lowered `node->body` into it, and verified if the final block has a valid terminator (inserting `OP_BR exit_blk` if not).
5. **Validation**: Verified that `t_switch.c` compiles under the IR backend, produces identical exit codes (`20`) and stdout outputs as the AST path, and passes the targeted `switch_stmt` test. All 32 targeted test suite cases are now passing.
