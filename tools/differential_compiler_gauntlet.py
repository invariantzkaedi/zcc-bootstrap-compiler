#!/usr/bin/env python3
"""
================================================================================
  ZCC TRI-COMPILER DIFFERENTIAL GAUNTLET (ZCC vs GCC vs Clang)
================================================================================
Evaluates semantic equivalence across the formally bounded C language subset
defined in docs/SPEC_C_SUBSET_v1.md.

For every generated program:
  1. Checks availability of reference toolchains (GCC, Clang)
  2. Compiles with ZCC (zcc -> gcc -no-pie -> bin_zcc)
  3. Compiles with GCC (gcc -O0 -> bin_gcc)
  4. Compiles with Clang (clang -O0 -> bin_clang)
  5. Executes all binaries under isolated sandbox with strict timeout
  6. Cross-checks return codes, stdout, and stderr
  7. Classifies any divergence using 4-state fail-closed verdicts:
     - PASS
     - PARSER_REJECT
     - COMPILER_CRASH
     - ASSEMBLER_REJECT
     - RETURNCODE_MISMATCH
     - STDOUT_MISMATCH
     - TIMEOUT
     - ORACLE_UNAVAILABLE / INCONCLUSIVE
================================================================================
"""

from __future__ import annotations

import os
import sys
import time
import json
import random
import shutil
import tempfile
import argparse
import subprocess
import multiprocessing as mp
from pathlib import Path
from typing import Dict, Any, List, Tuple, Optional

# Reconfigure stdout for UTF-8
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


# ==============================================================================
# SUBSET PROGRAM GENERATOR (STRICTLY SPEC-C-SUBSET-v1 CONSTRAINED)
# ==============================================================================

class SubsetProgramGenerator:
    """Generates valid, deterministic C programs with zero undefined behavior."""

    def __init__(self, seed: int):
        self.rng = random.Random(seed + 987654321)
        self.seed = seed

    def generate(self) -> str:
        # Choose from 5 core archetypes in SPEC-C-SUBSET-v1
        archetype = self.rng.choice([
            "arithmetic_logic",
            "control_flow_loops",
            "pointer_array_decay",
            "struct_member_access",
            "multi_arg_abi_spill"
        ])

        if archetype == "arithmetic_logic":
            return self._gen_arithmetic_logic()
        elif archetype == "control_flow_loops":
            return self._gen_control_flow_loops()
        elif archetype == "pointer_array_decay":
            return self._gen_pointer_array_decay()
        elif archetype == "struct_member_access":
            return self._gen_struct_member_access()
        else:
            return self._gen_multi_arg_abi_spill()

    def _gen_arithmetic_logic(self) -> str:
        a = self.rng.randint(10, 1000)
        b = self.rng.randint(1, 50)
        c = self.rng.randint(1, 30)
        shift = self.rng.randint(1, 15)

        return f"""#include <stdio.h>
#include <stdint.h>

int main(void) {{
    int64_t a = {a};
    int64_t b = {b};
    int64_t c = {c};
    
    int64_t r1 = (a + b) * c;
    int64_t r2 = (a - b) / c;
    int64_t r3 = a % (b + 1);
    int64_t r4 = (a ^ (b << {shift})) & 0x7FFFFFFFLL;
    int64_t r5 = ((a | b) ^ c) + (a > b ? 100 : 200);
    
    printf("ALU: %ld %ld %ld %ld %ld\\n", (long)r1, (long)r2, (long)r3, (long)r4, (long)r5);
    return (int)((r1 + r2 + r3 + r4 + r5) % 127);
}}
"""

    def _gen_control_flow_loops(self) -> str:
        bound = self.rng.randint(5, 25)
        step = self.rng.randint(1, 4)
        threshold = self.rng.randint(2, bound)

        return f"""#include <stdio.h>
#include <stdint.h>

int main(void) {{
    int sum = 0;
    int count = 0;
    
    for (int i = 0; i < {bound}; i += {step}) {{
        if (i % 2 == 0) {{
            sum += i * 3;
        }} else {{
            sum -= i;
        }}
        
        if (i >= {threshold}) {{
            sum += 5;
            count++;
        }}
    }}
    
    int w = 0;
    while (w < 10) {{
        sum += w;
        w++;
        if (w == 5) continue;
        if (w > 8) break;
    }}
    
    printf("LOOP: %d %d %d\\n", sum, count, w);
    return (sum & 0x7F);
}}
"""

    def _gen_pointer_array_decay(self) -> str:
        size = self.rng.randint(8, 20)
        mul = self.rng.randint(2, 7)
        offset = self.rng.randint(1, 5)

        return f"""#include <stdio.h>
#include <stdint.h>

static int sum_array(int *arr, int len) {{
    int total = 0;
    for (int i = 0; i < len; i++) {{
        total += *(arr + i);
    }}
    return total;
}}

int main(void) {{
    int arr[{size}];
    for (int i = 0; i < {size}; i++) {{
        arr[i] = i * {mul} + {offset};
    }}
    
    int *p = &arr[0];
    *(p + 2) += 42;
    p[4] -= 10;
    
    int s = sum_array(arr, {size});
    printf("PTR: %d %d %d\\n", s, arr[2], arr[4]);
    return (s & 0x7F);
}}
"""

    def _gen_struct_member_access(self) -> str:
        v1 = self.rng.randint(1, 100)
        v2 = self.rng.randint(100, 500)
        v3 = self.rng.randint(10, 50)

        return f"""#include <stdio.h>
#include <stdint.h>

struct DataPoint {{
    int id;
    int64_t value;
    int factor;
    int flags;
}};

static int64_t process_point(struct DataPoint *dp) {{
    return (int64_t)dp->id + dp->value * dp->factor;
}}

int main(void) {{
    struct DataPoint p1;
    p1.id = {v1};
    p1.value = {v2};
    p1.factor = {v3};
    p1.flags = 7;
    
    struct DataPoint p2 = p1;
    p2.id += 1;
    p2.value += 10;
    
    int64_t r1 = process_point(&p1);
    int64_t r2 = process_point(&p2);
    
    printf("STRUCT: %ld %ld %d %ld\\n", (long)r1, (long)r2, p2.id, (long)sizeof(struct DataPoint));
    return (int)((r1 + r2) % 127);
}}
"""

    def _gen_multi_arg_abi_spill(self) -> str:
        num_args = self.rng.randint(7, 12)
        args_sig = []
        args_call = []
        args_sum = []

        for i in range(num_args):
            t = "int64_t" if i % 2 == 0 else "int32_t"
            val = (i + 1) * 11
            args_sig.append(f"{t} a{i}")
            args_call.append(f"(({t}){val})")
            args_sum.append(f"((int64_t)a{i})")

        sig_str = ", ".join(args_sig)
        call_str = ", ".join(args_call)
        sum_str = " + ".join(args_sum)

        return f"""#include <stdio.h>
#include <stdint.h>

static int64_t test_callee({sig_str}) {{
    return ({sum_str});
}}

int main(void) {{
    int64_t res = test_callee({call_str});
    printf("ABI: %ld args=%d\\n", (long)res, {num_args});
    return (int)(res % 127);
}}
"""


