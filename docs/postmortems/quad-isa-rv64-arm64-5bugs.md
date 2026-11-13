# Postmortem: 5 Quad-ISA RV64 & ARM64 Codegen / ABI Defects (`BUG-QUAD-RV64-01..03`, `BUG-QUAD-ARM64-01..02`)

## Trigger
Running the stratified 3-tier Quad-ISA differential campaign (`tests/zcc_quad_isa_torture.c`, `tools/zcc_quad_isa_gate.py`) at Tier 2 (`-DZCC_TORTURE_STRUCT_UNION`) and Tier 3 (`-DZCC_TORTURE_STRUCT_UNION -DZCC_TORTURE_EDGE_ABI`) surfaced 5 defects across `--target=riscv64` and `--target=arm64`.

## Impact / Scope
- **Silent Wrong-Code / Memory Corruption (Severity High):**
  - `BUG-QUAD-RV64-01`: RV64 `(unsigned short)` cast (`uxth`) leaked upper 48 sign/junk bits, producing silent wrong FNV-1a digest `0x45b674f0a55e6d23 != 0xfb986fd4f88819a9` in K7/K8.
  - `BUG-QUAD-RV64-02`: RV64 sub-word `u32` struct/union member store emitted 8-byte `sd` instead of 4-byte `sw`, overwriting adjacent fields/bytes in K7/K8 (`packed_u8_u32` and union punning).
- **Assemble Failures / ABI Violations (Tier 3):**
  - `BUG-QUAD-RV64-03`: RV64 `>8`-arg call path emitted ARMv7 mnemonics (`sub sp, sp, #...`, `str r0, [sp, #...]`, `add sp, sp, #...`) and used only 6 register args, while prologue failed to home 9th+ stack arguments.
  - `BUG-QUAD-ARM64-01`: ARM64 `>8`-arg call path emitted 32-bit Thumb `str r0, [sp, #...]` instead of 64-bit `str x0, [sp, #...]` / 16-byte SP alignment, used only 6 register args, and prologue never loaded incoming stack arguments from `[x29, #16]`.
  - `BUG-QUAD-ARM64-02` (and RV64 indirect-call twin): `ND_CALL` indirect call saved `func->func_name` as empty, stored the computed callee address to `[x29, #16]` / `16(s0)` (clobbering caller frame / 9th arg slot and violating negative frame-offset convention), and `emit_call` emitted empty `bl ` / `call `. On RV64, `riscv_map_reg("x16")` also fell back to `"x16"` (`a6` alias), risking arg-register clobbering.

## Root Cause & Violated Invariants
1. **`part6_riscv.c` `backend_normalize_asm` (`uxth`/`sxtb`/`sxth`):** Replaced ARM narrowing shifts with single left-shifts (`slli a0, a0, 48`), violating `(uint16_t)` / `(int8_t)` / `(int16_t)` bit-width normalization by omitting the matching `srli` / `srai`.
2. **`part4.c` `ND_MEMBER` assignment (`L1371`):** When `backend_ops` was active, struct/union member store skipped width dispatch and fell back to `pop_reg(..., "r1")` + `emit_store` with pointer/8-byte default instead of passing `Type` (`TY_PTR` or `TY_INT` with `node->lhs->member_size`) to `backend_ops->emit_store`.
3. **`part4.c` `ND_CALL` + `part6_riscv.c` / `part6_arm.c` prologues (`>8` args, SP alignment, indirect fnptr slot):**
   - AAPCS64 (`x0..x7`) and LP64D (`a0..a7`) allocate 8 GP arg registers before spilling at `[sp, #0]` / `0(sp)` with 16-byte SP alignment (`sub sp, sp, #16` / `addi sp, sp, -16`) and prologue copying from `[x29, #16 + 8*(i-8)]` / `(16 + 8*(i-8))(s0)`.
   - `cc->abi_scratch_offset` stayed positive (`16`), clobbering caller stack arg slot `16(s0)` / `[x29, #16]`, instead of living below frame locals at `-aligned_stack` (`-stack_size`).
   - `aarch64_emit_call` and `riscv_emit_call` lacked empty-`func_name` dispatch (`blr x16` / `jalr t6`), and `riscv_map_reg` lacked `"x16" -> "t6"`.

## Minimal Patch Summary
- `part4.c` (`+32 / -12`): Pass `mt` (`member_size` / `TY_PTR`) to `backend_ops->emit_store` on `ND_MEMBER` assign; support 8 GP arg regs (`a0..a7`, `x0..x7`), 16-byte SP-aligned stack reservations/teardown (`addi sp`/`sub sp`), 16-byte stack-slot scaling for `ptr_size == 8`, and `sd a0, N(sp)` / `str x0, [sp, #N]` spills.
- `part6_arm.c` (`+19 / -1`): Set `cc->abi_scratch_offset = -aligned_stack`, home stack args `>= 8` from `[x29, #16 + 8*(i-8)]` via `x9`, emit `blr x16` on empty `func_name`.
- `part6_riscv.c` (`+24 / -4`): Set `cc->abi_scratch_offset = -stack_size`, home stack args `>= 8` from `(16 + 8*(i-8))(s0)` via `t1`, map `"x16" -> "t6"`, emit `jalr t6` on empty `func_name`, and restore `srli` / `srai` second instruction for `sxtb`/`uxth`/`sxth`.
- `tools/zcc_quad_isa_gate.py` (`+8 / -2`, untracked UI test assertion): Update `verify_ui_headless` bug-ledger assertion to accept both non-empty defect rows and 0-defect state (`discovered_defects.length === 0`).

## Verification Evidence
See `docs/evidence/2026-11-12/quad-isa-rv64-arm64-5bugs/{commands.txt,stdout.log,artifacts.md,verdict.md}`:
- `make selfhost`: PASS (`stage2.s == stage3.s`, `cmp` rc=0, SHA-256 `540d806ebf7d751bfa03a10f4223e19c592c10733d0777ba3908224cea3a5194`), `SELFHOST-MV1.3b PASS`, ARGON 4 `6/6` suites PASS.
- `tools/zcc_quad_isa_gate.py --strict-tier 3 --verify-ui` & `--self-test`: PASS across all 3 tiers (`7/7` per tier: `0x7e07f55a67f3cc08`, `0xfb986fd4f88819a9`, `0x6d4f29801741710f`), `discovered_defects: []`, `EF-7` UI check PASS, `EF-0` 3/3 negative controls killed.
- Rule EF-2 `.s` inspection confirms `sw a1, 0(a0)`, `slli`+`srli` 48, `ld t1, 16(s0)`, `jalr t6`, `ldr x9, [x29, #16]`, `str x0, [sp, #0]`, and `blr x16` in emitted assembly.

## Rollback / Recovery Path
Revert changes to `part4.c`, `part6_arm.c`, and `part6_riscv.c` with `git checkout -- part4.c part6_arm.c part6_riscv.c` and rebuild with `make clean && make zcc`.

