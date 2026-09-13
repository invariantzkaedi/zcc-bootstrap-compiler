# CG-IR-013 Artifacts and Checksums

## Key Files Touched
- `compiler_passes.c`: Added `ZCC_IR_SPILL` environment variable gate controlling the linear-scan spill fallback path.
- `part4.c`: Whitelisted `next_token` and `forced_spill_calc` in `wl[]`, implemented `is_ir_eligible` to protect target functions returning 16-byte SystemV structs (preventing E-LEARN-016 regressions).
- `tests/test_ir_spill_forced.c`: Unit test exercising linear-scan spilling with 12 simultaneously live variables (pressure > 7).
- `BOOTSTRAP_BASELINES.tsv`: Appended new convergence hash `53cd1862760748f654f6caa815d7fdb7` for WSL2 environment.

## Checksums
| File | MD5 Checksum |
|---|---|
| `zcc2.s` | `53cd1862760748f654f6caa815d7fdb7` |
| `zcc3.s` | `53cd1862760748f654f6caa815d7fdb7` |
| `tests/test_ir_spill_forced.c` | `cf6b41295b93d09a5b33ca82b01281db` |
| `tests/opt/test_ir_spill_forced.c` | `cf6b41295b93d09a5b33ca82b01281db` |
