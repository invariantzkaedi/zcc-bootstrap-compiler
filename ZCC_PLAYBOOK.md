# ⚡ ZCC SYSTEMS ENGINEERING & FORENSIC BUG-HUNTING PLAYBOOK

> **ZCC Protocol Compliance**: Tailored specifically for ZCC compiler internals, bootstrap chain integrity, low-level memory correctness, SQLite/runtime integration, and embedded C systems.

---

## 0) Core Mindset (Forensic-First & Zero-Drift)

- **Don’t prompt for “an answer.”** Prompt for a **forensic workflow**.
- **Don’t ask “what’s wrong?”** Ask “**under these protocol constraints, capture the raw artifact and prove the violated invariant**.”
- Treat AI as:
  1) **Strategist** (Phase 0 analysis & architectural trade-offs),  
  2) **Operator** (executes minimal coherent patches & runs gates),  
  3) **Auditor** (runs fault-injections, inspects `.s` codegen, and enforces zero spin).

---

## 1) ZCC Default Control Block (Copy/Paste for Tasks)

Use this at the top of serious compiler debugging or optimization requests:

```text
Role: ZCC Systems & Compiler Forensic Specialist
Goal: [e.g., Fix struct parameter stack-slot collision in part4.c / Verify float ICP]
Context: System V x86-64 ABI, WSL Ubuntu, 3-stage selfhost pipeline.
Constraints: 
  - Follow Phase 0 Read-Before-Touch protocol.
  - No edits before Phase 0 verdict with PROCEED: YES.
  - Max diff < 50 lines unless authorized. No edits directly to zcc.c (edit part*.c).
  - Gate claims require raw command output (cmp zcc2.s zcc3.s = 0).
Quality bar: Byte-identical self-host seal (H0 Convergence) + 100% GCC identity.
Process:
  1) Phase 0: Forensic log snapshot + baseline selfhost check.
  2) Probe before patch: Minimal repro showing symptom BEFORE fix.
  3) Apply minimal coherent patch to part*.c file.
  4) Run Gate 1 - 5 verification & fault-injection control.
  5) Produce mandatory commit body template with raw evidence.
Output:
  - Format: Raw command outputs + diffs + commit template
  - Include: Gate 1-5 outputs, residual risk, postmortem (if crash class)
```

---

## 2) The 7 High-Value ZCC Traits to Dial

When compiler quality or verification is off, explicitly set these:

- **Depth**: “Give principal-compiler-engineer depth; audit raw x86-64 assembly instructions.”
- **Precision**: “If AST node type or stack depth is uncertain, inspect definitions in `part1.c`-`part5.c` explicitly.”
- **Strictness**: “Enforce Phase 0 & EF-1 through EF-5 rules strictly; no PASS without raw outputs.”
- **Creativity**: “Propose 3 distinct structural frame-allocation mechanisms for sret temporaries.”
- **Skepticism**: “Assume the test pass might be a false positive; run fault-injection to prove gate sensitivity.”
- **Compression**: “Max signal, zero fluff, raw command log outputs only.”
- **Verifiability**: “Every claim must map to a line number in `part*.c` or exact diff line.”

---

## 3) ZCC Compiler Prompt Patterns

### A) Decision Memo Pattern (ABI & Optimizer Trade-offs)
```text
I need a ZCC decision memo.
Context: Handling 16-byte SSE struct return ABI in part4.c.
Options: Generate 3 (e.g., rbp-relative frame slots, rsp-relative subq, reg-spill buffers).
For each: ABI compliance, self-host stability, stack overhead, diff complexity, reversibility.
End with recommendation + why now.
```

### B) Architect + Forensic Auditor Pattern
```text
Design compiler patch A for floating-point ICP evaluation in part3.c.
Then switch roles to Forensic Auditor and attempt to break IEEE 754 precision or trigger SIGFPE.
Patch weaknesses and return final minimal patch.
```

### C) Spec-to-Execution Pattern (New Compiler Pass)
```text
Turn this optimization idea into:
1) One-page spec (AST / IR transformation rules)
2) Verification gates (InstCombine oracle + regression tests)
3) Invariants to preserve (caller-save registers, stack depth)
4) Definition of done (cmp zcc2.s zcc3.s = 0)
```

### D) Fast Iteration Pattern
```text
Give v1 minimal patch in 2 minutes.
Then ask me 3 sharp questions about edge cases (e.g., va_list, sret, float alignment).
Then produce v2 that is production-ready.
```

