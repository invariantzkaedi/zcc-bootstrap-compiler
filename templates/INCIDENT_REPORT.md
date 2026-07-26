# 🚨 ZCC CI / Self-Host Incident Report (Template)

## Incident Overview
- **Incident ID:** INC-[YYYYMMDD]-[AREA]
- **Failing Gate:** [Gate 1 (Selfhost) | Gate 2 (Regressions) | Gate 3 (InstCombine) | Gate 4 (QEMU) | Gate 5 (CI)]
- **Commit SHA:** [ ]
- **Detected By:** [GitHub Actions / Local WSL Build]
- **Severity:** [P0 - Bootstrap Break | P1 - Codegen Regress | P2 - Flaky / Environment]

---

## Failure Signature (Raw Log Extract)
```text
[Paste exact failing stdout/stderr traceback here]
```

---

## Triage Checklist
- [ ] Is baseline clean on main? (`git checkout main && make selfhost`)
- [ ] Is failure reproducible locally in WSL?
- [ ] Which stage failed? [Stage 1 -> zcc1 | Stage 2 -> zcc2 | Stage 3 -> zcc3]
- [ ] Did `.s` assembly drift? (`cmp -l zcc2.s zcc3.s | head -20`)
- [ ] Is defect caused by missing dependency or environment mismatch?

---

## Root Cause Analysis
- **Violated Invariant:** [ ]
- **Divergent Hunk / File:** [part*.c:L###]
- **Why it failed:** [ ]

---

## Remediation & Recovery Plan
1. **Immediate Action:** [Revert commit | Apply hotfix patch]
2. **Patch Details:** [ ]
3. **Verification Command:**
```bash
[make selfhost / make boundary-gates]
```
4. **Postmortem Artifact Created:** [`docs/postmortems/INC-[ID].md`]

---

## Incident Sign-Off
- Remediation Verified By: [ ]
- Gate Status Post-Fix: ALL GREEN ✅
