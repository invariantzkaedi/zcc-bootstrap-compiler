---
name: zkaedi-reverse-tumbler-courtroom
description: Operational arm of zkaedi-gate-discipline. A runnable T0–T7 release-gate decision engine with adversarial Prosecutor, surgical Defense Engineer, hash-chained append-only Ledger, and a Marshal that defers to the gate compiler. Includes a differential fuzzer that probes case-state perturbations against five gate invariants (T0, T0-waiver, expiry, order, NA), JSONL persistence with verify-on-load and rotation, declarative JSON case schema, and a CLI (`python -m audit_cli audit case.json`). Use whenever a release decision needs to be auditable, when "all gates green" needs mechanical verification rather than narrative, when a multi-tumbler decision is too complex for ad-hoc review, when defending a closure that another agent generated, or when verifying that a previously-claimed audit closure has its evidence intact on disk. Trigger on phrases like "audit this release", "run the courtroom on", "fuzz this case", "verify the ledger", "compile invariants", "is this ready to ship", "tumbler status", "release courtroom", "evidence ledger", "decision report", or any mention of T0/P0 / T1/P1 / T7/P7 invariant tiers from the Reverse Tumbler vocabulary.
---

# ZKAEDI Reverse Tumbler Courtroom — Toolchain

This skill is the **operational** counterpart to `zkaedi-gate-discipline`. Where gate-discipline names the five failure modes that corrupt a forensic chain (phantom closure, threshold-mismatch gate, spin-framing, narrative drift, test-ordering pollution), the Courtroom mechanizes the discipline so a human or agent can't drift from it through eloquence alone.

The load-bearing claim of the whole stack:

> *The Marshal cannot whitewash a HOLD. The compiler decides; every other role just packages and audits.*

If you ever find a Marshal verdict that disagrees with `decide()`, that is the bug — not a tunable.

## When to invoke

- A release decision involves more than two or three invariants and ad-hoc review will miss something.
- An agent (Copilot, AG, another Claude) has generated a "ready to ship" claim that needs mechanical verification.
- You want a hash-chained record of *why* a release was held or shipped, durable across process death.
- You want to detect gate bugs — apply the differential fuzzer to your `decide()` and watch for invariance violations.
- You want to defend or attack a previously-claimed closure by re-running its case through the Courtroom.

## When NOT to invoke

- Single-line bugfix in a personal project. Just commit it.
- Anything where the existing test suite is the audit. The Courtroom is for releases, not commits.
- Cases with one tumbler. `decide()` is fine on its own.

## File manifest (~1,950 LOC, 115 tests)

| File | Purpose | Tests |
|---|---|---|
| `tier_parser.py` | Real tier parsing (T0..T15, no T1-vs-T10 prefix collision) | 23/23 |
| `invariant_compiler.py` | `decide(case) → DecisionReport` with full-pass accumulator | 10/10 |
| `courtroom.py` | Prosecutor / Defense / Notary / Marshal + `CourtSession` | 23/23 |
| `ledger_store.py` | JSONL save/load/append/rotate with verify-on-load + fcntl lock | 31/31 |
| `release_energy.py` | Energy + PRIME Hamiltonian + differential fuzzer (5 invariants) | 12/12 |
| `case_schema.py` | Declarative JSON case loader with strict validation | 16/16 |
| `audit_cli.py` | CLI: `audit`, `fuzz`, `verify-ledger`, `evolve` | (subject) |
| `audit_compiler.py` | Repros for the four T0-bypass bugs in the original Copilot artifact | (forensic) |

Plus six test files and a sample `sample_case.json`.

## The flow

```
case.json ──► load_case() ──► ReleaseCase
                                  │
                                  ▼
                             decide() ──► DecisionReport
                                  │           │
                ┌─────────────────┼───────────┴────────────────┐
                ▼                 ▼                            ▼
          Prosecutor         DefenseEngineer                 Marshal
            │                    │                             │
     ProsecutionBrief        DefensePlan                    Verdict
            │                    │                             │
            └────► Notary ◄──────┘                             │
                     │                                         │
                     └──────► ReleaseLedger (sha256 chain) ◄───┘
                                       │
                                  ledger_store.save()
                                       │
                                  *.jsonl on disk
```

## Decision API in three sentences

`decide(case, now=NOW) → DecisionReport`. The `findings` list has one entry per tumbler with `seated: bool` and `contribution: Verdict`. The verdict is the worst contribution across all tumblers — never an early return, always a full pass.

## The five fuzz invariants

The differential fuzzer probes the gate by perturbing case state and asserting:

| ID | Name | What it forbids |
|---|---|---|
| I1 | T0 invariance | Any case with at least one unseated T0 tumbler producing a SHIP-class verdict |
| I2 | Waiver T0 invariance | Any T0 tumbler with status=WAIVED producing a SHIP-class verdict regardless of waiver shape |
| I3 | Expiry invariance | Any expired waiver being treated as seating a tumbler |
| I4 | Order invariance | Tumbler list order changing the verdict |
| I5 | NA invariance | Adding NOT_APPLICABLE tumblers worsening the verdict |

Each invariant violation is a gate bug. The fuzzer caught the original Copilot bug when re-introduced as a regression test (F-2 in `test_release_energy.py`).

## Tier ladder (T0–T7) reminder

```
T0/P0  Must Not Ship           hard hold on any violation
T1/P1  Must Understand         repro, root-cause, evidence
T2/P2  Must Patch Safely       surgical fix, no scope creep
T3/P3  Must Prove and Preserve regression coverage
T4/P4  Must Operate Well       runtime, observability
T5/P5  Must Scale Under Stress load, tail latency
T6/P6  Must Resist Adversaries threat model
T7/P7  Must Improve the System hygiene, refactor, lessons
```

