# Gate Verdict: VLA Decay Variadic Spill (Vault Dimension 27)

## Gate Status Summary

| Gate | Description | Command | Status | Details |
|---|---|---|---|---|
| **Gate 1** | Self-Host Identity | `cmp zcc2.s zcc3.s` | **PASS** | Byte-identical assembly output (MD5: `215e562b67121f020c9fcc2d2608fc9d`) |
| **Gate 2** | Cross-Toolchain Interop | Bidirectional library execution | **PASS** | `zcc-lib + gcc-main`: 42, `gcc-lib + zcc-main`: 42 |
| **Gate 3** | Opt-Gauntlet Regression | `python3 tools/test_zcc_opt_passes.py` | **PASS** | 19/19 suites bit-exact, 0 divergences, 8,891 instructions elided (9.93%) |
| **Gate 4** | Vault Target Harness | `python3 tools/auto_patch_bug_vault.py` | **PASS** | 87 / 87 (100.0%) bug receipts resolved bit-exact vs GCC 13.3 & Clang 18.1 |
| **Gate 5** | Evidence Freshness | Verification re-run | **PASS** | Complete logs and artifacts preserved |

## Invariant Preserved
- SystemV AMD64 ABI §3.5.7: `va_list.overflow_arg_area` must point to the memory location of the first variadic argument passed on the stack.
- When a function defines more than 6 general-purpose parameters (or floats exceeding 8 XMM registers), named parameters spill to stack slots `16(%rbp)`, `24(%rbp)`, etc.
- In `part4.c`, `__builtin_va_start` now dynamically tracks `stack_arg_off`, emitting `leaq stack_arg_off(%rbp), %rax` instead of hardcoding `16(%rbp)`.
- Foundational file diff strictly bounded to +9 lines in `part4.c` (limit: <50 lines).
