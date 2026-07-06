# QEC Golden Update Policy

Version: 1.0.0  
Status: Enforced for PR review

---

## 1) Objective

Golden traces are correctness contracts, not convenience snapshots.  
Any update to golden files must be intentional, reviewed, and semantically explained.

---

## 2) Files in Scope

- `tests/golden/**/*.json`
- Any snapshot used by `tests/test_golden_traces.py`
- Semantic diff artifacts produced during golden checks

---

## 3) Allowed Reasons to Update Goldens

Valid:
1. Intended semantics change documented in `docs/qec_semantics.md`
2. Bug fix in propagation/decoder logic that changes correct output
3. Explicit convention migration (indexing/order/tolerance) approved by maintainers

Invalid:
- “Test was failing and re-recording fixed it”
- Non-deterministic drift
- Formatting-only changes without explanation

---

## 4) Required PR Conditions for Golden Changes

If a PR changes any golden trace file, it MUST include:

- `ALLOW_GOLDEN_UPDATE=1` (or equivalent workflow guard)
- semantic diff artifact (`artifacts/golden_semantic_diff.json`)
- PR section: **Golden Update Justification**
- link to related issue/bug
- reviewer acknowledgment from QEC owner

---

## 5) Reviewer Checklist (Golden Changes)

- [ ] Semantic diff reviewed (not just raw JSON diff)
- [ ] Change matches intended behavior in semantics contract
- [ ] Determinism gate passes after update
- [ ] No unrelated golden files changed
- [ ] Regression test/fixture added for root cause

---

## 6) Required PR Template Snippet

```markdown
### Golden Update Justification
- Why golden changed:
- Expected semantic delta:
- Evidence (semantic diff artifact):
- Linked issue:
- Determinism rerun evidence:
```

---

## 7) CI Enforcement Guidance

Recommended gating:
- Fail golden-change PR if `ALLOW_GOLDEN_UPDATE` not set
- Always upload semantic diff artifact on golden mismatch
- Block merge if semantic diff missing when golden files changed

---

## 8) Rollback Policy

If a golden update is found incorrect post-merge:
1. Revert golden commit immediately.
2. Open incident issue with signature and affected tests.
3. Add failing seed/minimized witness to permanent fixture set.

---

## 9) Audit Trail Requirements

For each approved golden update, retain:

- commit SHA
- approving reviewers
- semantic diff artifact
- justification text
- linked issue/incident ID

Store in `docs/evidence/YYYY-MM-DD/`.
