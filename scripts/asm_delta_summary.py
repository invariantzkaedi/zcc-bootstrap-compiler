#!/usr/bin/env python3
import re
import sys
from pathlib import Path
import difflib

def read_lines(p):
    return Path(p).read_text(errors="replace").splitlines()

def main():
    if len(sys.argv) != 4:
        print("usage: asm_delta_summary.py <baseline.s> <target.s> <out.txt>", file=sys.stderr)
        return 2

    baseline, target, out = map(Path, sys.argv[1:])
    if not baseline.exists() or not target.exists():
        Path(out).write_text("Assembly files missing.\n")
        return 1

    a = read_lines(baseline)
    b = read_lines(target)
    diff = list(difflib.unified_diff(a, b, fromfile=str(baseline), tofile=str(target), lineterm=""))

    fn_label = re.compile(r'^[\+\-]\s*([._A-Za-z][\w.$@]*):\s*$')
    rsp_delta = re.compile(r'^[\+\-].*\b(?:sub|add)(?:q|l)?\b.*\brsp\b', re.IGNORECASE)
    save_restore = re.compile(r'^[\+\-].*\b(?:push|pop|mov)\b', re.IGNORECASE)
    call_sites = re.compile(r'^[\+\-].*\bcall[q]?\b', re.IGNORECASE)

    fn_changes, rsp_changes, sr_changes, call_changes = [], [], [], []

    for line in diff:
        if fn_label.search(line):
            fn_changes.append(line)
        if rsp_delta.search(line):
            rsp_changes.append(line)
        if save_restore.search(line):
            sr_changes.append(line)
        if call_sites.search(line):
            call_changes.append(line)

    content = []
    content.append("Changed function labels (heuristic):")
    content.extend(fn_changes or ["(none)"])
    content.append("")
    content.append("Stack pointer deltas (sub/add ... rsp):")
    content.extend(rsp_changes or ["(none)"])
    content.append("")
    content.append("Register save/restore hints:")
    content.extend(sr_changes or ["(none)"])
    content.append("")
    content.append("Call-site deltas:")
    content.extend(call_changes or ["(none)"])
    content.append("")

    Path(out).write_text("\n".join(content))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
