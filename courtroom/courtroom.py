"""courtroom.py — Release Courtroom layer for the Reverse Tumbler protocol.

Sits on top of invariant_compiler.decide(). Four roles:

    Prosecutor      — case + DecisionReport → ProsecutionBrief (charges)
    DefenseEngineer — ProsecutionBrief → DefensePlan (remediations)
    LedgerNotary    — append-only hash-chained record of session events
    ReleaseMarshal  — synthesizes final verdict, signs the ledger close

Load-bearing invariant: the Marshal MUST defer to decide(). No narrative,
agreement, or eloquence from the Prosecutor/Defense can override a HOLD.
The Marshal's job is packaging, not adjudication. decide() adjudicates.

The Notary's hash chain is real (sha256(prev || canonical-json(entry))).
A tampered ledger fails verify().
"""
from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum

from invariant_compiler import (
    DecisionReport,
    Evidence,
    ReleaseCase,
    Status,
    Verdict,
    Waiver,
    decide,
)
from tier_parser import is_t0, severity_for


# ============================================================
# DOCUMENTS produced by each role
# ============================================================
class RemediationKind(str, Enum):
    PROVIDE_EVIDENCE = "provide_evidence"
    REQUEST_WAIVER = "request_waiver"
    MARK_NOT_APPLICABLE = "mark_not_applicable"
    UNREMEDIABLE = "unremediable"     # T0 in violation; only path is fix the underlying issue


@dataclass
class Charge:
    """One count in the prosecution brief — a tumbler that is not seated."""
    tier: str
    invariant: str
    status: Status
    reason: str
    severity: int      # higher = blocks ship harder. T0=100, T1=70, T2=50, T3=40, T4-T7 down from 30.

    @staticmethod
    def severity_for(tier: str) -> int:
        return severity_for(tier)


@dataclass
class ProsecutionBrief:
    case_id: str
    charges: list[Charge]
    summary: str

    @property
    def is_empty(self) -> bool:
        return len(self.charges) == 0

    @property
    def has_t0(self) -> bool:
        return any(is_t0(c.tier) for c in self.charges)


@dataclass
class RemediationAction:
    """One step the Defense proposes to seat a tumbler."""
    kind: RemediationKind
    target_tier: str
    target_invariant: str
    required_artifact: str        # what shape of evidence/waiver/NA-justification
    validation_hint: str          # how to prove it lands
    min_quality: int              # for evidence kind; -1 otherwise
    estimated_cost: int           # rough effort score; for ordering


@dataclass
class DefensePlan:
    case_id: str
    actions: list[RemediationAction]
    note: str

    @property
    def total_cost(self) -> int:
        return sum(a.estimated_cost for a in self.actions)

    @property
    def has_unremediable(self) -> bool:
        return any(a.kind == RemediationKind.UNREMEDIABLE for a in self.actions)


# ============================================================
# LEDGER — append-only, hash-chained
# ============================================================
class EntryKind(str, Enum):
    INDICTMENT = "indictment"
    DEFENSE_PLAN = "defense_plan"
    EVIDENCE_ADMITTED = "evidence_admitted"
    EVIDENCE_REJECTED = "evidence_rejected"
    WAIVER_ADMITTED = "waiver_admitted"
    WAIVER_REJECTED = "waiver_rejected"
    RECOMPUTE = "recompute"
    VERDICT = "verdict"


@dataclass
class LedgerEntry:
    seq: int
    timestamp: str
    kind: EntryKind
    actor: str
    payload: dict
    prev_hash: str
    entry_hash: str

    def canonical(self) -> bytes:
        """Stable JSON for hashing — entry_hash itself excluded.

        STABILITY CONTRACT: The shape of this dict and the JSON
        serialization options ARE the on-disk format. Any change to
        the dict's keys, key order policy (sort_keys=True), or the
        separators bumps LEDGER_FORMAT_VERSION below and invalidates
        every existing ledger. Don't add fields without a migration plan.
        """
        d = {
            "v": LEDGER_FORMAT_VERSION,
            "seq": self.seq,
            "timestamp": self.timestamp,
            "kind": self.kind.value,
            "actor": self.actor,
            "payload": self.payload,
            "prev_hash": self.prev_hash,
        }
        return json.dumps(d, sort_keys=True, separators=(",", ":")).encode()


