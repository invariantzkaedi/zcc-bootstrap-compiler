#!/usr/bin/env python3

import argparse
import re
import subprocess
from pathlib import Path

def run(cmd):
    return subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

def still_fails(path, zcc, cc):
    oracle_bin = "/tmp/layout-reduce.oracle"
    zcc_bin = "/tmp/layout-reduce.zcc"

    a = run([cc, str(path), "-o", oracle_bin])
    b = run([zcc, "--target=x86_64-linux-gnu", str(path), "-o", zcc_bin])

    if a.returncode != 0 or b.returncode != 0:
        return False

    oracle_out = run([oracle_bin]).stdout
    zcc_out = run([zcc_bin]).stdout

    return oracle_out != zcc_out

def remove_field_attempts(source):
    pattern = re.compile(r'^\s*[^;\n]+\s+f\d+(\[[^\]]+\])?;\s*$', re.MULTILINE)
    matches = list(pattern.finditer(source))

    for match in matches:
        yield source[:match.start()] + source[match.end():]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("case")
    parser.add_argument("--zcc", default="./zcc")
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--out", default=None)
    args = parser.parse_args()

    path = Path(args.case)
    best = path.read_text(encoding="utf-8")

    changed = True
    while changed:
        changed = False
        for candidate in remove_field_attempts(best):
            tmp = Path("/tmp/layout-reduce.c")
            tmp.write_text(candidate, encoding="utf-8")

            if still_fails(tmp, args.zcc, args.cc):
                best = candidate
                changed = True
                break

    out = Path(args.out) if args.out else path.with_suffix(".reduced.c")
    out.write_text(best, encoding="utf-8")
    print(out)

if __name__ == "__main__":
    main()