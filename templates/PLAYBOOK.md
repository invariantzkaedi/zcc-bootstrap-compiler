# ⚡ ZCC Systems Engineering & Forensic Bug-Hunting Playbook (Template)

## 0) Mission
- **Objective:** [ ]
- **Scope:** [ ]
- **Out of Scope / Non-Goals:**  
  - [ ]  
  - [ ]  
  - [ ]

---

## 1) Control Block (Paste into each task)

```text
Role: ZCC Systems & Compiler Forensic Specialist
Goal: [ ]
Context: [System V x86-64 ABI / WSL Ubuntu / 3-stage selfhost / etc.]
Constraints:
  - Phase 0 before any edits
  - No edits before PROCEED: YES
  - Max diff < [ ] lines unless authorized
  - Edit [part*.c], not [zcc.c]
  - Claims require raw command output
Quality Bar:
  - [ ] Byte-identical self-host seal
  - [ ] GCC identity / behavioral parity
Process:
  1) Phase 0 snapshot + verdict
  2) Minimal repro BEFORE patch
  3) Minimal coherent patch
  4) Gate 1..N verification
  5) Fault-injection control
Output:
  - Raw outputs
  - Diffs
  - Commit body template
```

---

## 2) Stop-Work Triggers (Hard Halt)
If any condition below is true, stop patching and resolve uncertainty first:
- [ ] Repro is non-deterministic
- [ ] ABI classification uncertain
- [ ] Selfhost seal failed (`cmp ... != 0`)
- [ ] Test signal inconsistent with expected fault model
- [ ] Missing artifact/log for a claimed result

**Recovery action required before continuing:** [ ]

---

## 3) Confidence & Risk Labels (Mandatory)
- **Root-cause confidence:** [High | Medium | Low]
- **Fix sufficiency confidence:** [High | Medium | Low]
- **Residual risk class:** [R0 | R1 | R2]
- **Reasoning (brief):** [ ]

---

## 4) Standard Workflow
1. **Phase 0**
   - Environment snapshot: [ ]
   - Baseline commands + outputs: [ ]
   - Verdict: `PROCEED: [YES|NO]`

2. **Probe Before Patch**
   - Minimal repro source: [ ]
   - Repro command: [ ]
   - Observed failure output: [ ]

3. **Patch**
   - File(s): [ ]
   - Rationale: [ ]
   - Diff size: [ ] lines

4. **Verification Gates**
   - Gate 1: [ ]
   - Gate 2: [ ]
   - Gate 3: [ ]
   - Gate 4: [ ]
   - Gate 5: [ ]

5. **Fault Injection**
   - Injection method: [ ]
   - Expected RED signal: [ ]
   - Actual RED signal: [ ]
   - Restore + GREEN recheck: [ ]

---

## 5) Assembly Delta Summary (Post-patch)
- Changed function(s): [ ]
- Stack pointer deltas (`sub/add rsp`): [ ]
- Saved/restored register changes: [ ]
- Call-site ABI impacts: [ ]
- Struct return / arg passing impacts: [ ]
- Notes: [ ]

---

## 6) Regression Taxonomy
For each added/updated test:
- Test name: [ ]
- Class: [ABI | stack | clobber | FP precision | parser | IR | codegen | runtime | other]
- Invariant protected: [ ]
- Failure mode prevented: [ ]
- Determinism notes: [ ]

---

## 7) Anti-Patterns Checklist
- [ ] No direct `zcc.c` edits
- [ ] No PASS claims without raw outputs
- [ ] No symptom masking
- [ ] No narrative-only conclusions
- [ ] No uninspected codegen changes
