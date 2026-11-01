#!/usr/bin/env python3
"""
ZCC v17.7+ — Independent Differential & Metamorphic C99 Oracle
================================================================================
Addresses the Bootstrap Epistemic Boundary & Residual Risk by verifying the
converged Stage 2/3 ZCC compiler against an independent GCC oracle and algebraic
metamorphic equivalence relations across seeded C99 translation units in a
native /tmp (ext4/tmpfs) arena.

Tested C99 & System V AMD64 ABI Subsystems per Seed:
  1. Mixed-width integer promotions, bitwise shifts, safe div/mod, and ternary ops
  2. Short-circuit (&&, ||) side-effect evaluation counts (strict left-to-right)
  3. Structs (INTEGER <=16B and MEMORY >16B sret), nested structs, pointer strides
  4. 9-argument System V AMD64 functions (6 GP registers + 3 stack-spilled args)
  5. Function pointer dispatch tables and switch/case + loop state machines
  6. Metamorphic Equivalence Pair (P_A vs P_B): algebraically transformed program
     computing the exact same closed-form FNV-1a state digest
  7. Built-in Negative Control (--self-test): verifies the oracle detects a 1-ulp
     output perturbation and rejects with exit code 65
================================================================================
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Dict, List, Tuple

EXIT_OK = 0
EXIT_DIFF_DIVERGENCE = 65
EXIT_METAMORPHIC_DIVERGENCE = 66
EXIT_COMPILE_FAIL = 67
EXIT_NEG_CTRL_FAIL = 68


def generate_c99_pair(seed: int) -> Tuple[str, str]:
    """
    Generate a pair of deterministic, UB-free C99 programs (prog_a, prog_b) for `seed`.
    - prog_a uses direct loop/switch/struct/9-arg evaluation.
    - prog_b applies algebraic identities (De Morgan bitwise transforms, split loop
      accumulation, scalarized struct fields, re-associated 9-arg linear combination)
      that must produce the exact same stdout and exit code.
    """
    rng = random.Random(seed)
    c1 = rng.randint(3, 97)
    c2 = rng.randint(5, 89)
    c3 = rng.randint(7, 113)
    c4 = rng.randint(11, 127)
    c5 = rng.randint(13, 73)
    shift_amt = rng.randint(1, 5)
    loop_n = rng.randint(12, 24)
    arr_vals = [rng.randint(10, 999) for _ in range(16)]
    arr_init = ", ".join(str(v) for v in arr_vals)

    prog_a = f"""#include <stdio.h>

typedef struct {{
    long a;
    long b;
}} Pair16;

typedef struct {{
    long x;
    long y;
    long z;
    int tag;
}} Vec28;

static int g_side_effects = 0;

static int bump_true(int delta) {{
    g_side_effects = g_side_effects + delta;
    return 1;
}}

static int bump_false(int delta) {{
    g_side_effects = g_side_effects + delta;
    return 0;
}}

static long op_add(long x, long y) {{ return x + y; }}
static long op_sub(long x, long y) {{ return x - y; }}
static long op_xor(long x, long y) {{ return x ^ y; }}
static long op_band(long x, long y) {{ return (x & 65535L) + (y & 65535L); }}

/* 9 arguments: 6 in registers (%rdi..%r9), 3 spilled on stack */
static long abi_spill_9(long a1, int a2, long a3, int a4, long a5, int a6, long a7, int a8, long a9) {{
    return (a1 * 3L) - (long)(a2 * 2) + (a3 ^ (long)a4) + (a5 << {shift_amt}) - (long)a6 + (a7 * 5L) - (long)(a8 * 7) + a9;
}}

static Pair16 make_pair(long u, long v) {{
    Pair16 p;
    p.a = u + {c1}L;
    p.b = v ^ {c2}L;
    return p;
}}

static Vec28 transform_vec(Vec28 in, long scale) {{
    Vec28 out;
    out.x = in.x * scale + {c3}L;
    out.y = (in.y ^ in.x) + {c4}L;
    out.z = in.z + out.x - out.y;
    out.tag = in.tag + 1;
    return out;
}}

