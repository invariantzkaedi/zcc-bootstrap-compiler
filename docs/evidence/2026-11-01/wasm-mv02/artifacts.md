# Artifact Manifest — `WASM-MV-02` .. `WASM-MV-13` + `ZCC-PROVE-01` .. `ZCC-PROVE-05`

## Toolchain & Environment Identity (MV-2)
- **Git HEAD Commit:** `eb8454ac2` (`2026-11-02`)
- **Host / Kernel:** WSL2 Ubuntu 24.04 (`Linux x86_64`)
- **Host C Compilers:** `gcc` (Ubuntu 13.3.0-6ubuntu2~24.04), `clang` (18.1.3)
- **Host Assembler / Binutils:** GNU `as` / `objdump` (GNU Binutils for Ubuntu 2.42)
- **WASM Runtimes:** `node` (`v22.22.0`), `wasmtime` (`43.0.0`), `wat2wasm` (`1.0.34`)
- **SMT Solver:** `z3-solver` (`4.13.0.0` Python `z3` BitVector + Array theory)

## SHA-256 Digest Table (MV-9)

| Artifact / File Path | SHA-256 Digest | Description |
| :--- | :--- | :--- |
| `zcc` | `4fcfa402c5e491deeddeceb15c61b63bbb38e4c39999b4dd73afa1a9f1e009c9` | Stage-0 compiler binary |
| `zcc2` | `950a92915c1af0dd0ba04d7672f9e1c5b1a471f6bcea4820739b4e61d0f3879c` | Stage-2 self-hosted compiler binary |
| `zcc3` | `950a92915c1af0dd0ba04d7672f9e1c5b1a471f6bcea4820739b4e61d0f3879c` | Stage-3 self-hosted compiler binary (`cmp zcc2 zcc3` == 0) |
| `zcc2.s` | `934a0c924d21c3315cf4b83bf03a3659e83a8c755a060427dea87ebdc3c5e47c` | Stage-2 assembly (`cmp zcc2.s zcc3.s` == 0) |
| `zcc3.s` | `934a0c924d21c3315cf4b83bf03a3659e83a8c755a060427dea87ebdc3c5e47c` | Stage-3 assembly (`cmp zcc2.s zcc3.s` == 0) |
| `part2.c` | `f42728fef1711ba687e305df5b8fb6ee7f8b0118356f52c8fb928905a8f7ef34` | Compatible typedef redeclaration check (`typedef_types_compatible`) |
| `part3.c` | `9033c32d0dba7bf239f838edafa13aa4f7222b093212897763f069dfce5e36b6` | Inline-asm (`ND_ASM`) output operand `write_count` tracking |
| `part5.c` | `7b35e511adcb478c6ac254e27e283ea3e1f16e4f593974f3e09162e1a2377de0` | Full-function bidirectional XMM liveness scan (`%xmm15` $\to$ `%xmm14` $\to$ `%xmm13` $\to$ `%xmm12`) + `movq/movl $0` $\to$ `xorq/xorl` EFLAGS lookahead guard + `movq %r11, %rax` dead-write RMW guard + TCO full-function local-stack escape scan |
| `part6_wasm.c` | `277d71608d161243cbc6ca493e96912b212cf40c1dc5ab797ef066d500b3efbb` | Full 64-bit WASM backend (`i64`) + `wasm32` ILP32 data model |
| `src/codegen.c` | `abfb8a2c3170cbb55cb0946ad5aec4f9026d8f2fc71874b249b68298ba239e29` | Internal ELF assembler `REX.X` SIB fix + `encode_movq_xmm` (`%xmm0..%xmm15` $\leftrightarrow$ GPR) + `movabsq` `strtoull` unsigned 64-bit hex parsing + `addq`/`subq`/`andq`/`orq`/`xorq` memory-operand dispatch |
| `src/x86_codegen_sse.c` | `4e6bf058ab2ebecc05c37f46f40a11659b2019eebd92538af49ea998e309da4c` | `zcc_x86_peephole_boost` Pass 0a `strtoull` hex parsing guard + Pass 5c `%rax` overwrite lookahead guard + Pass 7 `mov`/`lea` RMW guard + Pass 8 label/jump `rsp_depth` invalidation guard |
| `tests/wasm_mv02_signedness.c` | `7a24e999d4d870abe68b14d3e2df8da34fe6364b561e6f643d288c3a2c7d840a` | Adversarial WASM 64-bit signedness, load/store, & switch/loop probe |
| `tests/x86_mv02_imm64.c` | `2d1adf3ec7d9ed9c528af8d9089e8cb91303cbd843b42c9cad219587422f38a5` | Native x86-64 64-bit immediate `cmpq`, live `%r11`/`%r10`/`%r9`, red-zone, `32(%rsp)`/`24(%rsp)` SIB aliasing, backward-loop 128-bit `%xmm15`, 4-XMM `%xmm12` fallback, `movq $0` EFLAGS, `MV-11` RMW/u64-imm, `MV-12` TCO/Pass-7, & `MV-13` Pass-8/Pass-5c probe |
| `tests/typedef_compatible.c` | `8a59412b0e54d546eabe51e406f46944fdc9325647b9de096e6c793fbef16b00` | Compatible `typedef` redeclaration positive test |
| `tests/typedef_incompatible.c` | `043e9cb3c97a221eb8cf5431f44c0e51ed6ce9b88c4dd3b22683e4966c184a6f` | Incompatible `typedef` redeclaration negative test |
| `tests/test_wasm_mv02.py` | `6dff9612a3d46121f28becb1a896aa3dae176ef1dd0f1f325b0b9100180d73af` | 7-test adversarial MV-02..MV-13 + GNU `as` (`37/37`) + `ZCC-PROVE-01..05` harness |
| `tools/mv02_mutation.py` | `90bbdaab8b3c3a4fd7cfa2bf0dbb54a01dd8700b2b3dc3d362ef3eba0390e579` | Isolated `/tmp` (`ext4`) worktree mutation runner (`44/44` mutations) |
| `tools/zcc_prove_01.py` | `6fb358adebc52795d7d67ca1f7f909b5e8c9e1925828c5cd55a09b94486ab858` | `ZCC-PROVE-01..05` Peephole Truth Machine (`25` Z3 classes + `5,000` states + full 128-bit XMM0..XMM15 + full-function XMM ownership + super-peephole/sliding-window/`zcc_x86_peephole_boost`/TCO/Pass-8 Z3 proofs + `10/10` negative controls) |
| `docs/evidence/2026-11-01/wasm-mv02/wasm-mv02.patch` | `7fb7c3885fcbafe33dd7e5d32c1bf0184f2ef651718d0e527158205c651fd9c1` | Unified diff (`part5.c` + `src/codegen.c` + `src/x86_codegen_sse.c`) |
| `docs/evidence/2026-11-01/wasm-mv02/x86_mv02_imm64.s` | `bb067c98fe1630ee8366a0eea5567d8fced8602ab5ba8d53557a3209853f2f1f` | Emitted assembly for `tests/x86_mv02_imm64.c` |
| `docs/evidence/2026-11-01/wasm-mv02/mv02_mutation_ledger.json` | `9ca422a0ee651c1c4f4e0f1ea556a30c99f11bb89f32428a7d0456353ebf75e6` | JSON ledger for all `44/44` killed mutations (`MV02-01` .. `MV13-04`) |
| `docs/evidence/2026-11-01/wasm-mv02/zcc_prove_01.json` | `55e0168cc0f3890ef9bf66cb8d2a58d43c2668d569b230a485ad00b7d1bf0879` | JSON proof certificate from `ZCC-PROVE-01..05` (`25/25` `UNSAT`, `5,000` states, `10/10` negative controls, `ZCC-PROVE-02..05` super-peepholes, sliding windows, boost, TCO, & Pass 8 `UNSAT`) |
