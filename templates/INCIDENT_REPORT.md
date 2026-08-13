# 🚨 ZCC CI / Self-Host Incident Report

## Failure Signature & Overview
- **Incident ID:** [ ]
- **Date/Time (UTC):** [ ]
- **Reporter:** [ ]
- **Branch/Commit:** [ ]
- **Environment:** [WSL Ubuntu version, compiler/toolchain, CPU arch]
- **Severity:** [S0 | S1 | S2 | S3]
- **Status:** [Open | Mitigated | Resolved]

---

## 2) Executive Summary
- **What failed:** [ ]
- **Where detected:** [local gate | CI gate | selfhost stage]
- **User/System impact:** [ ]
- **Current state:** [failing/passing, blocked/unblocked]

---

## 3) Trigger & Detection
- **Trigger event:** [commit/test/run that introduced or exposed issue]
- **First failing signal:** [test name, cmp mismatch, segfault, wrong codegen, etc.]
- **Detection command:**
```bash
[ ]
```
- **Raw failing output:**
```text
[ ]
```
- **Exit code:** [ ]

---

## 4) Scope & Blast Radius
- **Affected component(s):** [part1.c/part2.c/... runtime/sqlite integration/etc.]
- **Defect class:** [ABI | stack | clobber | FP precision | parser | IR | codegen | runtime]
- **Potentially affected tests/features:** [ ]
- **Known unaffected areas:** [ ]

---

## 5) Reproduction (Deterministic)
- **Minimal repro source/input:** [ ]
- **Exact repro steps:**
```bash
[step 1]
[step 2]
[step 3]
```
- **Observed result:** [ ]
- **Expected result:** [ ]
- **Deterministic?** [Yes/No]

---

## 6) Root Cause Analysis
- **Violated invariant:** [ ]
- **Root cause location (file:line/function):** [ ]
- **Why it happened:** [ ]
- **Why existing gates didn’t prevent earlier:** [ ]

---

## 7) Mitigation / Fix
- **Immediate mitigation:** [rollback/revert/guard/disable path]
- **Final fix summary:** [ ]
- **Files changed:** [ ]
- **Diff size:** [ ] lines
- **Risk of fix:** [Low | Medium | High]

---

## 8) Verification Evidence (Raw)
### Gate 1 (Selfhost Convergence)
```bash
[ ]
```
```text
[ ]
```
Exit: [ ]

### Gate 2
```bash
[ ]
```
```text
[ ]
```
Exit: [ ]

### Gate 3
```bash
[ ]
```
```text
[ ]
```
Exit: [ ]

### Gate 4
```bash
[ ]
```
```text
[ ]
```
Exit: [ ]

### Gate 5
```bash
[ ]
```
```text
[ ]
```
Exit: [ ]

### Fault-Injection Sensitivity Proof
- **Injected break:** [ ]
- **fault_inject_exit:** [{{FAULT_INJECT_EXIT}}]
- **fault_restore_exit:** [{{FAULT_RESTORE_EXIT}}]
- **fault_injection_verdict:** [{{FAULT_INJECTION_VERDICT}}]
- **RED evidence:**
```text
[ ]
```
- **Restore + GREEN evidence:**
```text
[ ]
```

---

## 9) Assembly Delta Summary (if codegen touched)
- **Changed functions in `.s`:** [ ]
- **Stack frame changes:** [ ]
- **Register/clobber changes:** [ ]
- **ABI-visible changes:** [None | Describe]

---

## 10) Confidence, Residual Risk, and Follow-ups
- **Root-cause confidence:** [High | Medium | Low]
- **Fix sufficiency confidence:** [High | Medium | Low]
- **Residual risk class:** [R0 | R1 | R2]
- **Follow-up actions (owner + due date):**
  1. [ ] [Owner] — [Date]
  2. [ ] [Owner] — [Date]
  3. [ ] [Owner] — [Date]

---

## 11) Final Verdict
- **PROCEED:** [YES | NO]
- **Reason:** [ ]