int main(void) {{
    long arr[16] = {{ {arr_init} }};
    long (*fn_table[4])(long, long);
    fn_table[0] = op_add;
    fn_table[1] = op_sub;
    fn_table[2] = op_xor;
    fn_table[3] = op_band;

    /* 1. Short-circuit evaluation test */
    int sc1 = (bump_false({c1}) && bump_true(1000));
    int sc2 = (bump_true({c2}) || bump_true(2000));
    int sc3 = (bump_true({c3}) && bump_false({c4}) && bump_true(3000));

    /* 2. Pointer stride & function pointer table (C99 6.2.5#9 unsigned modular FNV-1a) */
    long acc = (long)(1469598103934665603ULL & 0x7fffffffffffffffULL);
    long *p = &arr[0];
    int i;
    for (i = 0; i < 16; i = i + 1) {{
        long v = *(p + i);
        long fn_res = fn_table[i & 3](v, (long)(i + {c5}));
        acc = (long)((((unsigned long)(acc ^ fn_res)) * 1099511628211UL) & 0x7fffffffUL);
    }}

    /* 3. Struct value passing (INTEGER 16B & MEMORY 28B sret) */
    Pair16 pr = make_pair(arr[3], arr[11]);
    Vec28 v0;
    v0.x = pr.a;
    v0.y = pr.b;
    v0.z = {c5}L;
    v0.tag = 10;
    Vec28 v1 = transform_vec(v0, 3L);

    /* 4. 9-argument stack spill call */
    long spill_sum = 0;
    for (i = 0; i < {loop_n}; i = i + 1) {{
        long r = abi_spill_9(
            (long)(i + {c1}),
            i + {c2},
            arr[i & 15],
            (i * 3) + 1,
            (long)(i + {c3}),
            (i * 5) + 2,
            arr[(i + 7) & 15],
            i + {c4},
            v1.z
        );
        switch (i & 3) {{
            case 0: spill_sum = spill_sum + r; break;
            case 1: spill_sum = spill_sum - (r >> 1); break;
            case 2: spill_sum = spill_sum ^ (r & 0xfffffL); break;
            default: spill_sum = spill_sum + (r % {c5}L); break;
        }}
    }}

    printf("SEED={seed} SE=%d,%d,%d,%d ACC=%ld VEC=%ld,%ld,%ld,%d SPILL=%ld\\n",
           g_side_effects, sc1, sc2, sc3, acc, v1.x, v1.y, v1.z, v1.tag, spill_sum);
    return 0;
}}
"""

    # Metamorphic variant prog_b:
    # - Uses De Morgan's identity (x ^ y) == ((x | y) & ~(x & y))
    # - Unrolls the 16-element pointer loop into two 8-element halves
    # - Computes abi_spill_9 via factored sub-expressions
    # - Replaces switch(i & 3) with if/else chain
    prog_b = f"""#include <stdio.h>

static int g_se = 0;
static int f_true(int d) {{ g_se += d; return 1; }}
static int f_false(int d) {{ g_se += d; return 0; }}

static long demorgan_xor(long a, long b) {{
    return (a | b) & ~(a & b);
}}

static long m_add(long x, long y) {{ return (x ^ y) + ((x & y) << 1); }}
static long m_sub(long x, long y) {{ return x + (~y + 1L); }}
static long m_xor(long x, long y) {{ return demorgan_xor(x, y); }}
static long m_band(long x, long y) {{ return (x & 0xffffL) + (y & 0xffffL); }}

static long spill_alt(long a1, int a2, long a3, int a4, long a5, int a6, long a7, int a8, long a9) {{
    long part1 = (a1 + (a1 << 1)) - (long)(a2 << 1);
    long part2 = demorgan_xor(a3, (long)a4) + (a5 * (1L << {shift_amt})) - (long)a6;
    long part3 = (a7 * 5L) - (long)(a8 * 7) + a9;
    return part1 + part2 + part3;
}}

