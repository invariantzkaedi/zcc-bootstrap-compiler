"""
ZCC RISC-V RV64GC Codegen Unit Test Suite
Tests RV64GC register mappings, 16-byte stack frame alignment math, prologue/epilogue generation, and assembly snippet formatting.
"""

import os
import subprocess
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def compile_c_riscv_harness():
    """Builds a standalone C test harness that exercises src/riscv_codegen.c functions."""
    harness_c = os.path.join(REPO_ROOT, "tests", "temp_riscv_harness.c")
    bin_out = os.path.join(REPO_ROOT, "tests", "temp_riscv_harness")
    
    code = """
#include <stdio.h>
#include "../src/riscv_codegen.h"

int main() {
    printf("REG_ZERO:%s\\n", riscv_get_reg_name(RISCV_REG_ZERO));
    printf("REG_RA:%s\\n", riscv_get_reg_name(RISCV_REG_RA));
    printf("REG_SP:%s\\n", riscv_get_reg_name(RISCV_REG_SP));
    printf("REG_FP:%s\\n", riscv_get_reg_name(RISCV_REG_FP));
    printf("REG_A0:%s\\n", riscv_get_reg_name(RISCV_REG_A0));
    printf("REG_A1:%s\\n", riscv_get_reg_name(RISCV_REG_A1));
    
    size_t align_8 = riscv_align_stack_frame(8);
    size_t align_24 = riscv_align_stack_frame(24);
    printf("ALIGN_8:%zu\\n", align_8);
    printf("ALIGN_24:%zu\\n", align_24);

    int res = zcc_emit_riscv_assembly_to_file("/tmp/test_riscv_out.s", "my_riscv_func", 16);
    printf("RISCV_EMIT_RES:%d\\n", res);
    return 0;
}
"""
    with open(harness_c, "w") as f:
        f.write(code)
        
    cmd = ["gcc", "-Isrc", harness_c, os.path.join(REPO_ROOT, "src", "riscv_codegen.c"), "-o", bin_out]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return bin_out, res

class TestRISCVCodegen(unittest.TestCase):
    
    @classmethod
    def setUpClass(cls):
        cls.bin_out, cls.build_res = compile_c_riscv_harness()

    @classmethod
    def tearDownClass(cls):
        harness_c = os.path.join(REPO_ROOT, "tests", "temp_riscv_harness.c")
        if os.path.exists(harness_c):
            os.remove(harness_c)
        if os.path.exists(cls.bin_out):
            os.remove(cls.bin_out)
        if os.path.exists("/tmp/test_riscv_out.s"):
            os.remove("/tmp/test_riscv_out.s")

    def test_01_build_harness(self):
        """Verify C harness builds with zero errors."""
        self.assertEqual(self.build_res.returncode, 0, f"Build failed: {self.build_res.stderr}")

    def test_02_riscv_alignment_and_assembly_emission(self):
        """Executes test harness and checks register names, 16-byte stack alignment, and assembly output."""
        res = subprocess.run([self.bin_out], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Harness crashed: {res.stderr}")
        self.assertIn("REG_ZERO:zero", res.stdout)
        self.assertIn("REG_RA:ra", res.stdout)
        self.assertIn("REG_SP:sp", res.stdout)
        self.assertIn("REG_FP:s0", res.stdout)
        self.assertIn("REG_A0:a0", res.stdout)
        self.assertIn("REG_A1:a1", res.stdout)
        self.assertIn("ALIGN_8:32", res.stdout)  # (8+16)=24 -> aligned to 32
        self.assertIn("ALIGN_24:48", res.stdout) # (24+16)=40 -> aligned to 48
        self.assertIn("RISCV_EMIT_RES:0", res.stdout)

        # Inspect emitted RISC-V assembly file
        self.assertTrue(os.path.exists("/tmp/test_riscv_out.s"))
        with open("/tmp/test_riscv_out.s", "r") as f:
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
