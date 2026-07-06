# QEC Mutation Strategy

Version: 1.0.0  
Goal: Prove tests detect realistic stabilizer/decoder defects.

---

## 1) Why mutation testing here

Traditional pass rates can hide weak assertions.  
Mutation testing injects controlled faults and verifies tests fail as expected.

Success criterion: high mutation kill-rate in critical rule paths.

---

## 2) Mutation Targets (Phase 1)

Minimum required operators:

1. **CNOT Z-propagation mutation**
   - mutate `z[c] ^= z[t]` to no-op or wrong direction
2. **CZ X-neighbor mutation**
   - omit one of:
     - `z[a] ^= x[b]`
     - `z[b] ^= x[a]`
3. **Tie-break inversion mutation**
   - invert deterministic ordering (e.g., reverse lexicographic)

Optional phase 2:
- H swap omission
- S transformation corruption
- Syndrome threshold off-by-one

---

## 3) Execution Model

Two modes:

- **Smoke mode (required in PR)**:
  - small set of high-sensitivity fixtures
  - fast kill confirmation
- **Deep mode (nightly)**:
  - broader seed set + varied code families

---

## 4) Kill-Rate Metrics

Definitions:
- `killed`: tests failed under mutant
- `survived`: tests still pass under mutant

Metric:
```text
kill_rate = killed / total_mutants
```

Thresholds:
- PR required smoke threshold: `>= 0.90`
- Nightly target threshold: `>= 0.95`

If below threshold:
- block merge for required smoke
- open strengthening task for survivor mutants

---

## 5) Reporting

Emit `artifacts/mutation_report.json` with:

- schema version
- mutant id/type
- touched rule
- killed/survived
- failing test IDs (if killed)
- runtime cost
- aggregate kill rate

Example shape:
```json
{
  "schema_version": "1.0.0",
  "total_mutants": 10,
  "killed": 9,
  "survived": 1,
  "kill_rate": 0.9,
  "mutants": [
    {
      "id": "mut_cnot_z_dir",
      "type": "rule_flip",
      "target": "CNOT z-propagation",
      "status": "killed",
      "failing_tests": ["tests/test_quantum_stabilizers.py::test_cnot_z"]
    }
  ]
}
```

---

## 6) Survivor Handling Workflow

For each survivor mutant:

1. Create issue labeled `qec-mutation-survivor`
2. Add minimal reproducer fixture
3. Add/strengthen assertion in relevant suite
4. Re-run mutation pack and confirm kill

No survivor closes without test hardening evidence.

---

## 7) CI Integration Guidance

- Run smoke mutation in PR required lane.
- Run deep mutation in nightly lane.
- Upload mutation report with `if: always()`.
- Show kill-rate in CI summary markdown.

---

## 8) Anti-patterns to avoid

- Counting equivalent mutants as survivors without triage
- Allowing mutation gate to be informational forever
- Running mutation without deterministic seed controls
- Ignoring runtime blowups from excessive mutant counts

---

## 9) Roadmap

- Phase 1: 3 required mutants (this doc)
- Phase 2: 10+ mutants across propagation and decoder layers
- Phase 3: auto-generated mutants + survivor prioritization by production risk

---

## 10) Definition of Done (Mutation)

- [ ] Smoke kill-rate meets PR threshold
- [ ] Nightly report generated and archived
- [ ] All new survivors tracked by issue
- [ ] At least one survivor converted into permanent regression test each sprint
