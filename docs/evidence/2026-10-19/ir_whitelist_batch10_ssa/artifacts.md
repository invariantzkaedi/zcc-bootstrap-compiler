# Artifacts: Batch 10 SSA Promotion
**Date:** 2026-10-19 / 2026-10-20  
**Target:** SSA-IR Lowering Whitelist Batch 10 Expansion

## Key Artifacts & Checksums

| Artifact | Path | MD5 Checksum | Lines / Size |
|---|---|---|---|
| Stage 2 Assembly | `zcc2.s` | `becaea8339cb7f3bb0d5d07c8e5cf787` | 493,351 lines |
| Stage 3 Assembly | `zcc3.s` | `becaea8339cb7f3bb0d5d07c8e5cf787` | 493,351 lines |
| Pre-Patch Probe Assembly | `/tmp/probe_batch10_before.s` | - | 0 `.Lir_b_` basic blocks |
| Post-Patch Probe Assembly | `/tmp/probe_batch10_after.s` | - | 321 `.Lir_b_` basic blocks |
| Whitelist Implementation | `part4.c` | - | +5 lines (`ir_whitelisted`) |
| Probe Test Suite | `tests/probe_batch10_ssa.c` | - | 15 candidate routines |
| Target Harness Binary | `/tmp/test_quickjs_runner` | - | 15/15 tests passing |
