# Fortify Agent Review Checklist

## Hard blockers

- [ ] No placeholder comments.
- [ ] No TODO/FIXME/STUB text unless explicitly policy-allowed.
- [ ] No uppercase `.ZCC-FORTIFY-POLICY.JSON`.
- [ ] `_Alignof` does not call size logic.
- [ ] `_Static_assert` is not macro-erased.
- [ ] Unknown target does not silently fall back.
- [ ] Layout arithmetic is checked.
- [ ] Production signing cannot be skipped when required.

## Compiler behavior

- [ ] `sizeof(T)` uses unified layout.
- [ ] `_Alignof(T)` uses unified layout alignment.
- [ ] Struct offsets are computed once.
- [ ] Union members offset to zero.
- [ ] Array size uses checked `element_size * count`.
- [ ] Recursive aggregate failure is diagnosed.
- [ ] Pointer recursion succeeds.
- [ ] Unsupported bitfields fail closed or are fully implemented.
- [ ] Flexible array behavior is explicit and tested.

## CI behavior

- [ ] `ci/fortify-layout.sh` runs in dev mode.
- [ ] `ci/fortify-layout-production.sh` requires signing and pins.
- [ ] Placeholder gate is policy-aware.
- [ ] Policy validator enforces expected rules.
- [ ] Attestation negative tests exist.
- [ ] Artifacts are hash-bound.
- [ ] Bundle verifies offline.

## Final report

- [ ] Files changed listed.
- [ ] Commands run listed.
- [ ] Failures explained.
- [ ] Remaining risks listed.