"""
ZCC WebAssembly MVP Emitter Unit Test Suite
Tests WASM binary header magic, section format, LEB128 encoding, and module file output.
"""

import os
import struct
import subprocess
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WASM_HEADER_MAGIC = b"\x00asm\x01\x00\x00\x00"

def compile_c_test_harness():
    """Builds a standalone C test harness that exercises src/wasm_emit.c functions."""
    harness_c = os.path.join(REPO_ROOT, "tests", "temp_wasm_harness.c")
    bin_out = os.path.join(REPO_ROOT, "tests", "temp_wasm_harness")
    
    code = """
#include <stdio.h>
#include "../src/wasm_emit.h"

int main() {
    uint8_t buf[16];
    size_t len = wasm_encode_uleb128(buf, 624485);
    printf("ULEB128_LEN:%zu\\n", len);
    for(size_t i = 0; i < len; i++) {
        printf("BYTE_%zu:0x%02X\\n", i, buf[i]);
    }
    
    WASMBuffer body;
    wasm_buf_init(&body);
    wasm_buf_append_u8(&body, WASM_OP_LOCAL_GET);
    wasm_buf_append_uleb128(&body, 0);
    wasm_buf_append_u8(&body, WASM_OP_LOCAL_GET);
    wasm_buf_append_uleb128(&body, 1);
    wasm_buf_append_u8(&body, WASM_OP_I32_MUL);
    wasm_buf_append_u8(&body, WASM_OP_END);
    
    int res = zcc_emit_wasm_module_to_file("/tmp/test_out.wasm", &body);
    wasm_buf_free(&body);
    printf("WASM_EMIT_RES:%d\\n", res);
    return 0;
}
"""
    with open(harness_c, "w") as f:
        f.write(code)
        
    cmd = ["gcc", "-Isrc", harness_c, os.path.join(REPO_ROOT, "src", "wasm_emit.c"), "-o", bin_out]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return bin_out, res

class TestWASMEmitter(unittest.TestCase):
    
    @classmethod
    def setUpClass(cls):
        cls.bin_out, cls.build_res = compile_c_test_harness()

    @classmethod
    def tearDownClass(cls):
        harness_c = os.path.join(REPO_ROOT, "tests", "temp_wasm_harness.c")
        if os.path.exists(harness_c):
            os.remove(harness_c)
        if os.path.exists(cls.bin_out):
            os.remove(cls.bin_out)
        if os.path.exists("/tmp/test_out.wasm"):
            os.remove("/tmp/test_out.wasm")

    def test_01_build_harness(self):
        """Verify C harness builds with zero errors."""
        self.assertEqual(self.build_res.returncode, 0, f"Build failed: {self.build_res.stderr}")

    def test_02_leb128_and_wasm_file_emission(self):
        """Executes test harness and checks LEB128 encoding + WASM module magic bytes."""
        res = subprocess.run([self.bin_out], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Harness crashed: {res.stderr}")
        self.assertIn("ULEB128_LEN:3", res.stdout)
        self.assertIn("BYTE_0:0xE5", res.stdout)
        self.assertIn("BYTE_1:0x8E", res.stdout)
        self.assertIn("BYTE_2:0x26", res.stdout)
        self.assertIn("WASM_EMIT_RES:0", res.stdout)

        # Inspect emitted WASM file
        self.assertTrue(os.path.exists("/tmp/test_out.wasm"))
        with open("/tmp/test_out.wasm", "rb") as f:
            data = f.read()

        self.assertTrue(data.startswith(WASM_HEADER_MAGIC), f"Invalid WASM header: {data[:8]}")
        self.assertGreater(len(data), 30, f"WASM file size too small: {len(data)} bytes")

if __name__ == "__main__":
    unittest.main()
