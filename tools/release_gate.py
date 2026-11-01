#!/usr/bin/env python3
"""ZCC Release Gate — Courtroom as Gate-Zero."""

import argparse
import sys
from pathlib import Path

# ensure courtroom is in path before importing
zcc_dir = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(zcc_dir / "courtroom"))

from case_schema import CaseSchemaError, load_case, parse_case
from courtroom import CourtSession
from invariant_compiler import ReleaseCase, Verdict


def _parse_markdown_ticket(p: Path) -> dict:
    import hashlib
    import re

    text = p.read_text(encoding="utf-8").strip()
    if not text:
        raise CaseSchemaError(f"{p}: empty markdown ticket")

    # Fail closed if explicit failure/block markers appear anywhere in ticket.
    if re.search(r"\b(CATASTROPHIC\s+FAIL|DO\s+NOT\s+SHIP|STATUS:\s*(FAIL|RED|BLOCKED|HOLD|INCONCLUSIVE)|\[(FAIL|UNVERIFIED|BLOCKED|INCONCLUSIVE)\])\b", text, re.IGNORECASE):
        raise CaseSchemaError(f"{p}: ticket contains explicit failure/unverified marker")

    claims: list[tuple[str, str]] = []
    artifacts: list[str] = []
    in_artifacts = False

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if line.lower().startswith("**artifacts"):
            in_artifacts = True
            continue
        if in_artifacts:
            if line.startswith("**") and not line.startswith("-"):
                in_artifacts = False
            else:
                m_art = re.match(r"^-\s+`([^`]+)`\s*$", line)
                if m_art:
                    artifacts.append(m_art.group(1).strip())
                    continue

        m_claim = re.match(r"^-\s+(.+?)\s+\*\*\[([A-Za-z0-9_-]+)\]\*\*\s*$", line)
        if m_claim:
            claims.append((m_claim.group(1).strip(), m_claim.group(2).strip().upper()))

    if not claims:
        raise CaseSchemaError(f"{p}: no structured **[VERIFIED]** claims found in markdown ticket")
    if not artifacts:
        raise CaseSchemaError(f"{p}: no structured artifact paths found in markdown ticket")

    evidence_list = []
    ev_ids = []
    for idx, rel_path in enumerate(artifacts, start=1):
        art_file = (zcc_dir / rel_path).resolve()
        if not art_file.is_file():
            raise CaseSchemaError(f"{p}: referenced artifact does not exist on disk: {rel_path}")
        digest = hashlib.sha256(art_file.read_bytes()).hexdigest()
        ev_id = f"ev-{idx}"
        ev_ids.append(ev_id)
        evidence_list.append({
            "id": ev_id,
            "type": "artifact_sha256",
            "claim_supported": f"on-disk artifact {rel_path} verified",
            "artifact": rel_path,
            "result": f"sha256:{digest}",
            "quality": 5,
        })

    tumblers = []
    for idx, (claim_desc, status_tok) in enumerate(claims, start=1):
        tier = f"T0-0{idx}" if idx <= 2 else f"T{min(idx - 2, 7)}-01"
        st = "seated" if status_tok in ("VERIFIED", "PASS", "SEATED") else "locked"
        tumblers.append({
            "tier": tier,
            "invariant": claim_desc,
            "status": st,
            "evidence_ids": ev_ids if st == "seated" else [],
        })

    return {
        "id": p.stem,
        "risk": "medium",
        "tumblers": tumblers,
        "evidence": evidence_list,
    }


def load_case_from_ticket(ticket_path: str) -> ReleaseCase:
    """Convert ZCC verification JSON or structured markdown into a ReleaseCase (fail-closed)."""
    p = Path(ticket_path)
    if not p.is_file():
        raise CaseSchemaError(f"ticket file not found: {p}")
    if p.suffix == ".json":
        return load_case(str(p))
    if p.suffix == ".md":
        data = _parse_markdown_ticket(p)
        return parse_case(data, path=str(p))
    raise CaseSchemaError(f"{p}: unsupported ticket extension {p.suffix!r} (expected .json or .md)")


def gate_zero(ticket_path: str, ledger_path: str = "releases/ledger.jsonl") -> int:
    """Return 0 only on SHIP. Everything else blocks merge/release."""
    try:
        case = load_case_from_ticket(ticket_path)
    except Exception as exc:
        print(f"\n🔱 ZKAEDI RELEASE GATE\n⛔ GATE FAILED — {exc}", file=sys.stderr)
        return 3

    session = CourtSession(case)
    session.indict()
    report = session.recompute()

    print("\n🔱 ZKAEDI RELEASE GATE")
    print(f"Case: {case.id}")
    print(f"Verdict: {report.verdict}")
    print(f"Summary: {report.summary}\n")

    if report.verdict in (Verdict.SHIP, "SHIP"):
        print("✅ GATE PASSED — SHIP AUTHORIZED")
        return 0
    else:
        print("⛔ GATE FAILED — HOLD or worse. Fix tumblers before release.")
        return 2 if report.verdict in (Verdict.SHIP_WITH_RISK_ACCEPTANCE, "SHIP_WITH_RISK_ACCEPTANCE") else 3


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("ticket", help="Verification ticket / case file")
    parser.add_argument("--ledger", default="releases/ledger.jsonl")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    sys.exit(gate_zero(args.ticket, args.ledger))


if __name__ == "__main__":
    main()
