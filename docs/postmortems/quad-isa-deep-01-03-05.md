# Postmortem: Quad-ISA Deep Bugs `DEEP-01`, `DEEP-03`, `DEEP-05` & Compound-Assign Sub-Word Extension

## 1. Trigger
Differential execution of `tests/test_quad_isa_deep_01_03_05.py` across `gcc -O2` / `clang -O2` vs ZCC (`x86_64`, `arm64` QEMU, `riscv64` QEMU) exposed four coupled codegen defects in `part4.c`:
1. **`DEEP-01` (`M1` — Nested Indirect Calls & Compound-Assign Sub-Word Wrapping):**
   - `fp_outer(fp_inner(5), 7)` where `fp_outer` is returned by `get_adder(fp_inner(2))` and `e.uc += 2u` on `unsigned char` `255u`.
   - Unpatched ZCC returned `M1=00009be7` across all 3 backends instead of `M1=000655aa`.
2. **`DEEP-03` (`M2` — ARM64/RV64 32-Bit `unsigned int` `+`/`-`/`*` Wrapping before Right-Shift/Division):**
   - `(u32_a + u32_b) >> 4`, `(u32_c - u32_d) >> 3`, and `(u32_a * 5u) / 3u` on 32-bit `unsigned int` overflowing `2^32`.
   - Unpatched ARM64 and RV64 returned `M2=80e9f67d` instead of `M2=bdf69ba3`.
3. **`DEEP-05` (`M3` — ARM64/RV64 Signed vs Unsigned 32-Bit `/` and `%`):**
   - `s32 / -3`, `u32 / 3u`, `s32 % -7`, `u32 % 7u` where `u32 = 0x80000007u` (bit 31 set).
   - Unpatched ARM64 and RV64 returned `M3=aaaaaa92` instead of `M3=9555556d`.
4. **`DEEP-03/05` (`M4` — ARM64/RV64 32-Bit Signed Shift Sign-Extension & Unsigned Shift Truncation):**
   - `((0x0fff0000 << 4) >> 16)` (signed `int` becoming negative after `<< 4`) and `((0xf0000000u << 1) >> 1)`.
   - Unpatched ARM64 and RV64 returned `M4=00001fff` instead of `M4=80001fff`.

## 2. Impact / Scope
- **Affected File:** `part4.c` (`emit_expr` for `ND_CALL`, `ND_ASSIGN` compound-assign, `ND_ADD`, `ND_SUB`, `ND_MUL`, `ND_DIV`, `ND_MOD`, `ND_SHL`, `ND_SHR`).
- **Affected Targets:**
  - `DEEP-01`: All backends (`x86_64`, `arm64`, `riscv64`).
  - `DEEP-03` & `DEEP-05`: 64-bit GP-register backends (`arm64` where `emit_add`/`sub`/`mul`/`div`/`mod`/`shl`/`shr` operate on 64-bit `x0`/`x1`, and `riscv64` where `add`/`sub`/`mul`/`div`/`rem`/`sll`/`sra`/`srl` operate on 64-bit `a0`/`a1`).

## 3. Root Cause & Violated Invariants
1. **`DEEP-01` (Caller-Saved Scratch Clobber + Missing Sub-Word Extension on `backend_ops->emit_store`):**
   - In `ND_CALL` (`part4.c:1671`), indirect calls evaluated `node->lhs` into `%r10` (`x86_64`), `x16` (`arm64`), or `t6` (`riscv64`) *before* evaluating arguments. Any nested call inside an argument clobbered caller-saved `%r10`/`x16`/`t6`.
   - In compound assignment (`part4.c:1817`), when `backend_ops && backend_ops->emit_store` was taken (`arm64`/`riscv64`), execution skipped the `else` branch containing `emit_node_extend(cc, node->lhs)`, leaving `unsigned char` `255u + 2u = 0x101` unmasked in `x0`/`a0`.
