# Artifacts: Setjmp Volatile Register Preservation (Vault Dimension 22)

## Key Source Files
- `part0_pp.c`
  - SHA256: `65e8e8dca5b3be7fb02ba91e1d672980c95b734ae6b77b04cf0cb7c2dfb76e10`
  - MD5: `d113331d4beb81e2881e794c1364f6da`
  - Modification: +7 lines in synthetic `zcc_stddef_text` declaring `setjmp`, `longjmp`, `_setjmp`, `_longjmp`, `sigjmp_buf`, `sigsetjmp`, `siglongjmp`.

## Self-Host Assembly Artifacts
- `zcc2.s`
  - SHA256: `e5f3d0af6f31c5f4412dac25ae50e5d980bd5076a849552b2c5ef68101ad1e0f`
  - MD5: `d9a54eb851ec1e9b9d966da80532e92d`
- `zcc3.s`
  - SHA256: `e5f3d0af6f31c5f4412dac25ae50e5d980bd5076a849552b2c5ef68101ad1e0f`
  - MD5: `d9a54eb851ec1e9b9d966da80532e92d`
  - `cmp zcc2.s zcc3.s` -> Byte identical (Exit 0)

## Verified Vault Test Artifacts
- Vault Path: `docs/evidence/proof_of_bug_vault/`
  - Total Bug Receipts: 80
  - Dimension 04 (`04_Designated_Union_Initializer_Stride`): 33 / 33 resolved (100.0%)
  - Dimension 22 (`22_Setjmp_Volatile_Register_Preservation`): 47 / 47 resolved (100.0%)
  - Total Passing: 80 / 80 (100.0%)
- Vault Audit Report: `docs/evidence/vault_audit_report.json`
