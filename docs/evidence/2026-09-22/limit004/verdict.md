# LIMIT-004 Verification Verdict

## Target Defect
- **Target**: LIMIT-004 (Autonomous Self-Hosting Linker `zld` & Dynamic Relocations)
- **Scope**: Multi-archive static library ingestion (`.a`), topological symbol resolution, freestanding System V AMD64 CRT0 entry synthesis, default layout fallbacks.

## Gate Verdicts

### Gate 1: Self-Host Identity (Mandatory)
- **Command**: `cmp zcc2.s zcc3.s`
- **Output**: Byte-identical (`c951344bc1dfe7b9e9395f49f22163d4`)
- **Status**: **PASS**

### Gate 2: Cross-Toolchain Interoperability & Passes (Mandatory)
- **Command**: `python3 tools/test_zcc_opt_passes.py`
- **Output**: 19/19 suites bit-exact, 0 semantic divergences, 2,067 instructions elided (2.31% net reduction)
- **Status**: **PASS**

### Gate 3: Structural Modification Budget (Mandatory)
- **Command**: `git diff --stat part0_pp.c part3.c part4.c`
- **Output**: 0 lines touched in `part0_pp.c`, `part3.c`, `part4.c`. Driver/parts diff (`part5.c` 5 lines, `src/codegen.c` 4 lines) = 9 lines (< 50 lines).
- **Status**: **PASS**

### Gate 4: Target Harness (Conditional)
- **Commands**:
  1. `/tmp/run_probe_limit004` -> `PROBE_LIMIT004 PASS: Multi-archive linking and freestanding execution verified!` (rc=0)
  2. `/tmp/test_quickjs_runner` -> 15/15 tests passing cleanly across 6 test suites (rc=0)
- **Status**: **PASS**

### Gate 5: Evidence Freshness & Ledger Verification (Mandatory)
- **Command**: Fresh selfhost verification + `BOOTSTRAP_BASELINES.tsv` entry locked.
- **Status**: **PASS**

## Overall Status: CLOSED (GREEN)
