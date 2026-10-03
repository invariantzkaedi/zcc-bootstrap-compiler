# Artifacts: Algebraic Identity & Constant Elimination Peephole

- `part5.c`: Added Patterns 2 (generalized add/sub $0 on all registers), 2b (redundant self-moves movq %reg, %reg), 2c (identity imulq $1), 2d (zero imulq $0 -> xorl), and 2e (zero add/sub fold).
- `tests/probe_algebraic_identities.c`: Deterministic regression probe testing all 5 algebraic cases (exit 0).
- `zcc2.s`, `zcc3.s`: Byte-identical bootstrap assembly artifacts.
