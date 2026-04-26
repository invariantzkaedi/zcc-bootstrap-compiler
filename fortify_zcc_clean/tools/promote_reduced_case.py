#!/usr/bin/env python3
import argparse
from pathlib import Path

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("case")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    src = Path(args.case)
    out = Path(args.out)

    content = src.read_text(encoding="utf-8")
    header = """/*
 * Promoted reduced layout regression.
 * Source: generated fuzz reducer artifact.
 */

"""
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(header + content, encoding="utf-8")

    print(f"[PROMOTE-REDUCED-CASE] wrote {out}")

if __name__ == "__main__":
    main()