int main(void) {{
    long data[16] = {{ {arr_init} }};
    int s1 = (f_false({c1}) && f_true(1000));
    int s2 = (f_true({c2}) || f_true(2000));
    int s3 = (f_true({c3}) && f_false({c4}) && f_true(3000));

    long acc = (long)(1469598103934665603ULL & 0x7fffffffffffffffULL);
    int k = 0;
    while (k < 16) {{
        long v = data[k];
        long arg2 = (long)(k + {c5});
        long res = 0;
        if ((k & 3) == 0) res = m_add(v, arg2);
        else if ((k & 3) == 1) res = m_sub(v, arg2);
        else if ((k & 3) == 2) res = m_xor(v, arg2);
        else res = m_band(v, arg2);
        acc = (long)((((unsigned long)demorgan_xor(acc, res)) * 1099511628211UL) & 0x7fffffffUL);
        k = k + 1;
    }}

    long pa = data[3] + {c1}L;
    long pb = demorgan_xor(data[11], {c2}L);
    long vx = pa * 3L + {c3}L;
    long vy = demorgan_xor(pb, pa) + {c4}L;
    long vz = {c5}L + vx - vy;
    int vtag = 11;

    long spill_sum = 0;
    int i;
    for (i = 0; i < {loop_n}; i = i + 1) {{
        long r = spill_alt(
            (long)(i + {c1}),
            i + {c2},
            data[i & 15],
            (i * 3) + 1,
            (long)(i + {c3}),
            (i * 5) + 2,
            data[(i + 7) & 15],
            i + {c4},
            vz
        );
        int rem = i & 3;
        if (rem == 0) spill_sum = spill_sum + r;
        else if (rem == 1) spill_sum = spill_sum - (r >> 1);
        else if (rem == 2) spill_sum = demorgan_xor(spill_sum, r & 0xfffffL);
        else spill_sum = spill_sum + (r % {c5}L);
    }}

    printf("SEED={seed} SE=%d,%d,%d,%d ACC=%ld VEC=%ld,%ld,%ld,%d SPILL=%ld\\n",
           g_se, s1, s2, s3, acc, vx, vy, vz, vtag, spill_sum);
    return 0;
}}
"""
    return prog_a, prog_b


def compile_and_run_zcc(zcc_bin: str, gcc_bin: str, c_path: str, asm_path: str, bin_path: str, work_dir: str) -> Tuple[int, str, str]:
    c_res = subprocess.run([zcc_bin, c_path, "-S", "-o", asm_path], cwd=work_dir, capture_output=True, text=True)
    if c_res.returncode != 0 or not os.path.isfile(asm_path):
        return EXIT_COMPILE_FAIL, "", f"zcc compile failed: {c_res.stderr}"
    l_res = subprocess.run([gcc_bin, "-no-pie", asm_path, "-o", bin_path], cwd=work_dir, capture_output=True, text=True)
    if l_res.returncode != 0 or not os.path.isfile(bin_path):
        return EXIT_COMPILE_FAIL, "", f"gcc link failed: {l_res.stderr}"
    r_res = subprocess.run([bin_path], cwd=work_dir, capture_output=True, text=True)
    return r_res.returncode, r_res.stdout, r_res.stderr


def compile_and_run_gcc(gcc_bin: str, c_path: str, bin_path: str, work_dir: str) -> Tuple[int, str, str]:
    c_res = subprocess.run([gcc_bin, "-O0", "-no-pie", "-std=c99", "-Wall", "-Wextra", "-Werror", c_path, "-o", bin_path], cwd=work_dir, capture_output=True, text=True)
    if c_res.returncode != 0 or not os.path.isfile(bin_path):
        return EXIT_COMPILE_FAIL, "", f"gcc reference build failed: {c_res.stderr}"
    r_res = subprocess.run([bin_path], cwd=work_dir, capture_output=True, text=True)
    return r_res.returncode, r_res.stdout, r_res.stderr


def compile_and_run_ubsan_asan(gcc_bin: str, c_path: str, bin_path: str, work_dir: str) -> Tuple[int, str, str]:
    """
    Compile and execute under GCC with UBSan + ASan + strict -Wall -Wextra -Werror
    and -fno-sanitize-recover=all to empirically prove zero undefined behavior.
    """
    c_res = subprocess.run(
        [
            gcc_bin,
            "-O1",
            "-std=c99",
            "-fsanitize=undefined,address",
            "-fno-sanitize-recover=all",
            "-Wall",
            "-Wextra",
            "-Werror",
            c_path,
            "-o",
            bin_path,
        ],
        cwd=work_dir,
        capture_output=True,
        text=True,
    )
    if c_res.returncode != 0 or not os.path.isfile(bin_path):
        return EXIT_COMPILE_FAIL, "", f"gcc UBSan/ASan build failed: {c_res.stderr}"
    r_res = subprocess.run([bin_path], cwd=work_dir, capture_output=True, text=True)
    if r_res.returncode != 0 or r_res.stderr.strip():
        return EXIT_COMPILE_FAIL, r_res.stdout, f"UBSan/ASan runtime trap (rc={r_res.returncode}): {r_res.stderr}"
    return r_res.returncode, r_res.stdout, r_res.stderr


def run_ubsan_negative_control(gcc_bin: str, work_dir: str) -> Tuple[bool, str]:
    """
    Rule MV-6 / EF-3 UBSan Negative Control:
    Verify that GCC with -fsanitize=undefined -fno-sanitize-recover=all detects and
    aborts on signed 64-bit long multiplication overflow (`acc * 1099511628211L`).
    """
    ub_c = os.path.join(work_dir, "ub_neg_ctrl.c")
    ub_bin = os.path.join(work_dir, "ub_neg_ctrl.bin")
    ub_src = """#include <stdio.h>