Severity weights used by the Prosecutor: T0=100, T1=70, T2=50, T3=40, T4=30, T5=25, T6=20, T7=15. The Defense Engineer marks T0 charges as `UNREMEDIABLE` (no waiver path exists) — only fix-and-evidence is admissible.

## CLI quickstart

```powershell
# Run the Courtroom on a case file, persist the ledger
python audit_cli.py audit case.json --ledger release.jsonl

# Differential fuzz: 200 random perturbations + 30 invariance checks
python audit_cli.py fuzz case.json -n 200 --seed 42

# Re-verify a persisted ledger long after the session ended
python audit_cli.py verify-ledger release.jsonl

# Evolve the case's Hamiltonian energy (PRIME recursion)
python audit_cli.py evolve case.json --steps 64 --seed 42
```

Exit codes: `0` SHIP, `1` SHIP_WITH_RISK_ACCEPTANCE, `2` HOLD, `3` ROLLBACK_OR_CONTAIN, `4` parsing/IO error, `5` fuzz violation. Wire into CI by gating the merge step on exit==0 (or exit<=1 if you accept SHIP_WITH_RISK).

## Case schema

One JSON file per release decision. Required keys: `id`, `risk` (one of low/medium/high/critical/deployed_breach), `tumblers` (non-empty list). Each tumbler needs `tier`, `invariant`, `status`. Evidence is a top-level list keyed by `id`. Every `evidence_ids` reference in a tumbler must resolve to an entry in the evidence list — dangling references are rejected at parse time.

```json
{
  "id": "rel-YYYY-MM-DD.N",
  "risk": "medium",
  "tumblers": [
    {"tier": "T0-01", "invariant": "Evidence Before Belief",
     "status": "seated", "evidence_ids": ["ev1"]},
    {"tier": "T2-04", "invariant": "Patch Safely",
     "status": "waived",
     "waiver": {"reason": "...", "owner": "...",
                "expires_at": "2026-12-01T00:00:00+00:00",
                "repayment": "HYG-...", "approved_by": "..."}}
  ],
  "evidence": [
    {"id": "ev1", "type": "cmd", "claim_supported": "...",
     "command": "...", "result": "...", "quality": 5}
  ]
}
```

## Ledger format

JSONL, one entry per line, sha256 chain. Each entry's `canonical()` shape is the on-disk format; **changing it bumps `LEDGER_FORMAT_VERSION` and invalidates every existing ledger**. Format version is currently 1.

Tampering with any payload, actor, prev_hash, or entry_hash on disk breaks `verify()` and identifies the seq of the first bad entry. A truncated last line (mid-fsync crash) is tolerated by default but can be made strict with `tolerate_partial_last_line=False`.

Rotation: `rotate(src, archive)` moves the current ledger to `archive`, opens a fresh ledger at `src`, and writes a CHECKPOINT entry that records the archived head_hash. Walking the chain across rotations means walking each archive in turn.

## What this skill enforces

- `decide()` is the source of truth. Every other role packages — none decides.
- Verdicts are derived from a full pass over all tumblers. No early returns short-circuit a SHIP-class verdict before the loop has examined the rest of the case.
- WAIVER is never a path to seating a T0/P0 tumbler.
- Expiry is real time, not "the field is non-empty".
- Tier strings are parsed, never prefix-matched. T1 ≠ T10.
- The on-disk ledger verifies bit-identical to the in-memory one (`reloaded.head_hash == ledger.head_hash`).
- A buggy `decide()` is detected by the fuzzer's invariance probes, not by hand-written tests alone.

## What this skill refuses

- A Marshal that contradicts `decide()`. Not a feature; a bug.
- A "we'll just add a flag to override T0" workflow. T0 is the floor.
- A ledger that fails `verify()` going to disk. We don't sign chains we can't trust.
- A waiver path on T0/P0. The Notary rejects them.
- A "pretty close to seated" tumbler counting as seated. Either the evidence meets quality threshold or it doesn't.

## Connection to zkaedi-gate-discipline

The Courtroom is what `zkaedi-gate-discipline` looks like as code rather than narrative. The five failure modes from gate-discipline map to specific defenses in this stack:

| gate-discipline failure mode | Courtroom defense |
|---|---|
| Phantom closure | Notary rejects evidence with no concrete artifact; ledger chain proves the closure happened |
| Threshold-mismatch gate | `required_quality_for_tier()` is a single source of truth; tier-parsed; not prefix-matched |
| Spin framing — failure as success | Marshal cannot return SHIP when `decide()` says HOLD; F-2 fuzz proves the contradiction is detected |
| Narrative drift in commit messages | The ledger is the commit body; chain of indictment → defense → admissions → recompute → verdict is what gets reviewed |
| Test-ordering pollution | I4 (order invariance) fuzz probe |

When `zkaedi-gate-discipline` says "name the load-bearing variable and measure it before drafting", this skill is the measuring instrument.

## Final word

The first time the original `decide()` was audited, four T0-bypass bugs surfaced inside an artifact that another agent had written and called complete. The fix wasn't clever; it was a full-pass accumulator over the tumbler list. Most production gate failures are this same shape: someone built a fast-path early-return, the test suite happened to not exercise the path that triggered it, and a release shipped with an unseated T0 tumbler.

The Courtroom is here so that doesn't happen again — and so that when it does happen, the ledger on disk is the receipt that says exactly when, why, and who.
