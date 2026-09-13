# Sovereign Pipeline Hardening Artifacts and Checksums

## Key Files Touched
- `part5.c`: Added `peephole_optimize(asm_file)` to the `--replay-ir` execution path, establishing 100% byte-exact parity between AST and replayed IR assembly.
- `tests/miniz_test_zcc.c`: Added missing `TDEFL_READ_UNALIGNED_WORD2` helper, enabling full deflate/inflate execution under ZCC.
- `tests/test_miniz_amalgamation.c`: Added miniz compression engine test to permanent C regression corpus.
- `tests/test_tcc_struct.c`: Added TinyCC struct/bitfield test to permanent C regression corpus.
- `BOOTSTRAP_BASELINES.tsv`: Appended new convergence baseline hash `7afc1dc0bed0e049a895cf06386fcc00` for WSL2.

## Checksums
| File | MD5 Checksum |
|---|---|
| `zcc2.s` | `7afc1dc0bed0e049a895cf06386fcc00` |
| `zcc3.s` | `7afc1dc0bed0e049a895cf06386fcc00` |
| `tests/test_tcc_struct.c` | `9d554a938fcfe35e985b5428489704ec` |
| `tests/test_miniz_amalgamation.c` | `4329a28b030491d9cbf663bfe1566cf2` |
