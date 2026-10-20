# ZKAEDI Agent Governance Protocol (EF-0 … EF-6)

Extracted from `README.md` § "Epistemic Fortification & Agent Protocol".
Enforced mechanically by `tools/agent_gate_warden.py`.

## ZCC-EF-0 — Independent Verification Authority

**Classification:** Mandatory engineering invariant\
**Priority:** Release-blocking\
**Applies to:** All ZCC agents, compilers, build scripts, bootstrap gates, test harnesses, provenance ledgers, dashboards, CI pipelines, and verification tools.

### The absolute rule

**NO COMPONENT MAY CERTIFY ITS OWN CORRECTNESS USING EVIDENCE OR VERDICTS THAT IT CAN FABRICATE, MODIFY, REPLACE, OR CONTROL WITHOUT INDEPENDENT VERIFICATION.**

The system under test may produce evidence. It must never be the sole authority deciding whether that evidence proves success.

### Mandatory enforcement requirements

1. **Separate evidence production from verification authority.** A trusted verifier must operate outside the monitored process's write authority. A child process must not control its authoritative verdict, receipt, or signing credentials.
2. **Never trust success messages.** Printed markers such as `PASS`, `VERIFIED`, `BYTE-IDENTICAL`, or `BOOTSTRAP_GREEN` are telemetry, not proof. Exit code `0` alone is also insufficient to establish correctness.
3. **Verify actual artifacts and execution.** A trusted runner must invoke the approved executable, independently capture its termination status, inspect its generated outputs, validate required invariants, and produce the final verdict. Command-name matching, timestamps, hashes, and child-written ledgers alone cannot establish authentic execution.
4. **Fail closed by default.** Missing evidence, skipped tests, unexpected exit codes, timeouts, malformed receipts, interrupted runs, stale artifacts, inconsistent hashes, and verifier exceptions must produce a nonzero gate result. Unknown is never PASS.
5. **Prove the verifier can detect failure.** Every critical gate must include adversarial negative controls, at least one valid positive control, and mutation tests. A verifier that passes when its checks are disabled, bypassed, or replaced is defective.
6. **Preserve independent evidence lineage.** Every release verdict must link the source revision, exact command, environment, toolchain, inputs, execution record, artifact hashes, test results, and verifier version. The authoritative receipt must be created and protected by the trusted verification process.
7. **Treat dashboards as observers, never authorities.** ZCC Quest and other interfaces may display a trusted verification result but must not invent certification from progress, stdout, file existence, or inferred success.
8. **Require independent review of the verification infrastructure.** No verification system may achieve release approval solely through its own tests or self-reported results.

### Mandatory release decision

**PASS** is permitted only when the trusted verification system has independently executed the required checks and validated the complete evidence.

**FAIL** is required when any mandatory invariant is violated.

**UNVERIFIED** is required when sufficient independent evidence is unavailable.

Neither FAIL nor UNVERIFIED may be represented as success.

### Violation policy

Any false-positive certification, vacuous test pass, fabricated provenance acceptance, silent skip, or fail-open condition is a **release-blocking verification defect**.

The affected gate must be repaired, regression-tested, mutation-tested, and independently revalidated before release approval.

**Foundational invariant: Evidence producers do not grant themselves verification authority.**

### Permanent Operating Instruction for Every Agent

> Before reporting PASS, attempt to make the verifier report PASS without the required work actually succeeding. If the attempt works, the verifier is defective and certification is prohibited.

---

| Rule | Name | Mechanical check |
|---|---|---|
| **EF-0** | Independent Verification Authority | No component may certify its own correctness using evidence or verdicts that it can fabricate, modify, replace, or control. |
| **EF-1** | Same-Turn Evidence | Every `PASS` claim must be preceded, in the same block, by a fenced command invocation and its raw output. |
| **EF-2** | Artifact ≠ Exit Code | Any claim about codegen output must cite an artifact inspection (`grep`/`objdump`/`readelf`/`sha256sum`/`cmp`/`xxd`/`hexdump`/`nm`/`wc` on a `.s`, `.o`, `.exe`, `.wasm`, `.bin`, `.elf`), not only `exit=0`. |
| **EF-3** | Probe Before Patch | The first edit to a protected path (`part*.c`, `ir_*.c`, `regalloc.c`, `evm_lifter.c`, `zld.c`) must be preceded by a reproduction probe (a compile/run command whose output shows the symptom). |
| **EF-4** | Full Gate Set | If a gate set is declared (`GATES: a, b, c`), every member must have a verdict before any single `PASS` is reported. |
| **EF-5** | Scope Audit | A `git diff --stat` block must be present, and every file in it must be inside the declared `SCOPE:` list. |
| **EF-6** | Four-State Verdicts | Verdict tokens are strictly `PASS`, `FAIL`, `INCONCLUSIVE`, `NOT-APPLICABLE`. Any other terminal word (`done`, `looks good`, `should work`, `✅`) is a violation. |

## Transcript conventions the warden understands

```
GATES: selfhost, abi-interop, meta-canary
SCOPE: part4.c tests/selfhost/meta_canary.c

$ ./zcc probe.c -o probe.s && gcc probe.s -o probe && ./probe
<raw output>

GATE selfhost: PASS
GATE abi-interop: PASS
GATE meta-canary: NOT-APPLICABLE
```

## Exit codes

| Code | Meaning |
|---|---|
| 0 | all rules PASS or NOT-APPLICABLE |
| 10 | at least one rule FAIL |
| 11 | at least one rule INCONCLUSIVE and none FAIL |
| 2 | input/usage error |
