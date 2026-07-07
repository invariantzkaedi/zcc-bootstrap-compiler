# 🔱 ZCC Optimizer — Rollback Playbook

This playbook defines the quick-response rollback procedures and escalation paths if an optimizer release introduces correctness regressions, compiler hangs, or performance degradation.

---

## 1. Rollback Trigger Criteria

Execute the rollback protocol immediately if any of the following occur post-release:
1. **Correctness Regression**: Any function compiled by ZCC behaves incorrectly or fails verification tests.
2. **Compiler Hang/Crash**: The compiler hangs (100% CPU loops) or crashes (SIGSEGV/SIGFPE) when building standard or user workloads.
3. **Performance Breach**: Geomean compilation overhead exceeds the 2.0% threshold, or runtime performance degrades significantly on production paths.

---

## 2. Step-by-Step Rollback Procedure

### Step 2.1: Deactivate Flag Gating (Immediate mitigation)
If the regression is linked to loop unrolling or inlining, disable the corresponding feature flags:
* Remove `--enable-unroll-mvp`
* Remove `--enable-inline-mvp`
Confirm compilation success and correctness baseline.

### Step 2.2: Revert to Last Known-Good Commit
If flag deactivation is insufficient, revert the codebase:
1. Identify the last certified stable release tag or commit SHA.
2. Hard-reset the branch to the stable SHA:
   ```bash
   git reset --hard <stable_commit_sha>
   ```
3. Perform a clean self-host bootstrap run:
   ```bash
   make clean
   make selfhost
   ```
4. Verify byte-identity:
   ```bash
   cmp zcc2.s zcc3.s
   ```

### Step 2.3: Re-verify Correctness
Ensure the reverted environment is clean:
```bash
./zcc_test_suite.sh --quick
```

---

## 3. Incident Logging & Reporting Template

Record the event in the incident log with the following details:
```text
INCIDENT ID: [INC-YYYYMMDD-XX]
Date/Time (UTC): 
Rolled back from Commit: 
Rolled back to Commit: 
Triggering Symptom: 
Remediation Steps Taken: 
Owner Assigned: 
Follow-up Issue/Ticket: 
```

---

## 4. Escalation Contacts & Ownership Map

* **Escalation Path 1: Compiler Correctness/Semantics**  
  * *Role*: Compiler Lead (Accountable)  
  * *Responsibilities*: Verifier correctness, pass semantics validation, instruction folding bugs.

* **Escalation Path 2: CI/CD Pipeline & Build Artifacts**  
  * *Role*: Infra Engineer (Responsible)  
  * *Responsibilities*: Workflows, artifact collection, git history issues.

* **Escalation Path 3: Release Gating & Deployment**  
  * *Role*: Release Manager (Accountable)  
  * *Responsibilities*: Gating policy enforcement, release tagging, rollback execution.
