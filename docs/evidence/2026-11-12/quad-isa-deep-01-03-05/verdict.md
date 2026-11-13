# Gate Verdicts (`quad-isa-deep-01-03-05`)

- **Gate 1 (Self-host identity `cmp zcc2.s zcc3.s`):** `PASS` (`0` byte delta; `sha256: 862259d78e54b853a0f178406d2892815ef126847caee3edd0c7fa66b2f79a08`).
- **Gate 2 (Cross-toolchain interoperability):** `PASS` (`zcc-lib + gcc-main` and `gcc-lib + zcc-main` both output `INTEROP=e7ffff00:c848d15a`, matching `GCC+GCC`).
- **Gate 3 (Corpus / ARGON 4 6-suite regression):** `PASS` (`bash gate.sh`: all 6 ARGON 4 suites + 12 SVG targets + 192 C99 differential/UBSan/ASan executions + `SELFHOST-MV1.3b PASS`).
- **Gate 4 (Target harness — Deep 01/03/05 + Deep Regression + Quad-ISA 3-Tier Torture + EF-7 UI + EF-0 Self-Test):** `PASS`
  - `test_quad_isa_deep_01_03_05.py`: `M1=000655aa M2=bdf69ba3 M3=9555556d M4=80001fff` across `gcc -O2`, `clang -O2`, `zcc_x86_64`, `zcc_arm64`, `zcc_riscv64` (`EF2_missing=[]`).
  - `test_quad_isa_deep_regression.py`: `D2=04e2ea19 F4=e05f3717` across all 3 native/QEMU backends.
  - `zcc_quad_isa_gate.py --strict-tier 3 --verify-ui`: `tier1_core` (`0x7e07f55a67f3cc08`), `tier2_struct_union` (`0xfb986fd4f88819a9`), `tier3_edge_abi` (`0x6d4f29801741710f`) `7/7` lanes `PASS`; EF-7 UI `8/8` suites `PASS`.
  - `zcc_quad_isa_gate.py --self-test`: `POS-01` + `NEG-01..03` all killed (`SELF-TEST PASS`).
- **Gate 5 (Evidence freshness):** `PASS` (All logs, binaries, and receipts generated in current turn).

