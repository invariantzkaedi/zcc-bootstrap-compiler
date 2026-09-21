# Verification Verdict: LIMIT-006 (Dual-Register Struct Return in SSA-IR: `%rax/%rdx`)

## Four-State Gate Verdicts (Rule EF-6)

- **Gate 1 — Self-Host Identity**: **PASS**
  - Command: `cmp zcc2.s zcc3.s`
  - Output: Byte-identical assembly (`exit 0`), SHA-256 `0f8398bbed202e4175552f1f2bb35668d6653ff0917de30fb741ac401a8f8df6`.
- **Gate 2 — Cross-Toolchain & Optimizer Interoperability**: **PASS**
  - Command: `python3 tools/test_zcc_opt_passes.py`
  - Output: 19/19 test suites bit-exact with 0 semantic divergences (`exit 0`).
  - Command: `gate.sh` cross-TU ABI suite (MV1.2, MV1.3a, MV1.3b): PASS.
- **Gate 3 — Corpus Regression (797-function diff)**: **NOT-APPLICABLE**
  - Reason: `part0_pp.c` and `part3.c` were not modified. Diff localized to `part4.c` (net 33 lines, <50 budget), `compiler_passes.c`, `ir.c`, `ir.h`, `ir_dominance.c`, `ir_emit_dispatch.h`, `ir_pass_manager.c`, and `ir_to_x86.c`.
- **Gate 4 — Target Harness Verification**: **PASS**
  - Command: `tests/probe_ret2.c` in default, `ZCC_IR_LOWER=1`, and `--ir` modes: all 3 output `PROBE_RET2 PASS: p.a=123456789, p.b=987654321` (`exit 0`).
  - Command: `/tmp/test_quickjs_runner` (QuickJS ES2020 native harness): 15/15 tests PASS (`exit 0`).
- **Gate 5 — Evidence Freshness**: **PASS**
  - All gates freshly executed in same session on active checked-out tree.

## Final Status
**STATUS: CLOSURE-READY (GREEN)**
