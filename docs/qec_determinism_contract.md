# QEC Determinism Contract

Version: 1.0.0

## Contract

Given fixed:
- source IR/circuit
- noise mode (deterministic/noisy)
- seed
- compiler/config version

the following MUST be byte-identical:
- propagated frame trace
- syndrome trace
- correction decisions
- serialized trace artifact JSON

## Rules

1. Seed source
   - Use `QEC_SEED` env var if provided.
   - Otherwise use fixed default (`1337`) in deterministic CI paths.
2. No hidden entropy
   - Do not call RNG in deterministic mode unless seeded from contract.
3. Stable ordering
   - Deterministic sort for map/dict outputs.
   - deterministic tie-breaks documented in semantics.
4. Thread consistency
   - Parallel paths must preserve deterministic reduction order or use single-thread in CI.
5. Artifact schema stability
   - JSON keys sorted.
   - Canonical formatting for floats/ints/booleans.
6. Repro command required
   - Every failure artifact must include exact rerun command.

## Acceptance test

Run same test 10x:
- hash(trace.json) must be identical for all 10 runs.
