# Gate Verdicts — Zero-Drop Structured LLM Gateway (Idea 1)

- **Gate 1 (Self-Host Identity):** `PASS` (`cmp zcc2.s zcc3.s` -> `0` byte delta, SHA-256 `d3d5fcf2043ff16cf17dee6641dfa216de2c6047fb0c20a0c4e78c78f770420c`)
- **Gate 2 (Cross-Toolchain Interoperability):** `NOT-APPLICABLE` (No C compiler/codegen/ABI files modified; Python-only gateway in `zkaedi_prime/squozen_engine.py` and `tools/prime/zero_drop_gateway.py`)
- **Gate 3 (797-Function Corpus Diff):** `NOT-APPLICABLE` (`part0_pp.c` and `part3.c` untouched)
- **Gate 4 (Target Harness — 4-Arm Paired Causal Gauntlet + HTTP Smoke Test):** `PASS`
  - **Arm 1 (`active`):** `100.00%` validity (6/6), `0.0%` backtick leakage
  - **Arm 2 (`unconstrained`):** `0.00%` validity (0/6), `100.0%` backtick leakage
  - **Arm 3 (`naive_sham`):** `0.00%` validity (0/6), fails on BPE merges, string-embedded braces, and trailing tokens
  - **Arm 4 (`stress`, $T_0=1.35$):** `100.00%` validity (6/6), `0.0%` backtick leakage
  - **HTTP Gateway Smoke Test (`/v1/health` + `/v1/chat/completions`):** `200 OK`, `PASS`
- **Gate 5 (Evidence Freshness):** `PASS` (All logs and artifacts generated in the current turn)

