# 🔱 ZCC Optimizer — Ownership Matrix & Status Reporting

This document defines the team ownership boundaries, RACI assignments, and status reporting guidelines for the ZCC optimizer codebase.

---

## 1. RACI Ownership Model

RACI classifications align with core architectural domains:
* **Responsible (R)**: Performs the work to implement features, passes, or tests.
* **Accountable (A)**: Has final sign-off authority and answerability for correctness and release stability.
* **Consulted (C)**: Provides input, design feedback, or code review.
* **Informed (I)**: Notified of status updates, milestones, or incidents.

| Core Architectural Domain | Compiler Lead | Opt Engineer | Infra Engineer | Perf Engineer | Release Manager | QA Lead |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **IR Verifier Correctness** | **A** | **R** | **I** | **I** | **I** | **C** |
| **InstCombine / SCCP Passes** | **A** | **R** | **I** | **C** | **I** | **C** |
| **CFG Simplify & Liveness** | **A** | **R** | **I** | **I** | **I** | **C** |
| **Loop Unrolling / Inlining** | **A** | **R** | **I** | **C** | **I** | **C** |
| **CI Workflows & Artifacts** | **C** | **I** | **A** / **R** | **C** | **I** | **I** |
| **Benchmark Harness & PGO** | **C** | **C** | **R** | **A** / **R** | **I** | **I** |
| **Gating Policy & Tagging** | **C** | **I** | **I** | **C** | **A** | **I** |

---

## 2. Weekly Status Update Procedure

Team updates must use the following standard markdown template to communicate weekly progress, metrics, and risks:

```markdown
# Weekly Optimization Status — [YYYY-MM-DD]

## Team Updates
* **Owner/Reporter**: [Name]
* **Milestone Focus**: [M1 / M2 / M3 / M4]

## 1. Completed Tasks
* [Pass / Issue ID] - Fleshed out...
* [Pass / Issue ID] - Added...

## 2. In-Flight Tasks
* [Pass / Issue ID] - Implementing...

## 3. Active Blockers
* **Blocker**: 
  * *Impact*: 
  * *Owner*: 
  * *ETA / Workaround*: 

## 4. Correctness Status
* Verifier Negative Suite: [PASS / FAIL]
* Verifier Positive Suite: [PASS / FAIL]
* InstCombine Normalized: [PASS / FAIL]
* SCCP Normalized: [PASS / FAIL]
* CI Required Status Checks: [PASS / FAIL]

## 5. Performance Metrics
* Compile-Time Geomean Overhead: [__ %]
* Runtime Execution Geomean Delta: [__ %]
* Regressed Benchmarks List: [None / Benches]
* Flake Rate: [__ %]

## 6. Risks & Mitigations
* **Risk**: [Description]
  * *Mitigation Plan*: [Description]
```
