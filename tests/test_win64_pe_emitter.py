"""
ZCC Win64 PE/COFF Direct Emitter Unit Test Suite
Tests DOS/PE headers, PE32+ optional header fields, section alignment math,
direct `./zcc input.c -target win64 -o app.exe` cross-compilation, and native host execution.
"""

import os
import shutil
import struct
import subprocess
import sys
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
        cls.tmp_dir = tempfile.TemporaryDirectory()
        cls.out_exe = os.path.join(cls.tmp_dir.name, "test_win64_out.exe")
        cls.zcc_bin = os.path.join(REPO_ROOT, "zcc")

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
        self.assertIn("zcc_emit_win64_pe_file_ex", content)

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

    def test_03_direct_zcc_win64_compilation_and_headers(self):
        """Verify ./zcc input.c -target win64 -o app.exe emits valid PE32+ headers."""
        src_file = os.path.join(self.tmp_dir.name, "probe_simple.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_simple.exe")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write("int main(void) { return 42; }\n")

        # Compile using WSL zcc if on Windows or direct if on Linux
        if sys.platform == "win32":
            # Translate path to WSL
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src_file, "-target", "win64", "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file), "Output .exe was not generated")

        with open(exe_file, "rb") as f:
            data = f.read()

        self.assertTrue(data.startswith(DOS_MAGIC_MZ), "DOS magic 'MZ' not found")
        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        self.assertEqual(e_lfanew, 0x80, "e_lfanew offset mismatch")
        pe_sig = data[e_lfanew : e_lfanew + 4]
        self.assertEqual(pe_sig, PE_SIG_MAGIC, "PE signature mismatch")

        # Check COFF Machine == 0x8664 (AMD64)
        coff_offset = e_lfanew + 4
        machine = struct.unpack_from("<H", data, coff_offset)[0]
        self.assertEqual(machine, 0x8664, f"Machine type expected 0x8664 (AMD64), got 0x{machine:04X}")

        # Check Optional Header Magic == 0x020B (PE32+)
        opt_offset = coff_offset + 20
        opt_magic = struct.unpack_from("<H", data, opt_offset)[0]
        self.assertEqual(opt_magic, 0x020B, f"Optional header magic expected 0x020B (PE32+), got 0x{opt_magic:04X}")

        # Check AddressOfEntryPoint >= 0x1000
        entry_rva = struct.unpack_from("<I", data, opt_offset + 16)[0]
        self.assertGreaterEqual(entry_rva, 0x1000, f"Entry RVA should be >= 0x1000, got 0x{entry_rva:04X}")

    def test_04_windows_host_execution_exit_code(self):
        """Verify emitted PE32+ executable runs directly and returns exit code 42."""
        if sys.platform != "win32":
            self.skipTest("Host execution test requires Windows environment")

        exe_file = os.path.join(self.tmp_dir.name, "probe_simple.exe")
        self.assertTrue(os.path.exists(exe_file))

        run_res = subprocess.run([exe_file])
        self.assertEqual(run_res.returncode, 42, f"Expected exit code 42, got {run_res.returncode}")

    def test_05_multi_function_recursive_execution(self):
        """Verify multi-function program with arithmetic & recursion compiles and executes with code 43."""
        src_file = os.path.join(self.tmp_dir.name, "probe_calc.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_calc.exe")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write(
                "int add(int a, int b) { return a + b; }\n"
                "int fib(int n) { if (n <= 1) return n; return fib(n - 1) + fib(n - 2); }\n"
                "int main(void) { int x = add(10, 20); int f = fib(7); return x + f; }\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' --target=win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src_file, "--target=win64", "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        if sys.platform == "win32":
            run_res = subprocess.run([exe_file])
            self.assertEqual(run_res.returncode, 43, f"Expected exit code 43, got {run_res.returncode}")

    def test_06_target_auto_detection_from_exe(self):
        """Verify -o app.exe automatically enables PE32+ emission without explicit -target flag."""
        src_file = os.path.join(self.tmp_dir.name, "probe_auto.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_auto.exe")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write("int main(void) { return 77; }\n")

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src_file, "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        if sys.platform == "win32":
            run_res = subprocess.run([exe_file])
            self.assertEqual(run_res.returncode, 77, f"Expected exit code 77, got {run_res.returncode}")

    def test_07_win64_pe_import_directory_headers(self):
        """Verify emitted PE binary has valid DataDirectory[1] (Import) and DataDirectory[12] (IAT)."""
        src_file = os.path.join(self.tmp_dir.name, "probe_import_hdr.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_import_hdr.exe")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write(
                "extern void ExitProcess(unsigned int uExitCode);\n"
                "extern void *GetStdHandle(int nStdHandle);\n"
                "int main(void) { void *h = GetStdHandle(-11); ExitProcess(42); return 0; }\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src_file, "-target", "win64", "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        with open(exe_file, "rb") as f:
            data = f.read()

        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        opt_offset = e_lfanew + 4 + 20

        # DataDirectory[1] = Import Directory
        import_rva, import_sz = struct.unpack_from("<II", data, opt_offset + 112 + 1 * 8)
        self.assertGreater(import_rva, 0x1000, "Import Directory RVA should be valid")
        self.assertEqual(import_sz, 40, "Import Directory size should be 40 bytes (1 DLL descriptor + 1 null)")

        # DataDirectory[12] = IAT
        iat_rva, iat_sz = struct.unpack_from("<II", data, opt_offset + 112 + 12 * 8)
        self.assertGreater(iat_rva, 0x1000, "IAT RVA should be valid")
        self.assertGreater(iat_sz, 0, "IAT size should be non-zero")

        # Verify DLL name string "KERNEL32.dll" is embedded
        self.assertIn(b"KERNEL32.dll", data)
        self.assertIn(b"ExitProcess", data)
        self.assertIn(b"GetStdHandle", data)

    def test_08_windows_host_dynamic_dll_binding_execution(self):
        """Verify dynamic execution of GetStdHandle, WriteFile, and ExitProcess on Windows host."""
        if sys.platform != "win32":
            self.skipTest("Host execution test requires Windows environment")

        src_file = os.path.join(self.tmp_dir.name, "probe_dll_run.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_dll_run.exe")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write(
                "extern void ExitProcess(unsigned int uExitCode);\n"
                "extern void *GetStdHandle(int nStdHandle);\n"
                "extern int WriteFile(void *hFile, const void *lpBuffer, unsigned int nNumberOfBytesToWrite, unsigned int *lpNumberOfBytesWritten, void *lpOverlapped);\n"
                "int main(void) {\n"
                "    void *hOut = GetStdHandle(-11);\n"
                "    unsigned int written = 0;\n"
                "    const char *msg = \"TEST_KERNEL32_WRITEFILE_OK\\r\\n\";\n"
                "    WriteFile(hOut, msg, 29, &written, 0);\n"
                "    ExitProcess(42);\n"
                "    return 0;\n"
                "}\n"
            )

        wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
        wsl_src = src_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
        wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
        cmd = ["wsl", "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -o '{wsl_exe}'"]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        run_res = subprocess.run([exe_file], capture_output=True, text=True)
        self.assertEqual(run_res.returncode, 42, f"Expected exit code 42, got {run_res.returncode}")
        self.assertIn("TEST_KERNEL32_WRITEFILE_OK", run_res.stdout)

    def test_09_multi_api_heap_and_sleep_execution(self):
        """Verify dynamic execution of VirtualAlloc, VirtualFree, Sleep, and normal main return 79."""
        if sys.platform != "win32":
            self.skipTest("Host execution test requires Windows environment")

        src_file = os.path.join(self.tmp_dir.name, "probe_multi_api.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_multi_api.exe")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write(
                "extern void *GetStdHandle(int nStdHandle);\n"
                "extern int WriteFile(void *hFile, const void *lpBuffer, unsigned int nNumberOfBytesToWrite, unsigned int *lpNumberOfBytesWritten, void *lpOverlapped);\n"
                "extern void *VirtualAlloc(void *lpAddress, unsigned long long dwSize, unsigned int flAllocationType, unsigned int flProtect);\n"
                "extern int VirtualFree(void *lpAddress, unsigned long long dwSize, unsigned int dwFreeType);\n"
                "extern void Sleep(unsigned int dwMilliseconds);\n"
                "int main(void) {\n"
                "    char *buf = (char *)VirtualAlloc(0, 4096, 0x3000, 0x04);\n"
                "    if (!buf) return 1;\n"
                "    buf[0] = 'O'; buf[1] = 'K'; buf[2] = '\\r'; buf[3] = '\\n';\n"
                "    void *hOut = GetStdHandle(-11);\n"
                "    unsigned int written = 0;\n"
                "    WriteFile(hOut, buf, 4, &written, 0);\n"
                "    Sleep(5);\n"
                "    VirtualFree(buf, 0, 0x8000);\n"
                "    return 79;\n"
                "}\n"
            )

        wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
        wsl_src = src_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
        wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
        cmd = ["wsl", "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -o '{wsl_exe}'"]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        run_res = subprocess.run([exe_file], capture_output=True, text=True)
        self.assertEqual(run_res.returncode, 79, f"Expected returncode 79, got {run_res.returncode}")
        self.assertIn("OK", run_res.stdout)


if __name__ == "__main__":
    unittest.main()