2. **`DEEP-03` (Missing 32-Bit Canonicalization after `backend_ops` `ND_ADD`/`ND_SUB`/`ND_MUL`):**
   - `part6_arm.c` and `part6_riscv.c` emit 64-bit `add`/`sub`/`mul` on `x0`/`a0`. Without 32-bit sign/zero extension back to canonical form when `type_size(node->type) == 4 && !is_pointer(node->type)`, bits `63:32` retained carry/borrow bits that corrupted subsequent `>>`, `/`, `%`, or comparisons.
3. **`DEEP-05` (Unconditional Signed 64-Bit `sdiv`/`div` and Uncanonicalized 32-Bit Shifts on `backend_ops`):**
   - `backend_ops->emit_div` and `emit_mod` emit 64-bit signed `sdiv` (`arm64`) and `div`/`rem` (`riscv64`). When operands were 32-bit `unsigned int` with bit 31 set (`0x80000007u`), prior operations left bit 31 sign-extended across bits `63:32` (`0xffffffff80000007`), causing signed 64-bit division to treat the unsigned dividend as negative (`-2147483641`). Zero-extending both operands (`mov w1, w1; mov w0, w0` on ARM64; `slli/srli 32` on RV64) before 64-bit signed `sdiv`/`div` makes 64-bit signed division over `[0, 2^32 - 1]` exact for 32-bit unsigned division and remainder.
   - For `ND_SHL`/`ND_SHR`, 32-bit shifts needed mask-to-5-bits (`0x1f`), pre-zero-extension for unsigned `>>`, and post-operation 32-bit canonicalization (`emit_backend_ext32`).

## 4. Patch Summary (`+23 / -23 = 46 < 50` Changed Lines in `part4.c`)
- Added `emit_backend_ext32(CompilerContext *cc, Type *ty)` (`part4.c:302-305`): emits `sxtw x0, w0` / `mov w0, w0` on ARM64 and `sext.w a0, a0` / `slli a0, a0, 32; srli a0, a0, 32` on RV64.
- `ND_CALL` (`part4.c:1671-1748`): Push evaluated indirect function pointer onto the stack before argument evaluation and pop into `%r10` (`x86_64`), `x16` (`arm64`), or `t6` (`riscv64`) immediately before indirect branch.
- `ND_ASSIGN` compound-assign (`part4.c:1819`): Call `emit_node_extend(cc, node->lhs)` after `backend_ops->emit_store`.
- `ND_ADD` / `ND_SUB` / `ND_MUL` / `ND_DIV` / `ND_MOD` / `ND_SHL` / `ND_SHR` (`part4.c:1877-2122`): Apply `emit_backend_ext32` on 32-bit non-pointer results and zero-extend 32-bit unsigned operands before `backend_ops` `/`, `%`, and `>>`.

## 5. Verification Evidence
Stored in `docs/evidence/2026-11-12/quad-isa-deep-01-03-05/`:
- `test_quad_isa_deep_01_03_05.py`: `PASS` (`M1=000655aa M2=bdf69ba3 M3=9555556d M4=80001fff` across `gcc -O2`, `clang -O2`, `zcc_x86_64`, `zcc_arm64`, `zcc_riscv64`; `EF2_missing=[]`).
- `test_quad_isa_deep_regression.py`: `PASS` (`D2=04e2ea19 F4=e05f3717`).
- `zcc_quad_isa_gate.py --strict-tier 3 --verify-ui` & `--self-test`: `PASS` (`21/21` lanes across Tiers 1-3 + `3/3` adversarial negative controls killed).
- `bash gate.sh`: `SELFHOST-MV1.3b PASS` (`cmp zcc2.s zcc3.s` `0` byte delta) + `ARGON 4 ALL 6 VERIFICATION SUITES PASSED`.

## 6. Rollback / Recovery Path
Run:
```bash
python3 patches/quad_isa_deep_01_03_05/apply_deep_fixes.py --revert
```
or `git checkout HEAD -- part4.c && make zcc`.

