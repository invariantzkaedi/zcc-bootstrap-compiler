# Quantum Superopt Hardening — Artifact Registry

| Artifact Path | Description | Checksum / Merkle Attestation |
| :--- | :--- | :--- |
| `tests/diff_read_escape.c` | Differential test harness for read_escape O(1) LUT lowering | Bit-exact across GCC & ZCC (267 vectors) |
| `tests/diff_parse_initializer_list.c` | Differential test harness for parse_initializer_list stack zero-alloc | Bit-exact across GCC & ZCC (5 suites) |
| `tests/diff_HUlib_drawTextLine.c` | Differential test harness for HUlib_drawTextLine glyph dispatch | Bit-exact across GCC & ZCC (120 configs) |
| `tests/test_simd_alignment_gauntlet.c` | Hardened SIMD alignment harness testing 4 vectorization classes | 53 configs, 8 unaligned offsets, 0 faults |
| `tools/test_zcc_opt_passes.py` | 12-suite multi-target optimization pass validator | 12/12 bit-exact PASS, 1,825 lines elided (-2.30%) |
| `H:/corpus_superopt_leaderboard.json` | Master leaderboard attestation root | Sealed SHA-256 Merkle root |
| `H:/superopt_receipt_read_escape.json` | Signed cryptographic receipt for read_escape | Merkle proof sealed |
| `H:/superopt_receipt_parse_initializer_list.json` | Signed cryptographic receipt for parse_initializer_list | Merkle proof sealed |
| `H:/superopt_receipt_HUlib_drawTextLine.json` | Signed cryptographic receipt for HUlib_drawTextLine | Merkle proof sealed |
| `H:/superopt_receipt_hardened_simd_12.json` | Signed cryptographic receipt for 12 Contested SIMD functions | Merkle proof sealed |
