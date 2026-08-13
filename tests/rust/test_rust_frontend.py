#!/usr/bin/env python3
"""
ZCC RUST FRONTEND SMOKE & INTEGRATION TEST HARNESS
==================================================
Verifies AST construction, IR emission, and execution of Rust programs
compiled via ZCC's native Rust frontend parser (part7_rust.c).
"""

import subprocess
import sys
import os
import tempfile

ZCC_BIN = "./zcc"

RUST_TEST_CASES = [
    {
        "name": "Rust Basic Return Value",
        "code": "fn main() -> i32 { return 42; }",
        "expected_exit": 42
    },
    {
        "name": "Rust Variable Assignment & Arithmetic",
        "code": "fn main() -> i32 { let x: i32 = 10; let y: i32 = 32; return x + y; }",
        "expected_exit": 42
    },
    {
        "name": "Rust Unary Arithmetic Negation (-x)",
        "code": "fn main() -> i32 { let x: i32 = 15; let y: i32 = -x; return y + 25; }",
        "expected_exit": 10
    },
    {
        "name": "Rust Logical OR (||) Short-Circuit",
        "code": "fn main() -> i32 { let a: i32 = 1; let b: i32 = 0; if a == 1 || b == 1 { return 99; } return 0; }",
        "expected_exit": 99
    },
    {
        "name": "Rust Logical AND (&&) Short-Circuit",
        "code": "fn main() -> i32 { let a: i32 = 1; let b: i32 = 1; if a == 1 && b == 1 { return 77; } return 0; }",
        "expected_exit": 77
    },
    {
        "name": "Rust While Loop Counter",
        "code": "fn main() -> i32 { let mut i: i32 = 0; let mut sum: i32 = 0; while i < 10 { sum = sum + i; i = i + 1; } return sum; }",
        "expected_exit": 45
    }
]

def run_tests():
    print("======================================================================")
    print("ZCC RUST FRONTEND SMOKE & INTEGRATION TEST HARNESS")
    print("======================================================================")

    if not os.path.exists(ZCC_BIN):
        print(f"ERROR: ZCC binary '{ZCC_BIN}' not found. Build ZCC first.")
        sys.exit(1)

    passed = 0
    failed = 0

    for i, test in enumerate(RUST_TEST_CASES, 1):
        name = test["name"]
        code = test["code"]
        expected_exit = test["expected_exit"]

        with tempfile.NamedTemporaryFile(suffix=".rs", mode="w", delete=False) as src_file:
            src_file.write(code)
            src_path = src_file.name

        asm_path = src_path + ".s"
        bin_path = src_path + ".bin"

        try:
            # Step 1: Compile Rust source to x86-64 assembly using ZCC --rust-backend-v1
            comp_res = subprocess.run([ZCC_BIN, "--rust-backend-v1", src_path, "-o", asm_path],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            if comp_res.returncode != 0:
                print(f"[TEST {i:2d}] {name:<45}: FAIL (ZCC Rust compile error)")
                print("  STDERR:", comp_res.stderr.strip()[:200])
                failed += 1
                continue

            # Step 2: Assemble assembly to binary using Host GCC
            gcc_res = subprocess.run(["gcc", asm_path, "-o", bin_path],
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            if gcc_res.returncode != 0:
                print(f"[TEST {i:2d}] {name:<45}: FAIL (GCC assemble error)")
                print("  STDERR:", gcc_res.stderr.strip()[:200])
                failed += 1
                continue

            # Step 3: Run compiled Rust binary and check exit status
            exec_res = subprocess.run([bin_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            actual_exit = exec_res.returncode

            if actual_exit == expected_exit:
                print(f"[TEST {i:2d}] {name:<45}: PASS (Exit Code: {actual_exit})")
                passed += 1
            else:
                print(f"[TEST {i:2d}] {name:<45}: FAIL (Expected exit {expected_exit}, got {actual_exit})")
                failed += 1

        finally:
            for p in [src_path, asm_path, bin_path]:
                if os.path.exists(p):
                    try: os.remove(p)
                    except: pass

    print("======================================================================")
    print(f"RUST FRONTEND RESULTS: {passed} Passed, {failed} Failed")
    print("======================================================================")

    if failed > 0:
        sys.exit(1)

if __name__ == "__main__":
    run_tests()
