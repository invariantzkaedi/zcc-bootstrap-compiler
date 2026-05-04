# 🔱 ZKAEDI Bug Hunter — v1.1.0

**Supercharged Error Management Squad** runner.

Operationalizes a doctrine of invariant-based defect hunting:

> Every bug is an invariant violation until proven otherwise.
> Every error path is part of the product.
> Every recovery path must be deterministic, observable, safe.
> Every production push requires evidence — not vibes.

## What's in v1.1.0

- **78 domain invariants** across 8 categories (was 42 / 3):
  - 15 global · 30 codegen · 16 error_mgmt · 5 IR · 3 docs · 3 tests · 3 security · 3 build
- **50 tier invariants** in the **T0–T7 release-readiness ladder**
  (Must Not Ship → Must Improve the System) — a separate, orthogonal axis from domain categories.
- **13 production gates** (0–12) — added Doc Sync, Artifact Hygiene, Adversarial Counterexample, Rollback Plan.
- **25 agent roles** — added DocSynchronizer, BootstrapMarshal, IRVerifier, ArtifactJanitor, SecurityLens, OracleDesigner.
- **4 workflows**: `doc_drift_audit`, `codegen_runtime_bug`, `error_management_bug`, `selfhost_regression`.
- **Multi-invariant attribution** per finding (one finding can violate many invariants at once).
- **Finding statuses**: `suspected`, `confirmed`, `fixed_unverified`, `fixed_verified`, `not_a_bug`, `doc_drift`, `superseded`, `blocked`.
- **Explicit `ship_decision_policy`** drives the verdict (ship / hold / needs_evidence) instead of ad-hoc heuristics.
- **Severity model** (P0–P3), **evidence model** (strong / moderate / uncertain), and **test_matrix** all addressable from the CLI.

Stdlib Python. No third-party dependencies.

## Quickstart

```bash
# Run the example corpus end-to-end
python3 bug_hunter.py run corpus/ \
  --report findings.md --json findings.json \
  --workdir /tmp/zkaedi-bh

# List domain invariants
python3 bug_hunter.py invariants                          # all categories
python3 bug_hunter.py invariants --category ir            # one category
python3 bug_hunter.py invariants --search 'phi'           # substring across id/name/rule
python3 bug_hunter.py invariants --id CG-25               # one invariant (JSON)

# T0–T7 release-readiness ladder
python3 bug_hunter.py tiers                               # full ladder
python3 bug_hunter.py tiers --tier T0                     # one tier
python3 bug_hunter.py tiers --id T0-04                    # one tier invariant

# Release policy / workflows / test matrix / severity
python3 bug_hunter.py release-policy
python3 bug_hunter.py workflow                            # all four
python3 bug_hunter.py workflow --name doc_drift_audit
python3 bug_hunter.py test-matrix
python3 bug_hunter.py severity

# Production-push gate checklist (0–12)
python3 bug_hunter.py gates

# Blank Finding template (v1.1 schema)
python3 bug_hunter.py template

# Version
python3 bug_hunter.py --version
```

Exit code: `0` if every corpus entry SHIPS · `1` if anything HOLDS or NEEDS-EVIDENCE.

## Corpus entry shape

A corpus entry is a JSON file describing one minimized reproducer. The runner
decodes the base64 source, compiles it with **target** and **reference**
compilers using the same source, runs both, and compares stdout / stderr / exit.

### Required

- `name`, `category`
- `source_base64` — source encoded with `base64 -w0`
- `target_compile_argv` — list of strings; may use `{SRC}` and `{OUT}` templates
- `reference_compile_argv` — same templating
- `target_run_argv` — may use `{OUT}`
- `reference_run_argv` — may use `{OUT}`

### Recommended

- `violated_invariants` — list of invariant ids (v1.1 schema).
  v1.0 corpus entries with singular `violated_invariant` still work.
- `status` — one of the 8 finding statuses (auto-derived if omitted).
- `severity` — one of `P0`/`P1`/`P2`/`P3` (auto-derived if omitted).
- `evidence_strength` — `strong`/`moderate`/`uncertain` (auto-derived if omitted).

### Optional

- `expected_stdout`, `expected_stderr`, `expected_exit`
- `trigger`, `decode_command`
- `causal_chain`, `suggested_fix`, `regression_test`
- `counterexamples_tried` (Gate 11), `remaining_uncertainty`

### Minimal example

```json
{
  "name": "smoke_return_42",
  "category": "codegen",
  "violated_invariants": ["CG-15"],
  "severity": "P2",
  "trigger": "int main(void) { return 42; }",
  "source_base64": "aW50IG1haW4odm9pZCkgeyByZXR1cm4gNDI7IH0K",
  "target_compile_argv":    ["zcc", "{SRC}", "-o", "{OUT}"],
  "reference_compile_argv": ["cc",  "{SRC}", "-o", "{OUT}"],
  "target_run_argv":    ["{OUT}"],
  "reference_run_argv": ["{OUT}"],
  "expected_exit": 42
}
```

## How the verdict is decided

The runner applies `ship_decision_policy` from `invariants.json`:

| Situation                                                    | Verdict          |
|--------------------------------------------------------------|------------------|
| Both compilers OK, outputs match, exit codes match           | `ship`           |
| Compile-side asymmetry (one compiles, the other doesn't)     | `needs_evidence` |
| Both reject (illegal C or parallel diagnostic)               | `needs_evidence` |
| Runtime stdout or exit divergent                             | `hold`           |
| Target timed out                                             | `hold`           |
| P0/P1 severity without targeted validation                   | `needs_evidence` |

Each verdict comes with a `ship_rationale` list — the policy criteria that
drove the decision. No ad-hoc reasoning; the policy is the audit trail.

## File layout

```
zkaedi-bug-hunter/
├── SKILL.md                               # ecosystem skill manifest (v1.1.0)
├── README.md                              # this file
├── CHANGELOG.md                           # v1.0.0 → v1.1.0
├── bug_hunter.py                          # stdlib CLI (run/invariants/gates/tiers/...)
├── invariants.json                        # schema v1.1.0 catalog (the doctrine)
├── corpus/                                # reproducer JSON entries
│   ├── return_42.json                     # smoke (CG-15)
│   ├── sign_extend_char_arg.json          # CG-09
│   ├── stack_align_at_printf_with_double.json  # CG-03
│   ├── callee_saved_rbx_clobber.json      # CG-06 / CG-21 / CG-24 (v1.1)
│   └── signed_div_missing_cqo.json        # CG-19 (v1.1)
└── prompts/
    └── codegen_audit.md                   # one-shot agent dispatch prompt
```

## Backward compatibility

v1.0 corpus entries (singular `violated_invariant` key) work unchanged. The
runner promotes them to single-element lists internally. The v1.0 finding
report shape is a strict subset of the v1.1 finding report — additional
v1.1 fields default to empty.

## License & ownership

Internal ZKAEDI tooling. Pairs with `zkaedi-gate-discipline`,
`zkaedi-correctness-floor`, `error-learner`, `zkaedi-compiler-forge`, and
`zkaedi-zcc-ir-bridge`. See `SKILL.md` for ecosystem integration details.
