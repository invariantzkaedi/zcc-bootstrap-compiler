#!/usr/bin/env python3
"""
Delta-debug failing gate sequences.
Input format:
{
  "seed": 123,
  "n_qubits": 2,
  "gate_sequence": [{"gate":"CNOT","qubits":[0,1]}, ...]
}
User must provide a checker module exposing:
  fails_case(case: dict) -> bool
"""
import argparse
import importlib
import json
from copy import deepcopy
from pathlib import Path

def ddmin(seq, fails):
    n = 2
    current = seq[:]
    while len(current) >= 2:
        chunk = max(1, len(current) // n)
        reduced = False
        for i in range(0, len(current), chunk):
            candidate = current[:i] + current[i+chunk:]
            if not candidate:
                continue
            if fails(candidate):
                current = candidate
                n = max(2, n - 1)
                reduced = True
                break
        if not reduced:
            if chunk == 1:
                break
            n = min(len(current), n * 2)
    return current

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in-case", required=True)
    ap.add_argument("--out-case", required=True)
    ap.add_argument("--checker-module", required=True, help="python module path, e.g. tests.qec_checker")
    args = ap.parse_args()

    case = json.loads(Path(args.in_case).read_text())
    checker = importlib.import_module(args.checker_module)

    def fails_seq(seq):
        c = deepcopy(case)
        c["gate_sequence"] = seq
        return checker.fails_case(c)

    original = case["gate_sequence"]
    if not fails_seq(original):
        raise SystemExit("Input case does not fail; nothing to minimize.")

    minimized = ddmin(original, fails_seq)
    out_case = deepcopy(case)
    out_case["minimized_gate_sequence"] = minimized
    Path(args.out_case).write_text(json.dumps(out_case, indent=2, sort_keys=True))
    print(f"Original length={len(original)}, minimized={len(minimized)}")

if __name__ == "__main__":
    main()
