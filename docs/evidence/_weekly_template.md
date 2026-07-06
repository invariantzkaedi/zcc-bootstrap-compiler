# QEC Weekly Stabilization Report

Week of: `<YYYY-MM-DD>`  
Baseline tag: `qec-verification-baseline-v1`  
Prepared by: `<owner>`

## 1) CI Health Summary
- Total CI runs:
- Required lane failures:
- Retries/re-runs:
- Overall required-lane pass rate (%):

### Required checks
- `qec-fast-required`: ✅/❌ (pass/total)
- `qec-determinism-required`: ✅/❌ (pass/total)
- `preflight-fast-required`: ✅/❌ (pass/total)
- `preflight-determinism-required`: ✅/❌ (pass/total)

### Monitored checks
- `qec-mutation-required`: ✅/❌ (pass/total)
- `preflight-mutation-required`: ✅/❌ (pass/total)

---

## 2) Determinism and Drift
- Determinism drift incidents:
- Drift signatures observed:
- Reproducible? (Y/N):
- Root cause status:

---

## 3) Mutation Metrics
- Smoke kill-rate (min/avg/max):
- Nights below 0.90 threshold:
- Survivor mutants opened:
- Survivor mutants closed:
- Oldest open survivor age (days):

---

## 4) Failure Forensics
- Total failure artifacts:
- Unique signatures:
- New signatures this week:
- Top 5 signatures:
  1.
  2.
  3.
  4.
  5.

- Avg minimization ratio (minimized/original):
- % artifacts schema-valid:

---

## 5) Golden Governance
- PRs touching `tests/golden/**/*.json`:
- With `ALLOW_GOLDEN_UPDATE=1`:
- With semantic diff artifact:
- Policy violations:

---

## 6) Performance Snapshot
- Fast suite runtime trend:
- Determinism lane runtime trend:
- Regression flags (>15%):
- Notes:

---

## 7) Incidents and Actions
- Incident issues opened:
- Incident issues resolved:
- Mean time to classify:
- Mean time to fix:

Key actions taken:
- 
- 
- 

---

## 8) Promotion Gate Status (Mutation -> Required)
- [ ] 14 consecutive days stable
- [ ] No flaky mutation behavior
- [ ] Kill-rate >= 0.90 consistently
- [ ] No survivor issue older than 7 days
- [ ] At least one survivor converted to permanent regression test

Current recommendation:
- [ ] Keep monitored
- [ ] Promote to required

---

## 9) Next Week Plan
1.
2.
3.
