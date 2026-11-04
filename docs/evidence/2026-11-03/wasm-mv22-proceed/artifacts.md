# Artifacts Manifest — WASM-MV-22 / BITOPS-FFS-PARITY-01 / ARCH-CMP-SH-01 / WASM-V128-ACCUM / ZCC-PROVE-13

| Artifact Path | SHA-256 Checksum | Notes |
| :--- | :--- | :--- |
| `part4.c` | `7da68fdaf8a14a099408b75d2d006db4db7056d4a73ad3dda414317777864589` | 22 lines changed (`< 50` line limit strictly respected); ARM64/RISC-V 64 `ND_COMPOUND_ASSIGN` (`emit_prep_rhs`, `emit_shr`, `emit_store`) & `emit_node_extend` 64-bit guard |
| `part5.c` | `4a9f4d1ebf3ae096ce88d4f2b63eccdfdc19132513dd70938abeb212941c2720` | `__builtin_ffs/ffsl/ffsll` & `__builtin_parity/parityl/parityll` symbol registration, constant folding (`zcc_eval_const_bit_builtin`), and x86-64/ARM64/RISC-V 64 lowering |
| `part6_wasm.c` | `916f892c503a062ab0a52375f590bc649c5cf97e5f091e52fd698724e9c3105a` | On-demand `WASM_TYPE_V128` (`0x7B`) scratch local `tmp_v128_idx` for 1-load/1-mul 4-lane vector reductions + WASM32 `ffs`/`parity` lowering |
| `src/wasm_emit.h` | `8bc1b108d3c70f54c1bc63a76f88abc5c65dab3088650745c155cec2179d844a` | Added `int tmp_v128_idx;` to `WasmLocalMap` |
| `src/wasm_emit.c` | `f0dd080104636820a599ce7075637a1acc8841d15c36c1ad1178365de99e7e81` | Updated `.zcc.prove` custom section payload to `ZCC-PROVE-11..13` |
| `tests/test_wasm_mv02.py` | `4f7a226b8a55c10d8a4ad16aac1297d3bea2e6112d7b89f5dc85c07afbb45ee2` | Added `test_13_ffs_parity_quad_isa_v128_accum_and_compound_assign` (13/13 tests pass) |
| `tools/zcc_prove_01.py` | `aa0f4c2ce40a2dd4b9635251d9bb806bf631a1e34051c89d2d8f41fda13d79ce` | Added `ZCC-PROVE-13` Z3 proofs & structural invariants |
| `tools/mv02_mutation.py` | `c643a8d9f9f3e6a34a2a7f1aebb066e50f5f1d150d35f9170f5deef01438c48b` | Added `MV22-01..04` (`76/76` mutations killed) |
| `scripts/run_mv02_gate.sh` | `264b8c683bd6d08483e6609192e9a907641b803d3e6e70130fbe2b5b8268c1bf` | Updated Gate 4D/5 headers to `ZCC-PROVE-13` / `76-Mutation Adversarial Kill Ledger` |
| `BOOTSTRAP_BASELINES.tsv` | `cc0a54f0355990f9b4a74b2c0912ea6f1dcba0d145975a2d171cb51e48bd0c43` | Locked `zcc2.s` MD5 `bbc3533c2990328c049c7472252fed05` |
| `zcc2.s` | `26f30ad9fcb06ad82018ac791bf5cb3a13bc7e4d49197c734a3d74f97333fe8d` | Stage 2 self-hosted assembly (`10,382,132` bytes; MD5 `bbc3533c2990328c049c7472252fed05`) |
| `zcc3.s` | `26f30ad9fcb06ad82018ac791bf5cb3a13bc7e4d49197c734a3d74f97333fe8d` | Stage 3 self-hosted assembly (`0` byte delta vs `zcc2.s`) |
| `zcc2` | `a96dfa292ff54ab22badd74216c39f453cb8e46749a08dd25eb006e5ad7449ad` | Stage 2 binary |
| `zcc3` | `a96dfa292ff54ab22badd74216c39f453cb8e46749a08dd25eb006e5ad7449ad` | Stage 3 binary (`0` byte delta vs `zcc2`) |
