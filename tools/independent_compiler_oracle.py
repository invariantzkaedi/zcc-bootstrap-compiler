#!/usr/bin/env python3
"""
ZCC-EF-0 Independent Compiler Oracle (v3.0 OMEGA)
=============================================================================
Independent differential verification oracle comparing ZCC code generation
and execution against GCC and Clang across defined-behavior C99 / System V AMD64
programs.
"""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SUITE = {
    "arithmetic_promotions": r"""
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint8_t a = 200;
    uint8_t b = 100;
    int c = a + b; // integer promotion to int
    uint32_t u = 0x80000000U;
    int64_t s = (int64_t)(int32_t)u;
    uint64_t shifted = ((uint64_t)a) << 32 | b;
    printf("PROMOTE: c=%d, s=%lld, shifted=%llx\n", c, (long long)s, (unsigned long long)shifted);
    return 0;
}
""",
    "sysv_abi_aggregate": r"""
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint64_t a;
    uint64_t b;
} Pair16;

typedef struct {
    uint64_t a;
    uint64_t b;
    uint64_t c;
} Triple24;

Pair16 make_pair(uint64_t x, uint64_t y) {
    Pair16 p;
    p.a = x * 3;
    p.b = y + 7;
    return p;
}

Triple24 make_triple(uint64_t x, uint64_t y, uint64_t z) {
    Triple24 t;
    t.a = x + 10;
    t.b = y * 2;
    t.c = z ^ 0xaa;
    return t;
}

int main(void) {
    Pair16 p = make_pair(10, 20);
    Triple24 t = make_triple(1, 2, 3);
    printf("PAIR: %llu, %llu | TRIPLE: %llu, %llu, %llu\n",
           (unsigned long long)p.a, (unsigned long long)p.b,
           (unsigned long long)t.a, (unsigned long long)t.b, (unsigned long long)t.c);
    return 0;
}
""",
    "sysv_mixed_fp_int": r"""
#include <stdio.h>

double mixed_call(int a, double x, int b, double y, int c, double z) {
    return (a * x) + (b * y) - (c * z);
}

int main(void) {
    double res = mixed_call(2, 3.5, 4, 1.25, 3, 0.5);
    printf("MIXED: %.6f\n", res);
    return 0;
}
""",
    "variadic_spill": r"""
#include <stdio.h>
#include <stdarg.h>

int sum_var(int count, ...) {
    va_list ap;
    va_start(ap, count);
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(ap, int);
    }
    va_end(ap);
    return total;
}

int main(void) {
    // 10 arguments ensures parameters spill past 6 register slots onto stack
    int res = sum_var(9, 10, 20, 30, 40, 50, 60, 70, 80, 90);
    printf("SUM_VAR: %d\n", res);
    return 0;
}
""",
    "recursive_stack_align": r"""
#include <stdio.h>
#include <stdint.h>

uintptr_t check_align(int depth) {
    uintptr_t sp;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(sp));
    if (depth <= 0) return (sp & 15);
    return (sp & 15) + check_align(depth - 1);
}

int main(void) {
    uintptr_t misalignment = check_align(5);
    printf("MISALIGN_ACCUM: %lu\n", (unsigned long)misalignment);
    return 0;
}
""",
    "sysv_fp_reg_exhaustion": r"""
#include <stdio.h>

double fp_sum(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j) {
    return a + b + c + d + e + f + g + h + i + j;
}

int main(void) {
    // 10 doubles: 8 in %xmm0..%xmm7, 2 spilled to stack
    double s = fp_sum(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0);
    printf("FP_SUM: %.1f\n", s);
    return 0;
}
""",
    "sysv_mixed_aggregate_fp_int": r"""
#include <stdio.h>
#include <stdint.h>

typedef struct {
    int64_t i;
    double d;
} Mixed16;

Mixed16 make_mixed(int64_t a, double b) {
    Mixed16 m;
    m.i = a + 42;
    m.d = b * 2.5;
    return m;
}

int main(void) {
    Mixed16 m = make_mixed(100, 4.0);
    printf("MIXED_16: i=%lld, d=%.2f\n", (long long)m.i, m.d);
    return 0;
}
""",
    "sysv_nested_struct_alignment": r"""
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint8_t c;
    uint32_t x;
    uint16_t s;
} Inner;

typedef struct {
    uint8_t a;
    Inner in;
    uint64_t b;
} Outer;

Outer make_outer(uint8_t a, uint32_t x, uint64_t b) {
    Outer o;
    o.a = a;
    o.in.c = a + 1;
    o.in.x = x;
    o.in.s = 0x1234;
    o.b = b;
    return o;
}

int main(void) {
    Outer o = make_outer(5, 0xdeadbeef, 0x1122334455667788ULL);
    printf("OUTER: a=%u, in.c=%u, in.x=%x, in.s=%x, b=%llx\n",
           o.a, o.in.c, o.in.x, o.in.s, (unsigned long long)o.b);
    return 0;
}
""",
    "sysv_variadic_mixed_fp_int": r"""
#include <stdio.h>
#include <stdarg.h>

void format_mixed(char *buf, int n, ...) {
    va_list ap;
    va_start(ap, n);
    int ival = va_arg(ap, int);
    double dval = va_arg(ap, double);
    int ival2 = va_arg(ap, int);
    double dval2 = va_arg(ap, double);
    va_end(ap);
    sprintf(buf, "%d:%.2f:%d:%.2f", ival, dval, ival2, dval2);
}

int main(void) {
    char buf[128];
    format_mixed(buf, 4, 12, 3.14, 99, 2.72);
    printf("FMT: %s\n", buf);
    return 0;
}
"""
}

