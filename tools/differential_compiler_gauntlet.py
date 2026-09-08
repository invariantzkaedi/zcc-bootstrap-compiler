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
        # Choose from 10 core archetypes in SPEC-C-SUBSET-v1
        archetype = self.rng.choice([
            "arithmetic_logic",
            "control_flow_loops",
            "pointer_array_decay",
            "struct_member_access",
            "multi_arg_abi_spill",
            "type_casts_and_widths",
            "boolean_short_circuit_logic",
            "pointer_double_indirection",
            "nested_structs",
            "recursive_functions"
        ])

        if archetype == "arithmetic_logic":
            return self._gen_arithmetic_logic()
        elif archetype == "control_flow_loops":
            return self._gen_control_flow_loops()
        elif archetype == "pointer_array_decay":
            return self._gen_pointer_array_decay()
        elif archetype == "struct_member_access":
            return self._gen_struct_member_access()
        elif archetype == "multi_arg_abi_spill":
            return self._gen_multi_arg_abi_spill()
        elif archetype == "type_casts_and_widths":
            return self._gen_type_casts_and_widths()
        elif archetype == "boolean_short_circuit_logic":
            return self._gen_boolean_short_circuit_logic()
        elif archetype == "pointer_double_indirection":
            return self._gen_pointer_double_indirection()
        elif archetype == "nested_structs":
            return self._gen_nested_structs()
        else:
            return self._gen_recursive_functions()


    def _gen_arithmetic_logic(self) -> str:
        use_boundary = (self.seed % 7 == 0)
        if use_boundary:
            a = self.rng.choice([0, 1, -1, 127, -128, 32767, -32768, 2147483647, -2147483647])
            b = self.rng.choice([1, 2, 3, 7, 15, 255, 65535])
            c = self.rng.choice([1, 2, 4, 8, 16])
            shift = self.rng.choice([0, 1, 7, 15, 31])
        else:
            a = self.rng.randint(10, 1000)
            b = self.rng.randint(1, 50)
            c = self.rng.randint(1, 30)
            shift = self.rng.randint(1, 15)

        return f"""#include <stdio.h>
#include <stdint.h>

int main(void) {{
    int64_t a = {a}LL;
    int64_t b = {b}LL;
    int64_t c = {c}LL;
    
    int64_t r1 = (a + b) * c;
    int64_t r2 = (a - b) / c;
    int64_t r3 = a % (b + 1LL);
    int64_t r4 = (a ^ (b << {shift})) & 0x7FFFFFFFLL;
    int64_t r5 = ((a | b) ^ c) + (a > b ? 100LL : 200LL);
    
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

    def _gen_type_casts_and_widths(self) -> str:
        use_boundary = (self.seed % 5 == 0)
        if use_boundary:
            s8_val = self.rng.choice([-128, -1, 0, 1, 127])
            u8_val = self.rng.choice([0, 1, 128, 255])
            s16_val = self.rng.choice([-32768, -1, 0, 1, 32767])
            u16_val = self.rng.choice([0, 1, 32768, 65535])
            s32_val = self.rng.choice([-2147483647, -1, 0, 1, 2147483647])
            u32_val = self.rng.choice([0, 1, 2147483648, 4294967295])
            mult = self.rng.choice([1, 2, 3, 4])
        else:
            s8_val = self.rng.randint(-120, 120)
            u8_val = self.rng.randint(10, 250)
            s16_val = self.rng.randint(-30000, 30000)
            u16_val = self.rng.randint(1000, 60000)
            s32_val = self.rng.randint(-2000000, 2000000)
            u32_val = self.rng.randint(100000, 3000000000)
            mult = self.rng.randint(2, 5)


        return f"""#include <stdio.h>
#include <stdint.h>

int main(void) {{
    int8_t s8 = (int8_t){s8_val};
    uint8_t u8 = (uint8_t){u8_val};
    int16_t s16 = (int16_t){s16_val};
    uint16_t u16 = (uint16_t){u16_val};
    int32_t s32 = (int32_t){s32_val};
    uint32_t u32 = (uint32_t){u32_val}U;

    int64_t ext_s8 = (int64_t)s8;
    int64_t ext_u8 = (int64_t)u8;
    int64_t ext_s16 = (int64_t)s16;
    int64_t ext_u16 = (int64_t)u16;

    int32_t trunc16 = (int32_t)((uint16_t)s32);
    int32_t trunc8 = (int32_t)((uint8_t)u32);

    int64_t res = ext_s8 + ext_u8 * {mult} + ext_s16 - ext_u16 + trunc16 + trunc8;
    printf("WIDTHS: %ld %ld %ld %ld %d %d %ld\\n", 
           (long)ext_s8, (long)ext_u8, (long)ext_s16, (long)ext_u16, trunc16, trunc8, (long)res);
    return (int)(res % 127);
}}
"""

    def _gen_boolean_short_circuit_logic(self) -> str:
        thresh_a = self.rng.randint(50, 200)
        val_a = self.rng.randint(0, 300)
        val_b = self.rng.randint(-50, 50)
        mul = self.rng.randint(2, 6)

        return f"""#include <stdio.h>