GENESIS = "0" * 64
LEDGER_FORMAT_VERSION = 1


class ReleaseLedger:
    """Append-only ledger with sha256 hash chain. Tampering breaks verify()."""

    def __init__(self):
        self._entries: list[LedgerEntry] = []

    def append(self, kind: EntryKind, actor: str,
               payload: dict, now: datetime | None = None) -> LedgerEntry:
        now = now or datetime.now(timezone.utc)
        prev_hash = self._entries[-1].entry_hash if self._entries else GENESIS
        entry = LedgerEntry(
            seq=len(self._entries),
            timestamp=now.isoformat(),
            kind=kind,
            actor=actor,
            payload=payload,
            prev_hash=prev_hash,
            entry_hash="",   # filled below
        )
        entry.entry_hash = hashlib.sha256(entry.canonical()).hexdigest()
        self._entries.append(entry)
        return entry

    def verify(self) -> tuple[bool, str]:
        """Walk the chain. Returns (ok, reason)."""
        prev = GENESIS
        for entry in self._entries:
            if entry.prev_hash != prev:
                return False, f"seq={entry.seq} prev_hash mismatch"
            recomputed = hashlib.sha256(entry.canonical()).hexdigest()
            if recomputed != entry.entry_hash:
                return False, f"seq={entry.seq} entry_hash mismatch (tampered)"
            prev = entry.entry_hash
        return True, "ok"

    @property
    def entries(self) -> list[LedgerEntry]:
        return list(self._entries)

    @property
    def head_hash(self) -> str:
        return self._entries[-1].entry_hash if self._entries else GENESIS


# ============================================================
# ROLES
# ============================================================
class Prosecutor:
    """Adversarial — assume unsafe until proven safe. Indicts unseated tumblers."""

    name = "Prosecutor"

    def indict(self, case: ReleaseCase,
               report: DecisionReport) -> ProsecutionBrief:
        charges = []
        for finding in report.findings:
            if not finding.seated:
                charges.append(Charge(
                    tier=finding.tier,
                    invariant=finding.invariant,
                    status=finding.status,
                    reason=finding.reason,
                    severity=Charge.severity_for(finding.tier),
                ))
        # Most severe first.
        charges.sort(key=lambda c: -c.severity)

        if not charges:
            summary = "No charges — all tumblers seated."
        else:
            t0_count = sum(1 for c in charges if is_t0(c.tier))
            summary = (f"{len(charges)} charges filed "
                       f"({t0_count} T0/P0). "
                       f"Highest severity: {charges[0].tier} {charges[0].invariant}.")
        return ProsecutionBrief(case_id=case.id, charges=charges, summary=summary)


