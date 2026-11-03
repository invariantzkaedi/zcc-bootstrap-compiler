# Artifact Manifest — `WASM-MV-02` .. `WASM-MV-15` + `ZCC-PROVE-01` .. `ZCC-PROVE-07`

## Toolchain & Environment Identity (MV-2)
- **Git HEAD Commit:** `905ebb23a` (`2026-11-02`)
- **Host / Kernel:** WSL2 Ubuntu 24.04 (`Linux x86_64`)
- **Host C Compilers:** `gcc` (Ubuntu 13.3.0-6ubuntu2~24.04), `clang` (18.1.3)
- **Host Assembler / Binutils:** GNU `as` / `objdump` (GNU Binutils for Ubuntu 2.42)
- **WASM Runtimes:** `node` (`v22.22.0`), `wasmtime` (`43.0.0`), `wat2wasm` (`1.0.34`)
- **SMT Solver:** `z3-solver` (`4.13.0.0` Python `z3` BitVector + Array theory)

## SHA-256 Digest Table (MV-9)

| Artifact / File Path | SHA-256 Digest | Description |
| :--- | :--- | :--- |
| `zcc` | `59e383cd6a2909169e298d094f5ca5de45c805bc8f450c9ab2073f716a80e0c3` | Stage-0 compiler binary |
| `zcc2` | `7c1bc1928ace87e1f00d11661b6612af1b06665c797bf3c76fcfecbce84eca4a` | Stage-2 self-hosted compiler binary |
| `zcc3` | `7c1bc1928ace87e1f00d11661b6612af1b06665c797bf3c76fcfecbce84eca4a` | Stage-3 self-hosted compiler binary (`cmp zcc2 zcc3` == 0) |
| `zcc2.s` | `934a0c924d21c3315cf4b83bf03a3659e83a8c755a060427dea87ebdc3c5e47c` | Stage-2 assembly (`cmp zcc2.s zcc3.s` == 0) |
| `zcc3.s` | `934a0c924d21c3315cf4b83bf03a3659e83a8c755a060427dea87ebdc3c5e47c` | Stage-3 assembly (`cmp zcc2.s zcc3.s` == 0) |
| `part2.c` | `f42728fef1711ba687e305df5b8fb6ee7f8b0118356f52c8fb928905a8f7ef34` | Compatible typedef redeclaration check (`typedef_types_compatible`) |
| `part3.c` | `9033c32d0dba7bf239f838edafa13aa4f7222b093212897763f069dfce5e36b6` | Inline-asm (`ND_ASM`) output operand `write_count` tracking |
| `part5.c` | `7b35e511adcb478c6ac254e27e283ea3e1f16e4f593974f3e09162e1a2377de0` | Full-function bidirectional XMM liveness scan (`%xmm15` $\to$ `%xmm14` $\to$ `%xmm13` $\to$ `%xmm12`) + `movq/movl $0` $\to$ `xorq/xorl` EFLAGS lookahead guard + `movq %r11, %rax` dead-write RMW guard + TCO full-function local-stack escape scan |
| `part6_wasm.c` | `277d71608d161243cbc6ca493e96912b212cf40c1dc5ab797ef066d500b3efbb` | Full 64-bit WASM backend (`i64`) + `wasm32` ILP32 data model |
| `src/codegen.c` | `016afc9e9ed04cecab504f3b95e73a7da8120c405ab70dbd16a239c3ac04411d` | Internal ELF assembler `REX.X` SIB fix + `encode_movq_xmm` (`%xmm0..%xmm15` $\leftrightarrow$ GPR) + `movabsq`/`movq`/`movl`/`addq`/`subq`/`cmpq`/`andq`/`orq`/`xorq`/`.quad` `strtoull` unsigned 64-bit hex parsing + memory-operand dispatch |
| `src/x86_codegen_sse.c` | `07e61943c1e197272e9a143db3c99c965b22ce653486571e7f7ae1de66bb5460` | `zcc_x86_peephole_boost` `parse_imm_line` (`strtoll`/`strtoull` + suffix check) across Pass 3/4a/4c/6/8 + Pass 0a `strtoull` hex parsing guard + Pass 5c/6 `, %eax` / `, %rax` destination-register overwrite guards + Pass 7 `mov`/`lea` RMW guard + Pass 8 label/jump `rsp_depth` invalidation guard |
| `tests/wasm_mv02_signedness.c` | `7a24e999d4d870abe68b14d3e2df8da34fe6364b561e6f643d288c3a2c7d840a` | Adversarial WASM 64-bit signedness, load/store, & switch/loop probe |
| `tests/x86_mv02_imm64.c` | `b5874b2ad2390cdba61e46bc7b936ddd2058faee9c07bc46aaff03453b06340b` | Native x86-64 64-bit immediate `cmpq`, live `%r11`/`%r10`/`%r9`, red-zone, `32(%rsp)`/`24(%rsp)` SIB aliasing, backward-loop 128-bit `%xmm15`, 4-XMM `%xmm12` fallback, `movq $0` EFLAGS, `MV-11` RMW/u64-imm, `MV-12` TCO/Pass-7, `MV-13` Pass-8/Pass-5c, `MV-14` Pass-6/Pass-3/codegen-u64, & `MV-15` Pass-5c/Pass-6/addq-u64 probe |
| `tests/typedef_compatible.c` | `8a59412b0e54d546eabe51e406f46944fdc9325647b9de096e6c793fbef16b00` | Compatible `typedef` redeclaration positive test |
| `tests/typedef_incompatible.c` | `043e9cb3c97a221eb8cf5431f44c0e51ed6ce9b88c4dd3b22683e4966c184a6f` | Incompatible `typedef` redeclaration negative test |
| `tests/test_wasm_mv02.py` | `6dff9612a3d46121f28becb1a896aa3dae176ef1dd0f1f325b0b9100180d73af` | 7-test adversarial MV-02..MV-15 + GNU `as` (`37/37`) + `ZCC-PROVE-01..07` harness |
| `tools/mv02_mutation.py` | `93979d36550adf4b749acbefc1b8f58728b495e3765377507fc3aa5b97003cb1` | Isolated `/tmp` (`ext4`) worktree mutation runner (`52/52` mutations) |
| `tools/zcc_prove_01.py` | `8d9ae4ef9bc83312ca05245628b024af24a3e37d0ffd5565f623f35433cc32b3` | `ZCC-PROVE-01..07` Peephole Truth Machine (`25` Z3 classes + `5,000` states + full 128-bit XMM0..XMM15 + full-function XMM ownership + super-peephole/sliding-window/`zcc_x86_peephole_boost`/TCO/Pass-6/Pass-8/Pass-5c-Pass-6-dst-guards Z3 proofs + `10/10` negative controls) |
| `docs/evidence/2026-11-01/wasm-mv02/wasm-mv02.patch` | `ede4263380f38f126a7984013061393a876fd6913a0ccd6baa31be94fc8a2c0f` | Unified diff (`src/codegen.c` + `src/x86_codegen_sse.c`) |
| `docs/evidence/2026-11-01/wasm-mv02/x86_mv02_imm64.s` | `c948825265e19e8334adedcb01a7a2b38df8b5c47c71a1aa5938a0705daa9542` | Emitted assembly for `tests/x86_mv02_imm64.c` |
| `docs/evidence/2026-11-01/wasm-mv02/mv02_mutation_ledger.json` | `1cd7a0df37039f2a8dce8ba4a73347a850c7fa56cb3442db95d9a1e45e345e58` | JSON ledger for all `52/52` killed mutations (`MV02-01` .. `MV15-04`) |
| `docs/evidence/2026-11-01/wasm-mv02/zcc_prove_01.json` | `2fa07e3b61d928a4e6d2279fbcdb5cec29570458c1415285733d5a6e713d9755` | JSON proof certificate from `ZCC-PROVE-01..07` (`25/25` `UNSAT`, `5,000` states, `10/10` negative controls, `ZCC-PROVE-02..07` super-peepholes, sliding windows, boost, TCO, Pass 6, Pass 8, & Pass 5c/6 dst guards `UNSAT`) |
