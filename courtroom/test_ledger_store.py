"""Regression suite for ledger_store.py."""
import json
import tempfile
from datetime import datetime, timedelta, timezone
from pathlib import Path

from invariant_compiler import (
    Evidence,
    ReleaseCase,
    Status,
    TumblerStatus,
    Waiver,
)
from ledger_store import (
    LedgerStoreError,
    PersistedLedger,
    load,
    rotate,
    save,
)

from courtroom import CourtSession, EntryKind

NOW = datetime(2026, 5, 5, tzinfo=timezone.utc)
FUTURE = (NOW + timedelta(days=30)).isoformat()


def valid_waiver():
    return Waiver(reason="r", owner="o", expires_at=FUTURE,
                  repayment="rp", approved_by="a")


good_ev = Evidence(id="ev1", type="cmd", claim_supported="ok",
                    quality=5, command="pytest -q", result="1 passed")


def make_filled_session(case_id="r1") -> CourtSession:
    """A session with a meaningful chain — indictment, defense, recompute, verdict."""
    case = ReleaseCase(
        id=case_id,
        risk="medium",
        tumblers=[
            TumblerStatus(tier="T0-01", invariant="Evidence",
                          status=Status.SEATED, evidence_ids=["ev1"]),
            TumblerStatus(tier="T2-01", invariant="Patch",
                          status=Status.WAIVED, waiver=valid_waiver()),
        ],
        evidence={"ev1": good_ev},
    )
    s = CourtSession(case=case)
    brief = s.indict(now=NOW)
    s.plan_defense(brief, now=NOW)
    s.adjudicate(now=NOW)
    return s


# ============================================================
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


def expect_false(name, actual):
    expect(name, bool(actual), False)


def expect_raises(name, exc_type, fn):
    try:
        fn()
        results.append((False, name))
        print(f"[*** FAIL ***] {name}  (no exception raised)")
    except exc_type as e:
        results.append((True, name))
        print(f"[PASS] {name}  ({type(e).__name__}: {e})")
    except Exception as e:
        results.append((False, name))
        print(f"[*** FAIL ***] {name}  (wrong exc type: {type(e).__name__}: {e})")


print("=" * 60)
print("LEDGER STORE — persistence regression")
print("=" * 60)

