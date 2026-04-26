# Layout Tests

This directory contains hand-authored and generated tests for ZCC ABI layout correctness.

## Test categories

- Golden ABI tests
- `_Alignof` tests
- `_Static_assert` pass/fail tests
- Struct offset tests
- Union layout tests
- Recursive aggregate tests
- Unsupported feature fail-closed tests
- Overflow tests
- Generated random oracle tests

## Important invariants

- `_Alignof(T)` must return alignment, not size.
- Array size is `element.size * count`.
- Struct size includes trailing padding.
- Union size is max member size rounded to max member alignment.
- Unsupported features must fail with stable diagnostics.