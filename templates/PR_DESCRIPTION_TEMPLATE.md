# ZCC Pull Request Description Generator

## Summary
- **Scope/Area:** `<scope>(<area>)`
- **Goal:** `<one sentence intended result>`
- **Outcome:** `<one sentence verified on-disk truth>`

---

## Technical Details & Root Cause
- **Violated Invariant:** `<specific invariant violated>`
- **Root Cause Location:** `file:function:line`
- **Mechanics:** `<where/why violation occurred>`

---

## Patch & Scope Audit
- **Files Modified:** `<file_list>`
- **Diff Stat:** `git diff --stat` output

```text
<paste git diff --stat output>
```

---

## Verification Evidence & Gate Matrix

### Gate 1 — Selfhost Identity
```bash
cmp zcc2.s zcc3.s
```
- **Status:** `PASS`
- **Output:** `byte-identical output`

### Gate 2 — Cross-Toolchain Interoperability
- **Status:** `PASS / N/A`
- **Details:** `<gcc-main + zcc-lib / zcc-main + gcc-lib test output>`

### Gate 3 — 797-Function Corpus Diff
- **Status:** `PASS / N/A`
- **Details:** `<0 unapproved deltas>`

### Gate 4 — Target Harness
- **Status:** `PASS / N/A`
- **Details:** `<SQLite / Lua / curl harness output>`

### Gate 5 — Evidence Freshness
- **Status:** `PASS`
- **Details:** `<Re-verified clean on latest HEAD>`

---

## Fault-Injection Proof
- Enabled: [{{FAULT_INJECTION_ENABLED}}]
- Verdict: [{{FAULT_INJECTION_VERDICT}}]
- Inject Exit: [{{FAULT_INJECT_EXIT}}]
- Restore Exit: [{{FAULT_RESTORE_EXIT}}]
- **Injected Break:** `<brief description of deliberate mutation>`
- **Failing Signal (RED):** `<failing command + output excerpt>`
- **Restored Signal (GREEN):** `<clean passing output post-revert>`

---

## Postmortem Scorecard & Risk Assessment
- **Postmortem Score:** `[ /20]` (Grade: Release-grade forensic closure)
- **Residual Risk:** `None` / `R0 (Bounded)`