def run_cmd(cmd, cwd=None):
    res = subprocess.run(cmd, shell=isinstance(cmd, str), cwd=cwd, capture_output=True, text=True)
    return res

def test_oracle(zcc_binary, repo_root):
    zcc_path = Path(zcc_binary).resolve()
    if not zcc_path.is_file():
        print(f"ORACLE ERROR: ZCC binary not found: {zcc_path}", file=sys.stderr)
        return False

    has_gcc = shutil.which("gcc") is not None
    has_clang = shutil.which("clang") is not None

    if not has_gcc:
        print("ORACLE ERROR: GCC is required as an independent reference oracle.", file=sys.stderr)
        return False

    print(f"=== INDEPENDENT COMPILER ORACLE GAUNTLET ===")
    print(f"Target ZCC: {zcc_path}")
    print(f"Oracle GCC:  {shutil.which('gcc')}")
    print(f"Oracle Clang:{shutil.which('clang') or 'NOT INSTALLED (OPTIONAL)'}")
    print(f"Test Cases:  {len(SUITE)}")
    print("=" * 60)

    work_dir = Path(tempfile.mkdtemp(prefix="zcc_oracle_test_"))
    passed = 0

    try:
        for name, src in SUITE.items():
            c_file = work_dir / f"{name}.c"
            c_file.write_text(src)

            # 1. Compile with GCC
            gcc_bin = work_dir / f"{name}_gcc"
            res_gcc = run_cmd(["gcc", "-O2", "-w", str(c_file), "-o", str(gcc_bin), "-lm"])
            if res_gcc.returncode != 0:
                print(f"FAIL [{name}]: GCC compilation failed: {res_gcc.stderr}")
                return False

            run_gcc = run_cmd([str(gcc_bin)])
            if run_gcc.returncode != 0:
                print(f"FAIL [{name}]: GCC execution failed with exit code {run_gcc.returncode}")
                return False

            # 2. Compile with Clang (if available)
            if has_clang:
                clang_bin = work_dir / f"{name}_clang"
                res_clang = run_cmd(["clang", "-O2", "-w", str(c_file), "-o", str(clang_bin), "-lm"])
                if res_clang.returncode == 0:
                    run_clang = run_cmd([str(clang_bin)])
                    if run_clang.returncode != run_gcc.returncode:
                        print(f"FAIL [{name}]: Clang exit code ({run_clang.returncode}) != GCC exit code ({run_gcc.returncode})")
                        return False
                    if run_clang.stdout != run_gcc.stdout:
                        print(f"FAIL [{name}]: GCC vs Clang stdout divergence detected!")
                        return False
                    if run_clang.stderr != run_gcc.stderr:
                        print(f"FAIL [{name}]: GCC vs Clang stderr divergence detected!")
                        return False

            # 3. Compile with ZCC
            zcc_asm = work_dir / f"{name}_zcc.s"
            zcc_bin = work_dir / f"{name}_zcc"

            res_zcc = run_cmd([str(zcc_path), str(c_file), "-o", str(zcc_asm)], cwd=repo_root)
            if res_zcc.returncode != 0:
                print(f"FAIL [{name}]: ZCC compilation failed: {res_zcc.stderr}")
                return False

            # Assemble ZCC output with system assembler/linker
            res_as = run_cmd(["gcc", "-w", str(zcc_asm), "-o", str(zcc_bin), "-lm"])
            if res_as.returncode != 0:
                print(f"FAIL [{name}]: Assembly of ZCC output failed: {res_as.stderr}")
                return False

            run_zcc = run_cmd([str(zcc_bin)])

            # 4. Compare execution exit codes, stdout, and stderr
            if run_zcc.returncode != run_gcc.returncode:
                print(f"FAIL [{name}]: Exit code divergence between ZCC ({run_zcc.returncode}) and GCC ({run_gcc.returncode})")
                return False

            if run_zcc.stdout != run_gcc.stdout:
                print(f"FAIL [{name}]: Stdout divergence between ZCC and GCC:")
                print(f"  GCC stdout: {run_gcc.stdout.strip()}")
                print(f"  ZCC stdout: {run_zcc.stdout.strip()}")
                return False

            if run_zcc.stderr != run_gcc.stderr:
                print(f"FAIL [{name}]: Stderr divergence between ZCC and GCC:")
                print(f"  GCC stderr: {run_gcc.stderr.strip()}")
                print(f"  ZCC stderr: {run_zcc.stderr.strip()}")
                return False

            print(f"  ✓ {name:<25} Consensus Verified (Exit: {run_zcc.returncode}, Output: {run_zcc.stdout.strip()})")
            passed += 1

        print("=" * 60)
        print(f"ORACLE PASS: All {passed}/{len(SUITE)} differential cases match GCC/Clang consensus byte-for-byte.")
        return True
    finally:
        shutil.rmtree(work_dir, ignore_errors=True)

if __name__ == "__main__":
    repo_root = Path(__file__).resolve().parent.parent
    zcc_bin = sys.argv[1] if len(sys.argv) > 1 else str(repo_root / "zcc")
    success = test_oracle(zcc_bin, repo_root)
    sys.exit(0 if success else 1)
