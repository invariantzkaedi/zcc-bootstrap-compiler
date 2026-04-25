#!/usr/bin/env python3

import re
import sys

def main():
    text = sys.stdin.read()

    current = None
    fields = []

    for line in text.splitlines():
        m = re.search(r'^\s*0 \| (struct|union) (\w+)', line)
        if m:
            if current:
                emit(current, fields)
            current = {
                "kind": m.group(1),
                "name": m.group(2),
                "size": None,
                "align": None,
            }
            fields = []
            continue

        m = re.search(r'^\s*(\d+) \|\s+[^ ]+\s+(\w+)$', line)
        if current and m:
            fields.append((m.group(2), m.group(1)))
            continue

        m = re.search(r'\[sizeof=(\d+), align=(\d+)\]', line)
        if current and m:
            current["size"] = m.group(1)
            current["align"] = m.group(2)

    if current:
        emit(current, fields)

def emit(record, fields):
    print(f"record={record['kind']} {record['name']}")
    print(f"sizeof={record['size']}")
    print(f"alignof={record['align']}")
    for name, offset in fields:
        print(f"field.{name}={offset}")

if __name__ == "__main__":
    main()