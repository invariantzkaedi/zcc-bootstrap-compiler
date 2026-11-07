# Artifact Manifest & SHA-256 Checksums (`WASM-MV-29..30` / `ZCC-PROVE-20..21`)

| Artifact Path | SHA-256 Digest | Description |
| :--- | :--- | :--- |
| `docs/evidence/2026-11-06/wasm-mv30-prove21/wasm-mv30.patch` | `6bfe55d1ff1dcd79921580bef485ba0bbfdb69819632142fe67faf765afdcac4` | Complete git diff for Track A + Track B |
| `docs/evidence/2026-11-06/wasm-mv30-prove21/probe_track_a_x86.s` | `eaa4bf6ab1d75ddbac0117a58ee851a26bd0379657a489acd68d3c04d958d57d` | x86-64 assembly for `smul/umul_overflow_p` and `sadd/ssub/uadd/usub_sat` |
| `docs/evidence/2026-11-06/wasm-mv30-prove21/probe_track_a_arm64.s` | `3ec241b891b80bd3f4c90efb3c7a795bb259ed9e977baae0750589be3678739a` | ARM64 assembly for `smul/umul_overflow_p` and `sadd/ssub/uadd/usub_sat` |
| `docs/evidence/2026-11-06/wasm-mv30-prove21/probe_track_a_riscv64.s` | `86b94e1ffbc9407ad14ca0aea5878722370c8a50c6532a42854f1c369269413b` | RISC-V 64 assembly for `smul/umul_overflow_p` and `sadd/ssub/uadd/usub_sat` |
| `docs/evidence/2026-11-06/wasm-mv30-prove21/probe_track_a.wasm` | `c4d2b0a09392a1814da9c00ab9665c63c2110a6c6b7be712af316bcb5990539f` | WASM32 binary for `smul/umul_overflow_p` and `sadd/ssub/uadd/usub_sat` |
| `docs/evidence/2026-11-06/wasm-mv30-prove21/probe_track_b.wasm` | `564b62cc76d8b409f8ba17aad278d33cf91cbf581497a10a01e1c5d9313fb7b7` | WASM32 SIMD128 binary for unary (`-a[i]`, `~a[i]`, `fabsf`, `sqrtf`) & float `min`/`max` |
| `docs/evidence/2026-11-06/wasm-mv30-prove21/zcc_prove_01.json` | `24a87f7b1201d43fb2b27049e57e584c03e785b5fa3ed86e3df684d18555c821` | Z3 SMT formal equivalence report (`ZCC-PROVE-01..21`) |
| `docs/evidence/2026-11-06/wasm-mv30-prove21/mv02_mutation_ledger.json` | `9a365611896ae8a101b3bdad769ddaa1a7b82a1ee00dce91e341515556b5a941` | 108/108 adversarial mutation kill ledger (`MV02..MV30`) |
| `part3.c` | `7d7a7457c093b4077b0f81bb252757878bb39b739738ac51d3b705051a15c0a7` | Parser return-type resolution for `__builtin_uadd_sat` / `__builtin_usub_sat` |
| `part5.c` | `a22073093c938519fcbe51e7822d83263ba1db16a508d3ba686ad6855528163a` | Constant folding & x86-64/ARM64/RISC-V 64 lowering for `ovf32` & `sat32` |
| `part6_wasm.c` | `6160d12da9ab9072be13a775c99b20455a4ae28d3d1422e2b1957b96912578b9` | WASM32 lowering for `ovf32`/`sat32`, scalar `fabsf`/`sqrtf`, SIMD128 unary & `f32x4` min/max |
| `src/wasm_emit.c` | `de939efa79b81e200e626461a9421768176f34921a445780e6595e472a1951e4` | `.zcc.prove` custom section payload (`"ZCC-PROVE-11..21"`) |
| `src/wasm_emit.h` | `8a28b1ca2201345836bdb10173e60e4a90855117335f231447c4d3eda4e799d0` | WASM SIMD128 opcodes (`I32X4_NEG`, `F32X4_ABS/NEG/SQRT/MIN/MAX`) |
| `zcc` | `0921d26c23ab595ab0cbeae63f297422c69a9ed0be13207773e0141ef7e66e35` | Stage 1 compiler binary |
| `zcc2.s` | `d40f882504b3f53d7a5ca1f86442c15302fd8592e9a0881dfc2ebbdbffc08617` | Stage 2 self-hosted assembly (`MD5: db587335c0237454fc9ed7a09ff40ced`) |
| `zcc3.s` | `d40f882504b3f53d7a5ca1f86442c15302fd8592e9a0881dfc2ebbdbffc08617` | Stage 3 self-hosted assembly (`0` byte delta vs `zcc2.s`) |
| `zcc2` | `c75d1e5a661ec68bdf2b5b37dc646eaee2af35602718b5ffe11c424566d2789b` | Stage 2 self-hosted binary |
| `zcc3` | `c75d1e5a661ec68bdf2b5b37dc646eaee2af35602718b5ffe11c424566d2789b` | Stage 3 self-hosted binary (`0` byte delta vs `zcc2`) |
