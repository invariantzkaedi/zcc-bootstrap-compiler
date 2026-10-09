# Artifacts: SMT-Backed Translation Validation (Candidate 3)

- `src/zcc_smt_prover.c`: Added implementation for `smt_prove_ir_strength_reduction` and `smt_prove_ir_peephole` synthesizing `QF_BV` SMT-LIB2 proofs.
- `verify_smt.c`: Added test calls verifying 5 strength reduction cases, 14 algebraic peepholes, and stack alignment proofs.
- `verify_smt.py`: Test runner asserting syntax, variable collision resistance, and Z3 refutation (`unsat`) for all 36 generated proofs.
- `test_proofs/`: 36 verified SMT-LIB2 proof artifacts in `QF_BV` logic.
