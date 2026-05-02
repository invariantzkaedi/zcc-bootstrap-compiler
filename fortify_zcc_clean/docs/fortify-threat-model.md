# Fortify Threat Model

## Compiler semantic threats

- `_Alignof` accidentally implemented as `sizeof`.
- `_Static_assert` erased by preprocessor compatibility stubs.
- Struct layout and codegen disagree.
- Union alignment drifts from struct alignment.
- Array size arithmetic overflows.
- Unsupported bitfields are approximated silently.
- Flexible arrays are mis-sized.
- Target ABI falls back silently.

## CI integrity threats

- Placeholder code lands.
- Legacy layout path returns.
- Fuzz mismatch lacks reproduction artifact.
- CI artifact is mutated after hashing.
- Signed manifest is mutated after signing.
- Production signing is skipped.
- Valid signature over wrong commit is accepted.
- Valid signature from wrong signer is accepted.
- Trust policy silently weakens verification.
- Allowlist exceptions rot.

## Controls

- Unified layout engine.
- Stable diagnostics.
- Golden ABI tests.
- Differential oracle.
- Random fuzzing.
- Cross-target dump oracle.
- Reducer.
- No-placeholder scanner.
- Layout-core freeze.
- Deterministic hashes.
- Immutable subject manifest.
- Attestation bundle.
- External trust policy.
- Signature verification.
- Anti-substitution pins.