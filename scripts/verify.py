#!/usr/bin/env python3
"""
ZKAEDI Secure Gate Verifier CLI
Executes verification commands safely with shell=False and tokenized argument arrays.
"""

import sys
import subprocess

GATES = [
    (
        "GATE-001",
        "bootstrap-fixed-point",
        ["bash", "-c", "cmp zcc2.s zcc3.s"]
    ),
    (
        "GATE-002",
        "rust-frontend-smoke",
        ["make", "rust-front-smoke"]
    ),
]

def run_gates():
    print("=" * 60)
    print("INVARIANT ZKAEDI SECURE GATE VERIFIER")
    print("=" * 60)
    passed = 0
    for gate_id, name, cmd_args in GATES:
        sys.stdout.write(f"[{gate_id}] {name:<30} ........ ")
        sys.stdout.flush()
        res = subprocess.run(cmd_args, shell=False, capture_output=True, text=True)
        if res.returncode == 0:
            print("\033[92mPASS\033[0m")
            passed += 1
        else:
            print("\033[91mFAIL\033[0m")
            print(f"  Error Log: {res.stderr.strip()}")
    print("-" * 60)
    print(f"Verification Summary: {passed}/{len(GATES)} gates passed.")
    if passed < len(GATES):
        sys.exit(1)

if __name__ == "__main__":
    run_gates()
