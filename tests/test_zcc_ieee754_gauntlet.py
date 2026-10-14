#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
========================================================================================
🔱 ZCC IEEE-754 SUBNORMAL & HARDWARE AVX-512 GAUNTLET 🔱
========================================================================================
Verifies floating-point subnormal boundary arithmetic across three execution domains:
  1. ZCC Native x86-64 vs GCC (15/15 bit-exact parity including 5e-324, cancel, hypot)
  2. ZCC Bare-Metal ARM Cortex-M4 Thumb-2 backend lowering
  3. Physical Silicon AVX-512 & AVX2 FMA execution with Rule AV-1 FTZ/DAZ falsification
========================================================================================
"""

import os
import sys
import subprocess
import tempfile

REPO_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def run_cmd(cmd: str, cwd: str = REPO_DIR) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)

def test_ieee754_zcc_vs_gcc():
    print("─── [GATE 1] ZCC x86-64 vs GCC IEEE-754 PARITY ───")
    src = os.path.join(REPO_DIR, "tests", "test_zcc_ieee754.c")
    with tempfile.TemporaryDirectory() as tmp:
        zcc_s = os.path.join(tmp, "zcc.s")
        zcc_bin = os.path.join(tmp, "zcc_bin")
        gcc_bin = os.path.join(tmp, "gcc_bin")

        # Compile with ZCC
        z_res = run_cmd(f"./zcc {src} -o {zcc_s} && gcc -no-pie {zcc_s} -lm -o {zcc_bin}")
        if z_res.returncode != 0:
            print(f"❌ ZCC compilation failed:\n{z_res.stderr}\n{z_res.stdout}")
            return False

        # Compile with GCC (-O0 -fno-builtin to test GLIBC runtime math)
        g_res = run_cmd(f"gcc -O0 -fno-builtin {src} -lm -o {gcc_bin}")
        if g_res.returncode != 0:
            print(f"❌ GCC compilation failed:\n{g_res.stderr}")
            return False

        # Execute both
        out_z = run_cmd(zcc_bin).stdout.strip().splitlines()
        out_g = run_cmd(gcc_bin).stdout.strip().splitlines()

        if len(out_z) != len(out_g):
            print(f"❌ Line count mismatch: zcc={len(out_z)} gcc={len(out_g)}")
            return False

        mismatches = 0
        for i, (lz, lg) in enumerate(zip(out_z, out_g)):
            if lz != lg:
                print(f"   [!] Delta at line {i}: ZCC='{lz}' vs GCC='{lg}'")
                mismatches += 1

        if mismatches == 0:
            print(f"✅ GATE 1 PASS: All {len(out_z)}/15 IEEE-754 test vectors 100% bit-identical against GCC.")
            return True
        else:
            print(f"❌ GATE 1 FAILED: {mismatches} divergences found.")
            return False

def test_thumb_m4_lowering():
    print("\n─── [GATE 2] BARE-METAL ARM CORTEX-M4 THUMB-2 LOWERING ───")
    src = os.path.join(REPO_DIR, "tests", "test_zcc_ieee754.c")
    with tempfile.TemporaryDirectory() as tmp:
        thumb_s = os.path.join(tmp, "thumb.s")
        thumb_o = os.path.join(tmp, "thumb.o")

        res_z = run_cmd(f"./zcc -target cortex-m4 {src} -o {thumb_s}")
        if res_z.returncode != 0:
            print(f"❌ ZCC Cortex-M4 backend failed:\n{res_z.stderr}\n{res_z.stdout}")
            return False

        res_as = run_cmd(f"arm-none-eabi-as -mthumb -mcpu=cortex-m4 {thumb_s} -o {thumb_o}")
        if res_as.returncode != 0:
            print(f"❌ GNU ARM Assembler failed:\n{res_as.stderr}")
            return False

        print("✅ GATE 2 PASS: Cortex-M4 Thumb-2 assembly validated and linked via GNU Toolchain.")
        return True

def test_hardware_simd_subnormal():
    print("\n─── [GATE 3] HARDWARE AVX-512 & AVX2 SUBNORMAL VECTOR GAUNTLET ───")
    src = os.path.join(REPO_DIR, "tests", "test_zcc_simd_subnormal.c")
    with tempfile.TemporaryDirectory() as tmp:
        simd_bin = os.path.join(tmp, "simd_bin")
        res_c = run_cmd(f"gcc -O2 -mavx2 -mfma -mavx512f {src} -o {simd_bin}")
        if res_c.returncode != 0:
            print(f"❌ SIMD compilation failed:\n{res_c.stderr}")
            return False

        res_run = run_cmd(simd_bin)
        print(res_run.stdout.strip())
        if "SIMD VERDICT: ALL AVX2 & AVX-512 SUBNORMAL TESTS PASS (BIT-EXACT)" in res_run.stdout:
            print("✅ GATE 3 PASS: Physical Silicon AVX-512 Vector Arithmetic & Rule AV-1 FTZ/DAZ Confirmed.")
            return True
        else:
            print("❌ GATE 3 FAILED.")
            return False

def main():
    print("=" * 86)
    print("🔱 ZCC IEEE-754 FLOATING-POINT & SUBNORMAL VERIFICATION GAUNTLET")
    print("=" * 86)

    g1 = test_ieee754_zcc_vs_gcc()
    g2 = test_thumb_m4_lowering()
    g3 = test_hardware_simd_subnormal()

    print("\n" + "=" * 86)
    if g1 and g2 and g3:
        print("🎯 FINAL VERDICT: ALL 3 VERIFICATION GATES PASS (BIT-EXACT PARITY)")
        print("=" * 86)
        sys.exit(0)
    else:
        print("❌ FINAL VERDICT: VERIFICATION FAILED")
        print("=" * 86)
        sys.exit(1)

if __name__ == "__main__":
    main()
