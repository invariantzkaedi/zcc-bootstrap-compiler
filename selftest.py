"""Self-test for the zkaedi import patcher.

Falsifiability: T4 proves the patch is ABSENT when disabled — a patch
detector that can never fire negative is decoration (E-LEARN corpus).

T3 lesson (prior run FAILED here): `import zkaedi` transitively imports
`random`, so random ALWAYS takes the fallback patch-on-install path, never
the meta_path route. Asserting "random not pre-imported" was a broken
precondition. Fix: T3a verifies the fallback path on random; T3b proves the
true lazy meta_path route on colorsys (never transitively imported) and
asserts the ROUTE MARKER, not just the sentinel — the sentinel alone could
be set by either path.

Exit code 0 only if all assertions hold.
"""

import json
import os
import pathlib
import sys
import tempfile

# Use temporary directories to avoid cross-test permission issues
default_ledger = pathlib.Path(tempfile.gettempdir()) / "zkaedi_ledger.jsonl"
LEDGER = pathlib.Path(os.environ.get("ZKAEDI_LEDGER", str(default_ledger)))
if LEDGER.exists():
    LEDGER.unlink()

# --- Test 1: banner + eager patch install on import -----------------------
import zkaedi  # noqa: E402  (banner prints to stderr here)
import subprocess  # noqa: E402

assert getattr(subprocess.run, "_zkaedi_patched", False), "eager patch missing"
print("T1 PASS: subprocess.run carries patch sentinel")

# --- Test 2: evidence rows actually land in the ledger --------------------
subprocess.run(["true"])
subprocess.run(["false"])
rows = [json.loads(l) for l in LEDGER.read_text(encoding="utf-8").splitlines()]
assert len(rows) == 2, f"expected 2 ledger rows, got {len(rows)}"
assert rows[0]["exit"] == 0 and rows[0]["verdict"] == "PASS"
assert rows[1]["exit"] == 1 and rows[1]["verdict"] == "FAIL"
print("T2 PASS: ledger captured exit 0/PASS and exit 1/FAIL rows")

# --- Test 3a: fallback path — random is pre-imported BY zkaedi itself -----
# (verified: bare interpreter lacks random; it appears after `import zkaedi`)
assert "random" in sys.modules, (
    "expectation drift: random no longer pre-imported by zkaedi's import "
    "chain — if this fires, T3a/T3b path assignments need re-verification"
)
import random  # noqa: E402

assert getattr(random, "_zkaedi_patched", False), "fallback patch missing on random"
random.random()  # triggers the no-seed warning exactly once
print("T3a PASS: random patched via fallback path (pre-imported at hook install)")

# --- Test 3b: TRUE lazy path — colorsys imported only now ------------------
assert "colorsys" not in sys.modules, (
    "test precondition broken: colorsys pre-imported — pick a different "
    "never-transitively-imported target"
)
import colorsys  # noqa: E402  (imported AFTER zkaedi — meta_path must catch it)

assert getattr(colorsys, "_zkaedi_patched", False), "lazy patch missing on colorsys"
assert getattr(colorsys, "_zkaedi_route", None) == "meta_path", (
    "colorsys patched but NOT via meta_path route — sentinel alone is not proof"
)
print("T3b PASS: colorsys patched via true meta_path route on later import")

# --- Test 4 (negative control): disabled run must NOT patch ---------------
code = (
    "import subprocess, sys;"
    "sys.exit(1 if getattr(subprocess.run, '_zkaedi_patched', False) else 0)"
)
env = dict(os.environ, ZKAEDI_DISABLE="1", PYTHONPATH=str(pathlib.Path(__file__).parent))
probe = subprocess.run(
    [sys.executable, "-c", "import zkaedi;" + code], env=env, capture_output=True
)
assert probe.returncode == 0, "NEGATIVE CONTROL FAILED: patch present while disabled"
print("T4 PASS: negative control — ZKAEDI_DISABLE=1 leaves subprocess.run unpatched")

# --- Test 5 (idempotency): re-import must not double-wrap ------------------
run_before = subprocess.run
import importlib  # noqa: E402

importlib.reload(zkaedi)
assert subprocess.run.__wrapped__ is run_before.__wrapped__, (
    "double-wrap detected: reload re-patched an already-patched subprocess.run"
)
finders = [f for f in sys.meta_path if getattr(f, "_zkaedi_finder", False)]
assert len(finders) == 1, f"finder stacking: {len(finders)} zkaedi finders in meta_path"
print("T5 PASS: reload does not double-wrap; exactly one finder in meta_path")

# --- Test 6: Popen escape hatch is closed, and run() does NOT double-log ---
rows_before = len(LEDGER.read_text(encoding="utf-8").splitlines())
subprocess.check_call(["true"])          # routes via Popen, NOT run (3.12 stdlib)
with subprocess.Popen(["false"]) as p:   # __exit__ calls wait() -> records
    pass
subprocess.run(["true"])                 # must add exactly ONE row, not two
rows = [json.loads(l) for l in LEDGER.read_text(encoding="utf-8").splitlines()]
new = rows[rows_before:]
assert len(new) == 3, f"expected 3 new rows (2 Popen + 1 run), got {len(new)}: {new}"
assert new[0]["via"] == "Popen" and new[0]["exit"] == 0
assert new[1]["via"] == "Popen" and new[1]["exit"] == 1 and new[1]["verdict"] == "FAIL"
assert new[2]["via"] == "run" and new[2]["exit"] == 0
print("T6 PASS: check_call/Popen now leave evidence; run() dedup holds (no double row)")

# --- Test 7: hash chain verifies end-to-end --------------------------------
ok, bad_line, reason = zkaedi.verify_ledger()
assert ok, f"chain verification failed at line {bad_line}: {reason}"
print(f"T7 PASS: {reason}")

# --- Test 8 (negative control): tampered row MUST fail verification --------
# A verifier that cannot fire negative is decoration (E-LEARN corpus).
lines = LEDGER.read_text(encoding="utf-8").splitlines()
victim = json.loads(lines[1])
victim["verdict"] = "PASS"  # forge a FAIL into a PASS — the classic spin
lines[1] = json.dumps(victim, ensure_ascii=False)
tampered = pathlib.Path(tempfile.gettempdir()) / "zkaedi_ledger_tampered.jsonl"
tampered.write_text("\n".join(lines) + "\n", encoding="utf-8")
ok, bad_line, reason = zkaedi.verify_ledger(tampered)
assert not ok, "NEGATIVE CONTROL FAILED: forged verdict passed verification"
assert bad_line == 1, f"tamper detected at wrong line: {bad_line}"
print(f"T8 PASS: negative control — forged verdict caught at line {bad_line} ({reason})")

print("ALL TESTS PASS")
