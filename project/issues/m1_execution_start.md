# M1 Execution Start — Correctness Foundation

## Objective
Execute Milestone M1 to deliver production-valid correctness foundation for IR optimization rollout:
- Verifier core
- InstCombine MVP
- SCCP MVP
- CFG simplify
- Required correctness CI gate

## Scope (M1)
1. Verifier core implementation
2. Stable verifier diagnostics/error IDs
3. InstCombine 15-rule MVP
4. SCCP MVP + constant branch pruning + PHI executable-edge logic
5. CFG simplify cleanup (unreachable + PHI repair)
6. Pass CLI/pipeline wiring
7. CI correctness gating and artifact triage

## Exit Criteria (Must All Pass)
- [ ] `tests/verify` negative suite green
- [ ] `tests/verify-positive` suite green
- [ ] `tests/opt/instcombine` normalized suite green
- [ ] `tests/opt/sccp` normalized suite green
- [ ] `.github/workflows/ir-opt-quality-gate.yml` required + green
- [ ] Verifier diagnostics stable and actionable
- [ ] No open P0 correctness regressions

## Deliverables
- Functional verifier implementation and diagnostics
- Working InstCombine + SCCP + cfg_simplify passes (MVP scope)
- Updated docs/changelog for M1 behavior
- CI artifacts sufficient for one-pass triage

## Dependencies
- Existing test fixtures and runners committed
- Project labels/milestones/automation configured

## Risks
- Parser/fixture mismatch on edge-case invalid IR tests
- SCCP over-pruning if executable-edge logic is incorrect
- Def-use corruption during rewrite sequences

## Mitigations
- Run verifier pre/post mutating passes in debug mode
- Gate all transforms with post-pass verifier checks in CI
- Keep MVP conservative; defer aggressive folds to M3

## Ownership
- Compiler Lead: accountable
- Optimizer Engineer: responsible (InstCombine/SCCP/CFG)
- Infra Engineer: responsible (CI/workflows/artifacts)
- QA: consulted (fixture fidelity)

## Target Window
- Start: Immediately
- Planned completion: End of Week 1
