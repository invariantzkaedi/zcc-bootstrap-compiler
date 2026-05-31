# Forensic Post-Mortem: Peephole Boundary Block Disappearance & CFG Topology Invariance

## 1. Goal
Ensure the structural control-flow graph (CFG) topology invariance checks in the ZCC computational replay engine do not fail due to instruction elision at basic block boundaries during backend peephole optimization passes.

## 2. Outcome
Stabilized Stage 2 and Stage 3 bootstraps under multi-stage self-hosting (`make selfhost`). The CFG topology invariance proof successfully verifies identical structures, and the sovereign execution ledger (`.zxr`) verifies deterministic replay.

---

## 3. The Structural Breakdown & Root Cause
During native peephole optimization in `part5.c`, redundant operations (such as redundant push/pop sequence pairs) are elided by setting their assembly line buffers to an empty string (`line_ptrs[i][0] = 0;`).

Pre-peephole, the assembly contains non-empty strings which are registered as instruction lines:
```assembly
    pushq %rax
    popq %rax
.L1:
```

The control-flow graph topologically groups instructions into sequential basic blocks, where labels (`.L1:`) force the boundary of a new basic block.
* **Pre-peephole stream**: Contains the sequential instruction lines (`pushq`, `popq`), grouping them into an active basic block that falls through to a second basic block beginning at `.L1:`.
* **Post-peephole stream**: The instruction lines are completely cleared (`""`). When the topological parser processed the post-peephole stream, it skipped these empty lines entirely. This caused the preceding basic block to have zero instructions, collapsing and disappearing from the instruction array entirely.

As a result, the topological parser generated a divergent number of basic blocks and shifted instruction indices, leading to a false-positive constitutional violation warning:
`[CONSTITUTIONAL-VIOLATION] Function 'pp_expand_ident' control-flow topology mutated by peephole! Pre: 0xb6ae493abbb60ea9, Post: 0x25ba391fa6930f59`

---

## 4. The Surgical Fix
To prevent the collapsing of basic blocks without compromising the elision of the actual assembly instructions, we modified the topological CFG parser and DFS topology fingerprinters in `src/zcc_oracle_substrate.c`.

Instead of skipping explicitly elided lines, the parser now checks `line[0] == '\0'`. If a line has been zeroed out, it is retained in the instruction stream as an empty string placeholder:
```c
        for (int k = func_start; k < func_end; k++) {
            char *line = lines[k];
            if (line[0] == '\0') {
                if (num_instr < 4096) {
                    instr_lines[num_instr++] = k;
                }
                continue;
            }
            ...
```

Since the empty string does not start with space/tab and does not match any control-flow instructions (`jmp`, `jcond`, `ret`, `label`), it is treated as a regular, straight-line non-control-flow placeholder. This guarantees that `instr_lines` retains the exact same size and structural block boundaries before and after peephole optimization.

---

## 5. Verification Gate Results

### Gate 1: Self-Host Parity verified
Under WSL, the Stage 2 to Stage 3 bootstrap stabilizes perfectly with identical assembly output:
```text
=== Stage 1: zcc compiles itself -> zcc2 ===
./zcc zcc.c -o zcc2
strip --strip-all zcc2
=== Stage 2: zcc2 compiles itself -> zcc3 ===
./zcc2 zcc.c -o zcc3
strip --strip-all zcc3
=== Verify: zcc2.s == zcc3.s (codegen parity) ===
./zcc  zcc.c -o zcc2.s
./zcc2 zcc.c -o zcc3.s
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)"
SELF-HOST VERIFIED (assembly identical)
```

### Gate 2: CLI Emitter & Replay verification
Compiling `part1.c` with `--emit-exec-record record.zxr` successfully seals the ledger:
```text
[ZXR] Successfully wrote 2 events and 1 proofs to record.zxr
```

Running verification using `--replay-record record.zxr`:
```text
[ZXR-REPLAY] Comparing 2 replayed events with 2 actual events...
[ZXR-REPLAY] Comparing 1 replayed proofs with 1 actual proofs...
[ZXR-REPLAY] VERIFICATION PASSED (deterministic replay verified)
```
