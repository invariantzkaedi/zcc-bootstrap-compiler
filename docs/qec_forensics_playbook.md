# QEC Forensics Playbook

Version: 1.0.0  
Audience: Compiler/QEC contributors, CI triage owners  
Scope: Stabilizer validation failures, decoder mismatches, determinism drift

---

## 1) Purpose

This playbook defines the first-response workflow for QEC test failures and non-deterministic behavior.  
Goal: turn any failure into a reproducible, minimized, and classified artifact within minutes.

---

## 2) Inputs and Artifacts

Expected artifact outputs under `artifacts/`:

- `failure_<seed>.json` — primary structured failure record
- `failure_<seed>.min.json` (optional) — minimized witness
- `repro_<seed>.sh` — one-command repro script
- `index.json` — run-level failure index with signatures
- `golden_semantic_diff.json` — semantic diff for golden mismatch
- `mutation_report.json` (if mutation job enabled)
- `perf_timeseries.json` (if perf job enabled)

---

## 3) First 15 Minutes (Mandatory Triage)

1. Open CI run and download `artifacts/`.
2. Read `artifacts/index.json`.
   - Note `unique_signatures`
   - Identify first/most frequent signature
3. Run repro locally:
   ```bash
   bash artifacts/repro_<seed>.sh
   ```
4. Validate schema:
   ```bash
   python3 -m pytest -q tests/test_trace_schema_validation.py
   ```
5. If failure persists, run minimization:
   ```bash
   python3 scripts/minimize_counterexample.py \
     --in-case artifacts/failure_<seed>.json \
     --out-case artifacts/failure_<seed>.min.json \
     --checker-module tests.qec_checker
   ```

---

## 4) Classification Matrix

Classify each failure as one of:

- `math_rule_mismatch`
- `indexing_mismatch`
- `decoder_tiebreak_mismatch`
- `serialization_mismatch`
- `numeric_tolerance_mismatch`
- `unknown`

### Heuristics

- Matrix+Tableau agree; C differs → likely C implementation bug.
- Matrix differs from Tableau/C → likely convention or oracle setup issue.
- Hash drift across identical seeded runs → determinism breach.
- Same logic but JSON mismatch only → serialization/stability issue.

---

## 5) Determinism Checks

Run deterministic replay 3x:

```bash
for i in 1 2 3; do
  QEC_SEED=1337 QEC_FUZZ_SEEDS=10 python3 tests/test_stabilizer_fuzz.py
done
```

Expected:
- identical emitted trace hash
- identical failure signatures (if failing)
- byte-stable `failure_*.json` for same scenario

---

## 6) Golden Trace Triage

When golden mismatch occurs:

1. Generate semantic diff:
   ```bash
   python3 scripts/golden_semantic_diff.py \
     --expected tests/golden/<case>.json \
     --actual artifacts/<case>.actual.json \
     --out artifacts/golden_semantic_diff.json
   ```
2. Determine if change is:
   - intentional semantic update (requires policy flag)
   - unintended regression (block merge)

Do not update goldens without policy requirements in `qec_golden_update_policy.md`.

---

## 7) Escalation Rules

Escalate to “P0 QEC” when any of:
- Determinism gate fails on required job
- Tri-oracle disagreement appears in required seed set
- Mutation kill-rate drops below threshold
- Same new signature appears across ≥3 consecutive runs

Escalation message template:
- Signature
- Seed(s)
- Minimized length ratio
- First bad commit (if bisected)
- Repro command

---

## 8) Required Issue Payload for New Failures

Every failure issue must include:

- Failure signature hash
- Seed and qubit count
- Minimized gate sequence
- Classification label
- Repro command
- Artifact links from CI run
- Suspected owner (`oracle`, `decoder`, `runtime`, `serialization`)

---

## 9) Exit Criteria (incident close)

A failure incident is closed only when:

- fix merged
- failing seed added to regression suite/fixture
- schema-valid artifact attached in issue
- 3 deterministic replays pass
- nightly confirms no recurrence for 3 consecutive runs

---

## 10) Quick Commands

```bash
# build artifact index
python3 scripts/artifact_utils.py

# generate repro scripts
python3 scripts/make_repro_script.py

# run preflight pack
python3 -m pytest -q \
  tests/test_trace_schema_validation.py \
  tests/test_invariants_battery.py \
  tests/test_determinism_contract.py
```
