#!/usr/bin/env python3
# SPDX-License-Identifier: BUSL-1.1
# Copyright (c) 2026 ZKAEDI. Change Date: 2030-10-02. Change License: Apache-2.0.
"""
ZKAEDI Agent Gate Warden — mechanical enforcement of EF-1..EF-6.   v1.0.0

Audits an agent transcript (and optionally a `git diff --stat`) and emits a
four-state verdict per rule. Stdlib only. Mirrors bug_hunter.py conventions.

  python3 tools/agent_gate_warden.py audit transcript.md [--diff diff.txt] [--json out.json]
  python3 tools/agent_gate_warden.py --self-test
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path

__version__ = "1.0.0"
SCHEMA_VERSION = "1.0.0"

VERDICTS = ("PASS", "FAIL", "INCONCLUSIVE", "NOT-APPLICABLE")

PROTECTED_PATHS = re.compile(
    r"\b(part[0-9]\w*\.c|ir_\w+\.c|regalloc\.c|evm_lifter\.c|zld\.c)\b"
)
ARTIFACT_INSPECT = re.compile(
    r"\b(grep|objdump|readelf|sha256sum|cmp|xxd|hexdump|nm|wc)\b.*\.(s|o|exe|wasm|bin|elf)\b"
)
CMD_LINE = re.compile(r"^\s*\$\s+(.+)$", re.M)
PASS_CLAIM = re.compile(r"\bPASS\b")
GATE_LINE = re.compile(r"^\s*(?:GATE|Gate)\s+([\w\-\(\)\s]+?)\s*:\s*([A-Z\-]+)\s*$", re.M)
GATES_DECL = re.compile(r"^\s*GATES:\s*(.+)$", re.M)
SCOPE_DECL = re.compile(r"^\s*SCOPE:\s*(.+)$", re.M)
DIFF_STAT = re.compile(r"git diff (?:--cached\s+|HEAD\s+)*--stat")
DIFF_FILE = re.compile(r"^\s*([\w./\\-]+)\s*\|\s*\d+", re.M)
SOFT_CLOSURE = re.compile(
    r"\b(done|looks good|should work|all good|fixed|complete[d]?|✅|works now)\b", re.I
)
EDIT_MARKER = re.compile(
    r"^[ \t]*(?:(?:---|\+\+\+)[ \t]+\S+|diff[ \t]+--git[ \t]+\S+|TargetFile:[ \t]*\S+)",
    re.M,
)
BOUNDARY = re.compile(
    r"^[ \t]*(?:(?:---|\+\+\+)[ \t]+\S+|diff[ \t]+--git[ \t]+\S+|TargetFile:[ \t]*\S+|(?:GATE|Gate)\s+[\w\-\(\)\s]+?\s*:\s*[A-Z\-]+|#{1,4}[ \t]+.*|---[ \t]*)$",
    re.M,
)


@dataclass
class RuleVerdict:
    id: str
    verdict: str
    evidence: list[str] = field(default_factory=list)
    line: int | None = None


@dataclass
class Report:
    schema_version: str
    input_sha256: str
    overall: str
    rules: list[RuleVerdict]

    def exit_code(self) -> int:
        if self.overall == "FAIL":
            return 10
        if self.overall == "INCONCLUSIVE":
            return 11
        return 0


def _lineno(text: str, pos: int) -> int:
    return text.count("\n", 0, pos) + 1


def _blocks(text: str) -> list[str]:
    """Split transcript into turns on blank-line-separated paragraphs."""
    return [b for b in re.split(r"\n\s*\n", text) if b.strip()]


# ── EF-1 ──────────────────────────────────────────────────────────
def check_ef1(text: str) -> RuleVerdict:
    ev: list[str] = []
    has_pass = False
    for m in PASS_CLAIM.finditer(text):
        has_pass = True
        pos = m.start()
        boundaries = [b.end() for b in BOUNDARY.finditer(text[:pos])]
        start = max(boundaries) if boundaries else 0
        window = text[start:pos]
        if not CMD_LINE.search(window):
            ev.append(f"L{_lineno(text, pos)}: PASS without same-block command/output")
    if ev:
        return RuleVerdict("EF-1", "FAIL", ev)
    if not has_pass:
        return RuleVerdict("EF-1", "NOT-APPLICABLE", ["no PASS claims"])
    return RuleVerdict("EF-1", "PASS", ["every PASS has same-block evidence"])


# ── EF-2 ──────────────────────────────────────────────────────────
def check_ef2(text: str) -> RuleVerdict:
    mentions_codegen = re.search(r"\.(s|o|exe|wasm|bin)\b|codegen|emit", text, re.I)
    if not mentions_codegen:
        return RuleVerdict("EF-2", "NOT-APPLICABLE", ["no codegen claims"])
    if ARTIFACT_INSPECT.search(text):
        return RuleVerdict("EF-2", "PASS", ["artifact inspection present"])
    if re.search(r"exit\s*=\s*0", text):
        return RuleVerdict("EF-2", "FAIL", ["codegen claim backed only by exit=0"])
    return RuleVerdict("EF-2", "INCONCLUSIVE", ["codegen mentioned; no inspection or exit code found"])


# ── EF-3 ──────────────────────────────────────────────────────────
def check_ef3(text: str) -> RuleVerdict:
    first_edit = None
    for m in EDIT_MARKER.finditer(text):
        window = text[m.start(): m.start() + 400]
        if PROTECTED_PATHS.search(window):
            first_edit = m.start()
            break
    if first_edit is None:
        return RuleVerdict("EF-3", "NOT-APPLICABLE", ["no edits to protected paths"])
    probe = CMD_LINE.search(text[:first_edit])
    if probe:
        return RuleVerdict("EF-3", "PASS", [f"probe at L{_lineno(text, probe.start())} precedes edit at L{_lineno(text, first_edit)}"])
    return RuleVerdict("EF-3", "FAIL", [f"protected-path edit at L{_lineno(text, first_edit)} with no prior probe"], _lineno(text, first_edit))


# ── EF-4 ──────────────────────────────────────────────────────────
def check_ef4(text: str) -> RuleVerdict:
    decl = GATES_DECL.search(text)
    if not decl:
        return RuleVerdict("EF-4", "NOT-APPLICABLE", ["no GATES: declaration"])
    declared = {g.strip().lower() for g in decl.group(1).split(",") if g.strip()}
    reported = {}
    bad = {}
    for m in GATE_LINE.finditer(text):
        gname = m.group(1).strip().lower()
        verdict = m.group(2).strip()
        reported[gname] = verdict
        m_sub = re.search(r"\(([\w\-]+)\)", gname)
        if m_sub:
            reported[m_sub.group(1).lower()] = verdict
        if verdict not in VERDICTS:
            bad[gname] = verdict
    missing = sorted(d for d in declared if d not in reported)
    ev = []
    if missing:
        ev.append(f"declared but unreported: {', '.join(missing)}")
    if bad:
        ev.append(f"non-four-state gate verdicts: {bad}")
    if ev:
        return RuleVerdict("EF-4", "FAIL", ev)
    return RuleVerdict("EF-4", "PASS", [f"all {len(declared)} declared gates reported"])


# ── EF-5 ──────────────────────────────────────────────────────────
def check_ef5(text: str, diff_stat: str | None) -> RuleVerdict:
    scope = SCOPE_DECL.search(text)
    stat_src = diff_stat if diff_stat is not None else text
    if not DIFF_STAT.search(text) and diff_stat is None:
        return RuleVerdict("EF-5", "FAIL", ["no `git diff --stat` present"])
    raw_files = DIFF_FILE.findall(stat_src)
    files = {f.replace("\\", "/").strip() for f in raw_files}
    if not scope:
        return RuleVerdict("EF-5", "INCONCLUSIVE", ["diff present but no SCOPE: declaration"])
    allowed = {a.replace("\\", "/").strip() for a in scope.group(1).split()}
    out_of_scope = sorted(f for f in files if not any(f == a or f.startswith(a.rstrip("/") + "/") for a in allowed))
    if out_of_scope:
        return RuleVerdict("EF-5", "FAIL", [f"files outside SCOPE: {', '.join(out_of_scope)}"])
    return RuleVerdict("EF-5", "PASS", [f"{len(files)} changed file(s) all within scope"])


# ── EF-6 ──────────────────────────────────────────────────────────
def check_ef6(text: str) -> RuleVerdict:
    ev = []
    for m in GATE_LINE.finditer(text):
        if m.group(2) not in VERDICTS:
            ev.append(f"L{_lineno(text, m.start())}: gate verdict {m.group(2)!r}")
    final = _blocks(text)[-1] if _blocks(text) else ""
    for m in SOFT_CLOSURE.finditer(final):
        ev.append(f"final block uses soft closure {m.group(0)!r}")
    if ev:
        return RuleVerdict("EF-6", "FAIL", ev)
    return RuleVerdict("EF-6", "PASS", ["strict four-state vocabulary"])


def audit(text: str, diff_stat: str | None = None) -> Report:
    rules = [
        check_ef1(text), check_ef2(text), check_ef3(text),
        check_ef4(text), check_ef5(text, diff_stat), check_ef6(text),
    ]
    verdicts = {r.verdict for r in rules}
    if "FAIL" in verdicts:
        overall = "FAIL"
    elif "INCONCLUSIVE" in verdicts:
        overall = "INCONCLUSIVE"
    elif verdicts <= {"NOT-APPLICABLE"}:
        overall = "NOT-APPLICABLE"
    else:
        overall = "PASS"
    return Report(SCHEMA_VERSION, hashlib.sha256(text.encode()).hexdigest(), overall, rules)


# ── Self-test fixtures (Control-style: positive baseline + one negative per rule)
_GOOD = """GATES: selfhost, abi
SCOPE: part4.c

