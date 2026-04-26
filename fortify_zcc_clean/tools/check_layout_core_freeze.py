#!/usr/bin/env python3

import argparse
import json
from pathlib import Path

FORBIDDEN = [
    "ad_hoc_sizeof",
    "old_alignof",
    "legacy_struct_layout",
    "compat_static_assert_stub",
    "#define _Static_assert",
]

SUFFIXES = {".c", ".h"}
EXCLUDE_DIRS = {".git", "artifacts", "build", "dist"}

def should_skip(path, root):
    rel = path.relative_to(root)
    return any(part in EXCLUDE_DIRS for part in rel.parts)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--report", default="artifacts/layout-core-freeze.json")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    findings = []

    for path in root.rglob("*"):
        if not path.is_file() or path.suffix not in SUFFIXES:
            continue

        if should_skip(path, root):
            continue

        text = path.read_text(encoding="utf-8", errors="replace")
        for line_no, line in enumerate(text.splitlines(), start=1):
            for symbol in FORBIDDEN:
                if symbol in line:
                    findings.append({
                        "symbol": symbol,
                        "path": str(path),
                        "line": line_no,
                        "text": line.rstrip(),
                    })

    result = {
        "status": "fail" if findings else "pass",
        "count": len(findings),
        "findings": findings,
    }

    report = Path(args.report)
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    if findings:
        for finding in findings:
            print(
                f"{finding['path']}:{finding['line']}: "
                f"forbidden legacy path {finding['symbol']}"
            )
        print(f"[LAYOUT-CORE-FREEZE] failed: report={report}")
        raise SystemExit(1)

    print(f"[LAYOUT-CORE-FREEZE] clean: report={report}")

if __name__ == "__main__":
    main()