"""Tests for release_energy.py — energy + Hamiltonian + differential fuzzer."""
from datetime import datetime, timezone, timedelta

from invariant_compiler import (
    Evidence, ReleaseCase, Status, TumblerStatus, Waiver, decide,
)
from release_energy import (
    CaseFuzzer, energy_of, evolve_H, fuzz_case, release_energy,
)


NOW = datetime(2026, 5, 5, tzinfo=timezone.utc)
FUTURE = (NOW + timedelta(days=30)).isoformat()


results: list[tuple[bool, str]] = []


def expect(name, actual, expected):
    ok = actual == expected
    results.append((ok, name))
    flag = "PASS" if ok else "*** FAIL ***"
    print(f"[{flag}] {name}")
    if not ok:
        print(f"        expected={expected!r}")
        print(f"        actual  ={actual!r}")


def expect_true(name, actual):
    expect(name, bool(actual), True)


def expect_close(name, actual, expected, tol=1e-6):
    ok = abs(actual - expected) <= tol
    results.append((ok, name))
    flag = "PASS" if ok else "*** FAIL ***"
    print(f"[{flag}] {name}")
    if not ok:
        print(f"        expected≈{expected}  actual={actual}  diff={abs(actual-expected)}")


def good_ev(qid="ev1", q=5):
    return {qid: Evidence(id=qid, type="cmd", claim_supported="ok", quality=q,
                          command="pytest -q", result="1 passed")}


def valid_waiver():
    return Waiver(reason="r", owner="o", expires_at=FUTURE,
                  repayment="rp", approved_by="a")


print("=" * 60)
print("RELEASE ENERGY")
print("=" * 60)

# ─── E-1: SHIP-ready case has zero energy ───
print()
print("--- energy ---")
clean = ReleaseCase(
    id="e1", risk="low",
    tumblers=[
        TumblerStatus(tier="T0-01", invariant="Evidence",
                      status=Status.SEATED, evidence_ids=["ev1"]),
    ],
    evidence=good_ev(),
)
expect_close("E-1: SHIP-ready energy = 0",
             energy_of(clean, now=NOW), 0.0)

# ─── E-2: T0 unseated → energy = 100 ───
unseated = ReleaseCase(
    id="e2", risk="critical",
    tumblers=[
        TumblerStatus(tier="T0-01", invariant="Evidence",
                      status=Status.UNKNOWN),
    ],
    evidence={},
)
expect_close("E-2: T0 unseated energy = 100",
             energy_of(unseated, now=NOW), 100.0)

# ─── E-3: T0+T1 unseated → energy = 170 ───
both = ReleaseCase(
    id="e3", risk="critical",
    tumblers=[
        TumblerStatus(tier="T0-01", invariant="A", status=Status.UNKNOWN),
        TumblerStatus(tier="T1-01", invariant="B", status=Status.UNKNOWN),
    ],
    evidence={},
)
expect_close("E-3: T0+T1 unseated energy = 170",
             energy_of(both, now=NOW), 170.0)

# ─── H-1: Hamiltonian trajectory shape ───
print()
print("--- Hamiltonian recursion ---")
traj = evolve_H(H_base=170.0, steps=128, seed=42)
expect("H-1a: trajectory length = steps + 1",
       len(traj.H_series), 129)
expect_close("H-1b: starts at H_base", traj.H_series[0], 170.0)
# Check it doesn't blow up — bounded above by 4*H_base for reasonable params.
max_h = max(abs(x) for x in traj.H_series)
expect_true("H-1c: trajectory bounded by 4*H_base",
            max_h < 4 * 170.0)

# ─── H-2: H_base=0 stays near 0 ───
zero_traj = evolve_H(H_base=0.0, steps=64, sigma=0.0, seed=42)
all_zero = all(abs(x) < 1e-9 for x in zero_traj.H_series)
expect_true("H-2: H_base=0 with sigma=0 stays at 0", all_zero)

# ─── F-1: clean case fuzz reports zero violations ───
print()
print("--- fuzzer on clean case ---")
clean_v2 = ReleaseCase(
    id="f1", risk="medium",
    tumblers=[
        TumblerStatus(tier="T0-01", invariant="A", status=Status.SEATED,
                      evidence_ids=["ev1"]),
        TumblerStatus(tier="T1-01", invariant="B", status=Status.SEATED,
                      evidence_ids=["ev1"]),
        TumblerStatus(tier="T2-01", invariant="C", status=Status.WAIVED,
                      waiver=valid_waiver()),
    ],
    evidence=good_ev(),
)
report = fuzz_case(clean_v2, n_perturbations=200, seed=42, now=NOW)
expect("F-1a: 200 perturbations + 30 invariance checks",
       report.perturbations_run, 230)
