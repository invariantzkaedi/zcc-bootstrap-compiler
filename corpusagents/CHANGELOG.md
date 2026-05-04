# Changelog

## v1.1.0 — 2026-05-04

**Additive merge.** Schema bumped to `1.1.0`. All v1.0 corpus entries continue
to work unchanged via backward-compatible normalization of the singular
`violated_invariant` key.

### Added — categories (5 new)

- `ir`        — SSA, CFG, PHI, liveness, optimizer correctness, IR/AST lowering
- `docs`      — documentation/source synchronization, stale claims
- `tests`     — regression quality, flake resistance, oracle strength
- `security`  — memory safety, trust boundaries, hostile input, supply-chain
- `build`     — self-hosting, bootstrap, generated artifacts, reproducibility

Total: 3 → **8** categories.

### Added — domain invariants (36 new, 78 total)

- **G-11 … G-15** (5)  — Documentation Synchronization, Source of Truth,
  Artifact Integrity, Assumption Disclosure, No Phantom Certainty.
- **CG-21 … CG-30** (10) — IR/AST Boundary, Bootstrap Identity,
  Backend Abstraction, Spill Slot Isolation, PHI Parallel Copy,
  Liveness Soundness, Optimizer Safety, Aggregate ABI,
  Generated Assembly Validity, Call Boundary Preservation.
- **IR-01 … IR-05** (5)  — CFG Integrity, PHI Completeness, Dominance,
  Reachability Refresh, Lowering Completeness.
- **EM-13 … EM-16** (4)  — Exit Status Truth, Atomic Output, Panic Context,
  Recovery Boundary.
- **DOC-01 … DOC-03** (3) — Known-Bug Ledger Accuracy, Line Number Freshness,
  Evidence Attachment.
- **TEST-01 … TEST-03** (3) — Oracle Strength, Flake Resistance, Negative Coverage.
- **SEC-01 … SEC-03** (3)  — Memory Safety, Hostile Source Input, Toolchain Trust Boundary.
- **BUILD-01 … BUILD-03** (3) — Clean Build Reproducibility, Selfhost Gate, Source Mirror Sync.

Total: 42 → **78** domain invariants.

### Added — T0–T7 release-readiness ladder (50 tier invariants)

A separate, orthogonal axis from domain categories. Each tier defines a set
of invariants that must hold before a release at that level of stakes:

| Tier  | Name                          | Invariants |
|-------|-------------------------------|-----------:|
| T0/P0 | Must Not Ship                 | 6 |
| T1/P1 | Must Understand               | 5 |
| T2/P2 | Must Patch Safely             | 6 |
| T3/P3 | Must Prove and Preserve       | 6 |
| T4/P4 | Must Operate Well             | 6 |
| T5/P5 | Must Scale Under Stress       | 7 |
| T6/P6 | Must Resist Adversaries       | 7 |
| T7/P7 | Must Improve the System       | 7 |

Tier invariants reference domain invariants via `mapped_invariants`
(e.g. T0-04 Artifact Integrity → G-13 + EM-06 + EM-14).

Total finding population (domain + tier): **128**.

### Added — gates (4 new, 13 total)

- **Gate 9** — Doc Sync
- **Gate 10** — Artifact Hygiene
- **Gate 11** — Adversarial Counterexample
- **Gate 12** — Rollback Plan

### Added — agents (6 new, 25 total)

- **DocSynchronizer** — keep BUGS.md / README / corpus / source aligned.
- **BootstrapMarshal** — own selfhost, byte-identical comparison, stage isolation.
- **IRVerifier** — CFG, PHI, dominance, reachability, liveness, optimizer safety.
- **ArtifactJanitor** — detect stale generated files, source-tree drift, partial outputs.
- **SecurityLens** — classify exploitability and memory-safety/security impact.
- **OracleDesigner** — pick the strongest validation oracle.

### Added — top-level sections

- `severity_model` (P0–P3 with criteria + ship_policy per level)
- `evidence_model` (confidence levels, evidence types, claim rules)
- `finding_statuses` (8 statuses)
- `finding_report_template` (25 fields, with required-fields per status)
- `workflows` (4: doc_drift_audit, codegen_runtime_bug, error_management_bug, selfhost_regression)
- `test_matrix` (codegen / error_management / docs class catalog)
- `base64_magic_protocol` (test-case schema + decode/compile/compare rules)
- `ship_decision_policy` (explicit ship / hold / needs_more_evidence rules)
- `quick_commands` (doc_sync_cg_ir_011, broad_codegen_validation, artifact_hygiene)
- `tiers` (T0–T7 ladder, 50 invariants)
- `release_policy` (normal_ship / high_risk_ship / emergency_hotfix gating)

### Added — runner (`bug_hunter.py`)

- `tiers` subcommand — list T0-T7 ladder; `--tier T0` filter; `--id T0-04` lookup.
- `release-policy` subcommand — print release gates (normal / high-risk / emergency).
- `workflow` subcommand — print workflow definitions; `--name` filter.
- `test-matrix` subcommand — print canonical test classes per area.
- `severity` subcommand — print P0–P3 severity model.
- `--version` flag.
- `invariants --search <substr>` — substring search across id / name / rule.
- `invariants --category` — now accepts any of the 8 categories.
- `invariants --id` — now resolves both domain (`CG-25`) and tier (`T0-04`) ids.

### Changed — Finding shape

- `violated_invariant_ids: list[str]` (was `violated_invariant: str` singular).
  Backward-compat: corpus entries with singular `violated_invariant` are
  promoted to single-element lists.
- New fields: `status`, `evidence_strength`, `schema_version`,
  `ship_rationale`, `counterexamples_tried`, `remaining_uncertainty`,
  `base64_payload` (echoed back in the report for traceability).
- Markdown report now lists all violated invariants with rule + smell each;
  status / severity / evidence-strength / schema all pinned at top.

### Changed — verdict logic

The runner now applies `ship_decision_policy` explicitly:

- compile asymmetric (one side compiles, other doesn't) → `needs_evidence`
- runtime divergent (stdout or exit) → `hold`
- timed out → `hold`
- P0/P1 without validation → `needs_evidence`
- otherwise → `ship`

Each verdict carries a `ship_rationale` list — the explicit policy criteria
that drove it. Replaces v1.0 ad-hoc heuristics.

### Backward compatibility

- v1.0 corpus entries continue to work (`violated_invariant` singular accepted).
- v1.0 Finding JSON schema is a strict subset of v1.1 — additional fields
  default to safe empty values.
- v1.0 subcommands (`run`, `invariants`, `gates`, `template`) unchanged in
  semantics; `invariants --category` now accepts the 5 new categories.

---

## v1.0.0 — 2026-05-04

Initial release.

- 42 invariants across 3 categories (10 G + 20 CG + 12 EM).
- 9 production push gates (0–8).
- 19 agent roles.
- Stdlib Python CLI runner (`run`, `invariants`, `gates`, `template`).
- 3 corpus entries: `return_42`, `sign_extend_char_arg`, `stack_align_at_printf_with_double`.
- One-shot Antigravity / agent dispatch prompt at `prompts/codegen_audit.md`.
