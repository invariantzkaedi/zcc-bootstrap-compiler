# Artifact Manifest — `WASM-MV-02` .. `WASM-MV-12` + `ZCC-PROVE-01` .. `ZCC-PROVE-04`

## Toolchain & Environment Identity (MV-2)
- **Git HEAD Commit:** `be93994` (`2026-10-31`)
- **Host / Kernel:** WSL2 Ubuntu 24.04 (`Linux x86_64`)
- **Host C Compilers:** `gcc` (Ubuntu 13.3.0-6ubuntu2~24.04), `clang` (18.1.3)
- **Host Assembler / Binutils:** GNU `as` / `objdump` (GNU Binutils for Ubuntu 2.42)
- **WASM Runtimes:** `node` (`v22.22.0`), `wasmtime` (`43.0.0`), `wat2wasm` (`1.0.34`)
- **SMT Solver:** `z3-solver` (`4.13.0.0` Python `z3` BitVector + Array theory)

## SHA-256 Digest Table (MV-9)

| Artifact / File Path | SHA-256 Digest | Description |
| :--- | :--- | :--- |
| `zcc` | `8592e470f99783986452b800e27c28dc32dc28d89341061dfb4eb50c3fd351d6` | Stage-0 compiler binary |
| `zcc2` | `5e9156f5a89d1b328ea981daf9153977c22ceefabbbc6a772dcefc2475fe5f91` | Stage-2 self-hosted compiler binary |
| `zcc3` | `5e9156f5a89d1b328ea981daf9153977c22ceefabbbc6a772dcefc2475fe5f91` | Stage-3 self-hosted compiler binary (`cmp zcc2 zcc3` == 0) |
| `zcc2.s` | `934a0c924d21c3315cf4b83bf03a3659e83a8c755a060427dea87ebdc3c5e47c` | Stage-2 assembly (`cmp zcc2.s zcc3.s` == 0) |
| `zcc3.s` | `934a0c924d21c3315cf4b83bf03a3659e83a8c755a060427dea87ebdc3c5e47c` | Stage-3 assembly (`cmp zcc2.s zcc3.s` == 0) |
| `part2.c` | `f42728fef1711ba687e305df5b8fb6ee7f8b0118356f52c8fb928905a8f7ef34` | Compatible typedef redeclaration check (`typedef_types_compatible`) |
| `part3.c` | `9033c32d0dba7bf239f838edafa13aa4f7222b093212897763f069dfce5e36b6` | Inline-asm (`ND_ASM`) output operand `write_count` tracking |
| `part5.c` | `7b35e511adcb478c6ac254e27e283ea3e1f16e4f593974f3e09162e1a2377de0` | Full-function bidirectional XMM liveness scan (`%xmm15` $\to$ `%xmm14` $\to$ `%xmm13` $\to$ `%xmm12`) + `movq/movl $0` $\to$ `xorq/xorl` EFLAGS lookahead guard + `movq %r11, %rax` dead-write RMW guard + TCO full-function local-stack escape scan |
| `part6_wasm.c` | `277d71608d161243cbc6ca493e96912b212cf40c1dc5ab797ef066d500b3efbb` | Full 64-bit WASM backend (`i64`) + `wasm32` ILP32 data model |
| `src/codegen.c` | `abfb8a2c3170cbb55cb0946ad5aec4f9026d8f2fc71874b249b68298ba239e29` | Internal ELF assembler `REX.X` SIB fix + `encode_movq_xmm` (`%xmm0..%xmm15` $\leftrightarrow$ GPR) + `movabsq` `strtoull` unsigned 64-bit hex parsing + `addq`/`subq`/`andq`/`orq`/`xorq` memory-operand dispatch |
| `src/x86_codegen_sse.c` | `407759fac438af4efbead926c3be8cebd3aab6e016c37c7743dcc8fbe825245b` | `zcc_x86_peephole_boost` Pass 0a `strtoull` hex parsing guard + Pass 7 `mov`/`lea` RMW guard & removal of `pxor %xmm0, %xmm0` / `movss/movsd ... %xmm0` false `%rax` overwrite elision |
| `tests/wasm_mv02_signedness.c` | `7a24e999d4d870abe68b14d3e2df8da34fe6364b561e6f643d288c3a2c7d840a` | Adversarial WASM 64-bit signedness, load/store, & switch/loop probe |
| `tests/x86_mv02_imm64.c` | `93b861d4ba8e9165684b91562756fbda97b84055f3602d70849ed653ae798b88` | Native x86-64 64-bit immediate `cmpq`, live `%r11`/`%r10`/`%r9`, red-zone, `32(%rsp)`/`24(%rsp)` SIB aliasing, backward-loop 128-bit `%xmm15`, 4-XMM `%xmm12` fallback, `movq $0` EFLAGS, `MV-11` RMW/u64-imm, & `MV-12` TCO/Pass-7 probe |
| `tests/typedef_compatible.c` | `8a59412b0e54d546eabe51e406f46944fdc9325647b9de096e6c793fbef16b00` | Compatible `typedef` redeclaration positive test |
| `tests/typedef_incompatible.c` | `043e9cb3c97a221eb8cf5431f44c0e51ed6ce9b88c4dd3b22683e4966c184a6f` | Incompatible `typedef` redeclaration negative test |
| `tests/test_wasm_mv02.py` | `6dff9612a3d46121f28becb1a896aa3dae176ef1dd0f1f325b0b9100180d73af` | 7-test adversarial MV-02..MV-12 + GNU `as` (`37/37`) + `ZCC-PROVE-01..04` harness |
| `tools/mv02_mutation.py` | `3f6cdd04d190b34114842dec8ff0563267203dbf6fee9652da79799fa773beb5` | Isolated `/tmp` (`ext4`) worktree mutation runner (`40/40` mutations) |
| `tools/zcc_prove_01.py` | `e412f9493b2b8b98ae4223e8c617582d901ca264daae5d74bb5ece6dd4b87902` | `ZCC-PROVE-01..04` Peephole Truth Machine (`25` Z3 classes + `5,000` states + full 128-bit XMM0..XMM15 + full-function XMM ownership + super-peephole/sliding-window/`zcc_x86_peephole_boost`/TCO Z3 proofs + `10/10` negative controls) |
| `docs/evidence/2026-11-01/wasm-mv02/wasm-mv02.patch` | `a688d65e23e29ea52971796adc3a162d414e56b91a4df22d02bcbe874c95f0c1` | Unified diff (`part5.c` + `src/codegen.c` + `src/x86_codegen_sse.c`) |
| `docs/evidence/2026-11-01/wasm-mv02/x86_mv02_imm64.s` | `79996476380c61ecbd4cd5e605b703c7aae15f73abd5aa8efc7a4b5d9a70ddd8` | Emitted assembly for `tests/x86_mv02_imm64.c` |
| `docs/evidence/2026-11-01/wasm-mv02/mv02_mutation_ledger.json` | `7b9e98308e91ba4bd144c61fa4b54e55cc93da38df260deaaa6319d124bf1cfc` | JSON ledger for all `40/40` killed mutations (`MV02-01` .. `MV12-04`) |
| `docs/evidence/2026-11-01/wasm-mv02/zcc_prove_01.json` | `3dd5e5fd3e9c0068fd3ff374e5bf9a24d2e0021d0c3bdba6d3b0a8b8e064e4ba` | JSON proof certificate from `ZCC-PROVE-01..04` (`25/25` `UNSAT`, `5,000` states, `10/10` negative controls, `ZCC-PROVE-02..04` super-peepholes, sliding windows, boost, & TCO `UNSAT`) |
