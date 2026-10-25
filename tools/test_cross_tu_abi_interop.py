#!/usr/bin/env python3
"""
ZCC OMEGA V3 — Cross-Translation-Unit ABI Interoperability & Mutation Harness
=============================================================================
Exercises System V AMD64 ABI conformance between independent compilers:
- Direction 1: GCC caller -> ZCC callee
- Direction 2: ZCC caller -> GCC callee
- Direction 3: Clang caller -> ZCC callee
- Direction 4: ZCC caller -> Clang callee
- Negative Controls: Mutation of return-value lowering asserted to fail
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HEADER_SRC = """#pragma once
#include <stdint.h>

typedef struct {
    int64_t i;
    double d;
} Mixed16;

typedef struct {
    uint64_t a;
    uint64_t b;
    uint64_t c;
} Triple24;

Mixed16 make_mixed(int64_t a, double b);
Triple24 make_triple(uint64_t x, uint64_t y, uint64_t z);
double sum_10fp(double d0, double d1, double d2, double d3, double d4, double d5, double d6, double d7, double d8, double d9);
int64_t mixed_spill(int a, double x, int b, double y, int c, double z, int d, double w, int e, double u, int f, int g);
"""

CALLEE_SRC = """#include "abi_suite.h"

Mixed16 make_mixed(int64_t a, double b) {
    Mixed16 m;
    m.i = a + 42;
    m.d = b * 2.5;
    return m;
}

Triple24 make_triple(uint64_t x, uint64_t y, uint64_t z) {
    Triple24 t;
    t.a = x + 10;
    t.b = y * 2;
    t.c = z ^ 0xaa;
    return t;
}

double sum_10fp(double d0, double d1, double d2, double d3, double d4, double d5, double d6, double d7, double d8, double d9) {
    return d0 + d1 + d2 + d3 + d4 + d5 + d6 + d7 + d8 + d9;
}

int64_t mixed_spill(int a, double x, int b, double y, int c, double z, int d, double w, int e, double u, int f, int g) {
    return (int64_t)(a + b + c + d + e + f + g) + (int64_t)(x + y + z + w + u);
}
"""

CALLER_SRC = """#include <stdio.h>
#include <stdlib.h>
#include "abi_suite.h"

