# Artifact Manifest — WASM-MV-19 / ARCH-AUDIT-02 / CFOLD-MATH-01 / WASM-SIMD128-AUTOVEC-V2 / ZCC-PROVE-10

## Changed Source & Harness Files (with SHA-256 Hashes)

| File | SHA-256 | Role |
| :--- | :--- | :--- |
| `part4.c` | `4b0b7053608c1d4a996e6d0f202c22b28f4ce53c4268815ff7e6f11e3f71e504` | Double-precision `sin`, `cos`, `pow` constant folding in `eval_const_call_math` (16 lines changed, `< 50` limit) |
| `part5.c` | `aa1aa31dccdea41d069731dfc9f9f7be93092d7faceada625bb290a57f6bed70` | Builtin symbol registration for `double sin(double)` and `double pow(double, double)` |
| `part6_wasm.c` | `95c5da2573d59b83aeb435acacb5c4c6adbbfee4d848be295e734a98f253dd9b` | 4-lane WASM `v128` auto-vectorization for bitwise (`&`, `|`, `^`) and compound assignment (`+=`, `-=`, `*=`, `&=`, `|=`, `^=`) loops |
| `src/arm64_codegen.c` | `8ff0b39fb17b7e5c627c71e674d6106b95b126711ee968d70815bd80e0a7e4a5` | Standalone ARM64 prologue/epilogue `> 504` immediate range guard and negative-offset `ldur`/`stur` emission |
| `src/riscv_codegen.c` | `a0c1b6a488707e8f0284c20b700e911ec842bd700161e26f296401f9860d20b0` | Standalone RISC-V 64 16-byte header `[s0..s0+16]` separating `ra`/`s0` from negative locals `-8(s0)`/`-16(s0)` |
| `tests/test_wasm_mv02.py` | `e7f830fb9af12b293e6882458967fe2a17e8fe82e6ab82c0036c9dfbd69880bf` | 10-test adversarial suite including `test_10_standalone_arm64_riscv_codegen_cfold_math_and_wasm_autovec_v2` |
| `tools/mv02_mutation.py` | `77a715d3e151bca306806017ee717391936ac686a8a0f09a0fb0d5096ca73e82` | 64-mutation adversarial kill ledger (`MV02..MV19`, `64/64` killed) |
| `tools/zcc_prove_01.py` | `a23b705c3feda236e2d4f6f4352ba9118108270b763b1e03d1914e7f71487957` | ZCC-PROVE-10 Z3 SMT + 5,000-state differential verifier |
| `scripts/run_mv02_gate.sh` | `1c600b26152950a4c7143e7d5336fc95c0f38b16a4ff733172716ab1340f204e` | End-to-end 5-gate verification runner |

## Bootstrap & Verification Artifacts

| Artifact | SHA-256 | Description |
| :--- | :--- | :--- |
| `zcc` | `83f17d03e781c526c7a7f3de1cbceb881e4d38c1e166276563e1c5e1ddea508d` | Stage-1 compiler binary |
| `zcc2.s` | `6e3074503bc96bf81ae08b1dbd89b1b1a7e71e6287b255c74afee161f06dfd2d` | Stage-2 self-hosted assembly |
| `zcc3.s` | `6e3074503bc96bf81ae08b1dbd89b1b1a7e71e6287b255c74afee161f06dfd2d` | Stage-3 self-hosted assembly (0-byte delta vs `zcc2.s`) |
| `zcc2` | `05233401084416ecf8af7a277a979129aab49ce4d0393cc482a56f668fe7dc1e` | Stage-2 self-hosted binary |
| `zcc3` | `05233401084416ecf8af7a277a979129aab49ce4d0393cc482a56f668fe7dc1e` | Stage-3 self-hosted binary (0-byte delta vs `zcc2`) |
| `docs/evidence/2026-11-01/wasm-mv02/zcc_prove_01.json` | `0518abd3e96c70b2ec4a387ed03095aa2c1507a56a1915b12e65a719da53267c` | Formal Z3 SMT proof receipt (`ZCC-PROVE-10`) |
| `docs/evidence/2026-11-01/wasm-mv02/mv02_mutation_ledger.json` | `f0f88fc3b82049d5c021c3a4021f02c1e914b01289dbd7f4433cef90c65d5b16a` | 64-mutation adversarial kill receipt (`64/64` killed) |