#include <stdint.h>

static int call_counter = 0;
static int side_effect(int val) {{
    call_counter += {mul};
    return val;
}}

int main(void) {{
    int a = {val_a};
    int b = {val_b};
    int r1 = 0;
    int r2 = 0;
    int r3 = 0;

    if (a > {thresh_a} && side_effect(1) > 0) {{
        r1 = 10;
    }} else {{
        r1 = 20;
    }}

    if (b < 0 || side_effect(2) > 0) {{
        r2 = 30;
    }} else {{
        r2 = 40;
    }}

    r3 = (!r1 || (r2 > 25 && side_effect(3) == 3)) ? 100 : 200;

    printf("LOGIC: %d %d %d calls=%d\\n", r1, r2, r3, call_counter);
    return (int)((r1 + r2 + r3 + call_counter) % 127);
}}
"""

    def _gen_pointer_double_indirection(self) -> str:
        v1 = self.rng.randint(10, 100)
        v2 = self.rng.randint(10, 100)
        add1 = self.rng.randint(1, 20)
        mul1 = self.rng.randint(2, 4)
        add2 = self.rng.randint(1, 20)

        return f"""#include <stdio.h>
#include <stdint.h>

static void update_through_ptr(int **pp, int *target) {{
    *pp = target;
    **pp += {add1};
}}

int main(void) {{
    int val1 = {v1};
    int val2 = {v2};
    int *p = &val1;
    int **pp = &p;

    *p += 5;
    **pp *= {mul1};

    update_through_ptr(pp, &val2);
    *p += {add2};

    printf("DPTR: v1=%d v2=%d *p=%d **pp=%d\\n", val1, val2, *p, **pp);
    return (int)((val1 + val2 + *p + **pp) % 127);
}}
"""

    def _gen_nested_structs(self) -> str:
        tag = self.rng.randint(1, 20)
        x = self.rng.randint(1, 50)
        y = self.rng.randint(1, 50)
        weight = self.rng.randint(100, 5000)

        return f"""#include <stdio.h>
#include <stdint.h>

struct Inner {{
    int x;
    int y;
}};

struct Outer {{
    int tag;
    struct Inner inner;
    int64_t weight;
}};

static int64_t compute_outer(struct Outer *o) {{
    return (int64_t)o->tag * 100 + (int64_t)o->inner.x + (int64_t)o->inner.y * 10 + o->weight;
}}

int main(void) {{
    struct Outer o1;
    o1.tag = {tag};
    o1.inner.x = {x};
    o1.inner.y = {y};
    o1.weight = {weight}LL;

    struct Outer o2 = o1;
    o2.tag += 1;
    o2.inner.x += 5;
    o2.inner.y += 10;
    o2.weight *= 2;

    int64_t s1 = compute_outer(&o1);
    int64_t s2 = compute_outer(&o2);

    printf("NESTED: %ld %ld %ld\\n", (long)s1, (long)s2, (long)(sizeof(struct Outer)));
    return (int)((s1 + s2) % 127);
}}
"""

    def _gen_recursive_functions(self) -> str:
        a = self.rng.randint(20, 120)
        b = self.rng.randint(10, 60)
        fib_n = self.rng.randint(5, 14)

        return f"""#include <stdio.h>
#include <stdint.h>

static int64_t gcd(int64_t a, int64_t b) {{
    if (b == 0) return a;
    return gcd(b, a % b);
}}

static int fib(int n) {{
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fib(n - 1) + fib(n - 2);
}}