expect_true("F-1b: zero violations on clean case", report.ok)
if not report.ok:
    print(report)

# ─── F-2: a deliberately broken decide() should fail the fuzzer ───
# Patch decide briefly to a wrong impl and verify fuzzer catches it.
print()
print("--- fuzzer catches a known-bad gate ---")
import release_energy as re_mod
import invariant_compiler as ic_mod

original_decide = ic_mod.decide

def buggy_decide(case, now=None):
    """Mimics the original Copilot bug: WAIVED non-T0 short-circuits SHIP-class."""
    from invariant_compiler import (
        DecisionReport, TumblerFinding, Verdict, Status,
        has_valid_evidence, has_valid_waiver, required_quality_for_tier,
    )
    from tier_parser import is_t0
    findings = []
    for t in case.tumblers:
        min_q = required_quality_for_tier(t.tier)
        if t.status == Status.NOT_APPLICABLE:
            findings.append(TumblerFinding(t.tier, t.invariant, t.status,
                                            True, "na", Verdict.SHIP))
            continue
        if t.status == Status.SEATED:
            seated = has_valid_evidence(t, case, min_q)
            findings.append(TumblerFinding(t.tier, t.invariant, t.status,
                                            seated, "seated",
                                            Verdict.SHIP if seated else Verdict.HOLD))
            continue
        if t.status == Status.WAIVED:
            if is_t0(t.tier):
                return DecisionReport(verdict=Verdict.HOLD, findings=findings,
                                       summary="bug")
            if has_valid_waiver(t, now=now):
                # THE BUG: short-circuit return.
                return DecisionReport(
                    verdict=Verdict.SHIP_WITH_RISK_ACCEPTANCE,
                    findings=findings, summary="bug")
        findings.append(TumblerFinding(t.tier, t.invariant, t.status,
                                        False, "unset", Verdict.HOLD))
    return DecisionReport(
        verdict=Verdict.HOLD if any(not f.seated for f in findings)
                else Verdict.SHIP,
        findings=findings, summary="bug")


# Monkey-patch decide in BOTH modules — release_energy imports decide
# directly so we patch where it lives.
ic_mod.decide = buggy_decide
re_mod.decide = buggy_decide

# A case that exercises the bug: T1 WAIVED + T0 UNKNOWN.
broken_case = ReleaseCase(
    id="f2", risk="critical",
    tumblers=[
        TumblerStatus(tier="T1-01", invariant="A",
                      status=Status.WAIVED, waiver=valid_waiver()),
        TumblerStatus(tier="T0-01", invariant="B",
                      status=Status.UNKNOWN),
    ],
    evidence={},
)
broken_report = fuzz_case(broken_case, n_perturbations=50, seed=7, now=NOW)
expect_true("F-2a: fuzzer detects T0 invariance violation in buggy decide()",
            not broken_report.ok)
violations_t0 = [v for v in broken_report.violations
                  if "I1" in v.invariant or "I2" in v.invariant]
expect_true("F-2b: at least one T0/waiver invariance violation reported",
            len(violations_t0) > 0)

# Restore the real decide.
ic_mod.decide = original_decide
re_mod.decide = original_decide


# ─── F-3: fuzzer on the rewritten compiler with random adversarial cases ───
print()
print("--- adversarial sweep on real compiler ---")
import random
rng = random.Random(123)

def random_case(i):
    tiers = ["T0-01", "T1-01", "T2-01", "T3-01", "T4-01", "T5-01",
             "T6-01", "T7-01"]
    n = rng.randint(1, 5)
    tumblers = []
    waiver = valid_waiver() if rng.random() < 0.5 else None
    for j in range(n):
        tier = rng.choice(tiers)
        status = rng.choice(list(Status))
        evidence_ids = ["ev1"] if status == Status.SEATED else []
        w = waiver if status == Status.WAIVED else None
        tumblers.append(TumblerStatus(
            tier=tier, invariant=f"inv-{j}",
            status=status, evidence_ids=evidence_ids, waiver=w,
        ))
    return ReleaseCase(id=f"sweep-{i}", risk="medium",
                      tumblers=tumblers, evidence=good_ev(q=5))


total_violations = 0
for i in range(50):
    c = random_case(i)
    r = fuzz_case(c, n_perturbations=20, seed=i, now=NOW)
    total_violations += len(r.violations)

expect("F-3: 50 random cases × 30 invariance checks each, zero violations",
       total_violations, 0)


print()
passed = sum(1 for ok, _ in results if ok)
print(f"=== {passed}/{len(results)} release-energy tests passed ===")
