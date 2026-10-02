# ZKAEDI Agent Governance Protocol (EF-1 … EF-6)

Extracted from `README.md` § "Epistemic Fortification & Agent Protocol".
Enforced mechanically by `tools/agent_gate_warden.py`.

| Rule | Name | Mechanical check |
|---|---|---|
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
