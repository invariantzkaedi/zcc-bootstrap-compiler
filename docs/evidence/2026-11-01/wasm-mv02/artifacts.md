# Artifact Manifest — `WASM-MV-02` .. `WASM-MV-17` + `WASM-SIMD128` + `ZCC-PROVE-01` .. `ZCC-PROVE-08`

## Toolchain & Environment Identity (MV-2)
- **Git HEAD Commit:** `b22cb741d` (`2026-11-03`)
- **Host / Kernel:** WSL2 Ubuntu 24.04 (`Linux x86_64`)
- **Host C Compilers:** `gcc` (Ubuntu 13.3.0-6ubuntu2~24.04), `clang` (18.1.3)
- **Host Assembler / Binutils:** GNU `as` / `objdump` (GNU Binutils for Ubuntu 2.42)
- **WASM Runtimes:** `node` (`v22.22.0`), `wasmtime` (`43.0.0`), `wat2wasm` (`1.0.34`)
- **SMT Solver:** `z3-solver` (`4.13.0.0` Python `z3` BitVector + Array theory)

## SHA-256 Digest Table (MV-9)

| Artifact / File Path | SHA-256 Digest | Description |
| :--- | :--- | :--- |
| `zcc` | `f1403b466a2e7cf88cf31ff6cd871de96d551afa30e669eb06a27774c9e2a57c` | Stage-0 compiler binary |
| `zcc2` | `b7494664dc233fafdda724f8eccab87a342b93cbb6fde883c16d075989e4fe1a` | Stage-2 self-hosted compiler binary |
| `zcc3` | `b7494664dc233fafdda724f8eccab87a342b93cbb6fde883c16d075989e4fe1a` | Stage-3 self-hosted compiler binary (`cmp zcc2 zcc3` == 0) |
| `zcc2.s` | `970fa9cb8b90093e31705c580597684c147833b410fa2566e4b54137a71262a4` | Stage-2 assembly (`cmp zcc2.s zcc3.s` == 0) |
| `zcc3.s` | `970fa9cb8b90093e31705c580597684c147833b410fa2566e4b54137a71262a4` | Stage-3 assembly (`cmp zcc2.s zcc3.s` == 0) |
| `part2.c` | `f42728fef1711ba687e305df5b8fb6ee7f8b0118356f52c8fb928905a8f7ef34` | Compatible typedef redeclaration check (`typedef_types_compatible`) |
| `part3.c` | `9033c32d0dba7bf239f838edafa13aa4f7222b093212897763f069dfce5e36b6` | Inline-asm (`ND_ASM`) output operand `write_count` tracking |
| `part5.c` | `65860d3bc4cc23fa04f20956b430d57d2aaade0dc566f1ce2537be3107923833` | Full-function bidirectional XMM liveness scan (`%xmm15` $\to$ `%xmm14` $\to$ `%xmm13` $\to$ `%xmm12`) + `movq/movl $0` $\to$ `xorq/xorl` EFLAGS lookahead guard + `movq %r11, %rax` dead-write RMW guard + Rule 3b `, %rax` destination guard + TCO full-function local-stack escape scan |
| `part6_wasm.c` | `4e21787c9a9cc5800d84562cefcaa6ac4a49e05e3e84f76eb8b62bbe0214d273` | Full 64-bit WASM backend (`i64`) + `wasm32` ILP32 data model + `__zcc_wasm_*` 128-bit SIMD (`v128`, `0xFD`) intrinsic lowering |
| `src/wasm_emit.h` | `ddf77bb369338c5f54df5d8410e42ff00e347108b422494802ebfed2fe98d7fc` | `WASM_TYPE_V128 = 0x7B`, `WASM_OP_SIMD_PREFIX = 0xFD`, and `WASMSimdOpcode` definitions |
| `src/codegen.c` | `4709cf1419643c89ed3a01acaa1d50dee9501f7d947b26aee2c8653062901ad3` | Internal ELF assembler `REX.X` SIB fix + `leaq` non-zero SIB displacement (`encode_cmpq_mem(..., 0x8D)`) + `encode_movq_xmm` (`%xmm0..%xmm15` $\leftrightarrow$ GPR) + `movabsq`/`movq`/`movl`/`addq`/`subq`/`cmpq`/`andq`/`orq`/`xorq`/`addb`/`subb`/`cmpb`/`andb`/`orb`/`xorb`/`.quad` `strtoull` unsigned 64-bit hex parsing + memory-operand dispatch |
| `src/x86_codegen_sse.c` | `fb911a9e965e72969cfe91a8605a65ecd618ab0cb72ba49a7d46164522bc20b7` | `zcc_x86_peephole_boost` `parse_imm_line` (`strtoll`/`strtoull` + suffix check) across Pass 3/4a/4c/6/8 + Pass 0a `strtoull` hex parsing & EFLAGS consumer lookahead guard + Pass 0b `.rodata` label identity (`lbl_buf == ref_lbl`) & scalar-width guard + Pass 2/5c/6 `, %eax` / `, %rax` destination-register overwrite guards + Pass 7 `mov`/`lea` RMW guard + Pass 8 label/jump `rsp_depth` invalidation guard |
| `tests/wasm_mv02_signedness.c` | `7a24e999d4d870abe68b14d3e2df8da34fe6364b561e6f643d288c3a2c7d840a` | Adversarial WASM 64-bit signedness, load/store, & switch/loop probe |
| `tests/x86_mv02_imm64.c` | `46c727bf2983895e7c463b21c5eeade31b45d0ac0e04efba0299ed817b0755ed` | Native x86-64 64-bit immediate `cmpq`, live `%r11`/`%r10`/`%r9`, red-zone, `32(%rsp)`/`24(%rsp)` SIB aliasing, backward-loop 128-bit `%xmm15`, 4-XMM `%xmm12` fallback, `movq $0` EFLAGS, `MV-11` RMW/u64-imm, `MV-12` TCO/Pass-7, `MV-13` Pass-8/Pass-5c, `MV-14` Pass-6/Pass-3/codegen-u64, `MV-15` Pass-5c/Pass-6/addq-u64, & `MV-16/17` Pass-0a/0b/2/Rule-3b/`leaq`-SIB/`addb`-u64 probe |
| `tests/typedef_compatible.c` | `8a59412b0e54d546eabe51e406f46944fdc9325647b9de096e6c793fbef16b00` | Compatible `typedef` redeclaration positive test |
| `tests/typedef_incompatible.c` | `043e9cb3c97a221eb8cf5431f44c0e51ed6ce9b88c4dd3b22683e4966c184a6f` | Incompatible `typedef` redeclaration negative test |
| `tests/test_wasm_mv02.py` | `497ee229c163f11bd1c2acae71e417a3f867ba001d2cea9846459b8f22370527` | 8-test adversarial MV-02..MV-17 + `WASM-SIMD128` + GNU `as` (`42/42`) + `ZCC-PROVE-01..08` harness |
| `tools/mv02_mutation.py` | `5b70ba8eedbe27805ecc8fc1e36ee62a2cee05267d434c047012a2358da4c169` | Isolated `/tmp` (`ext4`) worktree mutation runner (`56/56` mutations) |
| `tools/zcc_prove_01.py` | `9abe6597348ba1a56bb8dab09826d580e73794210be28bf0eeba5f097f9e69bb` | `ZCC-PROVE-01..08` Peephole Truth Machine (`25` Z3 classes + `5,000` states + full 128-bit XMM0..XMM15 + full-function XMM ownership + super-peephole/sliding-window/`zcc_x86_peephole_boost`/TCO/Pass-6/Pass-8/Pass-0a-0b-2-5c-6-guards/`leaq`-SIB-`addb`-hex Z3 proofs + `10/10` negative controls) |
| `docs/evidence/2026-11-01/wasm-mv02/wasm-mv02.patch` | `12ddee6a6eeff2298883be2fb8c2cc7d5afacc95ca0e4e849f75bf0318e5c865` | Unified diff (`part5.c` + `part6_wasm.c` + `src/wasm_emit.h` + `src/codegen.c` + `src/x86_codegen_sse.c`) |
| `docs/evidence/2026-11-01/wasm-mv02/x86_mv02_imm64.s` | `21e1196bdf749405bb375caa4c7617ee0e92e36941d60119f672ffb044a323cd` | Emitted assembly for `tests/x86_mv02_imm64.c` |
| `docs/evidence/2026-11-01/wasm-mv02/mv02_mutation_ledger.json` | `e6883dabb2a36d37d854e23c78a57f1816117916c262da51e539c81b7242cccf` | JSON ledger for all `56/56` killed mutations (`MV02-01` .. `MV16-04`) |
| `docs/evidence/2026-11-01/wasm-mv02/zcc_prove_01.json` | `2fa07e3b61d928a4e6d2279fbcdb5cec29570458c1415285733d5a6e713d9755` | JSON proof certificate from `ZCC-PROVE-01..08` (`25/25` `UNSAT`, `5,000` states, `10/10` negative controls, `ZCC-PROVE-02..08` super-peepholes, sliding windows, boost, TCO, Pass 6, Pass 8, Pass 0a/0b/2/5c/6, & `leaq`/`addb` guards `UNSAT`) |
