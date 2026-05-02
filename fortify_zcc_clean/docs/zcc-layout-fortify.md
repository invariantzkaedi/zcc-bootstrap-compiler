# ZCC Layout Fortify Design

## Goal

Make ZCC layout semantics correct, testable, fuzzed, and attestable.

## Compiler correctness targets

- `_Alignof(T)` returns ABI alignment.
- `sizeof(T)` returns ABI size.
- `_Static_assert` is a compile-time constraint.
- Struct and union layout use one shared ABI model.
- Arrays use checked multiplication.
- Alignment arithmetic is overflow checked.
- Unsupported layout features fail closed.

## Layout consumers

The unified layout engine must be used by:

- `sizeof`
- `_Alignof`
- `_Alignas`
- struct layout
- union layout
- member access
- codegen
- ABI tests
- static assertions
- layout dump mode

## CI fortify layers

1. Golden ABI tests.
2. Differential host compiler oracle.
3. Randomized layout fuzzing.
4. Cross-target layout dump oracle.
5. Reducer artifacts.
6. No-placeholder gate.
7. Layout-core-freeze gate.
8. Evidence manifest.
9. Attestation bundle.
10. Production trust-root verification.

## Production invariant

The compiler is not trusted because the code says it is correct.

It is trusted because:

- known regressions are tested
- unknown layout space is fuzzed
- failures are reduced
- artifacts are hashed
- evidence is signed
- production verification uses an external trust root