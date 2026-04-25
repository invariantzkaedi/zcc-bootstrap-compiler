#!/usr/bin/env python3
import argparse
from pathlib import Path

def normalize(text):
    return "\n".join(line.rstrip() for line in text.splitlines() if line.strip()) + "\n"

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("expected")
    parser.add_argument("actual")
    args = parser.parse_args()

    expected = normalize(Path(args.expected).read_text(encoding="utf-8"))
    actual = normalize(Path(args.actual).read_text(encoding="utf-8"))

    if expected != actual:
        print("[LAYOUT-COMPARE] mismatch")
        print("--- expected")
        print(expected)
        print("--- actual")
        print(actual)
        raise SystemExit(1)

    print("[LAYOUT-COMPARE] match")

if __name__ == "__main__":
    main()