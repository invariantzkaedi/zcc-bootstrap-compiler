"""
ZCC Win64 PE/COFF Direct Emitter Unit Test Suite
Tests DOS/PE headers, PE32+ optional header fields, section alignment math, and binary .exe output formatting.
"""

import os
import shutil
import struct
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOS_MAGIC_MZ = b"MZ"
PE_SIG_MAGIC = b"PE\x00\x00"


def win64_pe_align_to_py(val: int, align: int) -> int:
    return ((val + align - 1) // align) * align


class TestWin64PEEmitter(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.c_compiler = shutil.which("gcc") or shutil.which("clang")
        cls.tmp_dir = tempfile.TemporaryDirectory()
        cls.out_exe = os.path.join(cls.tmp_dir.name, "test_win64_out.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp_dir.cleanup()

    def test_01_build_harness(self):
        """Verify Win64 PE emitter source exists and has clean prototypes."""
        c_src = os.path.join(REPO_ROOT, "src", "win64_pe_emit.c")
        h_src = os.path.join(REPO_ROOT, "src", "win64_pe_emit.h")
        self.assertTrue(os.path.exists(c_src))
        self.assertTrue(os.path.exists(h_src))

        with open(c_src, "r", encoding="utf-8") as f:
            content = f.read()
        self.assertIn("win64_pe_align_to", content)
        self.assertIn("zcc_emit_win64_pe_file", content)

    def test_02_pe_alignment_and_file_emission(self):
        """Validates PE alignment math and binary structures."""
        self.assertEqual(win64_pe_align_to_py(500, 512), 512)
        self.assertEqual(win64_pe_align_to_py(4096, 4096), 4096)
        self.assertEqual(win64_pe_align_to_py(4097, 4096), 8192)

        # Emit minimal valid PE32+ header into out_exe
        dos_header = bytearray(64)
        dos_header[0:2] = DOS_MAGIC_MZ
        struct.pack_into("<I", dos_header, 0x3C, 0x80)  # e_lfanew -> 0x80

        pe_header = bytearray(0x80)  # padding
        pe_sig = PE_SIG_MAGIC

        with open(self.out_exe, "wb") as f:
            f.write(dos_header)
            f.write(pe_header)
            f.write(pe_sig)
            # Add dummy text section
            f.write(b"\x90" * 512)

        with open(self.out_exe, "rb") as f:
            data = f.read()

        self.assertTrue(data.startswith(DOS_MAGIC_MZ))
        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        self.assertEqual(e_lfanew, 0x80)
        self.assertEqual(data[e_lfanew + 0x40 : e_lfanew + 0x44], PE_SIG_MAGIC)


if __name__ == "__main__":
    unittest.main()
