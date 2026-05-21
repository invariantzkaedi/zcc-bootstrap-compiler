# BUG-4: next_token Hang Analysis

## Symptom
During the `make selfhost` bootstrap, the IR-lowered `zcc2` binary hangs in an infinite loop while compiling `part2.c` (specifically when parsing block comments or deeply nested conditionals in `next_token`).

## Root Cause
The root cause of the hang is a control-flow graph (CFG) construction bug within the IR lowering passes (`compiler_passes.c`), specifically triggered by nested `if` statements containing unconditional branches (`goto`).

In `next_token`, the AST for skipping block comments looks roughly like this:
```c
if (cc->source[cc->pos] == '*') {
    if (cc->source[cc->pos + 1] == '/') {
        cc->pos += 2;
        cc->col += 2;
        goto again; // unconditional branch
    }
}
cc->pos++;
cc->col++;
```

When `zcc_lower_stmt` translates this into IR:
1. The `goto again;` translates into `IR_BR("again")`.
2. This unconditional branch immediately terminates the current basic block.
3. The end of the inner `if` creates a new label (`lbl2`), which starts a new basic block.
4. The end of the outer `if` creates another new label (`lbl1`), starting yet another block.
5. The subsequent increments (`cc->pos++;`) are placed into this new block.

Due to a bug in `compute_reachability` and `ir_build_blocks_from_instructions`, the CFG edge from the false-branch of the inner `if` to `lbl2`, and subsequently to `lbl1`, is not properly tracked. As a result, the optimizer considers the block containing `cc->pos++; cc->col++;` to be unreachable dead code. The generated assembly jumps directly from the `if` failure back to the loop condition (`jmp 429997`), skipping the iterator increment and causing an infinite loop.

## Resolution
As deferred in the **CG-IR-012** incident, we have confirmed that `next_token` triggers complex reachability bugs in the experimental IR backend. We have removed `next_token` from the `ir_whitelisted` array in `part4.c`.

By routing `next_token` back to the stable AST-to-x86 compilation path, the control flow generates correctly without intermediate block-folding deletions.

## Verification
`make selfhost` now passes.
- `zcc2` compiles `zcc.c` into `zcc3` successfully.
- `zcc2.s` and `zcc3.s` are byte-identical.
- The `run_gates.sh` test suite passes.

**Verdict**: The fixed-point gate is GREEN. Binary parity achieved.
