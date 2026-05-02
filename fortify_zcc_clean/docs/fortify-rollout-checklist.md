# Fortify Rollout Checklist

## Phase 1: Compiler correctness

- [ ] Locate current type representation.
- [ ] Locate current parser handling of `sizeof`.
- [ ] Locate or add target ABI table.
- [ ] Add unified layout API.
- [ ] Route `sizeof` through layout API.
- [ ] Implement `_Alignof`.
- [ ] Implement `_Static_assert`.
- [ ] Add diagnostics.
- [ ] Add layout tests.

## Phase 2: ABI oracle

- [ ] Add golden layout tests.
- [ ] Add layout dump mode.
- [ ] Add host compiler oracle.
- [ ] Add Clang record-layout normalizer.
- [ ] Add random layout generator.
- [ ] Add fuzz runner.
- [ ] Add reducer.

## Phase 3: CI gates

- [ ] Add no-placeholder gate.
- [ ] Add layout-core-freeze gate.
- [ ] Add policy file.
- [ ] Add scanner selftests.
- [ ] Add attestation negative tests.

## Phase 4: Evidence

- [ ] Add artifact hashing.
- [ ] Add subject manifest.
- [ ] Add manifest verifier.
- [ ] Add attestation bundle.
- [ ] Add bundle verifier.

## Phase 5: Production trust

- [ ] Add signing hook.
- [ ] Add signature verification.
- [ ] Add external verify policy.
- [ ] Add production workflow.
- [ ] Require `GITHUB_SHA`.
- [ ] Require signer identity.