class DefenseEngineer:
    """Proposes the smallest evidence-backed action per charge."""

    name = "DefenseEngineer"

    def plan(self, brief: ProsecutionBrief) -> DefensePlan:
        actions = []
        for charge in brief.charges:
            actions.append(self._action_for(charge))
        cost = sum(a.estimated_cost for a in actions)
        if brief.is_empty:
            note = "Nothing to remediate."
        elif any(a.kind == RemediationKind.UNREMEDIABLE for a in actions):
            note = ("T0/P0 charge present — cannot be waived. The underlying "
                    "issue must be fixed and evidence produced before ship.")
        else:
            note = f"Plan total estimated cost: {cost}."
        return DefensePlan(case_id=brief.case_id, actions=actions, note=note)

    def _action_for(self, charge: Charge) -> RemediationAction:
        from tier_parser import parse_tier

        # T0/P0 cannot be waived — only fix-and-evidence is admissible.
        if is_t0(charge.tier):
            if charge.status == Status.SEATED:
                # SEATED but evidence quality below threshold — replace evidence.
                return RemediationAction(
                    kind=RemediationKind.PROVIDE_EVIDENCE,
                    target_tier=charge.tier,
                    target_invariant=charge.invariant,
                    required_artifact=("higher-quality evidence: command output, "
                                       "test result, or commit SHA"),
                    validation_hint="re-run decide(case); finding.seated must be True",
                    min_quality=4,
                    estimated_cost=2,
                )
            return RemediationAction(
                kind=RemediationKind.UNREMEDIABLE,
                target_tier=charge.tier,
                target_invariant=charge.invariant,
                required_artifact=("fix the underlying T0/P0 violation in the change, "
                                   "then provide quality>=4 evidence"),
                validation_hint=("change set must address the invariant; "
                                 "no waiver path exists for T0/P0"),
                min_quality=4,
                estimated_cost=10,
            )

        # Non-T0: evidence path is preferred; waiver path is fallback.
        tier_n = parse_tier(charge.tier).n
        if charge.status in (Status.UNKNOWN, Status.LOCKED, Status.PARTIAL):
            min_q = 5 if tier_n == 3 else (4 if tier_n == 6 else 3)
            return RemediationAction(
                kind=RemediationKind.PROVIDE_EVIDENCE,
                target_tier=charge.tier,
                target_invariant=charge.invariant,
                required_artifact=(f"evidence at quality >= {min_q}: "
                                   "test output, runtime observation, or referenced commit"),
                validation_hint="finding.seated == True after recompute",
                min_quality=min_q,
                estimated_cost=3,
            )

        if charge.status == Status.SEATED:
            min_q = 5 if tier_n == 3 else (4 if tier_n == 6 else 3)
            return RemediationAction(
                kind=RemediationKind.PROVIDE_EVIDENCE,
                target_tier=charge.tier,
                target_invariant=charge.invariant,
                required_artifact=f"replace weak evidence with quality >= {min_q}",
                validation_hint="re-run decide(); seated must hold",
                min_quality=min_q,
                estimated_cost=2,
            )

        if charge.status == Status.WAIVED:
            # WAIVED but invalid waiver shape or expired.
            return RemediationAction(
                kind=RemediationKind.REQUEST_WAIVER,
                target_tier=charge.tier,
                target_invariant=charge.invariant,
                required_artifact=("complete waiver: reason, owner, expires_at "
                                   "(future ISO-8601), repayment ticket, approver"),
                validation_hint="has_valid_waiver(tumbler, now=NOW) must return True",
                min_quality=-1,
                estimated_cost=2,
            )

        return RemediationAction(
            kind=RemediationKind.UNREMEDIABLE,
            target_tier=charge.tier,
            target_invariant=charge.invariant,
            required_artifact="unrecognized status; investigate",
            validation_hint="manual triage",
            min_quality=-1,
            estimated_cost=20,
        )


class LedgerNotary:
    """Validates evidence/waiver shape, writes ledger entries, rejects unsupported claims."""

    name = "LedgerNotary"

    def __init__(self, ledger: ReleaseLedger):
        self.ledger = ledger

    def admit_evidence(self, ev: Evidence,
                       now: datetime | None = None) -> tuple[bool, str]:
        if not ev.id.strip() or not ev.type.strip():
            self.ledger.append(EntryKind.EVIDENCE_REJECTED, self.name,
                               {"id": ev.id, "reason": "missing id or type"}, now=now)
            return False, "missing id or type"
        if ev.quality < 1:
            self.ledger.append(EntryKind.EVIDENCE_REJECTED, self.name,
                               {"id": ev.id, "reason": "quality < 1"}, now=now)
            return False, "quality < 1"
        if not (ev.command or ev.result or ev.artifact):
            self.ledger.append(EntryKind.EVIDENCE_REJECTED, self.name,
                               {"id": ev.id,
                                "reason": "no command, result, or artifact reference"}, now=now)
            return False, "no concrete artifact"
        self.ledger.append(EntryKind.EVIDENCE_ADMITTED, self.name,
                           {"id": ev.id, "quality": ev.quality, "type": ev.type}, now=now)
        return True, "admitted"

    def admit_waiver(self, waiver: Waiver, target_tier: str,
                     now: datetime | None = None) -> tuple[bool, str]:
        if is_t0(target_tier):
            self.ledger.append(EntryKind.WAIVER_REJECTED, self.name,
                               {"target_tier": target_tier,
                                "reason": "T0/P0 may not be waived"}, now=now)
            return False, "T0/P0 may not be waived"
        # Shape.
        if not all([waiver.reason.strip(), waiver.owner.strip(),
                    waiver.expires_at.strip(), waiver.repayment.strip(),
                    waiver.approved_by.strip()]):
            self.ledger.append(EntryKind.WAIVER_REJECTED, self.name,
                               {"target_tier": target_tier,
                                "reason": "missing required fields"}, now=now)
            return False, "missing required fields"
        # Expiry — same logic as has_valid_waiver, but explicit here for the rejection record.
        now_dt = now or datetime.now(timezone.utc)
        try:
            expiry = datetime.fromisoformat(waiver.expires_at.replace("Z", "+00:00"))
            if expiry.tzinfo is None:
                expiry = expiry.replace(tzinfo=timezone.utc)
        except ValueError:
            self.ledger.append(EntryKind.WAIVER_REJECTED, self.name,
                               {"target_tier": target_tier,
                                "reason": "expires_at not ISO-8601"}, now=now)
            return False, "expires_at not ISO-8601"
        if expiry <= now_dt:
            self.ledger.append(EntryKind.WAIVER_REJECTED, self.name,
                               {"target_tier": target_tier,
                                "reason": "expired",
                                "expires_at": waiver.expires_at}, now=now)
            return False, "expired"
        self.ledger.append(EntryKind.WAIVER_ADMITTED, self.name,
                           {"target_tier": target_tier,
                            "owner": waiver.owner,
                            "expires_at": waiver.expires_at,
                            "repayment": waiver.repayment}, now=now)
        return True, "admitted"


