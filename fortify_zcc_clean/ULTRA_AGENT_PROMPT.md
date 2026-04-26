# Ultra Task: Make ZCC Layout Fortify Real

You are implementing a production-grade compiler correctness and fortify attestation program in this repository.

## Highest priority instruction

Inspect before editing. Do not create disconnected architecture. Every implementation must integrate with actual repo files, actual build system, actual tests, and actual compiler paths.

## Forbidden

- Placeholder text.
- TODO/FIXME/STUB comments.
- “omitted for brevity”.
- Pseudocode.
- Uppercase `.ZCC-FORTIFY-POLICY.JSON`.
- `_Static_assert` macro erasure.
- `_Alignof` using `sizeof`.
- Silent ABI fallback.
- Unchecked layout arithmetic.
- Unsigned production evidence.
- Trusting the attestation bundle without external verify policy.

## Required phases

### Phase 0: Discovery report

Before edits, produce:

- parser map
- type system map
- layout/codegen map
- diagnostics map
- const-eval map
- target ABI map
- tests/CI map
- exact integration plan

### Phase 1: Compiler semantics

Implement:

- unified layout engine
- real `_Alignof`
- real `_Static_assert`
- stable diagnostics
- checked arithmetic
- fail-closed unsupported features
- target ABI fatal unknown target

### Phase 2: Tests

Add:

- golden ABI tests
- negative static assert tests
- non-constant static assert tests
- recursive aggregate tests
- struct/union/array tests
- overflow tests
- unsupported feature tests

### Phase 3: Oracles

Add:

- host compiler differential oracle
- Clang record layout normalizer
- random layout generator
- fuzz runner
- reducer
- artifact capture

### Phase 4: CI gates

Add:

- no-placeholder scanner
- layout-core-freeze scanner
- policy validator
- verify-policy validator
- scanner selftests
- negative attestation selftests

### Phase 5: Evidence and attestation

Add:

- deterministic artifact hashing
- immutable subject manifest
- manifest validator
- evidence verifier
- signing hook
- signature verifier
- attestation bundle
- bundle validator
- trust-root driven bundle verifier
- production workflow requiring signature and deployment pins

## Required development files

Add the Fortify Ops Pack if absent:

- `AGENTS.md`
- `AGENT_DISCOVERY_TEMPLATE.md`
- `AGENT_REVIEW_CHECKLIST.md`
- `FORTIFY_QUICKSTART.md`
- `FORTIFY_RUNBOOK.md`
- `FORTIFY_FAILURE_PLAYBOOK.md`
- `SECURITY.md`
- `.zcc-fortify-policy.json`
- `fortify-verify-policy.example.json`
- `ci/*fortify*.sh`
- `tools/*fortify*.py`
- layout tests
- GitHub workflow/templates where appropriate

## Required validation

Run:

```bash
ci/preflight-fortify.sh
ci/fortify-layout.sh
```

If production signing is configured, also run:

```bash
ci/verify-production-env.sh
ci/fortify-layout-production.sh
```

If a command cannot run, explain:

- why
- what dependency is missing
- how to run it later
- whether this blocks merge

## Required final answer

Return:

1. Discovery summary.
2. Implementation summary.
3. File list.
4. Diagnostics list.
5. Test list.
6. CI/evidence list.
7. Commands run.
8. Results.
9. Remaining risks.
10. Production signing setup instructions.
11. Reviewer checklist.

## Completion criteria

The task is complete only when:

- compiler semantics are real
- tests exist
- development fortify path exists
- production fortify path exists
- evidence bundle can be verified offline
- no placeholders remain