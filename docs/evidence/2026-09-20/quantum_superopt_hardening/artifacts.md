# Quantum Superopt Hardening — Artifact Registry

| Artifact Path | Description | Checksum (SHA-256) | Status |
| :--- | :--- | :--- | :--- |
| `tests/diff_read_escape.c` | Differential test harness for read_escape O(1) LUT lowering | `939794226406cb07143f8647b0efda417e052c5a5081344ccf799e0957da05b6` | Bit-exact across GCC & ZCC (267 vectors) |
| `tests/diff_parse_initializer_list.c` | Differential test harness for parse_initializer_list stack zero-alloc | `fc1be80ed3ae508b0f7037e144fa51bf295eb91a668bc564a73b6ee970042f05` | Bit-exact across GCC & ZCC (5 suites) |
| `tests/diff_HUlib_drawTextLine.c` | Differential test harness for HUlib_drawTextLine glyph dispatch | `0ec3271ee5fd8f87de3032e6dada2e9ba679b8f135f26f7d2e31f64bf0cb1a96` | Bit-exact across GCC & ZCC (120 configs) |
| `tests/test_simd_alignment_gauntlet.c` | Hardened SIMD alignment harness testing 4 vectorization classes | `06e4bfc1cf87cc8f4fd32bea295f32846537740802517529affc49cea2142d56` | 53 configs, 8 unaligned offsets, 0 faults |
| `tools/test_zcc_opt_passes.py` | 13-suite multi-target optimization pass validator | `7eb1be0f3118f39883f655ff146af366f250a04e4e567444c81f90f8c4565986` | 13/13 bit-exact PASS, 1,897 lines elided (-2.30%) |
| `H:/corpus_superopt_leaderboard.json` | Master leaderboard attestation root | `cf8be6f128f987f90f5404c670fad30fe9aae3edb040fb20f07f99f70ccd2b3f` | Sealed Merkle Root: `e5264a64defba4e6...` |
| `H:/superopt_receipt_read_escape.json` | Signed cryptographic receipt for read_escape | `bccbcdbb8a790e376c1cbf967fd5b757f45b2bea9a0b6e25dc5c43b5b687a30d` | Merkle proof sealed |
| `H:/superopt_receipt_parse_initializer_list.json` | Signed cryptographic receipt for parse_initializer_list | `d28edefa1b0ba4d59956181cc46aed71a1432add9fcc56e128cfa1422442edec` | Merkle proof sealed |
| `H:/superopt_receipt_HUlib_drawTextLine.json` | Signed cryptographic receipt for HUlib_drawTextLine | `2878062cec60637c1e6bd864439499179bfc1e2edd171af0c4bf65edbac0578f` | Merkle proof sealed |
| `H:/superopt_receipt_hardened_simd_12.json` | Signed cryptographic receipt for 12 Contested SIMD functions | `8655401c0eeefafc3964ff472a97d76d5abb93e3332f4f3c971962b9c99aae91` | Merkle proof sealed |
| `tests/diff_R_DrawColumn.c` | Differential test harness for Doom R_DrawColumn texture-scaling blitter | `a42d57385a89b411ed71ffb64bc964c17f15071cc7dfa1f8b3df3488776a30ba` | Bit-exact across GCC & ZCC (2,568 columns) |
| `H:/superopt_receipt_R_DrawColumn.json` | Signed cryptographic receipt for R_DrawColumn | `70d430b085e6278aa1825c8e92e476c98f50108b6aaddf112078bfe37e35914b` | Merkle proof sealed (-72 lines elided) |

