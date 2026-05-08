"""audit_cli.py — Reverse Tumbler Courtroom CLI.

Subcommands:

    audit <case.json> [--ledger <path>]
        Run Prosecutor → Defense → Marshal on a case JSON.
        Print indictment, plan, verdict.
        Optionally persist the ledger.

    fuzz <case.json> [-n N] [--seed S]
        Run the differential fuzzer against a case.
        Reports invariance violations.

    verify-ledger <path.jsonl>
        Verify the hash chain of a persisted ledger.

Exit codes:
    0  SHIP
    1  SHIP_WITH_RISK_ACCEPTANCE
    2  HOLD
    3  ROLLBACK_OR_CONTAIN
    4  CLI / parsing / I/O error
"""
from __future__ import annotations

import argparse
import sys
import traceback
from datetime import datetime, timezone

from case_schema import CaseSchemaError, load_case
from invariant_compiler import Verdict
from ledger_store import LedgerStoreError, PersistedLedger, load, save
from release_energy import energy_of, evolve_H, fuzz_case

from courtroom import CourtSession

_VERDICT_EXIT_CODE = {
    Verdict.SHIP: 0,
    Verdict.SHIP_WITH_RISK_ACCEPTANCE: 1,
    Verdict.HOLD: 2,
    Verdict.ROLLBACK_OR_CONTAIN: 3,
}


def _now() -> datetime:
    return datetime.now(timezone.utc)


def cmd_audit(args) -> int:
    case = load_case(args.case)
    print(f"case:    {case.id}  (risk={case.risk})")
    print(f"tumblers: {len(case.tumblers)},  evidence: {len(case.evidence)}")

    if args.ledger:
        # Use a PersistedLedger so the session is durable.
        pl = PersistedLedger(args.ledger)
        session = CourtSession(case=case, ledger=pl.underlying)
        # Replace session ledger appends with the persisted one.
        # Since CourtSession appends via self.ledger directly, swap:
        session.ledger = pl.underlying
        # Note: PersistedLedger.append goes through ReleaseLedger.append +
        # mirrors to disk. The session calls ReleaseLedger.append directly
        # via self.ledger.append, which won't mirror. We work around by
        # snapshotting after each step.
    else:
        session = CourtSession(case=case)
        pl = None

    now = _now()

    # 1. Indict.
    brief = session.indict(now=now)
    print()
    print("=== INDICTMENT ===")
    print(f"  {brief.summary}")
    for c in brief.charges:
        print(f"  • [{c.tier}] {c.invariant}  (severity {c.severity})  — {c.reason}")

    # 2. Defense plan.
    plan = session.plan_defense(brief, now=now)
    print()
    print("=== DEFENSE PLAN ===")
    print(f"  {plan.note}")
    for a in plan.actions:
        print(f"  • {a.kind.value:24} [{a.target_tier}] {a.target_invariant}")
        if a.required_artifact:
            print(f"      need:    {a.required_artifact}")

    # 3. Adjudicate.
    verdict = session.adjudicate(now=now)

    # 4. Energy.
    e = energy_of(case, now=now)

    print()
    print("=== VERDICT ===")
    print(f"  verdict:        {verdict.value}")
    print(f"  release_energy: {e:.1f}  (0.0 = SHIP-ready)")

    # If --ledger given, persist now (snapshot whole chain).
    if args.ledger:
        try:
            save(session.ledger, args.ledger)
            print(f"  ledger saved:   {args.ledger} "
                  f"({len(session.ledger.entries)} entries)")
        except LedgerStoreError as ex:
            print(f"  ledger ERROR:   {ex}", file=sys.stderr)

    return _VERDICT_EXIT_CODE[verdict]


def cmd_fuzz(args) -> int:
    case = load_case(args.case)
    print(f"fuzzing case: {case.id}")
    print(f"  perturbations: {args.n},  seed: {args.seed}")
    report = fuzz_case(case, n_perturbations=args.n, seed=args.seed,
                       now=_now())
    print()
    print(report)
    return 0 if report.ok else 5


def cmd_verify_ledger(args) -> int:
    try:
        ledger = load(args.path)
    except LedgerStoreError as ex:
        print(f"ledger LOAD FAILED: {ex}", file=sys.stderr)
        return 4
    ok, reason = ledger.verify()
    print(f"path:    {args.path}")
    print(f"entries: {len(ledger.entries)}")
    print(f"head:    {ledger.head_hash[:16]}...")
    print(f"verify:  {ok} ({reason})")
    return 0 if ok else 4


def cmd_evolve(args) -> int:
    case = load_case(args.case)
    e = energy_of(case, now=_now())
    traj = evolve_H(H_base=e, steps=args.steps, eta=args.eta,
                    gamma=args.gamma, beta=args.beta,
                    sigma=args.sigma, seed=args.seed)
    print(f"H_0 = {traj.H_base:.2f}")
    print(f"H_T = {traj.converged_value:.2f}  (T={args.steps})")
    print(f"stable: {traj.stable}")
    if args.show:
        for i, h in enumerate(traj.H_series):
            print(f"  t={i:3}  H={h:.4f}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="audit_cli",
        description="Reverse Tumbler Courtroom CLI",
    )
    sp = p.add_subparsers(dest="cmd", required=True)

    a = sp.add_parser("audit", help="Run a release audit")
    a.add_argument("case", help="path to case JSON")
    a.add_argument("--ledger", help="persist ledger to this JSONL path")
    a.set_defaults(fn=cmd_audit)

    f = sp.add_parser("fuzz", help="Run the differential fuzzer on a case")
    f.add_argument("case", help="path to case JSON")
    f.add_argument("-n", type=int, default=200,
                   help="random perturbations (default 200)")
    f.add_argument("--seed", type=int, default=0)
    f.set_defaults(fn=cmd_fuzz)

    v = sp.add_parser("verify-ledger", help="Verify a persisted ledger")
    v.add_argument("path", help="path to JSONL ledger")
    v.set_defaults(fn=cmd_verify_ledger)

    e = sp.add_parser("evolve",
                      help="Run PRIME Hamiltonian recursion on case energy")
    e.add_argument("case", help="path to case JSON")
    e.add_argument("--steps", type=int, default=64)
    e.add_argument("--eta", type=float, default=0.4)
    e.add_argument("--gamma", type=float, default=0.3)
    e.add_argument("--beta", type=float, default=0.1)
    e.add_argument("--sigma", type=float, default=0.05)
    e.add_argument("--seed", type=int, default=42)
    e.add_argument("--show", action="store_true",
                   help="print full trajectory")
    e.set_defaults(fn=cmd_evolve)

    return p


def main(argv=None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.fn(args)
    except CaseSchemaError as ex:
        print(f"case schema error: {ex}", file=sys.stderr)
        return 4
    except FileNotFoundError as ex:
        print(f"file not found: {ex}", file=sys.stderr)
        return 4
    except Exception:  # last-resort tracebacks for the CLI
        traceback.print_exc()
        return 4


if __name__ == "__main__":
    sys.exit(main())
