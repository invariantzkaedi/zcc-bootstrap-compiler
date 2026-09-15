# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // DUAL-SILICON BENCHMARK GAUNTLET V3 🔱
=======================================================================================================
 Side-by-side empirical benchmark comparing:
   1. Single-GPU Baseline (NVIDIA GeForce RTX 5070 Laptop GPU in resident VRAM)
   2. Dual-Silicon Speculative Orchestration:
        - AMD Ryzen AI NPU Krackan (DirectML / 51.3 TOPS / 47.12 GB Shared Memory) Speculative Drafter
        - NVIDIA GeForce RTX 5070 Laptop GPU Resident Grammar Pushdown Verifier
   3. Native ZCC Compiler (Stages 1-5) Compilation + GCC Link + Execution Verification
=======================================================================================================
"""

import os
import sys
import time
import json
import subprocess
from pathlib import Path
from typing import List, Dict, Any

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

# Import orchestrator
from zkaedi_prime.dual_silicon_orchestrator import DualSiliconOrchestrator, print_telemetry_banner

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

def compile_and_run_zcc(c_code: str, test_id: str) -> Dict[str, Any]:
    """Compile C code using native ZCC compiler and execute binary via WSL."""
    build_dir = Path("build/gauntlet_v3")
    build_dir.mkdir(parents=True, exist_ok=True)
    src_path = build_dir / f"{test_id}.c"
    asm_path = build_dir / f"{test_id}.s"
    bin_path = build_dir / f"{test_id}.bin"

    src_path.write_text(c_code, encoding="utf-8")

    # WSL paths
    wsl_src = f"/mnt/h/__DOWNLOADS/zcc_github_upload/{src_path.as_posix()}"
    wsl_asm = f"/mnt/h/__DOWNLOADS/zcc_github_upload/{asm_path.as_posix()}"
    wsl_bin = f"/mnt/h/__DOWNLOADS/zcc_github_upload/{bin_path.as_posix()}"

    # Step 1: ZCC Stages 1-5 compilation to assembly
    compile_cmd = [
        "wsl", "-e", "bash", "-c",
        f"/mnt/h/__DOWNLOADS/zcc_github_upload/zcc {wsl_src} -o {wsl_asm}"
    ]
    p_comp = subprocess.run(compile_cmd, capture_output=True, text=True)
    comp_ok = (p_comp.returncode == 0) and asm_path.exists() and (asm_path.stat().st_size > 0)

    if not comp_ok:
        return {
            "passed": False,
            "stage": "ZCC_COMPILE",
            "output": p_comp.stderr or p_comp.stdout,
            "exit_code": p_comp.returncode
        }

    # Step 2: GCC assembly & link
    link_cmd = [
        "wsl", "-e", "bash", "-c",
        f"gcc -o {wsl_bin} {wsl_asm} -lm"
    ]
    p_link = subprocess.run(link_cmd, capture_output=True, text=True)
    link_ok = (p_link.returncode == 0) and bin_path.exists()

    if not link_ok:
        return {
            "passed": False,
            "stage": "GCC_LINK",
            "output": p_link.stderr or p_link.stdout,
            "exit_code": p_link.returncode
        }

    # Step 3: Execution
    exec_cmd = ["wsl", "-e", "bash", "-c", wsl_bin]
    t0 = time.time()
    p_exec = subprocess.run(exec_cmd, capture_output=True, text=True, timeout=5.0)
    exec_dur = time.time() - t0

    return {
        "passed": p_exec.returncode == 0,
        "stage": "EXECUTE",
        "output": p_exec.stdout.strip(),
        "exit_code": p_exec.returncode,
        "exec_dur_ms": round(exec_dur * 1000, 2)
    }

def run_benchmark():
    print("\n" + "=" * 90)
    print(" 🔱 ZKAEDI PRIME // DUAL-SILICON BENCHMARK GAUNTLET V3 🔱")
    print("=" * 90)

    orchestrator = DualSiliconOrchestrator()
    telemetry = orchestrator.get_hardware_telemetry()
    print_telemetry_banner(telemetry)

    results = []
    overall_t0 = time.time()

    for idx, wl in enumerate(GAUNTLET_WORKLOADS, 1):
        print(f"\n================================================================================")
        print(f" [{idx}/{len(GAUNTLET_WORKLOADS)}] BENCHMARKING: {wl['id']} ({wl['title']})")
        print(f"================================================================================")
        print(f"Prompt: {wl['prompt']}")

        # --- A. Single-GPU Baseline ---
        print("\n--- [SILICON 1 // SINGLE-GPU BASELINE] ---")
        t_gpu_start = time.time()
        single_gpu_req = {
            "action": "generate",
            "prompt": wl["prompt"],
            "mode": "CODE_GRAMMAR",
            "max_tokens": 384,
            "pure_code": True,
            "terminal_scope_clamp": True
        }
        gpu_single_resp = orchestrator.query_gpu_verifier(single_gpu_req)
        single_gpu_dur = time.time() - t_gpu_start
        single_gpu_code = gpu_single_resp.get("c_source") or gpu_single_resp.get("code", "") if gpu_single_resp else ""
        print(f"  Single-GPU Latency: {single_gpu_dur:.3f}s")

        # --- B. Dual-Silicon Speculative Mode ---
        print("\n--- [SILICON 2 // DUAL-SILICON SPECULATIVE ORCHESTRATION] ---")
        t_dual_start = time.time()
        dual_res = orchestrator.orchestrate_generation(wl["prompt"], max_tokens=384, k_window=4)
        dual_dur = time.time() - t_dual_start
        dual_code = dual_res.get("code", "")
        npu_lat = dual_res.get("npu_latency_ms", 0.0)
        drafts_proposed = dual_res.get("draft_tokens_proposed", 0)
        print(f"  Dual-Silicon Latency : {dual_dur:.3f}s (NPU Latency: {npu_lat:.2f}ms)")
        print(f"  Draft Tokens Proposed: {drafts_proposed}")

        # Speedup comparison
        speedup = single_gpu_dur / dual_dur if dual_dur > 0 else 1.0
        print(f"  Relative Speedup     : {speedup:.2f}x")

        # --- C. Native ZCC Compilation & Execution Verification ---
        print("\n--- [COMPILER VERIFICATION // ZCC STAGES 1-5] ---")
        zcc_eval = compile_and_run_zcc(dual_code, wl["id"])
        expected_sub = wl["expected_substring"]
        zcc_passed = zcc_eval.get("passed", False) and (expected_sub in zcc_eval.get("output", ""))

        if zcc_passed:
            print(f"  [PASS] Output verified: '{zcc_eval.get('output')}' (Exec time: {zcc_eval.get('exec_dur_ms')}ms)")
        else:
            print(f"  [FAIL] Stage: {zcc_eval.get('stage')} | Output: {zcc_eval.get('output')}")

        verdict = "PASS" if zcc_passed else "FAIL"

        results.append({
            "id": wl["id"],
            "title": wl["title"],
            "domain": wl["domain"],
            "single_gpu_time_s": round(single_gpu_dur, 3),
            "dual_silicon_time_s": round(dual_dur, 3),
            "speedup_factor": round(speedup, 2),
            "npu_latency_ms": round(npu_lat, 2),
            "draft_tokens": drafts_proposed,
            "expected": expected_sub,
            "actual_output": zcc_eval.get("output", ""),
            "exec_time_ms": zcc_eval.get("exec_dur_ms", 0.0),
            "verdict": verdict
        })

    total_time = time.time() - overall_t0
    total_passed = sum(1 for r in results if r["verdict"] == "PASS")
    avg_speedup = sum(r["speedup_factor"] for r in results) / len(results) if results else 1.0

    print("\n" + "=" * 105)
    print(" 🔱 DUAL-SILICON GAUNTLET V3 SUMMARY MATRIX 🔱")
    print("=" * 105)
    print(f"{'Workload ID':<24} | {'Single-GPU':<10} | {'Dual-Silicon':<12} | {'Speedup':<8} | {'NPU Lat':<8} | {'Verdict'}")
    print("-" * 105)
    for r in results:
        v_str = "✔ PASS" if r["verdict"] == "PASS" else "❌ FAIL"
        print(f"{r['id']:<24} | {r['single_gpu_time_s']:>7.3f}s   | {r['dual_silicon_time_s']:>8.3f}s    | {r['speedup_factor']:>6.2f}x  | {r['npu_latency_ms']:>6.2f}ms | {v_str}")
    print("-" * 105)
    print(f"Overall Result : {total_passed}/{len(results)} Passed ({total_passed/len(results)*100:.1f}%)")
    print(f"Average Speedup: {avg_speedup:.2f}x across AMD NPU + NVIDIA GPU")
    print(f"Total Time     : {total_time:.2f}s")
    print("=" * 105 + "\n")

    report_path = Path("reports/DUAL_SILICON_GAUNTLET_V3_REPORT.json")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps({
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "hardware": telemetry,
        "total_workloads": len(results),
        "total_passed": total_passed,
        "average_speedup": round(avg_speedup, 2),
        "total_time_sec": round(total_time, 3),
        "results": results
    }, indent=2), encoding="utf-8")
    print(f"Detailed benchmark report sealed to: {report_path.resolve()}\n")

    return total_passed == len(results)

if __name__ == "__main__":
    success = run_benchmark()
    sys.exit(0 if success else 1)