### E) “Don’t Let Me Fool Myself” (Fault-Injection Control)
```text
Assume my compiler fix is incomplete or decorative.
Find top 5 failure modes (e.g., stack pointer drift, register clobbering), 
write a deterministic fault-injection probe, and prove the gate fails when broken.
```

---

## 4) ZCC Coding Playbook (Compiler Engineering)

### For Bug Hunting & Triage
```text
Act as a ZCC debugging specialist.
Given this failing test / segfault / divergence:
1) Capture exact failing command + complete output + environment.
2) Inspect .i, .ir, and .s artifacts.
3) Check first: lifetime, bounds, ABI alignment, stack depth, register clobbering.
4) Minimal repro probe steps.
5) Smallest safe patch to part*.c (< 50 lines).
6) Run all 5 gates and paste raw output.
```

### For Pass Refactoring (`part3.c` / `part4.c`)
```text
Propose compiler refactor with:
- Before/after AST/IR architecture
- Invariants to preserve (System V ABI, stack 16-byte alignment)
- Migration plan across part*.c files
- Rollback plan
- Self-host assembly diff comparison (cmp zcc2.s zcc3.s)
Keep behavior 100% identical unless fixing a target defect.
```

### For Commit Review
```text
Review this git diff as a strict ZCC gatekeeper.
Check: ABI correctness, zero-drift determinism, stack depth hygiene, uninitialized memory leaks.
Return:
- Blockers
- Nits
- Mandatory Commit Body Template filled out with raw evidence
- Final verdict (PROCEED: YES | NO)
```

---

## 5) ZCC Learning & Knowledge Playbook

### “Teach + Test + Transfer” (Compiler Engineering)
```text
Teach me System V x86-64 ABI struct return passing (sret vs SSE registers) in 10 minutes.
Then quiz me with 5 sharp compiler-backend questions.
Then map this directly to ZCC's part4.c stack slot allocator.
```

### “Laddering” (Low-Level Systems)
```text
Explain x86-64 stack frame allocation at 3 levels:
- Beginner (call stack, push/pop)
- Practitioner (RBP frame pointers, RSP 16-byte alignment, red zone)
- Expert (System V AMD64 ABI aggregate classification, sret hidden pointers)
```

---

## 6) ZCC Personal Reusable Stack (Quick Commands)

### Command: `/phase0`
```text
Run Phase 0 protocol: git log trace, mandatory file reads, baseline selfhost check, emit machine-readable verdict.
```

### Command: `/probe`
```text
Write a minimal C probe showing the compiler defect BEFORE applying any fix.
```

### Command: `/patch`
```text
Apply the smallest coherent patch to part*.c. Diff must be under 50 lines unless explicitly authorized.
```

### Command: `/fault-inject`
```text
Re-inject the original bug into the codebase, verify that the test suite turns RED, then restore the fix to prove gate sensitivity.
```

### Command: `/gate1`
```text
Execute Gate 1 selfhost seal: cmp zcc2.s zcc3.s. Paste raw output and verify exit code 0.
```

### Command: `/commit-body`
```text
Format the final verified changes into the mandatory ZCC commit body template with all raw gate outputs preserved verbatim.
```

---

## 7) Anti-Patterns to Avoid in ZCC Development

- **Editing `zcc.c` directly**: Always edit the individual `part*.c` files (`zcc.c` is concatenated by `make zcc`).
- **Claiming PASS without raw command logs**: Deferred evidence is a phantom closure.
- **Masking symptoms**: Swallowing exceptions or adding dummy fallbacks instead of resolving root-cause invariants.
- **Narrative conclusions**: Every conclusion must map to a physical file or raw command exit code.
- **Unverified codegen**: Declaring a codegen change fixed without inspecting emitted `.s` assembly.

---

## 8) ZCC Starter Kit (Copy-to-Clipboard Copilot Prompt)

```text
Act as my ZCC Senior Compiler Copilot.
Objective: [Debug / Optimize / Refactor ZCC]
Constraints: WSL Ubuntu, System V x86-64 ABI, Phase 0 protocol, cmp zcc2.s zcc3.s = 0.
Workflow:
1) Run Phase 0 snapshot + emit PROCEED verdict.
2) Write minimal probe to reproduce symptom.
3) Apply minimal patch to part*.c (< 50 lines).
4) Execute Gate 1-5 + fault-injection control.
5) Deliver final response with:
   - Raw gate command outputs verbatim
   - Git diff --stat
   - Filled-out mandatory commit body template
Style: High signal, zero fluff, empirical evidence first.
```