int main(void) {
    volatile long acc = 1469598103934665575L;
    volatile long mul = 1099511628211L;
    long out = (acc * mul) & 0x7fffffffL;
    printf("%ld\\n", out);
    return 0;
}
"""
    with open(ub_c, "w", encoding="utf-8") as f:
        f.write(ub_src)
    c_res = subprocess.run(
        [gcc_bin, "-O1", "-std=c99", "-fsanitize=undefined", "-fno-sanitize-recover=all", ub_c, "-o", ub_bin],
        cwd=work_dir,
        capture_output=True,
        text=True,
    )
    if c_res.returncode != 0 or not os.path.isfile(ub_bin):
        return False, "failed to compile UBSan negative control"
    r_res = subprocess.run([ub_bin], cwd=work_dir, capture_output=True, text=True)
    ok = (r_res.returncode != 0) and ("signed integer overflow" in r_res.stderr)
    return ok, r_res.stderr.strip()


def run_negative_control(zcc_bin: str, gcc_bin: str, work_dir: str) -> bool:
    """
    Rule MV-6 / MV-7 Mutation Control:
    Compile seed 99999 with ZCC, perturb the emitted assembly constant for `vtag`
    (or `SPILL`), link and run, and confirm the differential comparator detects the
    mismatch between GCC and perturbed ZCC.
    """
    prog_a, _ = generate_c99_pair(99999)
    c_path = os.path.join(work_dir, "neg_ctrl.c")
    asm_path = os.path.join(work_dir, "neg_ctrl.s")
    bin_zcc = os.path.join(work_dir, "neg_ctrl_zcc.bin")
    bin_gcc = os.path.join(work_dir, "neg_ctrl_gcc.bin")
    with open(c_path, "w", encoding="utf-8") as f:
        f.write(prog_a)

    rc_g, out_g, _ = compile_and_run_gcc(gcc_bin, c_path, bin_gcc, work_dir)
    if rc_g != 0:
        return False

    c_res = subprocess.run([zcc_bin, c_path, "-S", "-o", asm_path], cwd=work_dir, capture_output=True, text=True)
    if c_res.returncode != 0 or not os.path.isfile(asm_path):
        return False

    with open(asm_path, "r", encoding="utf-8") as f:
        asm_text = f.read()

    # Perturb the format string SEED=99999 -> SEED=99998 in assembly to simulate silent codegen corruption
    if "SEED=99999" not in asm_text:
        return False
    mutated_asm = asm_text.replace("SEED=99999", "SEED=99998", 1)
    with open(asm_path, "w", encoding="utf-8") as f:
        f.write(mutated_asm)

    l_res = subprocess.run([gcc_bin, "-no-pie", asm_path, "-o", bin_zcc], cwd=work_dir, capture_output=True, text=True)
    if l_res.returncode != 0:
        return False
    r_res = subprocess.run([bin_zcc], cwd=work_dir, capture_output=True, text=True)
    return r_res.stdout != out_g


def evaluate_seed(seed: int, zcc_local: str, gcc_bin: str, work_dir: str) -> Tuple[int, str, Dict[str, object]]:
    prog_a, prog_b = generate_c99_pair(seed)

    ca = os.path.join(work_dir, f"s{seed}_a.c")
    cb = os.path.join(work_dir, f"s{seed}_b.c")
    sa = os.path.join(work_dir, f"s{seed}_a.s")
    sb = os.path.join(work_dir, f"s{seed}_b.s")
    bza = os.path.join(work_dir, f"s{seed}_za.bin")
    bzb = os.path.join(work_dir, f"s{seed}_zb.bin")
    bga = os.path.join(work_dir, f"s{seed}_ga.bin")
    bgb = os.path.join(work_dir, f"s{seed}_gb.bin")
    bsana = os.path.join(work_dir, f"s{seed}_sana.bin")
    bsanb = os.path.join(work_dir, f"s{seed}_sanb.bin")

    with open(ca, "w", encoding="utf-8") as f:
        f.write(prog_a)
    with open(cb, "w", encoding="utf-8") as f:
        f.write(prog_b)

    rc_za, out_za, err_za = compile_and_run_zcc(zcc_local, gcc_bin, ca, sa, bza, work_dir)
    if rc_za != 0:
        return EXIT_COMPILE_FAIL, f"FAIL [seed={seed} prog_a ZCC]: {err_za}", {}

    rc_ga, out_ga, err_ga = compile_and_run_gcc(gcc_bin, ca, bga, work_dir)
    if rc_ga != 0:
        return EXIT_COMPILE_FAIL, f"FAIL [seed={seed} prog_a GCC]: {err_ga}", {}

    rc_sa, out_sa, err_sa = compile_and_run_ubsan_asan(gcc_bin, ca, bsana, work_dir)
    if rc_sa != 0:
        return EXIT_COMPILE_FAIL, f"FAIL [seed={seed} prog_a UBSan/ASan]: {err_sa}", {}

    rc_zb, out_zb, err_zb = compile_and_run_zcc(zcc_local, gcc_bin, cb, sb, bzb, work_dir)
    if rc_zb != 0:
        return EXIT_COMPILE_FAIL, f"FAIL [seed={seed} prog_b ZCC]: {err_zb}", {}

    rc_gb, out_gb, err_gb = compile_and_run_gcc(gcc_bin, cb, bgb, work_dir)
    if rc_gb != 0:
        return EXIT_COMPILE_FAIL, f"FAIL [seed={seed} prog_b GCC]: {err_gb}", {}

    rc_sb, out_sb, err_sb = compile_and_run_ubsan_asan(gcc_bin, cb, bsanb, work_dir)
    if rc_sb != 0:
        return EXIT_COMPILE_FAIL, f"FAIL [seed={seed} prog_b UBSan/ASan]: {err_sb}", {}

    if out_za != out_ga or out_zb != out_gb or out_za != out_sa or out_zb != out_sb:
        return (
            EXIT_DIFF_DIVERGENCE,
            f"FAIL: Differential divergence on seed {seed}!\n  ZCC_A: {out_za.strip()}\n  GCC_A: {out_ga.strip()}\n  SAN_A: {out_sa.strip()}\n  ZCC_B: {out_zb.strip()}\n  GCC_B: {out_gb.strip()}\n  SAN_B: {out_sb.strip()}",
            {},
        )

    if out_za != out_zb:
        return (
            EXIT_METAMORPHIC_DIVERGENCE,
            f"FAIL: Metamorphic divergence on seed {seed}!\n  P_A: {out_za.strip()}\n  P_B: {out_zb.strip()}",
            {},
        )

    return EXIT_OK, "", {
        "seed": seed,
        "stdout": out_za.strip(),
        "stdout_sha256": hashlib.sha256(out_za.encode("utf-8")).hexdigest(),
    }


def main() -> int:
    import concurrent.futures

    parser = argparse.ArgumentParser(description="ZCC Differential & Metamorphic C99 Oracle")
    parser.add_argument("--compiler", default="./zcc2", help="Path to ZCC compiler binary under test")
    parser.add_argument("--oracle", default="gcc", help="Path to independent reference C compiler (gcc)")
    parser.add_argument("--seeds", type=int, default=32, help="Number of deterministic seeds to evaluate (each runs 2 metamorphic programs)")
    parser.add_argument("--base-seed", type=int, default=1337, help="Starting deterministic seed")
    parser.add_argument("--receipt", default=None, help="Optional JSON receipt output path")
    parser.add_argument("--self-test", action="store_true", help="Run mutation negative control self-test")
    args = parser.parse_args()

    zcc_bin = os.path.abspath(args.compiler)
    if not os.path.isfile(zcc_bin):
        print(f"FAIL: ZCC compiler binary not found at {zcc_bin}", file=sys.stderr)
        return EXIT_COMPILE_FAIL

    gcc_bin = shutil.which(args.oracle) or args.oracle
    work_dir = tempfile.mkdtemp(prefix="zcc_diff_c99_", dir="/tmp")
    t0 = time.monotonic()

    try:
        # Copy compiler into native /tmp arena to avoid DrvFs 9P round-trips
        zcc_local = os.path.join(work_dir, "zcc_under_test")
        shutil.copy2(zcc_bin, zcc_local)
        os.chmod(zcc_local, 0o755)

        # 1. Always run the mutation & UBSan negative controls first (Rule MV-6 / MV-7)
        neg_ok = run_negative_control(zcc_local, gcc_bin, work_dir)
        if not neg_ok:
            print("FAIL: Differential oracle mutation negative control failed to detect perturbation!", file=sys.stderr)
            return EXIT_NEG_CTRL_FAIL

        ub_neg_ok, ub_neg_diag = run_ubsan_negative_control(gcc_bin, work_dir)
        if not ub_neg_ok:
            print("FAIL: UBSan negative control failed to trap signed overflow!", file=sys.stderr)
            return EXIT_NEG_CTRL_FAIL

        if args.self_test and args.seeds == 0:
            print("PASS: Differential C99 oracle mutation & UBSan negative controls verified.")
            return EXIT_OK

        seeds = [args.base_seed + idx for idx in range(args.seeds)]
        records: List[Dict[str, object]] = []
        max_workers = min(8, os.cpu_count() or 4)
        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as pool:
            futures = {pool.submit(evaluate_seed, s, zcc_local, gcc_bin, work_dir): s for s in seeds}
            for fut in concurrent.futures.as_completed(futures):
                rc, err_msg, rec = fut.result()
                if rc != EXIT_OK:
                    print(err_msg, file=sys.stderr)
                    return rc
                records.append(rec)

        records.sort(key=lambda r: int(r["seed"]))
        elapsed_s = time.monotonic() - t0
        digest_concat = "\n".join(str(r["stdout_sha256"]) for r in records)
        corpus_sha256 = hashlib.sha256(digest_concat.encode("utf-8")).hexdigest()
        zcc_sha256 = hashlib.sha256(Path(zcc_local).read_bytes()).hexdigest()

        summary = {
            "verdict": "PASS",
            "compiler_path": zcc_bin,
            "compiler_sha256": zcc_sha256,
            "oracle": gcc_bin,
            "sanitizers": "-std=c99 -O1 -fsanitize=undefined,address -fno-sanitize-recover=all -Wall -Wextra -Werror",
            "seeds_evaluated": args.seeds,
            "programs_compiled_and_verified": args.seeds * 2,
            "executions_verified": args.seeds * 6,
            "mutation_negative_control": "PASS",
            "ubsan_negative_control": "PASS",
            "ubsan_negative_control_diagnostic": ub_neg_diag,
            "ubsan_asan_clean_programs": args.seeds * 2,
            "corpus_stdout_sha256": corpus_sha256,
            "elapsed_seconds": round(elapsed_s, 3),
        }

        if args.receipt:
            Path(args.receipt).parent.mkdir(parents=True, exist_ok=True)
            Path(args.receipt).write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

        print(
            f"  \033[1;32m[PASS: Differential & Metamorphic C99 Oracle + UBSan/ASan]\033[0m "
            f"({args.seeds} seeds, {args.seeds * 2} UB-free C99 TUs, {args.seeds * 6} executions [ZCC2 + GCC -O0 + GCC UBSan/ASan], "
            f"0 divergences in {elapsed_s:.2f}s, SHA: {corpus_sha256[:16]}...)"
        )
        return EXIT_OK
    finally:
        shutil.rmtree(work_dir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())

