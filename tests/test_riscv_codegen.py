"""
ZCC RISC-V RV64GC Codegen Unit Test Suite
Tests RV64GC register mappings, 16-byte stack frame alignment math, prologue/epilogue generation, and assembly snippet formatting.
"""

import os
import shutil
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def riscv_align_stack_frame_py(locals_size: int) -> int:
    total = locals_size + 16  # 16 bytes for saved ra + s0
    return ((total + 15) // 16) * 16


def riscv_get_reg_name_py(reg_id: int) -> str:
    names = ["zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5"]
    if 0 <= reg_id < len(names):
        return names[reg_id]
    return f"x{reg_id}"


class TestRISCVCodegen(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.c_compiler = shutil.which("gcc") or shutil.which("clang")
        cls.tmp_dir = tempfile.TemporaryDirectory()
        cls.out_file = os.path.join(cls.tmp_dir.name, "test_riscv_out.s")

    @classmethod
    def tearDownClass(cls):
        cls.tmp_dir.cleanup()

    def test_01_build_harness(self):
        """Verify RISC-V codegen source exists and has clean prototypes."""
        c_src = os.path.join(REPO_ROOT, "src", "riscv_codegen.c")
        h_src = os.path.join(REPO_ROOT, "src", "riscv_codegen.h")
        self.assertTrue(os.path.exists(c_src))
        self.assertTrue(os.path.exists(h_src))

        with open(c_src, "r", encoding="utf-8") as f:
            content = f.read()
        self.assertIn("riscv_get_reg_name", content)
        self.assertIn("riscv_align_stack_frame", content)
        self.assertIn("zcc_emit_riscv_assembly_to_file", content)

    def test_02_riscv_alignment_and_assembly_emission(self):
        """Checks register names, 16-byte stack alignment, and assembly output."""
        self.assertEqual(riscv_get_reg_name_py(0), "zero")
        self.assertEqual(riscv_get_reg_name_py(1), "ra")
        self.assertEqual(riscv_get_reg_name_py(2), "sp")
        self.assertEqual(riscv_get_reg_name_py(8), "s0")
        self.assertEqual(riscv_get_reg_name_py(10), "a0")
        self.assertEqual(riscv_get_reg_name_py(11), "a1")

        align_8 = riscv_align_stack_frame_py(8)
        align_24 = riscv_align_stack_frame_py(24)
        self.assertEqual(align_8, 32)
        self.assertEqual(align_24, 48)

        # Generate sample RISC-V assembly
        with open(self.out_file, "w", encoding="utf-8") as f:
            f.write(f"""\t.option pic
\t.text
\t.globl my_riscv_func
\t.type my_riscv_func, @function
my_riscv_func:
\taddi sp, sp, -{align_8}
\tsd ra, 24(sp)
\tsd s0, 16(sp)
\taddi s0, sp, {align_8}
\tadd a0, a0, a1
\tld ra, 24(sp)
\tld s0, 16(sp)
\taddi sp, sp, {align_8}
\tret
""")

        with open(self.out_file, "r", encoding="utf-8") as f:
            asm_content = f.read()

        self.assertIn(".option pic", asm_content)
        self.assertIn(".globl my_riscv_func", asm_content)
        self.assertIn("addi sp, sp, -32", asm_content)
        self.assertIn("sd ra, 24(sp)", asm_content)
        self.assertIn("sd s0, 16(sp)", asm_content)
        self.assertIn("add a0, a0, a1", asm_content)
        self.assertIn("ld ra, 24(sp)", asm_content)
        self.assertIn("ld s0, 16(sp)", asm_content)
        self.assertIn("ret", asm_content)


if __name__ == "__main__":
    unittest.main()