$ ./zcc probe.c -o probe.s && grep -c movsd probe.s
144

--- a/part4.c
+++ b/part4.c

$ bash gate.sh
exit=0
$ cmp stage2.s stage3.s && echo IDENTICAL
IDENTICAL
GATE selfhost: PASS
GATE abi: NOT-APPLICABLE

$ git diff --stat
 part4.c | 12 ++++--
"""

_NEG = {
    "EF-1": _GOOD.replace("$ cmp stage2.s stage3.s && echo IDENTICAL\nIDENTICAL\n", "").replace("$ bash gate.sh\nexit=0\n", "") + "\nGATE extra: PASS\n",
    "EF-2": _GOOD.replace("$ cmp stage2.s stage3.s && echo IDENTICAL\nIDENTICAL\n", "").replace("grep -c movsd probe.s\n144", "true\nexit=0"),
    "EF-3": _GOOD.replace("$ ./zcc probe.c -o probe.s && grep -c movsd probe.s\n144\n\n", ""),
    "EF-4": _GOOD.replace("GATE abi: NOT-APPLICABLE\n", ""),
    "EF-5": _GOOD.replace(" part4.c | 12 ++++--", " part4.c | 12 ++++--\n part5.c | 3 +"),
    "EF-6": _GOOD + "\nAll done, looks good ✅\n",
}


def self_test() -> int:
    ok = True
    base = audit(_GOOD)
    print(f"[Control A] baseline → {base.overall}", file=sys.stderr)
    ok &= base.overall == "PASS"
    for rule, txt in _NEG.items():
        rep = audit(txt)
        got = next(r.verdict for r in rep.rules if r.id == rule)
        status = "PASS" if got == "FAIL" else "FAIL"
        ok &= got == "FAIL"
        print(f"[Control {rule}] injected violation → {rule}={got}  {status}", file=sys.stderr)
    print("SELF-TEST", "PASS" if ok else "FAIL", file=sys.stderr)
    return 0 if ok else 10


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="agent_gate_warden", description=__doc__.splitlines()[1])
    p.add_argument("--version", action="version", version=f"agent_gate_warden {__version__}")
    p.add_argument("--self-test", action="store_true")
    sub = p.add_subparsers(dest="cmd")
    pa = sub.add_parser("audit")
    pa.add_argument("transcript")
    pa.add_argument("--diff", help="Path to `git diff --stat` output (optional).")
    pa.add_argument("--json", help="Write schema-conformant verdict JSON here.")
    args = p.parse_args(argv)

    if args.self_test:
        return self_test()
    if args.cmd != "audit":
        p.print_help()
        return 2

    text = Path(args.transcript).read_text(encoding="utf-8", errors="replace")
    diff = Path(args.diff).read_text(encoding="utf-8") if args.diff else None
    rep = audit(text, diff)
    for r in rep.rules:
        print(f"{r.id:5} {r.verdict:15} {'; '.join(r.evidence)}")
    print(f"OVERALL {rep.overall}")
    if args.json:
        Path(args.json).write_text(json.dumps(asdict(rep), indent=2), encoding="utf-8")
    return rep.exit_code()


if __name__ == "__main__":
    sys.exit(main())
