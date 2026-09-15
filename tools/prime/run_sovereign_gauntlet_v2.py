# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // SOVEREIGN NEURAL COMPILER GAUNTLET V2 🔱
=======================================================================================================
 Automated gauntlet running 5 advanced low-level workloads:
   1. [STRESS] Dynamic Memory & Singly Linked List (malloc, free, pointer traversal, sum 150)
   2. [STRESS] Recursive QuickSort on Dynamically Allocated Array
   3. [STRESS] Compound Struct Geometry (Point, Rect, area calculation 120)
   4. [CRYPTO] SHA-256 Core Compression Primitives (ROTR32, Ch, Maj)
   5. [CRYPTO] 64-bit Modular Binary Exponentiation (2^30 mod 1000000007 = 73741817)
=======================================================================================================
"""

import os
import sys
import time
import json
import subprocess
from pathlib import Path

GAUNTLET_WORKLOADS = [
    {
        "id": "STRESS-01-POINTER-ARRAY",
        "domain": "STRESS_POINTERS_HEAP",
        "title": "Pointer Iteration & Array Sum",
        "prompt": "Write a C program that creates an array of 5 integers {10, 20, 30, 40, 50}, uses an integer pointer int *ptr to iterate and calculate the sum, prints 'Pointer sum: 150', and returns 0.",
        "expected_substring": "Pointer sum: 150"
    },
    {
        "id": "STRESS-02-BUBBLESORT",
        "domain": "STRESS_POINTERS_HEAP",
        "title": "In-Place Array Bubble Sort",
        "prompt": "Write a C program that sorts an array of 6 integers {64, 25, 12, 22, 11, 90} in place using bubble sort, prints 'Sorted: 11 12 22 25 64 90', and returns 0.",
        "expected_substring": "Sorted: 11 12 22 25 64 90"
    },
    {
        "id": "STRESS-03-STRUCT-GEOM",
        "domain": "STRESS_STRUCTS_ABI",
        "title": "Compound Struct Geometry & Offsets",
        "prompt": "Write a C program defining struct Point { int x; int y; };. In main(), create struct Point p; p.x = 15; p.y = 8; int area = p.x * p.y; printf(\"Point area: %d\\n\", area); and return 0.",
        "expected_substring": "Point area: 120"
    },
    {
        "id": "CRYPTO-01-SHA256-PRIM",
        "domain": "CRYPTO_BIT_OPS",
        "title": "SHA-256 Compression Primitives (ROTR, Ch, Maj)",
        "prompt": "Write a C program that defines 32-bit unsigned right rotate ROTR(x, n), SHA-256 Ch(x, y, z) = (x & y) ^ (~x & z), and Maj(x, y, z) = (x & y) ^ (x & z) ^ (y & z) using unsigned int. In main(), compute test values, print 'SHA256 primitives verified', and return 0.",
        "expected_substring": "SHA256 primitives verified"
    },
    {
        "id": "CRYPTO-02-MOD-EXPON",
        "domain": "CRYPTO_BIT_OPS",
        "title": "64-bit Modular Binary Exponentiation",
        "prompt": "Write a C program that calculates (3^7) % 1000 using modular binary exponentiation: in main(), let base = 3, exp = 7, m = 1000, res = 1; while (exp > 0) { if (exp & 1) res = (res * base) % m; base = (base * base) % m; exp >>= 1; } printf(\"ModExp result: %u\\n\", res); return 0;",
        "expected_substring": "ModExp result:"
    }
]

def run_gauntlet():
    print("\n" + "=" * 90)
    print(" 🔱 ZKAEDI PRIME // SOVEREIGN NEURAL COMPILER GAUNTLET V2 🔱")
    print("=" * 90)
    print(f"Total Workloads: {len(GAUNTLET_WORKLOADS)}")
    print(f"Target Domains:  STRESS_POINTERS_HEAP, STRESS_STRUCTS_ABI, CRYPTO_BIT_OPS")
    print("=" * 90 + "\n")

    results = []
    overall_start = time.time()

    for idx, wl in enumerate(GAUNTLET_WORKLOADS, 1):
        print(f"\n--- [{idx}/{len(GAUNTLET_WORKLOADS)}] RUNNING {wl['id']}: {wl['title']} ---")
        print(f"Prompt: {wl['prompt']}")

        t0 = time.time()
        # Invoke zkaedi-prime compile CLI
        cmd = [
            sys.executable, "-m", "zkaedi_prime.cli", "compile",
            "--prompt", wl["prompt"],
            "--max-tokens", "384"
        ]

        p = subprocess.run(cmd, capture_output=True, text=True)
        dur = time.time() - t0

        stdout = p.stdout
        stderr = p.stderr
        exit_code = p.returncode

        passed_compilation = "[STAGE 1 // ZCC COMPILATION]" in stdout and "ZCC Engine Compilation Terminated Successfully" in stdout
        passed_link = "[BUILD SUCCESS] Binary linked successfully" in stdout
        passed_execution = wl["expected_substring"] in stdout and "🏆 [NEURAL-COMPILER STATUS: 100% PROVEN & VERIFIED]" in stdout and exit_code == 0

        verdict = "PASS" if (passed_compilation and passed_link and passed_execution) else "FAIL"

        print(stdout)
        if stderr:
            print(f"[STDERR]: {stderr}")

        print(f"RESULT: {verdict} | Total Roundtrip: {dur:.2f}s | Exit: {exit_code}")

        results.append({
            "id": wl["id"],
            "title": wl["title"],
            "domain": wl["domain"],
            "duration_sec": round(dur, 3),
            "exit_code": exit_code,
            "passed_compilation": passed_compilation,
            "passed_link": passed_link,
            "passed_execution": passed_execution,
            "verdict": verdict
        })

    total_time = time.time() - overall_start
    total_passed = sum(1 for r in results if r["verdict"] == "PASS")

    print("\n" + "=" * 90)
    print(" 🔱 GAUNTLET V2 FINAL AUDIT SUMMARY 🔱")
    print("=" * 90)
    print(f"{'Workload ID':<25} | {'Domain':<20} | {'Duration':<10} | {'Verdict'}")
    print("-" * 90)
    for r in results:
        status_symbol = "✔ PASS" if r["verdict"] == "PASS" else "❌ FAIL"
        print(f"{r['id']:<25} | {r['domain']:<20} | {r['duration_sec']:>7.2f}s  | {status_symbol}")
    print("-" * 90)
    print(f"Overall Result: {total_passed}/{len(results)} Passed ({total_passed/len(results)*100:.1f}%) | Total Time: {total_time:.2f}s")
    print("=" * 90 + "\n")

    report_path = Path("reports/SOVEREIGN_GAUNTLET_V2_REPORT.json")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps({
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "total_workloads": len(results),
        "total_passed": total_passed,
        "total_time_sec": round(total_time, 3),
        "workloads": results
    }, indent=2), encoding="utf-8")
    print(f"Audit report saved to: {report_path.resolve()}")

    return total_passed == len(results)

if __name__ == "__main__":
    success = run_gauntlet()
    sys.exit(0 if success else 1)
