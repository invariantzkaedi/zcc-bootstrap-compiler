# Task: Implement ZCC Layout Fortify Program

You are working in an existing C compiler repository called ZCC or equivalent. Your job is to implement the layout correctness and fortify attestation program described below.

## Prime directive

Do not paste isolated fantasy code. First inspect the repository. Integrate with the actual parser, type system, diagnostics, target ABI model, build system, and CI layout.

## Absolute constraints

- No placeholders.
- No TODO comments.
- No FIXME comments.
- No omitted code.
- No pseudocode.
- No compatibility shim that hides semantic failure.
- No `_Static_assert` macro erasure.
- No `_Alignof` implemented via size.
- No unchecked layout arithmetic.
- No silent fallback target ABI.
- No mutation of signed attestation subject after signing.
- No production signature skip.

If something cannot be implemented because the repository architecture differs, stop and explain the exact blocker with file paths and proposed alternatives.

## Required discovery phase

Before editing, inspect and report:

1. Existing type representation.
2. Existing parser handling for declarations, `sizeof`, keywords, and static assertions.
3. Existing constant expression evaluator.
4. Existing diagnostic/error reporting system.
5. Existing target/ABI configuration.
6. Existing struct/union layout code.
7. Existing codegen consumers of type size/alignment/offset.
8. Existing test framework.
9. Existing CI/build scripts.

Use existing conventions unless they are unsafe.

## Implementation goals

### Compiler semantics

Implement or adapt a unified layout engine with these public concepts:

- `zcc_get_layout(Type *, LayoutPhase)`
- `zcc_sizeof(Type *)`
- `zcc_alignof(Type *)`

The actual names may follow repo conventions, but the invariants must hold.

Required behavior:

- `sizeof(T)` returns ABI size.
- `_Alignof(T)` returns ABI alignment.
- `_Static_assert(expr, "msg")` is parsed as a compile-time declaration/constraint.
- `_Static_assert` expression must be an integer constant expression.
- Failed `_Static_assert` emits a compile error with the message.
- Struct member offsets are computed once by the unified layout model.
- Union layout uses max size and max alignment.
- Array size is `element.size * count` with overflow checking.
- Alignment arithmetic is checked for overflow.
- Unsupported bitfields/flexible arrays must either be fully implemented or fail closed with stable diagnostics.
- Target ABI must be explicit; unknown target must be fatal.

### Diagnostics

Add stable diagnostic codes for relevant failures, such as:

- `E_LAYOUT_ZERO_SIZE`
- `E_LAYOUT_MISMATCH`
- `E_LAYOUT_UNKNOWN_KIND`
- `E_LAYOUT_RECURSIVE_TYPE`
- `E_LAYOUT_INCOMPLETE_TYPE`
- `E_LAYOUT_SIZE_OVERFLOW`
- `E_LAYOUT_UNSUPPORTED_BITFIELD`
- `E_LAYOUT_UNSUPPORTED_FLEXIBLE_ARRAY`
- `E_STATIC_ASSERT_FAILED`
- `E_STATIC_ASSERT_NOT_CONSTANT`
- `E_ALIGNOF_TYPE`
- `E_ALIGNAS_INVALID`
- `E_TARGET_UNKNOWN`

If the repo already has diagnostics, integrate with it.

### Tests

Add tests for:

- `_Alignof` basic types
- `_Alignof` struct/union
- `_Static_assert` success
- `_Static_assert` failure
- `_Static_assert` non-constant rejection
- struct offsets
- union size/alignment
- array size is not alignment
- recursive struct failure
- recursive pointer success
- unsupported bitfield failure or real support
- flexible array behavior or explicit unsupported failure
- layout size overflow

### Oracle and fuzzing

Add tools/scripts if missing:

- host compiler differential oracle
- Clang record-layout dump normalizer
- random layout generator
- fuzz runner
- reducer for mismatches
- artifact capture

### CI gates

Add or adapt:

- `ci/no-placeholders.sh`
- `ci/layout-core-freeze.sh`
- `ci/fortify-layout.sh`
- `ci/fortify-layout-production.sh`
- policy-aware scanner
- policy validator
- scanner selftests
- attestation negative tests

### Evidence and attestation

Add:

- deterministic artifact hashing
- immutable subject manifest
- manifest schema validation
- evidence verifier
- signing hook
- signature verifier
- attestation bundle writer
- attestation bundle validator
- trust-root driven verifier
- verify policy schema validation

Production mode must require:

- non-empty `GITHUB_SHA`
- non-empty target
- signature required
- external verify policy
- allowed signer identity
- expected git commit
- expected target
- optional expected policy SHA

## Required files to create if absent

Create these only if they do not already exist or if equivalent functionality is absent:

```text
AGENTS.md
docs/zcc-layout-fortify.md
docs/fortify-threat-model.md
docs/fortify-rollout-checklist.md
.zcc-fortify-policy.json
fortify-verify-policy.example.json

ci/fortify-layout.sh
ci/fortify-layout-production.sh
ci/no-placeholders.sh
ci/layout-core-freeze.sh
ci/validate-fortify-policy.sh
ci/selftest-no-placeholders.sh
ci/selftest-attestation-negative.sh
ci/sign-fortify-manifest.sh
ci/verify-fortify-signature.sh

tools/check_no_placeholders.py
tools/check_layout_core_freeze.py
tools/validate_fortify_policy.py
tools/validate_verify_policy.py
tools/write_layout_manifest.py
tools/hash_fortify_artifacts.py
tools/verify_fortify_evidence.py
tools/write_attestation_bundle.py
tools/validate_attestation_bundle.py
tools/verify_attestation_bundle.py
tools/gen_layout_oracle.py
tools/gen_random_layout_oracle.py
tools/normalize_clang_layout.py
tools/reduce_layout_case.py

tests/layout/
```

## Canonical filenames

The policy file must be exactly:

```text
.zcc-fortify-policy.json
```

Do not create or reference:

```text
.ZCC-FORTIFY-POLICY.JSON
```

That uppercase spelling is invalid.

## Required commands

After implementation, run the closest equivalent of:

```bash
ci/fortify-layout.sh
```

If production signing keys are unavailable, do not fake signatures. Run development mode and explain production verification requirements.

If possible, also run:

```bash
python3 tools/validate_verify_policy.py --policy fortify-verify-policy.example.json
```

If the example policy contains placeholder public keys by design, either skip with explanation or create a test policy fixture.

## Required final report

Return a final report with:

1. Summary.
2. Files changed.
3. Compiler semantics implemented.
4. Diagnostics added.
5. Tests added.
6. CI gates added.
7. Evidence/attestation features added.
8. Commands run.
9. Command outputs or summarized results.
10. Remaining risks.
11. Manual steps for production signing.
12. Any deviations from this task and why.

## Hard fail conditions

The task is incomplete if:

- `_Alignof` can return size instead of alignment.
- `_Static_assert` can be erased silently.
- Layout arithmetic can overflow silently.
- Unknown target silently falls back.
- Placeholders remain in source files.
- Production signing can be skipped when `FORTIFY_REQUIRE_SIGNATURE=1`.
- Attestation verifier trusts the bundle without external policy in production.
- CI scripts refer to uppercase `.ZCC-FORTIFY-POLICY.JSON`.