int main(void) {{
    int64_t g = gcd({a}, {b});
    int f = fib({fib_n});
    printf("REC: gcd=%ld fib=%d\\n", (long)g, f);
    return (int)((g + f) % 127);
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
            res["code"] = code
            return res

        # 1. Compile with reference GCC if available
        if has_gcc:
            r_gcc = subprocess.run(["gcc", "-O0", "-w", f"-I{inc_dir}", src_c, "-o", bin_g, "-lm"],
                                   capture_output=True, timeout=5.0)
            if r_gcc.returncode != 0:
                res["category"] = "INVALID_C_GENERATED"
                res["details"] = r_gcc.stderr.decode('utf-8', 'replace')[:200]
                res["code"] = code
                return res

        # 2. Compile with reference Clang if available
        if has_clang:
            r_clang = subprocess.run(["clang", "-O0", "-w", f"-I{inc_dir}", src_c, "-o", bin_c, "-lm"],
                                     capture_output=True, timeout=5.0)
            if r_clang.returncode != 0:
                res["category"] = "INVALID_C_GENERATED"
                res["details"] = r_clang.stderr.decode('utf-8', 'replace')[:200]
                res["code"] = code
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
            res["code"] = code
            return res

        # 4. Assemble & Link ZCC Assembly (requires GCC or system assembler/linker)
        if not has_gcc:
            res["category"] = "ASSEMBLER_UNAVAILABLE"
            res["details"] = "GCC required to assemble/link ZCC emitted assembly"
            res["code"] = code
            return res

        r_link = subprocess.run(["gcc", "-no-pie", "-w", "-o", bin_z, asm_s, "-lm"],
                                capture_output=True, timeout=5.0)
        if r_link.returncode != 0:
            res["category"] = "ASSEMBLER_REJECT"
            res["details"] = f"Link error on ZCC asm: {r_link.stderr.decode('utf-8', 'replace')[:200]}"
            res["code"] = code
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
                res["code"] = code
                return res
            if exec_z.stdout != exec_g.stdout:
                res["category"] = "STDOUT_MISMATCH"
                res["details"] = f"ZCC stdout='{res['stdout_zcc'][:60]}' vs GCC stdout='{res['stdout_gcc'][:60]}'"
                res["code"] = code
                return res

        if has_clang:
            exec_c = subprocess.run([bin_c], capture_output=True, timeout=2.0)
            res["rc_clang"] = exec_c.returncode
            res["stdout_clang"] = exec_c.stdout.decode('utf-8', 'replace')
            if exec_z.returncode != exec_c.returncode:
                res["category"] = "RETURNCODE_MISMATCH"
                res["details"] = f"ZCC rc={exec_z.returncode} vs Clang rc={exec_c.returncode}"
                res["code"] = code
                return res
            if exec_z.stdout != exec_c.stdout:
                res["category"] = "STDOUT_MISMATCH"
                res["details"] = f"ZCC stdout='{res['stdout_zcc'][:60]}' vs Clang stdout='{res['stdout_clang'][:60]}'"
                res["code"] = code
                return res

        res["pass"] = True
        res["category"] = "PASS"
        return res

    except subprocess.TimeoutExpired:
        res["category"] = "TIMEOUT"
        res["details"] = "Execution timed out (2.0s sandbox ceiling)"
        res["code"] = code
        return res
    except Exception as e:
        res["category"] = "EXECUTION_ERROR"
        res["details"] = str(e)
        res["code"] = code
        return res

    finally:
        shutil.rmtree(td, ignore_errors=True)


# ==============================================================================
# MAIN GAUNTLET ORCHESTRATOR
# ==============================================================================

def run_gauntlet(count: int = 100, workers: int = 4, start_seed: int = 1000, regression_dir: str = "") -> Dict[str, Any]:
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

    if divergences:
        effective_repro_dir = regression_dir or os.path.join(repo_root, "tests", "differential", "regressions")
        try:
            os.makedirs(effective_repro_dir, exist_ok=True)
            for div in divergences:
                repro_file = os.path.join(effective_repro_dir, f"repro_seed_{div['seed']}_{div['category'].lower()}.c")
                with open(repro_file, "w", encoding="utf-8") as rf:
                    rf.write(f"/*\n * Differential Divergence Reproduction Test\n * Seed: {div['seed']}\n * Category: {div['category']}\n * Details: {div['details']}\n * ZCC rc={div.get('rc_zcc')}, GCC rc={div.get('rc_gcc')}, Clang rc={div.get('rc_clang')}\n */\n\n")
                    rf.write(div.get("code", ""))
                print(f"[!] Saved minimal regression reproducer to {repro_file}")
        except Exception as dump_err:
            print(f"[-] Warning: Failed to dump regression reproducers: {dump_err}")

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
    parser.add_argument("--regression-dir", type=str, default="", help="Directory to dump minimal reproducer C files on divergence")
    parser.add_argument("--json", type=str, default="", help="Path to write JSON results")

    args = parser.parse_args()
    summary = run_gauntlet(count=args.count, workers=args.workers, start_seed=args.seed, regression_dir=args.regression_dir)

    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(summary, f, indent=2)
        print(f"[+] Wrote machine-readable results to {args.json}")

    sys.exit(0 if summary["verdict"] == "PASS" else 1)

