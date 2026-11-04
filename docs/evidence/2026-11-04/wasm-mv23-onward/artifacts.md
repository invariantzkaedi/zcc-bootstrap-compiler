# Artifact Manifest — Wave WASM-MV-23 / BITOPS-CLRSB-01 / ARCH-LOGIC-01 / WASM-SIMD128-SPLAT / ZCC-PROVE-14

| Artifact Path | SHA-256 Digest | Description |
| :--- | :--- | :--- |
| `part4.c` | `664707bab5c10733cdc2885a7b59c63dfc1d965504f318ccdb336d3667197560` | `ARCH-LOGIC-01`: `ND_LAND` (`&&`), `ND_LOR` (`||`), and `ND_TERNARY` (`?:`) backend-abstracted on ARM64 (`cmp x0, #0` / `cset x0, ne`) and RISC-V 64 (`snez a0, a0`) (`+21/-18` lines) |
| `part5.c` | `78d3677e2a77f34f9ab48b3f20752153eced15d549f7ed0d15551595672a8eba` | `BITOPS-CLRSB-01`: `__builtin_clrsb`, `__builtin_clrsbl`, `__builtin_clrsbll` registration, compile-time constant folding, and x86-64/ARM64 (`cls`)/RISC-V 64 lowering + `backend_normalize_asm` under `ZCC_OPT=0` |
| `part6_wasm.c` | `6bbb4dc755a863fe780f9356291582259c913a783059fc817f276db88f5fc544` | `WASM-SIMD128-SPLAT`: WASM32 `__builtin_clrsb/l/ll` lowering + loop-invariant scalar broadcast auto-vectorization (`i32x4.splat` / `f32x4.splat`) |
| `src/wasm_emit.c` | `2f163c8937a8c25f6ac1a5141a4733ee3fc9078bac82019b870b44a1ef46740d` | `.zcc.prove` custom section updated to `"ZCC-PROVE-11..14"` |
| `tests/test_wasm_mv02.py` | `cfc09ee00026ef39e4743b3dba3d0e6d8df295252919bed16730caae07488b30` | 14-test Quad-ISA adversarial suite (`test_14_clrsb_quad_isa_simd128_splat_and_logic_ops` added) |
| `tools/zcc_prove_01.py` | `34f0bd65ba24787d39accfc63bb7f2e4f379b5d92a8abd98a0a59a7b05ddfc13` | `ZCC-PROVE-14` Z3 formal equivalence proof of branchless `clrsb` identity + structural guards |
| `tools/mv02_mutation.py` | `38205f4fc17b25e127a3a9d3c98747645783fc2b34757b3235d72f5df42f984d` | 80-mutation adversarial harness (`MV02..MV23`, `MV23-01..04` added) |
| `zcc2.s` / `zcc3.s` | `dfb671559fb826eb82a7c2735d54d08dac880662659a0cd3b027cb14fb46abc1` | Stage-2 and Stage-3 self-hosted assembly (0-byte delta; MD5 `bf66665c625e353d824f630bb3912f27`) |
| `zcc2` / `zcc3` | `13b938b6055acdac3b992e3a35ca70788d080c07cd023dea3935da1d4273b6ed` | Stage-2 and Stage-3 self-hosted compiler binaries (0-byte delta) |
| `docs/evidence/2026-11-04/wasm-mv23-onward/zcc_prove_01.json` | `65c61cdc00014ce04557a4f203e41d7a0cbb6f26dffcaa4cf5dcecbbf4632042` | Formal Z3 SMT proof receipt (`ZCC-PROVE-01..14`) |
| `docs/evidence/2026-11-04/wasm-mv23-onward/mv02_mutation_ledger.json` | `5649de074ca3b5a3a011617ae05b5b0acdb87f4e6895f8904647f1e0f12a4d17` | 80-mutation adversarial kill receipt (`80/80` killed) |
| `docs/evidence/2026-11-04/wasm-mv23-onward/mv23_x86_64.s` | `4ff55d2588c93a76c917324debaba31fff46069e1167274c550f86a5d566c830` | Inspected x86-64 emitted assembly (`sarl`/`xorl`/`bsrl`/`xorl $31`) |
| `docs/evidence/2026-11-04/wasm-mv23-onward/mv23_arm64.s` | `064d26d774dc3c1d7ae8333c0d064e49a68fd76a0c7c1adb0e9b7b83d106b574` | Inspected ARM64 emitted assembly (`cls w0, w0` / `cls x0, x0` / `cmp x0, #0` / `cset x0, ne`) |
| `docs/evidence/2026-11-04/wasm-mv23-onward/mv23_riscv64.s` | `2df2060e7ec73e852cb46c04d181a7a788b4f0b8372e549994fa12b4d2cbef44` | Inspected RISC-V 64 emitted assembly (`.Lrv_clrsb_loop_` / `snez a0, a0`) |
| `docs/evidence/2026-11-04/wasm-mv23-onward/mv23_wasm32.wasm` | `d3df9b4772ef4ea02d32721e408cac4b61a17a8135419568b4f0b8372e549994` | Inspected WASM32 SIMD128 emitted module |
