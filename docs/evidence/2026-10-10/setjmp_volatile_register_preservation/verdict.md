# Gate Verdict: Setjmp Volatile Register Preservation (Vault Dimension 22)

## Gate Status Summary

| Gate | Description | Command | Status | Details |
|---|---|---|---|---|
| **Gate 1** | Self-Host Identity | `cmp zcc2.s zcc3.s` | **PASS** | Byte-identical assembly output (MD5: `d9a54eb851ec1e9b9d966da80532e92d`) |
| **Gate 2** | Cross-Toolchain Interop | Bidirectional library execution | **PASS** | `zcc-lib + gcc-main`: 42, `gcc-lib + zcc-main`: 42 |
| **Gate 3** | Opt-Gauntlet Regression | `python3 tools/test_zcc_opt_passes.py` | **PASS** | 19/19 suites bit-exact, 0 divergences, 8,891 instructions elided (9.93%) |
| **Gate 4** | Vault Target Harness | `python3 tools/auto_patch_bug_vault.py` | **PASS** | 80 / 80 (100.0%) bug receipts resolved bit-exact vs GCC 13.3 & Clang 18.1 |
| **Gate 5** | Evidence Freshness | Verification re-run | **PASS** | Complete logs and artifacts preserved |

## Invariant Preserved
- Standard C99 / SystemV `<setjmp.h>` function declarations (`setjmp`, `longjmp`, `_setjmp`, `_longjmp`, `sigsetjmp`, `siglongjmp`) declared in compiler synthetic standard library stubs.
- Local variables allocated in stack frames (`-X(%rbp)`) inherently preserve values across `longjmp` register unwinding.
- Foundational file diff strictly bounded to +7 lines in `part0_pp.c` (limit: <50 lines).
