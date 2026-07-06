# Week 1 Sprint Checklist — M1 Correctness Foundation

## Day 1 — Verifier CFG/Terminators
- [ ] Implement `verify_terminators`
- [ ] Implement `verify_cfg` core edge checks
- [ ] Ensure deterministic error emission
- [ ] Run negative tests 01–03 and fix failures
- [ ] Commit: verifier term/cfg baseline

## Day 2 — Verifier SSA/PHI
- [ ] Implement `verify_ssa` (undef + multi-def)
- [ ] Implement `verify_phi_wellformed`
- [ ] Add/confirm stable error IDs/substrings
- [ ] Full `tests/verify` and `tests/verify-positive` green
- [ ] Commit: verifier complete MVP

## Day 3 — InstCombine MVP
- [ ] Implement 15 rules in dispatcher order
- [ ] Ensure rewrite safety + def-use consistency
- [ ] Run `tests/opt/instcombine` normalized suite
- [ ] Fix any canonicalization mismatches
- [ ] Commit: instcombine mvp

## Day 4 — SCCP MVP
- [ ] Implement lattice states + transfer/meet
- [ ] Implement executable-edge worklist
- [ ] Implement rewrite/materialization stage
- [ ] Run SCCP tests 01–05
- [ ] Commit: sccp core

## Day 5 — CFG simplify + CI close
- [ ] Implement unreachable removal
- [ ] Implement PHI incoming cleanup
- [ ] Run full SCCP suite
- [ ] Verify end-to-end correctness workflow green
- [ ] Validate artifact upload completeness
- [ ] Commit: m1 correctness gate complete

---

## Daily Commands
- [ ] `make -C tests/verify test-negative`
- [ ] `make -C tests/verify-positive test-positive`
- [ ] `make -C tests/opt/instcombine test-normalized`
- [ ] `make -C tests/opt/sccp test-normalized`

## Definition of Done (Week 1)
- [ ] All four suites pass locally
- [ ] PR CI passes with required checks
- [ ] No flaky failures across 3 reruns
- [ ] M1 kickoff issue checklist updated with evidence links
