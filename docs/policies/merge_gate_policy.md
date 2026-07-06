# Merge Gate Policy — Optimizer/Verifier Program

- **Effective date:** 2026-07-06
- **Applies to:** `main` (and release branches)
- **Scope:** IR verifier, optimization passes, compiler CI/perf infrastructure

---

## 1) Branch Protection Requirements (Mandatory)

## Protected branches
- `main`
- `release/*`

## Required before merge
1. **PR approval**
   - Minimum: 1 approving review (2 for optimizer core files)
2. **Conversation resolution**
   - All review threads resolved
3. **Status checks (required, non-optional)**
   - `IR Opt Quality Gate (Correctness + Perf) / correctness`
   - `IR Opt Quality Gate (Correctness + Perf) / perf`
4. **Up-to-date branch**
   - PR head must be up to date with base before merge
5. **No force pushes**
6. **No direct pushes to protected branches**

## Admin bypass
- Disabled by default
- Emergency use only via Exception Protocol (Section 6)

---

## 2) Required CI Gates (Definitions)

## Correctness gate (must pass)
Includes:
- verifier negative suite
- verifier positive suite
- instcombine normalized suite
- sccp normalized suite

Failure in any sub-suite => merge blocked.

## Performance gate (must pass)
Policy thresholds (default):
- compile geomean overhead <= **8.0%**
- runtime geomean delta >= **+3.0%**
- hard regressed benchmarks (delta < -2.0%) <= **2**
- significance considered (Mann-Whitney, alpha 0.05)

Failure => merge blocked unless approved exception.

---

## 3) Evidence Requirements in Every PR

PR must include:
1. Linked issue(s) and milestone
2. Test evidence snippets or CI links
3. Artifact links (correctness/perf when applicable)
4. Risk assessment (Low/Med/High)
5. Rollback note for behavior-changing changes

Recommended template:
- `.github/pull_request_template.md`

---

## 4) Change Classes and Review Strictness

## Class A (High impact)
Examples:
- verifier core logic
- instcombine/sccp/cfg rewrite semantics
- pass manager ordering
- SSA/PHI/CFG mutation utilities

Requirements:
- 2 approvals
- both required gates pass
- no open P0/P1 related issues

## Class B (Medium impact)
Examples:
- test fixtures/runners
- diagnostics formatting
- metrics schema additions

Requirements:
- 1 approval
- correctness gate pass
- perf gate pass if codegen/runtime impact possible

## Class C (Low impact)
Examples:
- docs only
- comments/formatting/no-op refactors

Requirements:
- 1 approval
- CI may be reduced only if truly non-executable paths changed

---

## 5) Red/Yellow/Green Merge Decision

## Green (merge allowed)
- all required checks pass
- approvals satisfied
- no unresolved threads

## Yellow (hold for owner signoff)
- checks pass but:
  - elevated risk noted
  - noisy perf indicators near thresholds
  - dependency uncertainty

Requires explicit signoff by Compiler Lead or Release Manager.

## Red (merge blocked)
- any required check fails
- missing required evidence
- open P0/P1 linked regression
- exception not approved

---

## 6) Exception Protocol (Emergency Only)

Allowed only for:
- production incident mitigation
- security fix
- CI infrastructure outage requiring urgent patch

Process:
1. Create issue titled: `MERGE EXCEPTION: <short reason>`
2. Include:
   - reason and urgency
   - failed gate(s)
   - risk analysis
   - rollback plan
   - owner + time-bounded follow-up
3. Require approvals from:
   - Release Manager (required)
   - one of: Compiler Lead / Security Lead
4. Merge with `exception-approved` label
5. Post-merge within 24h:
   - run full gates
   - file remediation PR if needed

No silent/admin-only bypasses.

---

## 7) Rollback Requirements

Any PR that changes optimizer behavior must have:
- explicit rollback target (commit/tag)
- rollback steps in PR description
- confirmation that rollback preserves buildability

Reference:
- `docs/releases/optimizer_rollback_playbook.md`

---

## 8) Baseline and Drift Policy (Perf)

- Perf comparisons must use pinned baseline artifact/tag
- Baseline update is explicit and reviewed
- No implicit moving-baseline comparisons on protected branches

---

## 9) Flake Management Policy

If perf flake rate > 5% over rolling 20 runs:
1. Mark gate as degraded in status report
2. Increase sample count / tune trimming
3. Keep gate required unless Release Manager approves temporary downgrade
4. Restore required state once stable

---

## 10) Ownership and Escalation

- **Compiler Lead:** semantic correctness authority
- **Perf Owner:** benchmark policy + threshold stewardship
- **Infra Owner:** CI reliability + artifact fidelity
- **Release Manager:** merge policy authority and exceptions

Escalation SLA:
- P0 merge-blocking incident acknowledged within 1 hour
- mitigation plan within 4 hours

---

## 11) Auditability

For every merge to protected branches, retain:
- PR link
- CI run links
- artifact references
- issue linkage
- exception record (if any)

Retention target:
- at least 90 days for artifacts/records

---

## 12) Policy Change Control

Changes to this policy require:
1. PR with rationale
2. approval by Compiler Lead + Release Manager
3. effective date update in this document
