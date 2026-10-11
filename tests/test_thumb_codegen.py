#!/usr/bin/env python3
"""
Regression test suite for ZCC Bare-metal ARM/Thumb-2 lowering
Targeting ARM Cortex-M0+ (ARMv6-M) and Cortex-M4 (ARMv7E-M / Flipper Zero STM32WB55).
"""

import os
import subprocess
import sys
import tempfile

ZCC_BIN = "./zcc"
AS_BIN = "arm-none-eabi-as"
OBJDUMP_BIN = "arm-none-eabi-objdump"
GCC_BIN = "arm-none-eabi-gcc"

def run_cmd(cmd, check=True):
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if check and res.returncode != 0:
        print(f"FAILED: {cmd}\nSTDOUT:\n{res.stdout}\nSTDERR:\n{res.stderr}")
        sys.exit(1)
    return res

def test_cortex_m0plus_basic():
    print("[1/5] Testing Cortex-M0+ (thumbv6m) arithmetic and parameter slots...")
    src = """
    int test_calc(int a, int b, int c, int d) {
        int x = a + b;
        int y = c - d;
        int z = x * y;
        return z;
    }
    """
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as f:
        f.write(src)
        c_path = f.name
    s_path = c_path.replace(".c", ".s")
    o_path = c_path.replace(".c", ".o")
    try:
        run_cmd(f"{ZCC_BIN} {c_path} -target thumb -S -o {s_path}")
        with open(s_path, "r") as sf:
            content = sf.read()
        assert ".cpu cortex-m0plus" in content, "Missing .cpu cortex-m0plus"
        assert ".thumb" in content, "Missing .thumb"
        assert "adds r4, r7, r4" in content, "Missing thumb adds"
        run_cmd(f"{AS_BIN} -mthumb -mcpu=cortex-m0plus {s_path} -o {o_path}")
        dump = run_cmd(f"{OBJDUMP_BIN} -d {o_path}").stdout
        assert "<test_calc>:" in dump, "Missing test_calc symbol in disassembly"
        print("  --> Cortex-M0+ basic arithmetic & stack slots PASS")
    finally:
        for p in [c_path, s_path, o_path]:
            if os.path.exists(p): os.remove(p)

def test_cortex_m4_hardware_div_mod():
    print("[2/5] Testing Cortex-M4 hardware sdiv and mls (Flipper Zero)...")
    src = """
    int fast_divmod(int n, int d) {
        int q = n / d;
        int r = n % d;
        return q + r;
    }
    """
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as f:
        f.write(src)
        c_path = f.name
    s_path = c_path.replace(".c", ".s")
    o_path = c_path.replace(".c", ".o")
    try:
        run_cmd(f"{ZCC_BIN} {c_path} -target cortex-m4 -S -o {s_path}")
        with open(s_path, "r") as sf:
            content = sf.read()
        assert ".cpu cortex-m4" in content, "Missing .cpu cortex-m4"
        assert "sdiv r0, r0, r1" in content, "Missing hardware sdiv"
        assert "mls r0, r2, r1, r0" in content, "Missing hardware mls"
        assert "__aeabi_idiv" not in content, "Unexpected software div libcall on Cortex-M4"
        run_cmd(f"{AS_BIN} -mthumb -mcpu=cortex-m4 {s_path} -o {o_path}")
        dump = run_cmd(f"{OBJDUMP_BIN} -d {o_path}").stdout
        assert "sdiv" in dump, "sdiv missing in Cortex-M4 binary"
        assert "mls" in dump, "mls missing in Cortex-M4 binary"
        print("  --> Cortex-M4 hardware division & modulo PASS")
    finally:
        for p in [c_path, s_path, o_path]:
            if os.path.exists(p): os.remove(p)

def test_flipper_string_and_calls():
    print("[3/5] Testing Flipper Zero string literals and AAPCS call registers...")
    c_path = "tests/test_flipper_app.c"
    s_path = "tests/test_flipper_app.s"
    o_path = "tests/test_flipper_app.o"
    run_cmd(f"{ZCC_BIN} {c_path} -target flipper -S -o {s_path}")
    with open(s_path, "r") as sf:
        content = sf.read()
    assert "leaq" not in content, "Found x86 leaq instruction in ARM assembly!"
    assert "ldr r0, =.LS" in content, "Missing ARM string literal ldr"
    assert "bl canvas_draw_str" in content, "Missing call bl canvas_draw_str"
    run_cmd(f"{AS_BIN} -mthumb -mcpu=cortex-m4 {s_path} -o {o_path}")
    relocs = run_cmd(f"{OBJDUMP_BIN} -r {o_path}").stdout
    assert "R_ARM_THM_CALL" in relocs, "Missing R_ARM_THM_CALL relocation"
    assert "canvas_draw_str" in relocs, "Missing relocation for canvas_draw_str"
    print("  --> Flipper Zero string literals and AAPCS calls PASS")

