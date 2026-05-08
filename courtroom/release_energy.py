"""release_energy.py — PRIME-style energy field over release cases + fuzzer.

The legitimate Hamiltonian application to release engineering is NOT
maze-solving; it's *measuring how far a case is from SHIP* and using
recursive perturbation to find gate bugs.

Energy:
    E(case) = Σ severity(tumbler.tier) · indicator(tumbler not seated)

Lower energy = closer to SHIP. Zero energy = SHIP-ready.

Recursive Hamiltonian (from userPreferences):
    H_t = H_0 + η · H_{t-1} · σ(γ · H_{t-1}) + ε · N(0, 1 + β·|H_{t-1}|)

In this domain the recursion produces an *energy trajectory* across
candidate remediation plans. A plan whose H trajectory monotonically
decreases is a path to ship. A plan whose H trajectory diverges or
oscillates is a path to instability.

Differential fuzzer:
    Perturb a case (status flips, evidence quality bumps, ordering,
    expiry shifts) and check that decide() obeys the invariants:
        I1. T0 invariance      — unseated T0 must produce HOLD or worse
        I2. Waiver invariance  — T0 WAIVED must produce HOLD
        I3. Expiry invariance  — every WAIVED used to seat must have
                                 future expiry; otherwise HOLD
        I4. Order invariance   — verdict is invariant under tumbler
                                 list reordering
        I5. NA invariance      — adding NOT_APPLICABLE tumblers cannot
                                 worsen the verdict

Each invariant violation is a bug in decide(); the fuzzer is the test.
"""
from __future__ import annotations

import copy
import math
import random
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone

from invariant_compiler import (
    DecisionReport,
    ReleaseCase,
    Status,
    TumblerStatus,
    Verdict,
    decide,
)
from tier_parser import is_t0, severity_for

# Verdict severity ordering — same as compiler's _SEVERITY but local
# for fuzzer comparisons.
_SEVERITY = {
    Verdict.SHIP: 0,
    Verdict.SHIP_WITH_RISK_ACCEPTANCE: 1,
    Verdict.HOLD: 2,
    Verdict.ROLLBACK_OR_CONTAIN: 3,
}


# ============================================================
# ENERGY
# ============================================================
def release_energy(report: DecisionReport) -> float:
    """E(case) summed over unseated tumblers, weighted by severity.

    The release is at SHIP-ready energy iff every tumbler is seated.
    """
    total = 0.0
    for f in report.findings:
        if not f.seated:
            total += float(severity_for(f.tier))
    return total


def energy_of(case: ReleaseCase, now: datetime | None = None) -> float:
    return release_energy(decide(case, now=now))


