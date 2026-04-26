# ZCC Fortify Agent Instructions

You are implementing the ZCC layout correctness and fortify attestation program.

## Non-negotiable rules

- Do not add placeholders.
- Do not add TODO/FIXME/STUB comments.
- Do not leave pseudocode.
- Do not silently stub C semantics.
- Do not implement `_Alignof` using `sizeof`.
- Do not erase `_Static_assert`.
- Do not add compatibility shims that hide semantic failures.
- Do not mutate the signed attestation subject after signing.
- Do not trust the attestation bundle without external verify policy in production mode.

## Required implementation order

1. Inspect the existing compiler architecture.
2. Find existing type, parser, diagnostic, const-eval, layout, target, and CI code.
3. Integrate with existing patterns where possible.
4. Add or adapt the unified layout engine.
5. Implement real `_Alignof`.
6. Implement real `_Static_assert`.
7. Add stable diagnostics.
8. Add tests.
9. Add differential oracle/fuzz/reducer tooling.
10. Add CI gates.
11. Add evidence manifests and attestation bundle.
12. Add production trust-root verification.

## Required invariants

- `sizeof(T)` uses the unified layout model.
- `_Alignof(T)` uses alignment, not size.
- Struct/union offsets come from one layout engine.
- Array size is `element.size * count` with overflow checking.
- Layout alignment arithmetic is checked.
- `_Static_assert` requires an integer constant expression.
- Failed `_Static_assert` emits a compile-time error and no codegen.
- Unsupported bitfields/flexible arrays must fail closed unless fully implemented.
- CI must reject placeholder code.
- Production attestation must require a signature and external trust policy.

## Required validation

Run, or document why unavailable:

```bash
ci/fortify-layout.sh
```

For production mode:

```bash
FORTIFY_SIGNER=cosign \
FORTIFY_REQUIRE_SIGNATURE=1 \
GITHUB_SHA="$(git rev-parse HEAD)" \
ci/fortify-layout-production.sh
```

## Output expectations

At the end, report:

- files changed
- compiler behavior changed
- diagnostics added
- tests added
- commands run
- failures encountered
- remaining risks