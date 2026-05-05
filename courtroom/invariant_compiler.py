"""invariant_compiler.py — gate-discipline-clean rewrite.

Replaces the early-return decide() with a full-pass accumulator. Verdict starts
at SHIP and is monotonically downgraded as each tumbler is inspected. T0/P0
violations are detected before any non-T0 WAIVED can promote the verdict to
SHIP_WITH_RISK_ACCEPTANCE. Waiver expiry is compared against current UTC time.
LOCKED, PARTIAL, and UNKNOWN are distinguished.

Verdict lattice (higher beats lower; final verdict = highest violation seen):

    SHIP                                       (no violations)
    SHIP_WITH_RISK_ACCEPTANCE                  (only valid non-T0 waivers seated otherwise)
    HOLD                                       (any unseated tumbler at any tier)
    ROLLBACK_OR_CONTAIN                        (T0/P0 violation in deployed state — see note)

ROLLBACK_OR_CONTAIN is reserved for cases where the case itself signals
post-deploy state (case.risk == "deployed_breach"). Pre-deploy, T0/P0
violations escalate to HOLD, never SHIP-class.
"""
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum

from tier_parser import is_t0, required_quality_for_tier  # noqa: F401


class Status(str, Enum):
    UNKNOWN = "unknown"          # never inspected
    LOCKED = "locked"            # claim raised, no work started
    PARTIAL = "partial"          # work started, evidence incomplete
    SEATED = "seated"            # evidence meets tier threshold
    WAIVED = "waived"            # accepted as risk via valid waiver
    NOT_APPLICABLE = "not_applicable"


class Verdict(str, Enum):
    SHIP = "SHIP"
    SHIP_WITH_RISK_ACCEPTANCE = "SHIP_WITH_RISK_ACCEPTANCE"
    HOLD = "HOLD"
    ROLLBACK_OR_CONTAIN = "ROLLBACK_OR_CONTAIN"


# Severity ordering — only ever monotonically increase.
_SEVERITY = {
    Verdict.SHIP: 0,
    Verdict.SHIP_WITH_RISK_ACCEPTANCE: 1,
    Verdict.HOLD: 2,
    Verdict.ROLLBACK_OR_CONTAIN: 3,
}


@dataclass
class Evidence:
    id: str
    type: str
    claim_supported: str
    artifact: str | None = None
    command: str | None = None
    result: str | None = None
    quality: int = 0


@dataclass
class Waiver:
    reason: str
    owner: str
    expires_at: str       # ISO-8601, MUST be parseable
    repayment: str
    approved_by: str


@dataclass
class TumblerStatus:
    tier: str
    invariant: str
    status: Status
    evidence_ids: list[str] = field(default_factory=list)
    waiver: Waiver | None = None
    note: str = ""


@dataclass
class ReleaseCase:
    id: str
    risk: str
    tumblers: list[TumblerStatus]
    evidence: dict[str, Evidence]


@dataclass
class TumblerFinding:
    """One row of forensic record per tumbler. Always emitted."""
    tier: str
    invariant: str
    status: Status
    seated: bool
    reason: str          # human-readable why this row contributed what it did
    contribution: Verdict


@dataclass
class DecisionReport:
    verdict: Verdict
    findings: list[TumblerFinding]
    summary: str


def has_valid_evidence(tumbler: TumblerStatus, case: ReleaseCase,
                       min_quality: int) -> bool:
    if not tumbler.evidence_ids:
        return False
    for evidence_id in tumbler.evidence_ids:
        evidence = case.evidence.get(evidence_id)
        if evidence and evidence.quality >= min_quality:
            return True
    return False


def has_valid_waiver(tumbler: TumblerStatus, now: datetime | None = None) -> bool:
    waiver = tumbler.waiver
    if not waiver:
        return False
    # Shape check.
    if not all([
        waiver.reason.strip(),
        waiver.owner.strip(),
        waiver.expires_at.strip(),
        waiver.repayment.strip(),
        waiver.approved_by.strip(),
    ]):
        return False
    # Expiry check — the bug the original missed.
    now = now or datetime.now(timezone.utc)
    try:
        expiry = datetime.fromisoformat(waiver.expires_at.replace("Z", "+00:00"))
        if expiry.tzinfo is None:
            expiry = expiry.replace(tzinfo=timezone.utc)
    except ValueError:
        return False     # unparseable expiry is not a valid waiver
    return expiry > now


def _worse(a: Verdict, b: Verdict) -> Verdict:
    return a if _SEVERITY[a] >= _SEVERITY[b] else b


def decide(case: ReleaseCase, now: datetime | None = None) -> DecisionReport:
    findings: list[TumblerFinding] = []
    verdict = Verdict.SHIP

    for tumbler in case.tumblers:
        min_q = required_quality_for_tier(tumbler.tier)
        contribution = Verdict.SHIP
        seated = False
        reason = ""

        if tumbler.status == Status.NOT_APPLICABLE:
            seated = True
            reason = "not applicable to this change"

        elif tumbler.status == Status.SEATED:
            if has_valid_evidence(tumbler, case, min_q):
                seated = True
                reason = f"seated with evidence quality >= {min_q}"
            else:
                seated = False
                reason = f"SEATED claimed but no evidence at quality >= {min_q}"
                contribution = Verdict.HOLD

        elif tumbler.status == Status.WAIVED:
            if is_t0(tumbler.tier):
                reason = "T0/P0 may not be waived"
                contribution = Verdict.HOLD
            elif not has_valid_waiver(tumbler, now=now):
                reason = "waiver missing fields or expired"
                contribution = Verdict.HOLD
            else:
                seated = True
                reason = "waived with valid non-expired waiver"
                contribution = Verdict.SHIP_WITH_RISK_ACCEPTANCE

        elif tumbler.status == Status.UNKNOWN:
            reason = "tumbler never inspected"
            contribution = Verdict.HOLD

        elif tumbler.status == Status.LOCKED:
            reason = "tumbler raised but work not started"
            contribution = Verdict.HOLD

        elif tumbler.status == Status.PARTIAL:
            reason = "evidence collection in progress, threshold not yet met"
            contribution = Verdict.HOLD

        else:
            reason = f"unknown status enum value: {tumbler.status!r}"
            contribution = Verdict.HOLD

        findings.append(TumblerFinding(
            tier=tumbler.tier,
            invariant=tumbler.invariant,
            status=tumbler.status,
            seated=seated,
            reason=reason,
            contribution=contribution,
        ))
        verdict = _worse(verdict, contribution)

    # Optional explicit rollback escalation if the case is flagged post-deploy.
    if case.risk == "deployed_breach" and verdict == Verdict.HOLD:
        if any(is_t0(f.tier) and not f.seated for f in findings):
            verdict = Verdict.ROLLBACK_OR_CONTAIN

    seated_count = sum(1 for f in findings if f.seated)
    summary = (f"{seated_count}/{len(findings)} tumblers seated "
               f"→ {verdict.value}")

    return DecisionReport(verdict=verdict, findings=findings, summary=summary)
