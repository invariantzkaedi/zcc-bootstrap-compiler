# Key Artifacts & Checksums

## Source Files
- `part3.c`: SHA256 (updated with nested designator & designated union/struct initializer lookup)
- `BOOTSTRAP_BASELINES.tsv`: SHA256 (updated with baseline md5 `4f3dc63784c27ac868c2d87c10ea029a`)
- `tools/auto_patch_bug_vault.py`: Automated vault verification and auto-patch factory

## Assembly & Binary Outputs
- `zcc2.s`:
  - MD5: `4f3dc63784c27ac868c2d87c10ea029a`
  - SHA256: `ec2ccf1d9abf219509768316fdf54b190accbb1af8110f2f21516dbf3dd469fe`
- `zcc3.s`:
  - MD5: `4f3dc63784c27ac868c2d87c10ea029a`
  - SHA256: `ec2ccf1d9abf219509768316fdf54b190accbb1af8110f2f21516dbf3dd469fe`
- `cmp zcc2.s zcc3.s`: Byte-identical (Exit 0)

## Repro Files
- `docs/evidence/proof_of_bug_vault/APEX_BUG_03a6564c0351d740/repro.c`:
  - GCC exit: 109
  - Clang exit: 109
  - ZCC pre-patch: 139 (SIGSEGV)
  - ZCC post-patch: 109 (PASS)
- Vault Resolution: 33 / 33 bugs in `04_Designated_Union_Initializer_Stride` resolved (100.0%)
