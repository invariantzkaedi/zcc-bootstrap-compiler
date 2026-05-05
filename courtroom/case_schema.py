"""case_schema.py — declarative JSON case definitions.

Lets release cases be defined in YAML/JSON instead of constructed in
Python. Schema (one file = one case):

    {
      "id": "rel-2026-05-05.1",
      "risk": "medium",
      "tumblers": [
        {
          "tier": "T0-01",
          "invariant": "Evidence Before Belief",
          "status": "seated",
          "evidence_ids": ["ev1"],
          "note": ""
        },
        {
          "tier": "T2-04",
          "invariant": "Patch Safely",
          "status": "waived",
          "waiver": {
            "reason": "known limitation",
            "owner": "zkaedi",
            "expires_at": "2026-06-01T00:00:00+00:00",
            "repayment": "HYG-001",
            "approved_by": "release-marshal"
          }
        }
      ],
      "evidence": [
        {
          "id": "ev1",
          "type": "cmd",
          "claim_supported": "tumbler seated",
          "command": "pytest -q",
          "result": "1 passed",
          "artifact": "ci://run/123",
          "quality": 5
        }
      ]
    }

Loader is strict: missing required fields raise CaseSchemaError with
a path into the document.
"""
from __future__ import annotations

import json
from pathlib import Path

from invariant_compiler import (
    Evidence, ReleaseCase, Status, TumblerStatus, Waiver,
)
from tier_parser import parse_tier, TierParseError


class CaseSchemaError(ValueError):
    """Raised when a case JSON cannot be parsed."""


_VALID_RISKS = {"low", "medium", "high", "critical", "deployed_breach"}


def _require(obj, key, path):
    if not isinstance(obj, dict):
        raise CaseSchemaError(f"{path}: expected object, got {type(obj).__name__}")
    if key not in obj:
        raise CaseSchemaError(f"{path}: missing required key '{key}'")
    return obj[key]


def _opt(obj, key, default):
    if not isinstance(obj, dict):
        return default
    return obj.get(key, default)


def _parse_evidence(d, path) -> Evidence:
    return Evidence(
        id=_require(d, "id", path),
        type=_require(d, "type", path),
        claim_supported=_require(d, "claim_supported", path),
        artifact=_opt(d, "artifact", None),
        command=_opt(d, "command", None),
        result=_opt(d, "result", None),
        quality=int(_opt(d, "quality", 0)),
    )


def _parse_waiver(d, path) -> Waiver:
    return Waiver(
        reason=_require(d, "reason", path),
        owner=_require(d, "owner", path),
        expires_at=_require(d, "expires_at", path),
        repayment=_require(d, "repayment", path),
        approved_by=_require(d, "approved_by", path),
    )


def _parse_status(s, path) -> Status:
    try:
        return Status(s)
    except (ValueError, TypeError):
        valid = ", ".join(st.value for st in Status)
        raise CaseSchemaError(
            f"{path}: invalid status {s!r}. Must be one of: {valid}"
        )


def _parse_tumbler(d, path) -> TumblerStatus:
    tier = _require(d, "tier", path)
    try:
        parse_tier(tier)
    except TierParseError as ex:
        raise CaseSchemaError(f"{path}/tier: {ex}") from ex

    invariant = _require(d, "invariant", path)
    status = _parse_status(_require(d, "status", path), f"{path}/status")
    evidence_ids = list(_opt(d, "evidence_ids", []))
    note = _opt(d, "note", "")
    waiver_d = _opt(d, "waiver", None)
    waiver = _parse_waiver(waiver_d, f"{path}/waiver") if waiver_d else None

    return TumblerStatus(
        tier=tier,
        invariant=invariant,
        status=status,
        evidence_ids=evidence_ids,
        waiver=waiver,
        note=note,
    )


def parse_case(d: dict, *, path: str = "$") -> ReleaseCase:
    """Parse a dict (already JSON-decoded) into a ReleaseCase."""
    if not isinstance(d, dict):
        raise CaseSchemaError(f"{path}: expected object, got {type(d).__name__}")

    case_id = _require(d, "id", path)
    risk = _require(d, "risk", path)
    if risk not in _VALID_RISKS:
        raise CaseSchemaError(
            f"{path}/risk: {risk!r} not in {sorted(_VALID_RISKS)}"
        )

    tumblers_raw = _require(d, "tumblers", path)
    if not isinstance(tumblers_raw, list):
        raise CaseSchemaError(f"{path}/tumblers: expected list")
    if not tumblers_raw:
        raise CaseSchemaError(f"{path}/tumblers: must be non-empty")
    tumblers = [_parse_tumbler(t, f"{path}/tumblers[{i}]")
                for i, t in enumerate(tumblers_raw)]

    evidence_raw = _opt(d, "evidence", [])
    if not isinstance(evidence_raw, list):
        raise CaseSchemaError(f"{path}/evidence: expected list")
    evidence_list = [_parse_evidence(e, f"{path}/evidence[{i}]")
                     for i, e in enumerate(evidence_raw)]
    evidence = {e.id: e for e in evidence_list}

    # Cross-validate: every evidence_id referenced in a tumbler must exist.
    for i, t in enumerate(tumblers):
        for ev_id in t.evidence_ids:
            if ev_id not in evidence:
                raise CaseSchemaError(
                    f"{path}/tumblers[{i}]/evidence_ids: "
                    f"referenced evidence '{ev_id}' not found in evidence list"
                )

    return ReleaseCase(id=case_id, risk=risk, tumblers=tumblers, evidence=evidence)


def load_case(path: str) -> ReleaseCase:
    """Load and parse a case JSON file."""
    p = Path(path)
    if not p.exists():
        raise CaseSchemaError(f"case file not found: {p}")
    try:
        d = json.loads(p.read_text(encoding="utf-8"))
    except json.JSONDecodeError as ex:
        raise CaseSchemaError(f"{p}: malformed JSON ({ex})") from ex
    return parse_case(d)
