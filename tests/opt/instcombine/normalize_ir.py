#!/usr/bin/env python3
import re
import sys
from typing import Dict, List

# Normalizes:
# - SSA vreg names: %foo, %12 -> %r0, %r1 ... in first-seen order
# - bb labels: bb17 -> bb0, bb1 ...
# Leaves opcodes/types/constants untouched.

VREG_RE = re.compile(r'%[A-Za-z_][A-Za-z0-9_]*|%[0-9]+')
BBDEF_RE = re.compile(r'^\s*(bb[0-9]+)\s*:\s*$')
BBREF_RE = re.compile(r'\bbb[0-9]+\b')

def normalize_lines(lines: List[str]) -> List[str]:
    vmap: Dict[str, str] = {}
    bmap: Dict[str, str] = {}
    next_v = 0
    next_b = 0

    def map_v(tok: str) -> str:
        nonlocal next_v
        if tok not in vmap:
            vmap[tok] = f"%r{next_v}"
            next_v += 1
        return vmap[tok]

    def map_b(tok: str) -> str:
        nonlocal next_b
        if tok not in bmap:
            bmap[tok] = f"bb{next_b}"
            next_b += 1
        return bmap[tok]

    out = []

    # pass 1: map bb defs first to keep stable block numbering
    for line in lines:
        m = BBDEF_RE.match(line)
        if m:
            _ = map_b(m.group(1))

    for line in lines:
        # normalize bb definitions
        m = BBDEF_RE.match(line)
        if m:
            out.append(f"{map_b(m.group(1))}:\n")
            continue

        # normalize bb references
        def bb_sub(mm):
            return map_b(mm.group(0))
        line2 = BBREF_RE.sub(bb_sub, line)

        # normalize registers
        def vr_sub(mm):
            return map_v(mm.group(0))
        line3 = VREG_RE.sub(vr_sub, line2)

        if not line3.endswith('\n'):
            line3 += '\n'
        out.append(line3)

    return out

def main():
    if len(sys.argv) != 2:
        print("usage: normalize_ir.py <file.ir>", file=sys.stderr)
        sys.exit(2)

    with open(sys.argv[1], "r", encoding="utf-8") as f:
        lines = f.readlines()

    norm = normalize_lines(lines)
    sys.stdout.writelines(norm)

if __name__ == "__main__":
    main()
