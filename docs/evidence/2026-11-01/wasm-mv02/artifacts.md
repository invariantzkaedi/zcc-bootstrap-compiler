# Artifact Manifest — `WASM-MV-02` .. `WASM-MV-14` + `ZCC-PROVE-01` .. `ZCC-PROVE-06`

## Toolchain & Environment Identity (MV-2)
- **Git HEAD Commit:** `54d209b32` (`2026-11-02`)
- **Host / Kernel:** WSL2 Ubuntu 24.04 (`Linux x86_64`)
- **Host C Compilers:** `gcc` (Ubuntu 13.3.0-6ubuntu2~24.04), `clang` (18.1.3)
- **Host Assembler / Binutils:** GNU `as` / `objdump` (GNU Binutils for Ubuntu 2.42)
- **WASM Runtimes:** `node` (`v22.22.0`), `wasmtime` (`43.0.0`), `wat2wasm` (`1.0.34`)
- **SMT Solver:** `z3-solver` (`4.13.0.0` Python `z3` BitVector + Array theory)

## SHA-256 Digest Table (MV-9)

| Artifact / File Path | SHA-256 Digest | Description |
| :--- | :--- | :--- |
| `zcc` | `8b546e794afce15a79ea22f3c61e87f05c1077451a7eb589e4c6a96c1a591ec9` | Stage-0 compiler binary |
| `zcc2` | `b5f93559c68cdbc743485a622afcebf03106c197f3b86f6e697b2c3940da9011` | Stage-2 self-hosted compiler binary |
| `zcc3` | `b5f93559c68cdbc743485a622afcebf03106c197f3b86f6e697b2c3940da9011` | Stage-3 self-hosted compiler binary (`cmp zcc2 zcc3` == 0) |
| `zcc2.s` | `934a0c924d21c3315cf4b83bf03a3659e83a8c755a060427dea87ebdc3c5e47c` | Stage-2 assembly (`cmp zcc2.s zcc3.s` == 0) |
| `zcc3.s` | `934a0c924d21c3315cf4b83bf03a3659e83a8c755a060427dea87ebdc3c5e47c` | Stage-3 assembly (`cmp zcc2.s zcc3.s` == 0) |
| `part2.c` | `f42728fef1711ba687e305df5b8fb6ee7f8b0118356f52c8fb928905a8f7ef34` | Compatible typedef redeclaration check (`typedef_types_compatible`) |
| `part3.c` | `9033c32d0dba7bf239f838edafa13aa4f7222b093212897763f069dfce5e36b6` | Inline-asm (`ND_ASM`) output operand `write_count` tracking |
| `part5.c` | `7b35e511adcb478c6ac254e27e283ea3e1f16e4f593974f3e09162e1a2377de0` | Full-function bidirectional XMM liveness scan (`%xmm15` $\to$ `%xmm14` $\to$ `%xmm13` $\to$ `%xmm12`) + `movq/movl $0` $\to$ `xorq/xorl` EFLAGS lookahead guard + `movq %r11, %rax` dead-write RMW guard + TCO full-function local-stack escape scan |
| `part6_wasm.c` | `277d71608d161243cbc6ca493e96912b212cf40c1dc5ab797ef066d500b3efbb` | Full 64-bit WASM backend (`i64`) + `wasm32` ILP32 data model |
| `src/codegen.c` | `21e5c6685b7cf80af2ba08b1d71e2f19cdf3fec521f3c9eb3e856947423799fd` | Internal ELF assembler `REX.X` SIB fix + `encode_movq_xmm` (`%xmm0..%xmm15` $\leftrightarrow$ GPR) + `movabsq`/`movq`/`movl` `strtoull` unsigned 64-bit hex parsing + `addq`/`subq`/`andq`/`orq`/`xorq` memory-operand dispatch |
| `src/x86_codegen_sse.c` | `af809995f8d01d7f35ba7d6fc35cfd31d8f856f23216ef972d4c1cd7aea385b8` | `zcc_x86_peephole_boost` `parse_imm_line` (`strtoll`/`strtoull` + suffix check) across Pass 3/4a/4c/6/8 + Pass 0a `strtoull` hex parsing guard + Pass 5c `%rax` overwrite lookahead guard + Pass 6 `, %eax` / `, %rax` zero-extension guard + Pass 7 `mov`/`lea` RMW guard + Pass 8 label/jump `rsp_depth` invalidation guard |
| `tests/wasm_mv02_signedness.c` | `7a24e999d4d870abe68b14d3e2df8da34fe6364b561e6f643d288c3a2c7d840a` | Adversarial WASM 64-bit signedness, load/store, & switch/loop probe |
| `tests/x86_mv02_imm64.c` | `3c1652ecf4efff47f205631a9055794734521e7414f62918ea7216dde43e7038` | Native x86-64 64-bit immediate `cmpq`, live `%r11`/`%r10`/`%r9`, red-zone, `32(%rsp)`/`24(%rsp)` SIB aliasing, backward-loop 128-bit `%xmm15`, 4-XMM `%xmm12` fallback, `movq $0` EFLAGS, `MV-11` RMW/u64-imm, `MV-12` TCO/Pass-7, `MV-13` Pass-8/Pass-5c, & `MV-14` Pass-6/Pass-3/codegen-u64 probe |
| `tests/typedef_compatible.c` | `8a59412b0e54d546eabe51e406f46944fdc9325647b9de096e6c793fbef16b00` | Compatible `typedef` redeclaration positive test |
| `tests/typedef_incompatible.c` | `043e9cb3c97a221eb8cf5431f44c0e51ed6ce9b88c4dd3b22683e4966c184a6f` | Incompatible `typedef` redeclaration negative test |
| `tests/test_wasm_mv02.py` | `6dff9612a3d46121f28becb1a896aa3dae176ef1dd0f1f325b0b9100180d73af` | 7-test adversarial MV-02..MV-14 + GNU `as` (`37/37`) + `ZCC-PROVE-01..06` harness |
| `tools/mv02_mutation.py` | `f6a698ccb9e2045760b6b47f195c83d9d82ff813167374acbf10bcc1b4610f49` | Isolated `/tmp` (`ext4`) worktree mutation runner (`48/48` mutations) |
| `tools/zcc_prove_01.py` | `64d5463fe99d107e6bee68155d668e463b2f70032199dfac5bbbdc9a00c0791d` | `ZCC-PROVE-01..06` Peephole Truth Machine (`25` Z3 classes + `5,000` states + full 128-bit XMM0..XMM15 + full-function XMM ownership + super-peephole/sliding-window/`zcc_x86_peephole_boost`/TCO/Pass-6/Pass-8 Z3 proofs + `10/10` negative controls) |
| `docs/evidence/2026-11-01/wasm-mv02/wasm-mv02.patch` | `0fe0f51175fc6b4de59a1731a56e26843f75aad80e87fecabc779b6582967e7a` | Unified diff (`src/codegen.c` + `src/x86_codegen_sse.c`) |
| `docs/evidence/2026-11-01/wasm-mv02/x86_mv02_imm64.s` | `c55a1a68d9c8e5c64646c1a517b8174b8441a11089165dcff5f1237d7676d3dc` | Emitted assembly for `tests/x86_mv02_imm64.c` |
| `docs/evidence/2026-11-01/wasm-mv02/mv02_mutation_ledger.json` | `f7a1299bd48724093a25f64c6b5b382872882db164e56e8e5c5b8602eeb326e8` | JSON ledger for all `48/48` killed mutations (`MV02-01` .. `MV14-04`) |
| `docs/evidence/2026-11-01/wasm-mv02/zcc_prove_01.json` | `4de60f43bf706d03ca8fe398a7fccfc360bafa89d029432439c97b12453feba4` | JSON proof certificate from `ZCC-PROVE-01..06` (`25/25` `UNSAT`, `5,000` states, `10/10` negative controls, `ZCC-PROVE-02..06` super-peepholes, sliding windows, boost, TCO, Pass 6, & Pass 8 `UNSAT`) |
