"""Tests for case_schema.py — declarative case loading."""
import json
import tempfile
from datetime import datetime, timedelta, timezone
from pathlib import Path

from case_schema import CaseSchemaError, load_case, parse_case
from invariant_compiler import Status, Verdict, decide

NOW = datetime(2026, 5, 5, tzinfo=timezone.utc)
FUTURE = (NOW + timedelta(days=30)).isoformat()
PAST = (NOW - timedelta(days=365 * 6)).isoformat()


results = []


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


def expect_raises(name, exc_type, fn, msg_substr=None):
    try:
        fn()
        results.append((False, name))
        print(f"[*** FAIL ***] {name}  (no exception raised)")
    except exc_type as e:
        if msg_substr and msg_substr not in str(e):
            results.append((False, name))
            print(f"[*** FAIL ***] {name}  (msg lacks {msg_substr!r}: {e})")
        else:
            results.append((True, name))
            print(f"[PASS] {name}  ({e})")


print("=" * 60)
print("CASE SCHEMA")
print("=" * 60)

# ─── S-1: full happy path ───
print()
print("--- happy path ---")
doc = {
    "id": "rel-2026-05-05.1",
    "risk": "medium",
    "tumblers": [
        {
            "tier": "T0-01",
            "invariant": "Evidence Before Belief",
            "status": "seated",
            "evidence_ids": ["ev1"],
        },
        {
            "tier": "T2-04",
            "invariant": "Patch Safely",
            "status": "waived",
            "waiver": {
                "reason": "known limitation",
                "owner": "zkaedi",
                "expires_at": FUTURE,
                "repayment": "HYG-001",
                "approved_by": "release-marshal",
            },
        },
    ],
    "evidence": [
        {
            "id": "ev1",
            "type": "cmd",
            "claim_supported": "tumbler seated",
            "command": "pytest -q",
            "result": "1 passed",
            "quality": 5,
        }
    ],
}
case = parse_case(doc)
expect("S-1a: id parsed", case.id, "rel-2026-05-05.1")
expect("S-1b: 2 tumblers", len(case.tumblers), 2)
expect("S-1c: tumbler 0 status", case.tumblers[0].status, Status.SEATED)
expect("S-1d: tumbler 1 has waiver", case.tumblers[1].waiver is not None, True)
expect("S-1e: 1 evidence", len(case.evidence), 1)
report = decide(case, now=NOW)
expect("S-1f: case parses to SHIP_WITH_RISK_ACCEPTANCE",
       report.verdict, Verdict.SHIP_WITH_RISK_ACCEPTANCE)


# ─── S-2: missing required fields ───
print()
print("--- malformed ---")
expect_raises("S-2a: missing id", CaseSchemaError,
              lambda: parse_case({"risk": "low", "tumblers": []}),
              msg_substr="'id'")
expect_raises("S-2b: missing risk", CaseSchemaError,
              lambda: parse_case({"id": "x", "tumblers": []}),
              msg_substr="'risk'")
expect_raises("S-2c: empty tumblers", CaseSchemaError,
              lambda: parse_case({"id": "x", "risk": "low", "tumblers": []}),
              msg_substr="non-empty")
expect_raises("S-2d: invalid risk value", CaseSchemaError,
              lambda: parse_case({"id": "x", "risk": "yolo",
                                   "tumblers": [{"tier": "T0", "invariant": "x",
                                                 "status": "seated"}]}),
              msg_substr="risk")
expect_raises("S-2e: invalid status enum", CaseSchemaError,
              lambda: parse_case({"id": "x", "risk": "low",
                                   "tumblers": [{"tier": "T0", "invariant": "x",
                                                 "status": "yolo"}]}),
              msg_substr="status")
expect_raises("S-2f: malformed tier", CaseSchemaError,
              lambda: parse_case({"id": "x", "risk": "low",
                                   "tumblers": [{"tier": "TX", "invariant": "x",
                                                 "status": "seated"}]}),
              msg_substr="tier")
expect_raises("S-2g: dangling evidence_id", CaseSchemaError,
              lambda: parse_case({"id": "x", "risk": "low",
                                   "tumblers": [{"tier": "T0", "invariant": "x",
                                                 "status": "seated",
                                                 "evidence_ids": ["NO-SUCH"]}],
                                   "evidence": []}),
              msg_substr="not found")


# ─── S-3: file roundtrip ───
print()
print("--- file roundtrip ---")
with tempfile.TemporaryDirectory() as td:
    p = Path(td) / "case.json"
    p.write_text(json.dumps(doc))
    case2 = load_case(p)
    expect("S-3a: file roundtrip preserves id",
           case2.id, "rel-2026-05-05.1")

    expect_raises("S-3b: missing file raises",
                  CaseSchemaError,
                  lambda: load_case(Path(td) / "nope.json"),
                  msg_substr="not found")

    bad = Path(td) / "bad.json"
    bad.write_text("{ not valid json")
    expect_raises("S-3c: malformed JSON raises",
                  CaseSchemaError,
                  lambda: load_case(bad),
                  msg_substr="malformed JSON")


print()
passed = sum(1 for ok, _ in results if ok)
print(f"=== {passed}/{len(results)} case-schema tests passed ===")
