# Drop-In Patch Bundle: `BUG-DEEP-01`, `BUG-DEEP-03`, `BUG-DEEP-05` & 32-Bit Wrap Fixes

Surgical `< 50`-line follow-up bundle for `part4.c` (`+23 / -23 = 46` changed lines) fixing:

1. **`BUG-DEEP-01` (`part4.c` `ND_CALL`)**: Replace single fixed-slot `abi_scratch_offset + 16` indirect-callee spill/load with balanced stack `push_reg`/`pop_reg` (`rax` -> `r10`/`x16`/`r4`) across argument evaluation, fixing 2/3-level nested indirect calls, chained `get_fn()(...)`, and `>8`-arg indirect calls across x86-64, ARM64, and RV64.
2. **`BUG-DEEP-03` (`part4.c` `ND_PRE_INC`/`PRE_DEC`/`POST_INC`/`POST_DEC` + `L1819` compound-assign)**: Dispatch `backend_ops->emit_store(cc, node->lhs->type)` instead of raw `str r1, [r0]` in all 4 inc/dec operators, and invoke `emit_node_extend(cc, node->lhs)` in memory/member compound-assign (`+=`, `-=`, etc.) so `uint8`/`uint16`/`uint32` wraparound truncates properly and never corrupts adjacent struct fields on 64-bit targets.
3. **`BUG-DEEP-05` & 32-bit siblings (`part4.c` `emit_backend_ext32`, `emit_node_extend`, `ND_ADD`/`ND_SUB`/`ND_MUL`/`ND_SHL`/`ND_BNOT`/`ND_NEG`/`ND_CAST`)**: Zero- or sign-extend 32-bit results (`mov w0, w0` / `sxtw x0, w0` on ARM64; `slli+srli` / `addiw` on RV64) and stop `emit_node_extend` from short-circuiting 8/16-bit normalization on `ptr_size == 8`.

## Quick Usage (from WSL repo root)

```bash
# 1. Verify zero-conflict applicability & diff budget (non-mutating):
python3 patches/quad_isa_deep_01_03_05/apply_deep_fixes.py --check

# 2. Optional: Full end-to-end shadow dry run in ephemeral /tmp (zero repo changes):
python3 patches/quad_isa_deep_01_03_05/apply_deep_fixes.py --verify-shadow

# 3. After committing prior work, apply + rebuild + run all differential & EF-2 gates:
python3 patches/quad_isa_deep_01_03_05/apply_deep_fixes.py --apply --selfhost

# 4. Roll back cleanly at any time:
python3 patches/quad_isa_deep_01_03_05/apply_deep_fixes.py --revert
```

Direct patch commands (`git apply patches/quad_isa_deep_01_03_05/deep_01_03_05.patch && make zcc && python3 tests/test_quad_isa_deep_01_03_05.py`) work identically.