with tempfile.TemporaryDirectory() as td:
    td = Path(td)

    # ─── P-1: round trip ───
    print()
    print("--- round trip ---")
    s1 = make_filled_session("p1")
    path1 = td / "p1.jsonl"
    save(s1.ledger, path1)
    expect_true("P-1a: file written", path1.exists())
    loaded = load(path1)
    ok, reason = loaded.verify()
    expect_true("P-1b: loaded ledger verifies", ok)
    expect("P-1c: entry count survives round trip",
           len(loaded.entries), len(s1.ledger.entries))
    expect("P-1d: head hash matches", loaded.head_hash, s1.ledger.head_hash)
    expect("P-1e: kinds preserved",
           [e.kind for e in loaded.entries],
           [e.kind for e in s1.ledger.entries])

    # ─── P-2: refuses to save an unverified ledger ───
    print()
    print("--- refuse-bad-save ---")
    s2 = make_filled_session("p2")
    s2.ledger._entries[0].payload["case_id"] = "TAMPERED"  # break chain
    expect_raises("P-2: save raises on broken chain",
                  LedgerStoreError, lambda: save(s2.ledger, td / "p2.jsonl"))

    # ─── P-3: tamper-on-disk caught at load ───
    print()
    print("--- tamper-on-disk ---")
    s3 = make_filled_session("p3")
    path3 = td / "p3.jsonl"
    save(s3.ledger, path3)

    # Mutate one line on disk: change the actor of seq=0.
    lines = path3.read_text().splitlines()
    rec = json.loads(lines[0])
    rec["actor"] = "EVIL"
    lines[0] = json.dumps(rec, sort_keys=True, separators=(",", ":"))
    path3.write_text("\n".join(lines) + "\n")

    expect_raises("P-3a: load raises on payload tamper",
                  LedgerStoreError, lambda: load(path3))

    # Different tamper: flip an entry_hash byte but leave payload alone.
    s3b = make_filled_session("p3b")
    path3b = td / "p3b.jsonl"
    save(s3b.ledger, path3b)
    lines = path3b.read_text().splitlines()
    rec = json.loads(lines[1])
    h = rec["entry_hash"]
    rec["entry_hash"] = ("0" + h[1:]) if h[0] != "0" else ("1" + h[1:])
    lines[1] = json.dumps(rec, sort_keys=True, separators=(",", ":"))
    path3b.write_text("\n".join(lines) + "\n")
    expect_raises("P-3b: load raises on entry_hash flip",
                  LedgerStoreError, lambda: load(path3b))

    # ─── P-4: partial last line tolerated by default ───
    print()
    print("--- partial last line ---")
    s4 = make_filled_session("p4")
    path4 = td / "p4.jsonl"
    save(s4.ledger, path4)
    txt = path4.read_text()
    # Append a half-written record (simulates crash mid-fsync).
    path4.write_text(txt + '{"seq":99,"timesta')
    loaded4 = load(path4)
    expect("P-4a: partial last line dropped, valid entries kept",
           len(loaded4.entries), len(s4.ledger.entries))

    # And not tolerated when explicitly disabled.
    expect_raises("P-4b: partial last line raises when not tolerated",
                  LedgerStoreError,
                  lambda: load(path4, tolerate_partial_last_line=False))

    # ─── P-5: missing file ───
    print()
    print("--- missing file ---")
    expect_raises("P-5: load raises on missing file",
                  LedgerStoreError, lambda: load(td / "does_not_exist.jsonl"))

    # ─── P-6: streaming append durability via PersistedLedger ───
    print()
    print("--- persisted ledger streaming append ---")
    path6 = td / "p6.jsonl"
    pl = PersistedLedger(path6)
    pl.append(EntryKind.INDICTMENT, "Prosecutor",
              {"case_id": "p6", "charges": []}, now=NOW)
    pl.append(EntryKind.VERDICT, "ReleaseMarshal",
              {"case_id": "p6", "verdict": "SHIP"}, now=NOW)

    # Reload from disk in a fresh process-equivalent — just call load again.
    fresh = load(path6)
    expect("P-6a: streamed entries readable", len(fresh.entries), 2)
    ok, reason = fresh.verify()
    expect_true("P-6b: streamed chain verifies", ok)

    # And appending to an existing file resumes correctly.
    pl2 = PersistedLedger(path6)
    expect("P-6c: PersistedLedger reloads existing file",
           len(pl2.entries), 2)
    pl2.append(EntryKind.RECOMPUTE, "system",
               {"case_id": "p6", "verdict": "SHIP"}, now=NOW)
    expect("P-6d: append after reload extends correctly",
           len(pl2.entries), 3)
    fresh2 = load(path6)
    expect("P-6e: extension persisted", len(fresh2.entries), 3)
    ok, reason = fresh2.verify()
    expect_true("P-6f: extended chain verifies", ok)

    # ─── P-7: out-of-order seq detected ───
    print()
    print("--- out-of-order seq ---")
    s7 = make_filled_session("p7")
    path7 = td / "p7.jsonl"
    save(s7.ledger, path7)
    lines = path7.read_text().splitlines()
    # Swap seq 1 and seq 2.
    lines[1], lines[2] = lines[2], lines[1]
    path7.write_text("\n".join(lines) + "\n")
    expect_raises("P-7: out-of-order seq raises",
                  LedgerStoreError, lambda: load(path7))

    # ─── P-8: rotation / compaction ───
    print()
    print("--- rotation ---")
    s8 = make_filled_session("p8")
    path8 = td / "p8.jsonl"
    archive8 = td / "p8.archive.jsonl"
    save(s8.ledger, path8)
    archived_count = len(s8.ledger.entries)
    archived_head = s8.ledger.head_hash

    new_ledger = rotate(path8, archive8, now=NOW)
    expect_true("P-8a: archive file exists", archive8.exists())
    expect_true("P-8b: new ledger file exists", path8.exists())

    # Archive verifies independently.
    arch = load(archive8)
    ok, _ = arch.verify()
    expect_true("P-8c: archive verifies", ok)
    expect("P-8d: archive entry count preserved",
           len(arch.entries), archived_count)
    expect("P-8e: archive head hash preserved",
           arch.head_hash, archived_head)

    # New ledger has exactly one CHECKPOINT entry referencing the archive.
    expect("P-8f: new ledger has 1 entry (the checkpoint)",
           len(new_ledger.entries), 1)
    cp = new_ledger.entries[0]
    expect_true("P-8g: checkpoint has _checkpoint=True",
                cp.payload.get("_checkpoint"))
    expect("P-8h: checkpoint references archived head_hash",
           cp.payload.get("archived_head_hash"), archived_head)
    expect("P-8i: checkpoint records archived count",
           cp.payload.get("archived_entry_count"), archived_count)

    # Subsequent appends to the rotated ledger continue cleanly.
    new_ledger.append(EntryKind.RECOMPUTE, "system",
                      {"after_rotation": True}, now=NOW)
    fresh = load(path8)
    ok, _ = fresh.verify()
    expect_true("P-8j: post-rotation chain verifies", ok)
    expect("P-8k: post-rotation entry count = 2",
           len(fresh.entries), 2)

    # Cannot rotate a non-existent ledger.
    expect_raises("P-8l: rotate raises on missing source",
                  LedgerStoreError,
                  lambda: rotate(td / "nope.jsonl", td / "nope.archive.jsonl"))

    # Cannot rotate a corrupt ledger.
    s8b = make_filled_session("p8b")
    path8b = td / "p8b.jsonl"
    archive8b = td / "p8b.archive.jsonl"
    save(s8b.ledger, path8b)
    # Corrupt on disk.
    lines = path8b.read_text().splitlines()
    rec = json.loads(lines[0])
    rec["actor"] = "EVIL"
    lines[0] = json.dumps(rec, sort_keys=True, separators=(",", ":"))
    path8b.write_text("\n".join(lines) + "\n")
    expect_raises("P-8m: rotate refuses corrupt source",
                  LedgerStoreError,
                  lambda: rotate(path8b, archive8b))


print()
passed = sum(1 for ok, _ in results if ok)
print(f"=== {passed}/{len(results)} ledger-store tests passed ===")
if passed < len(results):
    print("FAILURES:")
    for ok, name in results:
        if not ok:
            print(f"  - {name}")