# ============================================================
# RECURSIVE HAMILTONIAN
# ============================================================
@dataclass
class HTrajectory:
    """One trajectory of the recursive Hamiltonian over an energy series."""
    H_series: list[float]
    H_base: float

    @property
    def stable(self) -> bool:
        """True iff H stops moving meaningfully — std of last quarter < 1% of base."""
        n = len(self.H_series)
        if n < 4:
            return False
        tail = self.H_series[3 * n // 4:]
        mean = sum(tail) / len(tail)
        var = sum((x - mean) ** 2 for x in tail) / len(tail)
        std = math.sqrt(var)
        return std < max(0.01 * abs(self.H_base), 1e-9)

    @property
    def converged_value(self) -> float:
        return self.H_series[-1]


def _sigmoid(x: float, gamma: float) -> float:
    # Clamp to avoid overflow.
    z = -gamma * x
    if z > 500:
        return 0.0
    if z < -500:
        return 1.0
    return 1.0 / (1.0 + math.exp(z))


def evolve_H(H_base: float, steps: int = 64,
             eta: float = 0.4, gamma: float = 0.3,
             beta: float = 0.1, sigma: float = 0.05,
             seed: int | None = None) -> HTrajectory:
    """ZKAEDI PRIME recursive Hamiltonian over a scalar energy.

    H_t = H_0 + η · H_{t-1} · σ(γ · H_{t-1}) + ε · N(0, 1 + β·|H_{t-1}|)

    Returns the full trajectory so callers can inspect convergence,
    bifurcations, or chaos.
    """
    rng = random.Random(seed)
    H = H_base
    series = [H]
    for _ in range(steps):
        s = _sigmoid(H, gamma)
        std = math.sqrt(1.0 + beta * abs(H))
        noise = rng.gauss(0.0, std)
        H = H_base + eta * H * s + sigma * noise
        series.append(H)
    return HTrajectory(H_series=series, H_base=H_base)


# ============================================================
# DIFFERENTIAL FUZZER
# ============================================================
@dataclass
class FuzzViolation:
    invariant: str
    case_id: str
    description: str
    perturbation: str
    expected_max_severity: int
    actual_severity: int


@dataclass
class FuzzReport:
    cases_run: int
    perturbations_run: int
    violations: list[FuzzViolation] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.violations

    def __str__(self) -> str:
        if self.ok:
            return (f"FUZZ OK — {self.cases_run} cases × "
                    f"{self.perturbations_run} perturbations, no violations.")
        lines = [f"FUZZ FAIL — {len(self.violations)} violations:"]
        for v in self.violations[:20]:
            lines.append(f"  - {v.invariant} on {v.case_id}: {v.description}")
            lines.append(f"      perturbation: {v.perturbation}")
        if len(self.violations) > 20:
            lines.append(f"  ... and {len(self.violations) - 20} more")
        return "\n".join(lines)


class CaseFuzzer:
    """Perturbs a base case and asserts decide() obeys gate invariants."""

    def __init__(self, base_case: ReleaseCase, *,
                 now: datetime | None = None,
                 seed: int = 0):
        self.base = base_case
        self.now = now or datetime.now(timezone.utc)
        self.rng = random.Random(seed)
        self.report = FuzzReport(cases_run=0, perturbations_run=0)

    def fuzz(self, n_perturbations: int = 200) -> FuzzReport:
        # I1, I2, I3 — generate random cases and check core invariants.
        for _ in range(n_perturbations):
            case = self._random_perturbation()
            self.report.perturbations_run += 1
            self._check_t0_invariance(case)
            self._check_waiver_t0_invariance(case)
            self._check_expiry_invariance(case)

        # I4 — order invariance against the original case.
        self._check_order_invariance(self.base, n=20)

        # I5 — adding NA tumblers cannot worsen verdict.
        self._check_na_invariance(self.base, n=10)

        self.report.cases_run = 1
        return self.report

    # ─── perturbations ───
    def _random_perturbation(self) -> ReleaseCase:
        case = copy.deepcopy(self.base)
        case.id = f"{self.base.id}#fuzz-{self.report.perturbations_run}"

        # 50% chance: flip 1-2 tumbler statuses to random states.
        if self.rng.random() < 0.5 and case.tumblers:
            n_flips = self.rng.randint(1, min(2, len(case.tumblers)))
            for _ in range(n_flips):
                t = self.rng.choice(case.tumblers)
                t.status = self.rng.choice(list(Status))

        # 30% chance: shuffle tumbler order.
        if self.rng.random() < 0.3:
            self.rng.shuffle(case.tumblers)

        # 30% chance: bump or trash an evidence quality.
        if self.rng.random() < 0.3 and case.evidence:
            ev_id = self.rng.choice(list(case.evidence.keys()))
            case.evidence[ev_id].quality = self.rng.randint(0, 6)

        # 20% chance: swap a waiver to expired.
        if self.rng.random() < 0.2:
            for t in case.tumblers:
                if t.waiver and self.rng.random() < 0.5:
                    past = (self.now - timedelta(days=365)).isoformat()
                    t.waiver.expires_at = past
                    break

        return case

    # ─── invariants ───
    def _check_t0_invariance(self, case: ReleaseCase) -> None:
        """I1: any case with at least one not-seated T0 tumbler must
        produce HOLD or ROLLBACK_OR_CONTAIN — never SHIP-class."""
        report = decide(case, now=self.now)
        for f in report.findings:
            if is_t0(f.tier) and not f.seated:
                if _SEVERITY[report.verdict] < _SEVERITY[Verdict.HOLD]:
                    self.report.violations.append(FuzzViolation(
                        invariant="I1: T0 invariance",
                        case_id=case.id,
                        description=(f"T0 tumbler {f.tier}/{f.invariant} "
                                     f"not seated, verdict={report.verdict.value}"),
                        perturbation=self._describe_case(case),
                        expected_max_severity=_SEVERITY[Verdict.HOLD],
                        actual_severity=_SEVERITY[report.verdict],
                    ))
                return

    def _check_waiver_t0_invariance(self, case: ReleaseCase) -> None:
        """I2: a T0 tumbler with status=WAIVED must produce HOLD or worse."""
        report = decide(case, now=self.now)
        for t in case.tumblers:
            if is_t0(t.tier) and t.status == Status.WAIVED:
                if _SEVERITY[report.verdict] < _SEVERITY[Verdict.HOLD]:
                    self.report.violations.append(FuzzViolation(
                        invariant="I2: waiver T0 invariance",
                        case_id=case.id,
                        description=(f"T0 WAIVED tumbler {t.tier}/{t.invariant} "
                                     f"yet verdict={report.verdict.value}"),
                        perturbation=self._describe_case(case),
                        expected_max_severity=_SEVERITY[Verdict.HOLD],
                        actual_severity=_SEVERITY[report.verdict],
                    ))
                return

    def _check_expiry_invariance(self, case: ReleaseCase) -> None:
        """I3: if any seated-via-WAIVED tumbler has expired waiver, verdict
        must be HOLD or worse (the seating wasn't legitimate)."""
        report = decide(case, now=self.now)
        for t, f in zip(case.tumblers, report.findings):
            if (t.status == Status.WAIVED and t.waiver
                    and not is_t0(t.tier)):
                try:
                    expiry_str = t.waiver.expires_at.replace("Z", "+00:00")
                    expiry = datetime.fromisoformat(expiry_str)
                    if expiry.tzinfo is None:
                        expiry = expiry.replace(tzinfo=timezone.utc)
                except ValueError:
                    expiry = self.now - timedelta(days=1)  # treat unparseable as expired
                if expiry <= self.now and f.seated:
                    self.report.violations.append(FuzzViolation(
                        invariant="I3: expiry invariance",
                        case_id=case.id,
                        description=(f"expired waiver on {t.tier} treated as seated, "
                                     f"verdict={report.verdict.value}"),
                        perturbation=self._describe_case(case),
                        expected_max_severity=_SEVERITY[Verdict.HOLD],
                        actual_severity=_SEVERITY[report.verdict],
                    ))
                    return

    def _check_order_invariance(self, base: ReleaseCase, n: int) -> None:
        """I4: shuffling tumbler list must not change verdict."""
        baseline = decide(base, now=self.now).verdict
        for i in range(n):
            shuffled = copy.deepcopy(base)
            self.rng.shuffle(shuffled.tumblers)
            shuffled.id = f"{base.id}#shuffle-{i}"
            v = decide(shuffled, now=self.now).verdict
            self.report.perturbations_run += 1
            if v != baseline:
                self.report.violations.append(FuzzViolation(
                    invariant="I4: order invariance",
                    case_id=shuffled.id,
                    description=f"shuffle changed verdict {baseline.value} → {v.value}",
                    perturbation=self._describe_case(shuffled),
                    expected_max_severity=_SEVERITY[baseline],
                    actual_severity=_SEVERITY[v],
                ))

    def _check_na_invariance(self, base: ReleaseCase, n: int) -> None:
        """I5: adding NOT_APPLICABLE tumblers can never worsen the verdict."""
        baseline = decide(base, now=self.now).verdict
        for i in range(n):
            extended = copy.deepcopy(base)
            extended.id = f"{base.id}#na-{i}"
            extended.tumblers.append(TumblerStatus(
                tier=f"T{self.rng.randint(0, 7)}-99",
                invariant="synthetic-NA",
                status=Status.NOT_APPLICABLE,
            ))
            v = decide(extended, now=self.now).verdict
            self.report.perturbations_run += 1
            if _SEVERITY[v] > _SEVERITY[baseline]:
                self.report.violations.append(FuzzViolation(
                    invariant="I5: NA invariance",
                    case_id=extended.id,
                    description=(f"adding NA tumbler worsened verdict "
                                 f"{baseline.value} → {v.value}"),
                    perturbation=self._describe_case(extended),
                    expected_max_severity=_SEVERITY[baseline],
                    actual_severity=_SEVERITY[v],
                ))

    @staticmethod
    def _describe_case(case: ReleaseCase) -> str:
        parts = [f"id={case.id}", f"risk={case.risk}"]
        for t in case.tumblers:
            parts.append(f"{t.tier}={t.status.value}")
        return ", ".join(parts)


def fuzz_case(base_case: ReleaseCase, *,
              n_perturbations: int = 200,
              seed: int = 0,
              now: datetime | None = None) -> FuzzReport:
    """One-shot helper: fuzz a case and return the report."""
    return CaseFuzzer(base_case, now=now, seed=seed).fuzz(n_perturbations)