class ReleaseMarshal:
    """Synthesizes final verdict from the most recent compiler decision.

    Crucially: the Marshal does NOT make the verdict. decide() does. The
    Marshal packages and signs the closure. If the Marshal disagrees with
    decide(), the Marshal loses. This is the only way the Courtroom layer
    avoids becoming theater.
    """

    name = "ReleaseMarshal"

    def adjudicate(self, case: ReleaseCase, ledger: ReleaseLedger,
                   now: datetime | None = None) -> Verdict:
        report = decide(case, now=now)
        # Confirm the ledger has at least an indictment for this case.
        case_indicted = any(
            e.kind == EntryKind.INDICTMENT and e.payload.get("case_id") == case.id
            for e in ledger.entries
        )
        ledger.append(
            EntryKind.VERDICT, self.name,
            {
                "case_id": case.id,
                "verdict": report.verdict.value,
                "summary": report.summary,
                "indicted": case_indicted,
                "seated_count": sum(1 for f in report.findings if f.seated),
                "total_count": len(report.findings),
            },
            now=now,
        )
        return report.verdict


# ============================================================
# SESSION — orchestrates the duo + clerk + marshal
# ============================================================
@dataclass
class CourtSession:
    case: ReleaseCase
    ledger: ReleaseLedger = field(default_factory=ReleaseLedger)

    def __post_init__(self):
        self.prosecutor = Prosecutor()
        self.defense = DefenseEngineer()
        self.notary = LedgerNotary(self.ledger)
        self.marshal = ReleaseMarshal()

    def indict(self, now: datetime | None = None) -> ProsecutionBrief:
        report = decide(self.case, now=now)
        brief = self.prosecutor.indict(self.case, report)
        self.ledger.append(EntryKind.INDICTMENT, self.prosecutor.name, {
            "case_id": self.case.id,
            "charges": [
                {"tier": c.tier, "invariant": c.invariant,
                 "severity": c.severity, "reason": c.reason}
                for c in brief.charges
            ],
            "summary": brief.summary,
        }, now=now)
        return brief

    def plan_defense(self, brief: ProsecutionBrief,
                     now: datetime | None = None) -> DefensePlan:
        plan = self.defense.plan(brief)
        self.ledger.append(EntryKind.DEFENSE_PLAN, self.defense.name, {
            "case_id": self.case.id,
            "total_cost": plan.total_cost,
            "actions": [
                {"kind": a.kind.value, "target_tier": a.target_tier,
                 "invariant": a.target_invariant,
                 "required_artifact": a.required_artifact,
                 "estimated_cost": a.estimated_cost}
                for a in plan.actions
            ],
        }, now=now)
        return plan

    def recompute(self, now: datetime | None = None) -> DecisionReport:
        report = decide(self.case, now=now)
        self.ledger.append(EntryKind.RECOMPUTE, "system", {
            "case_id": self.case.id,
            "verdict": report.verdict.value,
            "summary": report.summary,
        }, now=now)
        return report

    def adjudicate(self, now: datetime | None = None) -> Verdict:
        return self.marshal.adjudicate(self.case, self.ledger, now=now)
