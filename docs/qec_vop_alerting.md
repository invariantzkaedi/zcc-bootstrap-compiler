# QEC-VOP Alerting Framework & Operator Rules

**QEC-VOP: deterministic verification control plane**

---

## 🚨 Alert Thresholds Matrix

| Code | Severity | Trigger Threshold | Metric Tracked |
|---|---|---|---|
| `DETERMINISM_DRIFT` | **Critical** | `drifts > 0` | DRIFT across identical seeds |
| `LOW_KILL_RATE` | **High** | `kill_rate < 0.90` | Mutation smoke testing rate |
| `STALE_INCIDENTS` | **High** | `incidents_open > 0` for `age > 7 days` | Unresolved mathematical survivors |
| `SIGNATURE_SPIKE_WOW`| **Medium** | `wow_spike > 50%` | Week-over-week unique failures spike |

---

## 📋 Operator Action Items

### 🔴 Critical Alert: `DETERMINISM_DRIFT`
1. **Freeze protected branches** (No new merges to `main`).
2. Run replay replication:
   ```bash
   bash artifacts/repro_<drift_seed>.sh
   ```
3. Classify error source and commit regression tests.

### 🟡 High Alert: `LOW_KILL_RATE`
1. Audit survivors file `artifacts/mutation_report.json`.
2. Locate survived mutants and assign owners to add missing test assertions.

### 🟡 High Alert: `STALE_INCIDENTS`
1. Re-evaluate oldest incidents in issue log.
2. File recovery plan or assign a waiver if incident is verified minor.