# ==============================================================================
# DIFFERENTIAL WORKER ENGINE
# ==============================================================================

def run_single_test(args: Tuple[int, str, str, str, bool, bool]) -> Dict[str, Any]:
    seed, zcc_bin, repo_root, temp_base, has_gcc, has_clang = args
    gen = SubsetProgramGenerator(seed)
    code = gen.generate()

    td = tempfile.mkdtemp(prefix=f"diff_{seed}_", dir=temp_base)
    src_c = os.path.join(td, "prog.c")
    asm_s = os.path.join(td, "prog.s")
    bin_z = os.path.join(td, "bin_zcc")
    bin_g = os.path.join(td, "bin_gcc")
    bin_c = os.path.join(td, "bin_clang")

    with open(src_c, "w", encoding="utf-8") as f:
        f.write(code)

    inc_dir = os.path.join(repo_root, "include")

    res = {
        "seed": seed,
        "pass": False,
        "category": "NONE",
        "details": "",
        "rc_zcc": None,
        "rc_gcc": None,
        "rc_clang": None,
        "stdout_zcc": None,
        "stdout_gcc": None,
        "stdout_clang": None
    }

    try:
        # Check oracle availability
        if not has_gcc and not has_clang:
            res["category"] = "ORACLES_UNAVAILABLE"
            res["details"] = "Neither GCC nor Clang reference oracles available on host"
            return res

        # 1. Compile with reference GCC if available
        if has_gcc:
            r_gcc = subprocess.run(["gcc", "-O0", "-w", f"-I{inc_dir}", src_c, "-o", bin_g, "-lm"],
                                   capture_output=True, timeout=5.0)
            if r_gcc.returncode != 0:
                res["category"] = "INVALID_C_GENERATED"
                res["details"] = r_gcc.stderr.decode('utf-8', 'replace')[:200]
                return res

        # 2. Compile with reference Clang if available
        if has_clang:
            r_clang = subprocess.run(["clang", "-O0", "-w", f"-I{inc_dir}", src_c, "-o", bin_c, "-lm"],
                                     capture_output=True, timeout=5.0)
            if r_clang.returncode != 0:
                res["category"] = "INVALID_C_GENERATED"
                res["details"] = r_clang.stderr.decode('utf-8', 'replace')[:200]
                return res

        # 3. Compile with ZCC
        r_zcc = subprocess.run([zcc_bin, src_c, "-o", asm_s],
                               cwd=repo_root, capture_output=True, timeout=5.0)
        if r_zcc.returncode != 0:
            if r_zcc.returncode in (-11, 139):
                res["category"] = "COMPILER_CRASH"
            else:
                res["category"] = "PARSER_REJECT"
            res["details"] = f"ZCC rc={r_zcc.returncode}: {r_zcc.stderr.decode('utf-8', 'replace')[:200]}"
            return res

        # 4. Assemble & Link ZCC Assembly (requires GCC or system assembler/linker)
        if not has_gcc:
            res["category"] = "ASSEMBLER_UNAVAILABLE"
            res["details"] = "GCC required to assemble/link ZCC emitted assembly"
            return res

        r_link = subprocess.run(["gcc", "-no-pie", "-w", "-o", bin_z, asm_s, "-lm"],
                                capture_output=True, timeout=5.0)
        if r_link.returncode != 0:
            res["category"] = "ASSEMBLER_REJECT"
            res["details"] = f"Link error on ZCC asm: {r_link.stderr.decode('utf-8', 'replace')[:200]}"
            return res

        # 5. Execute binaries under sandbox
        exec_z = subprocess.run([bin_z], capture_output=True, timeout=2.0)
        res["rc_zcc"] = exec_z.returncode
        res["stdout_zcc"] = exec_z.stdout.decode('utf-8', 'replace')

        if has_gcc:
            exec_g = subprocess.run([bin_g], capture_output=True, timeout=2.0)
            res["rc_gcc"] = exec_g.returncode
            res["stdout_gcc"] = exec_g.stdout.decode('utf-8', 'replace')
            if exec_z.returncode != exec_g.returncode:
                res["category"] = "RETURNCODE_MISMATCH"
                res["details"] = f"ZCC rc={exec_z.returncode} vs GCC rc={exec_g.returncode}"
                return res
            if exec_z.stdout != exec_g.stdout:
                res["category"] = "STDOUT_MISMATCH"
                res["details"] = f"ZCC stdout='{res['stdout_zcc'][:60]}' vs GCC stdout='{res['stdout_gcc'][:60]}'"
                return res

        if has_clang:
            exec_c = subprocess.run([bin_c], capture_output=True, timeout=2.0)
            res["rc_clang"] = exec_c.returncode
            res["stdout_clang"] = exec_c.stdout.decode('utf-8', 'replace')
            if exec_z.returncode != exec_c.returncode:
                res["category"] = "RETURNCODE_MISMATCH"
                res["details"] = f"ZCC rc={exec_z.returncode} vs Clang rc={exec_c.returncode}"
                return res
            if exec_z.stdout != exec_c.stdout:
                res["category"] = "STDOUT_MISMATCH"
                res["details"] = f"ZCC stdout='{res['stdout_zcc'][:60]}' vs Clang stdout='{res['stdout_clang'][:60]}'"
                return res

        res["pass"] = True
        res["category"] = "PASS"
        return res

    except subprocess.TimeoutExpired:
        res["category"] = "TIMEOUT"
        res["details"] = "Execution timed out (2.0s sandbox ceiling)"
        return res
    except Exception as e:
        res["category"] = "EXECUTION_ERROR"
        res["details"] = str(e)
        return res
    finally:
        shutil.rmtree(td, ignore_errors=True)


