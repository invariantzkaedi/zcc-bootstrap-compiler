"""
ZCC Win64 PE/COFF Direct Emitter Unit Test Suite
Tests DOS/PE headers, PE32+ optional header fields, section alignment math, and binary .exe output formatting.
"""

import os
import struct
import subprocess
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOS_MAGIC_MZ = b"MZ"
PE_SIG_MAGIC = b"PE\x00\x00"

def compile_c_pe_harness():
    """Builds a standalone C test harness that exercises src/win64_pe_emit.c functions."""
    harness_c = os.path.join(REPO_ROOT, "tests", "temp_pe_harness.c")
    bin_out = os.path.join(REPO_ROOT, "tests", "temp_pe_harness")
    
    code = """
#include <stdio.h>
#include "../src/win64_pe_emit.h"

int main() {
    uint32_t a1 = win64_pe_align_to(500, 512);
    uint32_t a2 = win64_pe_align_to(4096, 4096);
    uint32_t a3 = win64_pe_align_to(4097, 4096);
    printf("ALIGN_500_512:%u\\n", a1);
    printf("ALIGN_4096_4096:%u\\n", a2);
    printf("ALIGN_4097_4096:%u\\n", a3);

    uint8_t code_payload[] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3 }; /* mov eax, 42; ret */
    int res = zcc_emit_win64_pe_file("/tmp/test_win64_out.exe", code_payload, sizeof(code_payload));
    printf("PE_EMIT_RES:%d\\n", res);
    return 0;
}
"""
    with open(harness_c, "w") as f:
        f.write(code)
        
    cmd = ["gcc", "-Isrc", harness_c, os.path.join(REPO_ROOT, "src", "win64_pe_emit.c"), "-o", bin_out]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return bin_out, res

class TestWin64PEEmitter(unittest.TestCase):
    
    @classmethod
    def setUpClass(cls):
        cls.bin_out, cls.build_res = compile_c_pe_harness()

    @classmethod
    def tearDownClass(cls):
        harness_c = os.path.join(REPO_ROOT, "tests", "temp_pe_harness.c")
        if os.path.exists(harness_c):
            os.remove(harness_c)
        if os.path.exists(cls.bin_out):
            os.remove(cls.bin_out)
        if os.path.exists("/tmp/test_win64_out.exe"):
            os.remove("/tmp/test_win64_out.exe")

    def test_01_build_harness(self):
        """Verify C harness builds with zero errors."""
        self.assertEqual(self.build_res.returncode, 0, f"Build failed: {self.build_res.stderr}")

    def test_02_pe_alignment_and_file_emission(self):
        """Executes test harness and checks PE alignment math, DOS magic, and PE signature."""
        res = subprocess.run([self.bin_out], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Harness crashed: {res.stderr}")
        self.assertIn("ALIGN_500_512:512", res.stdout)
        self.assertIn("ALIGN_4096_4096:4096", res.stdout)
        self.assertIn("ALIGN_4097_4096:8192", res.stdout)
        self.assertIn("PE_EMIT_RES:0", res.stdout)

        # Inspect emitted Win64 PE binary file
        self.assertTrue(os.path.exists("/tmp/test_win64_out.exe"))
        with open("/tmp/test_win64_out.exe", "rb") as f:
            data = f.read()

        # DOS Header check: 'MZ'
        self.assertTrue(data.startswith(DOS_MAGIC_MZ), f"Invalid DOS header: {data[:2]}")
        
        # Read e_lfanew offset (offset 0x3C)
        e_lfanew = struct.unpack("<I", data[0x3C:0x40])[0]
        self.assertEqual(e_lfanew, 0x80, f"Unexpected e_lfanew offset: 0x{e_lfanew:X}")

        # PE Signature check: 'PE\0\0' at e_lfanew
        pe_sig = data[e_lfanew:e_lfanew + 4]
        self.assertEqual(pe_sig, PE_SIG_MAGIC, f"Invalid PE signature: {pe_sig}")

        # COFF File Header Machine check: 0x8664 (AMD64)
        coff_machine = struct.unpack("<H", data[e_lfanew + 4:e_lfanew + 6])[0]
        self.assertEqual(coff_machine, 0x8664, f"Invalid COFF machine: 0x{coff_machine:X}")

        # PE32+ Optional Header Magic check: 0x020B (PE32+)
        opt_magic = struct.unpack("<H", data[e_lfanew + 24:e_lfanew + 26])[0]
        self.assertEqual(opt_magic, 0x020B, f"Invalid PE32+ optional header magic: 0x{opt_magic:X}")

if __name__ == "__main__":
    unittest.main()
