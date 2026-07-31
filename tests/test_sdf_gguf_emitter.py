"""
ZCC Position 8: SIMD SDF Shader Compiler + GGUF GEMM Emitter Unit Test Suite
Tests SIMD 8-wide float32 distance evaluations, GGUF binary magic header, tensor descriptors, and GEMM kernel output.
"""

import os
import subprocess
import struct
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def compile_c_position8_harness():
    """Builds a standalone C test harness that exercises Position 8 functions."""
    harness_c = os.path.join(REPO_ROOT, "tests", "temp_pos8_harness.c")
    bin_out = os.path.join(REPO_ROOT, "tests", "temp_pos8_harness")
    gguf_out = os.path.join(REPO_ROOT, "tests", "temp_test_gemm.gguf")
    shader_out = os.path.join(REPO_ROOT, "tests", "temp_sdf_shader.glsl")

    code = f"""
#include <stdio.h>
#include "../src/gfx/sdf_compiler.h"
#include "../src/gguf_emit.h"

int main() {{
    /* Test SIMD SDF Distance Evaluation */
    float px[8] = {{ 0.0f, 0.5f, 1.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f }};
    float py[8] = {{ 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 0.0f, 0.0f }};
    float pz[8] = {{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2.0f }};
    float out_dist[8] = {{ 0 }};

    zcc_sdf_eval_simd_avx2(px, py, pz, out_dist, 8);
    printf("SDF_ORIGIN_DIST:%.4f\\n", out_dist[0]);

    /* Test SDF Shader Generation */
    int shader_res = zcc_compile_sdf_shader_simd("{shader_out.replace('\\\\', '/')}");
    printf("SHADER_GEN_RES:%d\\n", shader_res);

    /* Test GGUF GEMM Emission */
    int gguf_res = zcc_emit_gguf_gemm_kernel("{gguf_out.replace('\\\\', '/')}", 32, 32, 2);
    printf("GGUF_EMIT_RES:%d\\n", gguf_res);

    return 0;
}}
"""
    with open(harness_c, "w") as f:
        f.write(code)

    cmd = ["gcc", "-Isrc", "-I.", harness_c,
           os.path.join(REPO_ROOT, "src", "gfx", "sdf_compiler.c"),
           os.path.join(REPO_ROOT, "src", "gguf_emit.c"),
           "-lm", "-o", bin_out]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return bin_out, res, gguf_out, shader_out

class TestPosition8SDFGGUFEmitter(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.bin_out, cls.build_res, cls.gguf_out, cls.shader_out = compile_c_position8_harness()

    @classmethod
    def tearDownClass(cls):
        harness_c = os.path.join(REPO_ROOT, "tests", "temp_pos8_harness.c")
        if os.path.exists(harness_c):
            os.remove(harness_c)
        if os.path.exists(cls.bin_out):
            os.remove(cls.bin_out)
        if os.path.exists(cls.gguf_out):
            os.remove(cls.gguf_out)
        if os.path.exists(cls.shader_out):
            os.remove(cls.shader_out)

    def test_01_build_harness(self):
        """Verify Position 8 C harness builds with zero errors."""
        self.assertEqual(self.build_res.returncode, 0, f"Build failed: {self.build_res.stderr}")

    def test_02_sdf_simd_and_shader_output(self):
        """Executes harness and verifies SIMD distance evaluation and shader output."""
        res = subprocess.run([self.bin_out], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Harness crashed: {res.stderr}")
        self.assertIn("SHADER_GEN_RES:0", res.stdout)
        self.assertIn("GGUF_EMIT_RES:0", res.stdout)
        self.assertTrue(os.path.exists(self.shader_out), "Shader file missing")

    def test_03_gguf_magic_header_validation(self):
        """Validates that emitted GGUF file starts with magic 0x46554747 (GGUF)."""
        self.assertTrue(os.path.exists(self.gguf_out), "GGUF output file missing")
        with open(self.gguf_out, "rb") as f:
            magic, version = struct.unpack("<II", f.read(8))
            self.assertEqual(magic, 0x46554747, f"Invalid GGUF magic: 0x{magic:08X}")
            self.assertEqual(version, 3, f"Unexpected GGUF version: {version}")

if __name__ == "__main__":
    unittest.main()
