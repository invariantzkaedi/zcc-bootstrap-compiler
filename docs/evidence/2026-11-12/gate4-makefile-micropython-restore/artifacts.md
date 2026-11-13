# Artifacts — Gate 4 Makefile Targets & MicroPython `part3.c` Restoration

- **Modified Files**:
  - `Makefile` (`+32 / -0` lines: wired `test_quad_isa_deep_01_03_05.py` into `test-quad-isa` and added `test-sqlite`, `test-quickjs`, `test-micropython`, `test-doom`, `qemu`, and `valgrind` targets).
  - `part3.c` (`+29 / -1 = 30 < 50` changed lines: restored MicroPython `sizeof((allowed_args)[0])` inner parenthesized expression postfix, compound-literal nested sub-struct braces `{{type_in}, ...}`, and local struct partial-initializer zeroing for `compiler_t comp_state = {0};`).
- **Self-Host Stage 2 / Stage 3 Assembly Hash**:
  - `stage2.s` / `stage3.s` SHA-256 prefix: `4566f347977db700...` (`0` byte delta)
- **Target Harness Receipts**:
  - SQLite (`test-sqlite`): `docs/evidence/2026-11-13/core_03_sqlite/stage3_ledger_20261113_045128_112904.json` (`CORE-03A` `ad41bbcb79f1`, `CORE-03B` `2c7ee1ff2040`)
  - QuickJS (`test-quickjs`): `build/quickjs_zcc/test_quickjs_runner` (`Failures: 0`)
  - MicroPython (`test-micropython`): `build/micropython_native_runner` (`26 / 26 PASSED (0 FAILURES)`)
  - DOOM (`test-doom`): `/tmp/doom_verify_elf` (`822 KB` ELF64, `47,025` peepholes elided)