def test_end_to_end_link():
    print("[4/5] Testing end-to-end GNU EABI ELF32 linking with arm-none-eabi-gcc...")
    src = """
    int compute(int a, int b) {
        return (a * 3) + (b / 2);
    }
    """
    harness = """
    extern int compute(int a, int b);
    int main(void) {
        return compute(10, 20);
    }
    """
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as f:
        f.write(src)
        src_c = f.name
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as f:
        f.write(harness)
        main_c = f.name
    src_s = src_c.replace(".c", ".s")
    src_o = src_c.replace(".c", ".o")
    elf_path = src_c.replace(".c", ".elf")
    try:
        run_cmd(f"{ZCC_BIN} {src_c} -target flipper -S -o {src_s}")
        run_cmd(f"{AS_BIN} -mthumb -mcpu=cortex-m4 {src_s} -o {src_o}")
        run_cmd(f"{GCC_BIN} -mcpu=cortex-m4 -mthumb --specs=nosys.specs {main_c} {src_o} -o {elf_path}")
        size_out = run_cmd(f"arm-none-eabi-size {elf_path}").stdout
        print(f"  Linked ELF size:\n{size_out.strip()}")
        print("  --> End-to-end ARM EABI link PASS")
    finally:
        for p in [src_c, main_c, src_s, src_o, elf_path]:
            if os.path.exists(p): os.remove(p)

def test_flag_aliases():
    print("[5/5] Testing CLI target flag aliases...")
    src = "int foo(void) { return 42; }"
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as f:
        f.write(src)
        c_path = f.name
    s_path = c_path.replace(".c", ".s")
    aliases = ["-target thumb", "-target cortex-m0plus", "-target cortex-m4", "-target flipper", "-target thumbv7em"]
    try:
        for flg in aliases:
            run_cmd(f"{ZCC_BIN} {c_path} {flg} -S -o {s_path}")
            with open(s_path, "r") as sf:
                txt = sf.read()
            assert ".thumb" in txt, f"Flag {flg} did not activate thumb mode"
        print("  --> All CLI target flag aliases PASS")
    finally:
        for p in [c_path, s_path]:
            if os.path.exists(p): os.remove(p)

def test_unary_and_indirect_calls():
    print("[6/6] Testing hardened unary ops (mvns, negs, cmp) and indirect blx calls...")
    src = """
    typedef int (*op_fn)(int, int);
    int apply_op(op_fn fn, int x, int y) {
        int not_x = ~x;
        int neg_y = -y;
        int is_zero = !x;
        return fn(not_x + neg_y, is_zero);
    }
    """
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as f:
        f.write(src)
        c_path = f.name
    s_path = c_path.replace(".c", ".s")
    o_path = c_path.replace(".c", ".o")
    try:
        run_cmd(f"{ZCC_BIN} {c_path} -target flipper -S -o {s_path}")
        with open(s_path, "r") as sf:
            content = sf.read()
        assert "mvns" in content, "Missing mvns for bitwise not"
        assert "negs" in content, "Missing negs for negation"
        assert "blx r4" in content, "Missing blx r4 for indirect call"
        assert ".thumb_func" in content, "Missing .thumb_func directive"
        assert "sete" not in content, "Found x86 sete in ARM assembly!"
        run_cmd(f"{AS_BIN} -mthumb -mcpu=cortex-m4 {s_path} -o {o_path}")
        dump = run_cmd(f"{OBJDUMP_BIN} -d {o_path}").stdout
        assert "blx\tr4" in dump, "blx r4 missing in assembled object"
        print("  --> Unary ops and indirect blx call PASS")
    finally:
        for p in [c_path, s_path, o_path]:
            if os.path.exists(p): os.remove(p)

if __name__ == "__main__":
    test_cortex_m0plus_basic()
    test_cortex_m4_hardware_div_mod()
    test_flipper_string_and_calls()
    test_end_to_end_link()
    test_flag_aliases()
    test_unary_and_indirect_calls()
    print("\nALL 6 HARDENED ARM/THUMB-2/FLIPPER REGRESSION TESTS PASSED CLEANLY!")