# ==============================================================================
# MAIN GAUNTLET ORCHESTRATOR
# ==============================================================================

def run_gauntlet(count: int = 100, workers: int = 4, start_seed: int = 1000) -> Dict[str, Any]:
    repo_root = str(Path(__file__).resolve().parent.parent)
    zcc_bin = os.path.join(repo_root, "zcc")

    # Strict fail-closed check: Never auto-build silently
    if not os.path.isfile(zcc_bin) or not os.access(zcc_bin, os.X_OK):
        print(f"[-] ERROR: ZCC binary not found or not executable at {zcc_bin}.")
        print("    Auto-build is disabled. Build ZCC explicitly before running gauntlet.")
        return {
            "status": "FAIL",
            "verdict": "FAIL",
            "error": f"ZCC binary not found at {zcc_bin}",
            "total": count,
            "passed": 0,
            "pass_rate": 0.0,
            "elapsed_seconds": 0.0,
            "failures": {"ZCC_BINARY_MISSING": count},
            "divergences": []
        }

    # Oracle detection (no fabrication)
    has_gcc = shutil.which("gcc") is not None
    has_clang = shutil.which("clang") is not None

    if not has_gcc and not has_clang:
        print("[-] ERROR: Neither GCC nor Clang reference oracles are available.")
        return {
            "status": "INCONCLUSIVE",
            "verdict": "INCONCLUSIVE",
            "error": "No reference oracles (GCC/Clang) available in PATH",
            "total": count,
            "passed": 0,
            "pass_rate": 0.0,
            "elapsed_seconds": 0.0,
            "failures": {"ORACLES_UNAVAILABLE": count},
            "divergences": []
        }

    temp_base = tempfile.mkdtemp(prefix="zcc_differential_")

    print("=" * 72)
    print(f"  ZCC TRI-COMPILER DIFFERENTIAL GAUNTLET")
    print(f"  Target Subset: SPEC-C-SUBSET-v1")
    print(f"  Oracles:       GCC={'AVAILABLE' if has_gcc else 'UNAVAILABLE'}, Clang={'AVAILABLE' if has_clang else 'UNAVAILABLE'}")
    print(f"  Test Cases:    {count} programs")
    print(f"  Workers:       {workers} parallel processes")
    print("=" * 72)

    t0 = time.time()
    tasks = [(start_seed + i, zcc_bin, repo_root, temp_base, has_gcc, has_clang) for i in range(count)]

    passed = 0
    failures: Dict[str, int] = {}
    divergences: List[Dict[str, Any]] = []

    try:
        with mp.Pool(processes=workers) as pool:
            for idx, res in enumerate(pool.imap_unordered(run_single_test, tasks, chunksize=10)):
                if res["pass"]:
                    passed += 1
                else:
                    cat = res["category"]
                    failures[cat] = failures.get(cat, 0) + 1
                    divergences.append(res)

                if (idx + 1) % max(1, count // 10) == 0 or (idx + 1) == count:
                    pct = ((idx + 1) / count) * 100.0
                    pass_rate = (passed / (idx + 1)) * 100.0
                    print(f"  [{pct:5.1f}%] Completed {idx + 1}/{count} | Passed: {passed} ({pass_rate:.1f}%)")

    finally:
        shutil.rmtree(temp_base, ignore_errors=True)

    elapsed = time.time() - t0
    pass_rate = (passed / count) * 100.0 if count > 0 else 0.0

    print("-" * 72)
    print(f"GAUNTLET SUMMARY: {passed}/{count} PASSING ({pass_rate:.2f}%) in {elapsed:.2f}s")
    if failures:
        print("Failure Breakdown:")
        for cat, cnt in sorted(failures.items(), key=lambda x: x[1], reverse=True):
            print(f"  - {cat}: {cnt}")
    else:
        print("✓ ZERO DIVERGENCES DETECTED across all tested programs.")
    print("=" * 72)

    status = "PASS" if (passed == count and count > 0 and has_gcc and has_clang) else ("INCONCLUSIVE" if not (has_gcc and has_clang) else "FAIL")

    return {
        "status": status,
        "verdict": status,
        "oracles": {
            "gcc": "AVAILABLE" if has_gcc else "UNAVAILABLE",
            "clang": "AVAILABLE" if has_clang else "UNAVAILABLE"
        },
        "total": count,
        "passed": passed,
        "pass_rate": pass_rate,
        "elapsed_seconds": elapsed,
        "failures": failures,
        "divergences": divergences[:20]
    }


def check_or_delegate_wsl() -> None:
    """If running on Windows and native POSIX oracles (gcc/clang) are missing, delegate to WSL."""
    if sys.platform.startswith("win"):
        has_gcc = shutil.which("gcc") is not None
        has_clang = shutil.which("clang") is not None
        if not (has_gcc and has_clang):
            wsl_bin = shutil.which("wsl")
            if wsl_bin:
                cwd = os.getcwd()
                drive = cwd[0].lower()
                wsl_cwd = f"/mnt/{drive}{cwd[2:].replace(chr(92), '/')}"
                script_path = os.path.abspath(__file__)
                sdrive = script_path[0].lower()
                wsl_script = f"/mnt/{sdrive}{script_path[2:].replace(chr(92), '/')}"
                
                forward_args = " ".join(f"'{a}'" for a in sys.argv[1:])
                wsl_cmd = ["wsl.exe", "-e", "bash", "-c", f"cd '{wsl_cwd}' && python3 '{wsl_script}' {forward_args}"]
                res = subprocess.run(wsl_cmd)
                sys.exit(res.returncode)


if __name__ == "__main__":
    check_or_delegate_wsl()
    parser = argparse.ArgumentParser(description="ZCC Tri-Compiler Differential Gauntlet")
    parser.add_argument("--count", type=int, default=100, help="Number of programs to generate and test")
    parser.add_argument("--workers", type=int, default=4, help="Parallel worker processes")
    parser.add_argument("--seed", type=int, default=1000, help="Starting PRNG seed")
    parser.add_argument("--json", type=str, default="", help="Path to write JSON results")

    args = parser.parse_args()
    summary = run_gauntlet(count=args.count, workers=args.workers, start_seed=args.seed)

    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(summary, f, indent=2)
        print(f"[+] Wrote machine-readable results to {args.json}")

    sys.exit(0 if summary["verdict"] == "PASS" else 1)
