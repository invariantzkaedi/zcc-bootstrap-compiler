# zkaedi-lab

Tier-0 configuration-evolution sandbox. First deliverable of the agent-evolution
program: **runner + evidence plane, canonical receipt schema, append-only
hash-chained ledger, anti-fabrication battery (E8)**.

Design authority: configuration evolution first — prompts, tool policies,
scaffolds, eval configs. No self-modifying code until the exit criteria below
are all PASS *in the lab distro*.

## Architecture

```
candidate.json (untrusted data, never executed)
      │  content-addressed: sha256(domain || 7 identity fields)
      ▼
runner/run_candidate.py          ← the only door
      │  tmpfs cwd · sanitized env · rlimits · netns (unshare -n) · wall-kill
      ▼
evals/evaluate_config.py         ← trusted interpreter of untrusted config
      │
      ▼  runner determines verdict from exit code — candidate prose is never evidence
receipt (0444) ──► receipts/append-only/ledger.jsonl   (hash-chained, verifiable)
      │
      ▼
lineage/promotion_record.py      ← separate signed authority object;
                                    evidence re-verified against ledger
                                    at create AND verify time
```

Runner exit codes: `0` pass · `10` candidate fail (incl. crash) · `11` timeout ·
`≥20` runner error (no receipt — never mistakable for a verdict).

## Certification — this environment (Ubuntu 24-based container, Python 3.12.3, 2026-07-18)

Command: `python3 tests/selftest.py > logs/selftest.log 2>&1` → **EXIT:0**, 14/14.
Independent ledger check: `python3 runner/receipt_writer.py receipts/append-only/ledger.jsonl` → **EXIT:0**, 16 entries, chain intact.

| Gate | Claim | Evidence (logged exit) | Verdict |
|---|---|---|---|
| T1a/T1b | runner distinguishes PASS/FAIL | 0 / 10 | PASS |
| T2 | survives SIGSEGV candidate, receipt written | 10, exit_code −11 in receipt | PASS |
| T3 | fail→pass byte-flip in ledger detected | verify exit 2, restore exit 0 | PASS |
| T4a/b/c | E8: unbacked PASS claim → V1; honest claim clean; foreign receipt → V4 | 3 / 0 / 3 | PASS |
| T5 | candidate ids reproduce across processes; 1-char mutation → new id | 3 invocations, 1 id | PASS |
| T6a/T6b | promotion w/o evidence refused; valid signs+verifies; forged sig refused | 5 / 0 | PASS |
| T7 | planted secrets absent from candidate env, PATH minimal | 0 | PASS |
| T8 | 2 s wall timeout kills process group | 11, SIGKILL (−9) | PASS |
| T9 | 1 GiB alloc under 512 MiB RLIMIT_AS fails candidate | 10 | PASS |
| T10 | Tier-0 network egress blocked (netns via `unshare -n`) | 10 | PASS |
| — | host-mount denial / UID separation / pids hard wall | enforced by `scripts/run_tier0.sh` podman flags — not runnable here | UNVERIFIED |
| — | snapshot rollback (`scripts/snapshot_lab.sh`) | needs WSL host | UNVERIFIED |
| — | `create_lab_distro.ps1` | needs Windows host | UNVERIFIED |

Honest scope notes:
- **Tamper model in-container is detection, not prevention.** Same-UID processes
  could rewrite the ledger; the hash chain catches it (T3). Prevention is
  podman's job (`:ro` mounts, separate UID) — claimed only for the lab distro.
- One fix during certification: `receipt_writer.py` lacked repo-path insertion
  when invoked as a CLI (ImportError, first battery run 13/14). Fixed, full
  battery re-run clean. No gate logic changed.
- `_zk_selftest` hooks in the evaluator are fault-injection ports proving each
  gate can go red; they simulate misbehaving candidates, they are not a bypass
  (the runner ignores candidate output for verdicts regardless).

## Usage

```sh
python3 runner/run_candidate.py candidates/sha256/<id>.json          # one run
python3 tests/selftest.py                                            # full battery
python3 runner/receipt_writer.py receipts/append-only/ledger.jsonl   # audit chain
python3 evals/fabrication/e8_unsupported_pass.py <stdout> <cid> <ledger>
sh scripts/run_tier0.sh <candidate.json>                             # prod (podman)
```

## Next milestones

1. Deploy to the lab distro; run this same battery there; flip the three
   UNVERIFIED rows with logged exit codes.
2. Milestone E mutation engine: declarative patches against
   `policies/mutation_allowlist.json`, each mutation birthing a new
   content-addressed candidate.
3. Extend battery E1–E7 (correctness, replay determinism, tool-policy
   compliance) on top of the E8 spine.
