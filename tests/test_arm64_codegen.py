"""
ZCC ARM64 AAPCS64 Codegen Unit Test Suite
Tests AArch64 register mappings, 16-byte stack frame alignment math, prologue/epilogue generation, and assembly snippet formatting.
"""

import os
import subprocess
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def compile_c_arm64_harness():
    """Builds a standalone C test harness that exercises src/arm64_codegen.c functions."""
    harness_c = os.path.join(REPO_ROOT, "tests", "temp_arm64_harness.c")
    bin_out = os.path.join(REPO_ROOT, "tests", "temp_arm64_harness")
    
    code = r"""
#include <stdio.h>
#include "../src/arm64_codegen.h"

int main() {
    printf("REG_X0:%s\n", arm64_get_reg_name64(ARM64_REG_X0));
    printf("REG_W0:%s\n", arm64_get_reg_name32(ARM64_REG_X0));
    printf("REG_FP:%s\n", arm64_get_reg_name64(ARM64_REG_FP));
    printf("REG_LR:%s\n", arm64_get_reg_name64(ARM64_REG_LR));
    printf("REG_SP:%s\n", arm64_get_reg_name64(ARM64_REG_SP));
    
    size_t align_8 = arm64_align_stack_frame(8);
    size_t align_24 = arm64_align_stack_frame(24);
    printf("ALIGN_8:%zu\n", align_8);
    printf("ALIGN_24:%zu\n", align_24);

    /* AAPCS64 Nested Struct Pass-by-Value Classification Tests */
    AAPCS64StructArgLayout s8 = arm64_classify_struct_arg(8);   /* 8-byte struct (1 GP reg: x0) */
    AAPCS64StructArgLayout s16 = arm64_classify_struct_arg(16); /* 16-byte nested struct (2 GP regs: x0, x1) */
    AAPCS64StructArgLayout s32 = arm64_classify_struct_arg(32); /* 32-byte nested struct (passed by ref pointer) */

    printf("STRUCT_8_MODE:%d_REGS:%d\n", s8.mode, s8.num_gp_regs);
    printf("STRUCT_16_MODE:%d_REGS:%d\n", s16.mode, s16.num_gp_regs);
    printf("STRUCT_32_MODE:%d_REGS:%d\n", s32.mode, s32.num_gp_regs);

    int res = zcc_emit_arm64_assembly_to_file("/tmp/test_arm64_out.s", "my_arm_func", 16);
    printf("ARM64_EMIT_RES:%d\n", res);
    return 0;
}
"""
    with open(harness_c, "w") as f:
        f.write(code)
        
    cmd = ["gcc", "-Isrc", harness_c, os.path.join(REPO_ROOT, "src", "arm64_codegen.c"), "-o", bin_out]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return bin_out, res

class TestARM64Codegen(unittest.TestCase):
    
    @classmethod
    def setUpClass(cls):
        cls.bin_out, cls.build_res = compile_c_arm64_harness()

    @classmethod
    def tearDownClass(cls):
        harness_c = os.path.join(REPO_ROOT, "tests", "temp_arm64_harness.c")
        if os.path.exists(harness_c):
            os.remove(harness_c)
        if os.path.exists(cls.bin_out):
            os.remove(cls.bin_out)
        if os.path.exists("/tmp/test_arm64_out.s"):
            os.remove("/tmp/test_arm64_out.s")

    def test_01_build_harness(self):
        """Verify C harness builds with zero errors."""
        self.assertEqual(self.build_res.returncode, 0, f"Build failed: {self.build_res.stderr}")

    def test_02_arm64_alignment_and_assembly_emission(self):
        """Executes test harness and checks register names, 16-byte stack alignment, and assembly output."""
        res = subprocess.run([self.bin_out], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Harness crashed: {res.stderr}")
        self.assertIn("REG_X0:x0", res.stdout)
        self.assertIn("REG_W0:w0", res.stdout)
        self.assertIn("REG_FP:x29", res.stdout)
        self.assertIn("REG_LR:x30", res.stdout)
        self.assertIn("REG_SP:sp", res.stdout)
        self.assertIn("ALIGN_8:32", res.stdout)  # (8+16)=24 -> aligned to 32
        self.assertIn("ALIGN_24:48", res.stdout) # (24+16)=40 -> aligned to 48 (next multiple of 16)
        self.assertIn("ARM64_EMIT_RES:0", res.stdout)

        # AAPCS64 Nested Struct Pass-by-Value Assertions
        self.assertIn("STRUCT_8_MODE:0_REGS:1", res.stdout)   # In 1 GP reg (x0)
        self.assertIn("STRUCT_16_MODE:0_REGS:2", res.stdout)  # In 2 GP regs (x0, x1)
        self.assertIn("STRUCT_32_MODE:1_REGS:1", res.stdout)  # Passed by ref (1 pointer reg)

        # Inspect emitted ARM64 assembly file
        self.assertTrue(os.path.exists("/tmp/test_arm64_out.s"))
        with open("/tmp/test_arm64_out.s", "r") as f:
            asm_content = f.read()

        self.assertIn(".arch armv8-a", asm_content)
        self.assertIn(".global my_arm_func", asm_content)
        self.assertIn("stp x29, x30, [sp, #-32]!", asm_content)
        self.assertIn("add x0, x0, x1", asm_content)
        self.assertIn("ldp x29, x30, [sp], #32", asm_content)
        self.assertIn("ret", asm_content)

    def test_03_zcc_cli_aarch64_full_pipeline(self):
        """Compiles C code using ./zcc -target aarch64 and verifies assembly via clang oracle."""
        zcc_bin = os.path.join(REPO_ROOT, "zcc")
        src_file = os.path.join(REPO_ROOT, "tests", "regress_arm_arith.c")
        asm_out = "/tmp/test_aarch64_pipeline.s"
        obj_out = "/tmp/test_aarch64_pipeline.o"

        # 1. Run ZCC with AArch64 target
        zcc_cmd = [zcc_bin, "-target", "aarch64", src_file, "-o", asm_out]
        res = subprocess.run(zcc_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"ZCC compilation failed: {res.stderr}")

        # 2. Inspect emitted assembly tokens
        with open(asm_out, "r") as f:
            asm_content = f.read()
        self.assertIn(".arch armv8-a", asm_content)
        self.assertIn("stp x29, x30", asm_content)
        self.assertIn("ldp x29, x30", asm_content)
        self.assertIn("sdiv x2, x0, x1", asm_content)
        self.assertIn("msub x0, x2, x1, x0", asm_content)
        self.assertIn("udiv x2, x0, x1", asm_content)
        self.assertIn("sdiv x0, x0, x1", asm_content)
        self.assertIn("udiv x0, x0, x1", asm_content)
        self.assertIn("asr x0, x0, x1", asm_content)
        self.assertIn("lsr x0, x0, x1", asm_content)
        self.assertIn("ret", asm_content)

        # 3. Assemble with independent oracle (clang --target=aarch64-linux-gnu)
        clang_cmd = ["clang", "--target=aarch64-linux-gnu", "-c", asm_out, "-o", obj_out]
        clang_res = subprocess.run(clang_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(clang_res.returncode, 0, f"Clang oracle assembly failed: {clang_res.stderr}")

        # 4. Verify ELF64 AArch64 object header
        readelf_cmd = ["llvm-readelf", "-h", obj_out]
        readelf_res = subprocess.run(readelf_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(readelf_res.returncode, 0)
        self.assertIn("ELF64", readelf_res.stdout)
        self.assertIn("AArch64", readelf_res.stdout)

if __name__ == "__main__":
    unittest.main()
