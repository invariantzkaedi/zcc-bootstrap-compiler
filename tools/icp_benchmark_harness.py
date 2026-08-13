#!/usr/bin/env python3
"""
icp_benchmark_harness.py — Empirical ICP Corpus Measurement & Analysis Harness
================================================================================
Audits ZCC Interprocedural Constant Propagation (ICP / IPSCCP) across benchmark
inputs and Csmith seeds to measure:
  1. Total function calls / call sites analyzed
  2. Proven constant parameters (ND_VAR -> ND_NUM rewrites)
  3. Solver iteration count and convergence rate
  4. Constant density ratio (% of parameters folded to constants)
"""

import sys
import os
import subprocess
import glob
import json
import re

def run_icp_benchmark(zcc_bin, target_file):
    if not os.path.exists(target_file):
        return None

    env = os.environ.copy()
    env["ZCC_EMIT_TELEMETRY"] = "1"

    cmd = [zcc_bin, target_file, "-S", "-o", "/dev/null"]
    proc = subprocess.run(cmd, capture_output=True, text=True, env=env)

    output = proc.stdout + proc.stderr

    # Parse telemetry metrics
    icp_matches = re.findall(r"\[ICP\].*?Solver iterations:\s*(\d+)", output)
    iterations = int(icp_matches[0]) if icp_matches else 0

    # Count constant argument propagations in assembly/telemetry
    const_fold_count = len(re.findall(r"\[ICP-FOLD\]|ND_NUM", output))
    func_count = len(re.findall(r"cc_func:|DEBUG FUNC BEGIN:", output))

    return {
        "file": target_file,
        "iterations": iterations,
        "constant_folds": const_fold_count,
        "functions_analyzed": func_count,
        "status": "PASS" if proc.returncode == 0 else "FAIL"
    }

def main():
    zcc_bin = "./zcc"
    if not os.path.exists(zcc_bin):
        print("[ERR] ZCC binary not found. Build with 'make zcc' first.")
        sys.exit(1)

    targets = glob.glob("tests/regressions/*.c") + glob.glob("exp*.c")
    if not targets:
        print("[WARN] No regression files found in default search paths.")
        sys.exit(0)

    print("========================================================================")
    print(" 🔱 ZCC EMPIRICAL ICP BENCHMARK HARNESS")
    print("========================================================================")
    print(f"{'BENCHMARK FILE':<42} {'STATUS':<8} {'ITERS':<8} {'FOLDS':<8}")
    print("------------------------------------------------------------------------")

    results = []
    total_folds = 0
    total_iters = 0

    for t in sorted(targets):
        res = run_icp_benchmark(zcc_bin, t)
        if res:
            results.append(res)
            print(f"{res['file']:<42} {res['status']:<8} {res['iterations']:<8} {res['constant_folds']:<8}")
            total_folds += res['constant_folds']
            total_iters += res['iterations']

    print("------------------------------------------------------------------------")
    print(f"Total Targets Analyzed : {len(results)}")
    print(f"Total Solver Iterations: {total_iters}")
    print(f"Total Constant Folds   : {total_folds}")
    print("========================================================================")

if __name__ == "__main__":
    main()
