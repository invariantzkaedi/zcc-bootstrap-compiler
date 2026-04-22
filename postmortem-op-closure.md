# Post-Mortem: OP_CLOSURE Segmentation Fault

## Discrepancy in `MAXARG_Bx` Evaluation
The prior diagnosis claimed the corrupted index was caused by a zero mask resulting in `MAXARG_Bx >> 1`.
Actual Lua 5.4 opcode encoding from `lopcodes.h` (L38-L40):
```c
#define SIZE_C		8
#define SIZE_B		8
#define SIZE_Bx		(SIZE_C + SIZE_B + 1)
```
`SIZE_Bx` is 17 bits. 
`17 bits` of all ones equals `131,071`, which corresponds to `-1` correctly extracted by ZCC. The ZCC bitfield extraction code compiled flawlessly. 

## Causal Isolation of the Crash
The evaluation of `fs->np - 1` natively fell back to `-1` because `fs->np` was evaluating to `0` inside ZCC-compiled `codeclosure`.
To causally trace the root fix:
1. Checked out `141bced^` (pre-implicit-cast ABI fix) -> ` closures` smoke test fails with segfault.
2. Checked out `141bced` (implicit-cast fix) -> ` closures` smoke test fails with segfault.
3. Checked out `8a43fe7` (System V aggregate passing and stack spill boundries cg-ir-019 fix) -> ` closures` smoke test evaluates flawlessly, returning `42`.

The runtime corruption inside `lparser.c` parser memory subroutines was caused by System V struct-by-value/aggregate boundaries failing to align stack arguments properly when passing parser states, which was definitively resolved in commit `8a43fe7`. 

*Note: Workstation tested via single reproducer probe_b.lua. HYGIENE-LUA-TESTES remains open until testes/all.lua passes.*
