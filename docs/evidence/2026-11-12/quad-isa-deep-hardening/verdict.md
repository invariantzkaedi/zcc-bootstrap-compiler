# Gate Verdicts — quad-isa-deep-hardening (2026-11-12)

- **Gate 1 (Self-host identity):** `PASS` — `make selfhost` exited `0` (`elapsed=140.3s`); `cmp zcc2.s zcc3.s` returned `0` with identical SHA-256 `c38fc91a16e3836cc1a214636e51de7dd443841520c3aadb600980890171e357` (and `stage2.bin == stage3.bin` SHA-256 `c7d5516ad8447fe01640c830d36f5fb81b7eabf1cdb7d7e0871e2cc93d3f36b8`).
- **Gate 2 (Cross-toolchain interoperability):** `PASS` — `make selfhost` passed `[Cross-TU ABI Oracle]` (`[PASS: Triple-Corpus Bidirectional Parity]`) and `[Semantic Oracle]` (`[PASS: Byte-Identical Semantic Parity]`); explicit `run_gate2.py` bidirectional object interop (`zcc -S`+`gcc -c` linked with `gcc -O2 -c`) passed for both `zcc-lib + gcc-main` and `gcc-lib + zcc-main` (`INTEROP=e7ffff00:c848d15a`, exit `0`).
- **Gate 3 (797-function corpus diff):** `NOT-APPLICABLE` — Neither `part0_pp.c` nor `part3.c` was modified (`0` diff lines).
- **Gate 4 (Target harness — Quad-ISA + Deep Regression + ARGON 4):** `PASS` — `make test-quad-isa` exited `0`:
  - `python3 tools/zcc_quad_isa_gate.py --strict-tier 3 --verify-ui`: Tier 1 (`0x7e07f55a67f3cc08`, `7/7`), Tier 2 (`0xfb986fd4f88819a9`, `7/7`), Tier 3 (`0x6d4f29801741710f`, `7/7`), `discovered_defects: []`, `strict_gate_tier: 3`, EF-7 UI verifier `PASS` (`1 script(s) AST-clean, 95 DOM IDs bound, WASM=2765B, 8/8 suites PASS, EF-0 self-certify trap killed, 0x0 canvas safe, Stop=INCONCLUSIVE`).
  - `python3 tools/zcc_quad_isa_gate.py --self-test`: `POS-01` + `NEG-01` + `NEG-02` + `NEG-03` ALL KILLED / `PASS`.
  - `python3 tests/test_quad_isa_deep_regression.py`: GCC/Clang `-O2` oracle `D2=04e2ea19 F4=e05f3717`; `zcc_x86_64`, `zcc_arm64` (`qemu-aarch64-static`), and `zcc_riscv64` (`qemu-riscv64-static`) all `PASS` (`D2=04e2ea19 F4=e05f3717`).
  - `make selfhost` ARGON 4 passed (`[✔ ARGON:6/6]`).
- **Gate 5 (Evidence freshness):** `PASS` — All gates executed cleanly against the final edits in this turn; `pre_iterate_verify.py` passed (`5/5`, `0` violations) on modified scripts, regenerated `zcc_hyperdim_test_harness.html`, and regenerated `artifacts/ZCC_QUAD_ISA_GATE_RECEIPT.json`.

## Scope & Open Residual Findings (`part4.c` `< 50` Cap Respected)
- `part4.c` sits at `+34 / -14` (**48** changed lines, strictly `< 50` vs `HEAD`).
- Three additional defects discovered via EF-3 probes (`probe_deep2.c` / `probe_deep4.c`) require further `part4.c` edits beyond the `< 50` ceiling and are preserved unpatched with exact reproducers pending commit or explicit `> 50` authorization:
  1. `BUG-DEEP-01` (`part4.c:4381-4390, 4636-4646`, ALL 64-bit targets x86-64/ARM64/RV64): nested indirect calls overwrite `cc->abi_scratch_offset + 16` (`V3=00000174` vs GCC/Clang `00000154`).
  2. `BUG-DEEP-03` (`part4.c:3886, 3974, 4062, 4149`, ARM64/RV64): 32-bit `++`/`--` emits `str r1, [r0]` -> 8-byte `sd`/`str x1` store clobbering adjacent struct fields (`V1=8396e7f1` vs `70c9044b`).
  3. `BUG-DEEP-05` (`part4.c:2769-2774`, ARM64/RV64): 32-bit `ND_BNOT` emits 64-bit `not`/`mvn` without zero-extension, leaking upper 1-bits on right shift (`Q1=aaaaaa6c` vs `2aaaaa6c`).