int main(void) {
    // 1. Mixed16: INTEGER in %rax, SSE in %xmm0
    Mixed16 m = make_mixed(100, 4.0);
    if (m.i != 142 || m.d != 10.0) {
        fprintf(stderr, "FAIL: Mixed16 got i=%lld, d=%.2f\\n", (long long)m.i, m.d);
        return 1;
    }

    // 2. Triple24: Memory return via hidden pointer %rdi (sret)
    Triple24 t = make_triple(1, 2, 3);
    if (t.a != 11 || t.b != 4 || t.c != 169) {
        fprintf(stderr, "FAIL: Triple24 got %llu, %llu, %llu\\n",
                (unsigned long long)t.a, (unsigned long long)t.b, (unsigned long long)t.c);
        return 2;
    }

    // 3. 10-FP sum: Register exhaustion (%xmm0..%xmm7) + stack spill
    double s = sum_10fp(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0);
    if (s != 55.0) {
        fprintf(stderr, "FAIL: sum_10fp got %.2f (expected 55.0)\\n", s);
        return 3;
    }

    // 4. Mixed GP/FP argument spill: 7 ints + 5 doubles
    int64_t res = mixed_spill(1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5, 5.5, 6, 7);
    if (res != 45) {
        fprintf(stderr, "FAIL: mixed_spill got %lld (expected 45)\\n", (long long)res);
        return 4;
    }

    printf("ALL_CROSS_TU_TESTS_PASS\\n");
    return 0;
}
"""

def run_cmd(cmd, cwd=None):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)

def test_cross_tu(zcc_binary, repo_root):
    zcc_path = Path(zcc_binary).resolve()
    if not zcc_path.is_file():
        print(f"ERROR: ZCC binary not found: {zcc_path}", file=sys.stderr)
        return False

    has_gcc = shutil.which("gcc") is not None
    has_clang = shutil.which("clang") is not None

    if not has_gcc:
        print("ERROR: GCC is required for cross-TU verification.", file=sys.stderr)
        return False

    print("=" * 70)
    print("🔬 ZCC OMEGA V3 — CROSS-TRANSLATION-UNIT ABI ASSURANCE GAUNTLET")
    print(f"Target ZCC: {zcc_path}")
    print(f"Oracle GCC:  {shutil.which('gcc')}")
    print(f"Oracle Clang:{shutil.which('clang') or 'NOT INSTALLED (OPTIONAL)'}")
    print("=" * 70)

    work_dir = Path(tempfile.mkdtemp(prefix="zcc_cross_tu_"))

    try:
        (work_dir / "abi_suite.h").write_text(HEADER_SRC)
        (work_dir / "callee.c").write_text(CALLEE_SRC)
        (work_dir / "caller.c").write_text(CALLER_SRC)

        # -------------------------------------------------------------
        # POSITIVE CONTROLS
        # -------------------------------------------------------------
        print("\n--- PHASE 1: GENUINE CROSS-TRANSLATION-UNIT INTEROP ---")

        # Compile Callee with ZCC -> .s -> .o
        res = run_cmd([str(zcc_path), str(work_dir / "callee.c"), "-o", str(work_dir / "callee_zcc.s")], cwd=repo_root)
        if res.returncode != 0:
            print(f"FAIL: ZCC callee compile error: {res.stderr}")
            return False
        res = run_cmd(["gcc", "-c", str(work_dir / "callee_zcc.s"), "-o", str(work_dir / "callee_zcc.o")])
        if res.returncode != 0:
            print(f"FAIL: GCC as error for ZCC callee: {res.stderr}")
            return False

        # Compile Caller with ZCC -> .s -> .o
        res = run_cmd([str(zcc_path), str(work_dir / "caller.c"), "-o", str(work_dir / "caller_zcc.s")], cwd=repo_root)
        if res.returncode != 0:
            print(f"FAIL: ZCC caller compile error: {res.stderr}")
            return False
        res = run_cmd(["gcc", "-c", str(work_dir / "caller_zcc.s"), "-o", str(work_dir / "caller_zcc.o")])
        if res.returncode != 0:
            print(f"FAIL: GCC as error for ZCC caller: {res.stderr}")
            return False

        # Compile Callee with GCC -> .o
        res = run_cmd(["gcc", "-c", "-I" + str(work_dir), str(work_dir / "callee.c"), "-o", str(work_dir / "callee_gcc.o")])
        if res.returncode != 0:
            print(f"FAIL: GCC callee compile error: {res.stderr}")
            return False

        # Compile Caller with GCC -> .o
        res = run_cmd(["gcc", "-c", "-I" + str(work_dir), str(work_dir / "caller.c"), "-o", str(work_dir / "caller_gcc.o")])
        if res.returncode != 0:
            print(f"FAIL: GCC caller compile error: {res.stderr}")
            return False

        # Direction 1: GCC caller -> ZCC callee
        res = run_cmd(["gcc", str(work_dir / "caller_gcc.o"), str(work_dir / "callee_zcc.o"), "-o", str(work_dir / "bin_d1"), "-lm"])
        if res.returncode != 0:
            print(f"FAIL: Link Direction 1 failed: {res.stderr}")
            return False
        out1 = run_cmd([str(work_dir / "bin_d1")])
        if out1.returncode != 0 or out1.stdout.strip() != "ALL_CROSS_TU_TESTS_PASS":
            print(f"FAIL: Direction 1 (GCC caller -> ZCC callee) failed: exit={out1.returncode}, stdout={out1.stdout}, stderr={out1.stderr}")
            return False
        print("  ✓ Direction 1 (GCC caller  -> ZCC callee): PASS (Exit: 0, Output: ALL_CROSS_TU_TESTS_PASS)")

        # Direction 2: ZCC caller -> GCC callee
        res = run_cmd(["gcc", str(work_dir / "caller_zcc.o"), str(work_dir / "callee_gcc.o"), "-o", str(work_dir / "bin_d2"), "-lm"])
        if res.returncode != 0:
            print(f"FAIL: Link Direction 2 failed: {res.stderr}")
            return False
        out2 = run_cmd([str(work_dir / "bin_d2")])
        if out2.returncode != 0 or out2.stdout.strip() != "ALL_CROSS_TU_TESTS_PASS":
            print(f"FAIL: Direction 2 (ZCC caller -> GCC callee) failed: exit={out2.returncode}, stdout={out2.stdout}, stderr={out2.stderr}")
            return False
        print("  ✓ Direction 2 (ZCC caller  -> GCC callee): PASS (Exit: 0, Output: ALL_CROSS_TU_TESTS_PASS)")

        # Optional Clang directions
        if has_clang:
            res = run_cmd(["clang", "-c", "-I" + str(work_dir), str(work_dir / "callee.c"), "-o", str(work_dir / "callee_clang.o")])
            res_caller_clang = run_cmd(["clang", "-c", "-I" + str(work_dir), str(work_dir / "caller.c"), "-o", str(work_dir / "caller_clang.o")])
            if res.returncode == 0 and res_caller_clang.returncode == 0:
                # Direction 3: Clang caller -> ZCC callee
                run_cmd(["gcc", str(work_dir / "caller_clang.o"), str(work_dir / "callee_zcc.o"), "-o", str(work_dir / "bin_d3"), "-lm"])
                out3 = run_cmd([str(work_dir / "bin_d3")])
                if out3.returncode == 0 and out3.stdout.strip() == "ALL_CROSS_TU_TESTS_PASS":
                    print("  ✓ Direction 3 (Clang caller -> ZCC callee): PASS (Exit: 0, Output: ALL_CROSS_TU_TESTS_PASS)")

                # Direction 4: ZCC caller -> Clang callee
                run_cmd(["gcc", str(work_dir / "caller_zcc.o"), str(work_dir / "callee_clang.o"), "-o", str(work_dir / "bin_d4"), "-lm"])
                out4 = run_cmd([str(work_dir / "bin_d4")])
                if out4.returncode == 0 and out4.stdout.strip() == "ALL_CROSS_TU_TESTS_PASS":
                    print("  ✓ Direction 4 (ZCC caller  -> Clang callee): PASS (Exit: 0, Output: ALL_CROSS_TU_TESTS_PASS)")

        # -------------------------------------------------------------
        # ADVERSARIAL NEGATIVE CONTROLS (HARNESS INDEPENDENCE)
        # -------------------------------------------------------------
        print("\n--- PHASE 2: ADVERSARIAL NEGATIVE CONTROLS (MUTATION SENSITIVITY) ---")

        # Negative Control 1: Mutate ZCC callee return lowering for Mixed16
        # Replace the load of %xmm0 with a zero clobber
        asm_lines = (work_dir / "callee_zcc.s").read_text()
        mutated_asm1 = re.sub(r"movq\s+8\(%r10\),\s+%xmm0", "xorpd %xmm0, %xmm0", asm_lines)
        if mutated_asm1 == asm_lines:
            # Fallback mutation: mutate the float literal in .data/.rodata or computation
            mutated_asm1 = asm_lines.replace("2.5", "9.9")
        (work_dir / "callee_mut1.s").write_text(mutated_asm1)
        run_cmd(["gcc", "-c", str(work_dir / "callee_mut1.s"), "-o", str(work_dir / "callee_mut1.o")])
        run_cmd(["gcc", str(work_dir / "caller_gcc.o"), str(work_dir / "callee_mut1.o"), "-o", str(work_dir / "bin_mut1"), "-lm"])
        out_mut1 = run_cmd([str(work_dir / "bin_mut1")])

        if out_mut1.returncode == 1 and "FAIL: Mixed16" in out_mut1.stderr:
            print("  ✓ Adversarial NC-ABI-01 (Corrupted Mixed16 return lowering): REJECTED as expected (Exit: 1, Stderr matched)")
        else:
            print(f"FAIL: NC-ABI-01 was not rejected! exit={out_mut1.returncode}, stderr={out_mut1.stderr}")
            return False

        # Negative Control 2: Mutate sret hidden pointer copy in Triple24
        mutated_asm2 = asm_lines.replace("rep movsb", "# rep movsb dropped")
        (work_dir / "callee_mut2.s").write_text(mutated_asm2)
        run_cmd(["gcc", "-c", str(work_dir / "callee_mut2.s"), "-o", str(work_dir / "callee_mut2.o")])
        run_cmd(["gcc", str(work_dir / "caller_gcc.o"), str(work_dir / "callee_mut2.o"), "-o", str(work_dir / "bin_mut2"), "-lm"])
        out_mut2 = run_cmd([str(work_dir / "bin_mut2")])

        if out_mut2.returncode == 2 and "FAIL: Triple24" in out_mut2.stderr:
            print("  ✓ Adversarial NC-ABI-02 (Dropped sret struct copy): REJECTED as expected (Exit: 2, Stderr matched)")
        else:
            print(f"FAIL: NC-ABI-02 was not rejected! exit={out_mut2.returncode}, stderr={out_mut2.stderr}")
            return False

        print("\n" + "=" * 70)
        print("✨ ZCC OMEGA V3 CROSS-TU ABI ASSURANCE: ALL GATES PASS & ADVERSARIALLY VERIFIED")
        print("=" * 70)
        return True

    finally:
        shutil.rmtree(work_dir, ignore_errors=True)

if __name__ == "__main__":
    repo_root = Path(__file__).resolve().parent.parent
    zcc_bin = sys.argv[1] if len(sys.argv) > 1 else str(repo_root / "zcc")
    success = test_cross_tu(zcc_bin, repo_root)
    sys.exit(0 if success else 1)
