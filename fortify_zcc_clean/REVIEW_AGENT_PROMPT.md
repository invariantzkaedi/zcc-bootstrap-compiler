# Review Task: Attack the Fortify Implementation

Review the implementation adversarially.

Look for:

- `_Alignof` calling size logic
- `_Static_assert` being macro-erased
- const-eval accepting non-constant expressions
- array size using alignment
- unchecked overflow in layout arithmetic
- target fallback
- duplicate layout logic
- codegen bypassing layout engine
- placeholder comments
- uppercase policy filename
- attestation subject mutation after signing
- production signing skip
- bundle verifier trusting bundle policy
- valid signature accepted for wrong commit
- unallowed signer accepted
- missing cosign certificate hashing
- empty `GITHUB_SHA` accepted
- tests that do not actually assert diagnostics
- CI scripts that pass even when tools are missing

Return blocking findings first, with file paths and exact fixes.