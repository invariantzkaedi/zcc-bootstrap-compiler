# Key Artifacts and Provenance Checksums

## Source and Backend Modifications
- `part1.c`: `extern TargetBackend backend_cortex_m4;`
- `part4.c`: Corrected `ND_STR` `.LS` lowering for ARM literal pool, configured `ND_CALL` argument registers (`r0..r3` under AAPCS), hardened unary operators (`ND_BNOT`, `ND_NEG`, `ND_LNOT`), and routed indirect function pointer dispatch (`blx r4`).
- `part5.c`: CLI target flags for `-target cortex-m4`, `-target flipper`, `-target thumbv7em`, `-target cortex-m0plus`.
- `part6_arm.c`: Implemented all 20 hooks for `backend_thumbv6m` and `backend_cortex_m4` with hardware `sdiv`/`mls` for Flipper Zero (STM32WB55), hardened AAPCS 8-byte stack alignment math, and `.thumb_func` symbol annotations.

## Verification Artifacts & Checksums
| File | Size (Bytes) | SHA-256 Checksum | Description |
|---|---|---|---|
| `tests/test_thumb_codegen.py` | 5,595 | `aaf62aa0f683e039105eeea1dd5403b02456244c80a199939b05b38b36891d28` | 6-Arm automated ARM/Thumb/Flipper hardened regression suite |
| `tests/test_flipper_app.c` | 913 | `ce122b8ae5d4892ffa7c88274965b01a969e5c052a6321d3b160f716c392baf0` | Flipper Zero GUI test kernel source |
| `tests/test_flipper_app.s` | 2,827 | `f025c7c7f0147a60871e5caf8d3fedd5dce8ecb6c2a275b869c4af528753966f` | Emitted Cortex-M4 Thumb-2 assembly |
| `tests/test_flipper_app.o` | 2,492 | `a55cbe003c89895bfe0f75728a063afb4287f75a4caa2947180df3e32e06c1c5` | GNU EABI relocatable ELF32 object file |
| `zcc2.s` | 9,269,011 | `1d881cf2fdb1db8d795309700efaeb03e8198ec40ed6c88c818d53c457833fcc` | Stage 2 self-host assembly output |
| `zcc3.s` | 9,269,011 | `1d881cf2fdb1db8d795309700efaeb03e8198ec40ed6c88c818d53c457833fcc` | Stage 3 self-host assembly output |
