"""F1 structural-close test suite for authoritative-base validate_safe_path.

Runs against the reference implementation with a REAL temp filesystem:
real directories, a real symlink escape, real nonexistent targets.
Each negative control must raise; a control that does not fire fails the suite.
"""

import os
import sys
import tempfile
from pathlib import Path

import zkaedi_safe_path_patch as sp
from zkaedi_safe_path_patch import validate_safe_path

PASS, FAIL = 0, 0
results = []


def check(name, fn, expect_exc=None):
    global PASS, FAIL
    try:
        out = fn()
    except Exception as e:
        if expect_exc and isinstance(e, expect_exc):
            results.append(("PASS", name, f"fired {type(e).__name__}: {e}"))
            PASS += 1
        else:
            results.append(("FAIL", name, f"unexpected {type(e).__name__}: {e}"))
            FAIL += 1
    else:
        if expect_exc is None:
            results.append(("PASS", name, f"returned {out}"))
            PASS += 1
        else:
            results.append(("FAIL", name, "DID NOT FIRE — control cannot fail"))
            FAIL += 1


with tempfile.TemporaryDirectory() as td:
    root = Path(td).resolve()
    broad = root / "broad"            # simulates internal get_safe_bases() default
    narrow = broad / "narrow"         # runtime authoritative base (narrower)
    outside = root / "outside"        # entirely outside broad
    for d in (broad, narrow, outside):
        d.mkdir(parents=True)

    inside_narrow = narrow / "data.parquet"
    inside_narrow.write_text("x")
    inside_broad_only = broad / "sibling.parquet"   # in broad, NOT in narrow
    inside_broad_only.write_text("x")
    outside_file = outside / "evil.parquet"
    outside_file.write_text("x")

    # symlink inside narrow pointing outside everything
    link = narrow / "escape_link"
    os.symlink(outside_file, link)

    # Monkeypatch internal defaults to the broad base — the union-vs-replace trap.
    sp.get_safe_bases = lambda: [broad]

    # === Minimum test list ===
    check("T1a narrow-authoritative: inside narrow -> PASS",
          lambda: validate_safe_path(inside_narrow, must_exist=True,
                                     authoritative_safe_bases=[narrow]))

    check("T1b narrow-authoritative: inside broad only -> REJECT "
          "(internal default MUST NOT rescue)",
          lambda: validate_safe_path(inside_broad_only, must_exist=True,
                                     authoritative_safe_bases=[narrow]),
          expect_exc=ValueError)

    check("T2 symlink escape under authoritative base -> REJECT",
          lambda: validate_safe_path(link, must_exist=True,
                                     authoritative_safe_bases=[narrow]),
          expect_exc=ValueError)

    check("T3 nonexistent output inside authoritative base -> PASS",
          lambda: validate_safe_path(narrow / "out" / "run-004", must_exist=False,
                                     authoritative_safe_bases=[narrow]))

    check("T4 nonexistent output, parent outside -> REJECT",
          lambda: validate_safe_path(outside / "out" / "run-004", must_exist=False,
                                     authoritative_safe_bases=[narrow]),
          expect_exc=ValueError)

    check("T5 both additive and authoritative -> REJECT",
          lambda: validate_safe_path(inside_narrow, must_exist=True,
                                     extra_safe_bases=[broad],
                                     authoritative_safe_bases=[narrow]),
          expect_exc=ValueError)

    check("T6 empty authoritative list -> REJECT",
          lambda: validate_safe_path(inside_narrow, must_exist=True,
                                     authoritative_safe_bases=[]),
          expect_exc=ValueError)

    # === Additional fail-closed requirements ===
    check("T7 relative authoritative base -> REJECT",
          lambda: validate_safe_path(inside_narrow, must_exist=True,
                                     authoritative_safe_bases=[Path("relative/base")]),
          expect_exc=ValueError)

    check("T8 nonexistent authoritative base -> REJECT",
          lambda: validate_safe_path(inside_narrow, must_exist=True,
                                     authoritative_safe_bases=[root / "ghost"]),
          expect_exc=ValueError)

    check("T9 `..` escape via existing path -> REJECT",
          lambda: validate_safe_path(narrow / ".." / "sibling.parquet",
                                     must_exist=True,
                                     authoritative_safe_bases=[narrow]),
          expect_exc=ValueError)

    check("T10 `..` in nonexistent suffix -> REJECT",
          lambda: validate_safe_path(narrow / "ghostdir" / ".." / ".." / "x",
                                     must_exist=False,
                                     authoritative_safe_bases=[narrow]),
          expect_exc=ValueError)

    check("T11 must_exist=True on missing path -> REJECT (FileNotFoundError)",
          lambda: validate_safe_path(narrow / "missing.parquet", must_exist=True,
                                     authoritative_safe_bases=[narrow]),
          expect_exc=FileNotFoundError)

    check("T12 legacy additive semantics preserved: broad-only file with "
          "extra_safe_bases=None under internal default -> PASS",
          lambda: validate_safe_path(inside_broad_only, must_exist=True))

    check("T13 legacy extra_safe_bases still additive: outside file admitted "
          "only via explicit extra base -> PASS",
          lambda: validate_safe_path(outside_file, must_exist=True,
                                     extra_safe_bases=[outside]))

    check("T14 authoritative base that is a file, not dir -> REJECT",
          lambda: validate_safe_path(inside_narrow, must_exist=True,
                                     authoritative_safe_bases=[inside_narrow]),
          expect_exc=ValueError)

for verdict, name, detail in results:
    print(f"{verdict}: {name}\n      {detail}")
print(f"\nTOTAL: {PASS} pass, {FAIL} fail")
sys.exit(1 if FAIL else 0)
