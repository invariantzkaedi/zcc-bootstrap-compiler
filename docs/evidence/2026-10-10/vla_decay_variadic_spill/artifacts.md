# Artifacts: VLA Decay Variadic Spill (Vault Dimension 27)

## Key Source Files
- `part4.c`
  - SHA256: `a03455d6b9f8bcde1d9cae1e4fd7cd50a19ed75260252300fb4e94c6ed587860`
  - MD5: `d8c5d524c41302642e4371947ed1c12e`
  - Modification: Track `stack_arg_off` in `__builtin_va_start` (+9 lines diff), correctly setting `overflow_arg_area` to point to the first variadic argument when named parameters spill to stack slots.

## Self-Host Assembly Artifacts
- `zcc2.s`
  - SHA256: `d294da42f92c11c56ba451aaad15c02515e994f9f522df1bfc0493574d3247d9`
  - MD5: `215e562b67121f020c9fcc2d2608fc9d`
- `zcc3.s`
  - SHA256: `d294da42f92c11c56ba451aaad15c02515e994f9f522df1bfc0493574d3247d9`
  - MD5: `215e562b67121f020c9fcc2d2608fc9d`
  - `cmp zcc2.s zcc3.s` -> Byte identical (Exit 0)

## Verified Vault Test Artifacts
- Vault Path: `docs/evidence/proof_of_bug_vault/`
  - Total Bug Receipts: 87
  - Dimension 04 (`04_Designated_Union_Initializer_Stride`): 33 / 33 resolved (100.0%)
  - Dimension 22 (`22_Setjmp_Volatile_Register_Preservation`): 47 / 47 resolved (100.0%)
  - Dimension 27 (`27_VLA_Decay_Variadic_Spill`): 7 / 7 resolved (100.0%)
  - Total Passing: 87 / 87 (100.0%)
- Vault Audit Report: `docs/evidence/vault_audit_report.json`
