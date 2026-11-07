# Verification Gate Verdict (`WASM-MV-29..30` / `BITOPS-MULOVF-SAT-01` / `WASM-SIMD128-UNARY-F32-01` / `ZCC-PROVE-20..21`)

| Gate | Proposition | Command | Verdict |
| :--- | :--- | :--- | :--- |
| **Gate 1** | 3-Stage Self-Host Identity (`zcc2.s == zcc3.s`, `zcc2 == zcc3`) | `make selfhost && cmp -s zcc2.s zcc3.s && cmp -s zcc2 zcc3` | **PASS** (`0` byte delta, SHA-256 `d40f882504b3f53d7a5ca1f86442c15302fd8592e9a0881dfc2ebbdbffc08617`) |
| **Gate 2** | Cross-Toolchain ABI Interoperability (`zcc-lib+gcc-main` & `gcc-lib+zcc-main`) | `/tmp/mv02_interop_1 && /tmp/mv02_interop_2` | **PASS** (`EXIT 0` both directions) |
| **Gate 3** | Native Regression Corpus (`zcc_test_suite.sh --quick`) | `bash zcc_test_suite.sh --quick` | **PASS** (`PASS: 35  FAIL: 0  SKIP: 3`) |
| **Gate 4A** | WASM i64 & ILP32 Regression Suite | `python3 -m unittest discover -s tests -p "test_wasm_i64.py" -v` | **PASS** (`Ran 5 tests — OK`, 62 sub-assertions) |
| **Gate 4B** | Adversarial Multi-ISA & WASM-MV-02..30 Semantic Suite (`test_01..test_21`) | `python3 -m unittest discover -s tests -p "test_wasm_mv02.py" -v` | **PASS** (`Ran 21 tests in 16.441s — OK`) |
| **Gate 4C** | Negative-Control Oracle Harness | `python3 ZCC_ORACLE_NEGATIVE_CONTROLS.py` | **PASS** (`0` false certifications across 3 negative controls) |
| **Gate 4D** | Formal Z3 SMT Equivalence Proof (`ZCC-PROVE-01..21`) | `python3 tools/zcc_prove_01.py --out-json ...` | **PASS** (`25/25` operand classes + `5000` diff states + `ZCC-PROVE-02..21` `UNSAT`) |
| **Gate 5** | 108-Mutation Adversarial Kill Ledger (`MV02..MV30`) | `python3 tools/mv02_mutation.py --run-ledger ...` | **PASS** (`108/108` mutations killed in `123.35s`) |
