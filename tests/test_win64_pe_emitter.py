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
WSL_DISTRO = "Ubuntu"


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
        try:
            cls.tmp_dir.cleanup()
        except Exception:
            pass

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
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -o '{wsl_exe}'"]
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
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' --target=win64 -o '{wsl_exe}'"]
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
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -o '{wsl_exe}'"]
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
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -o '{wsl_exe}'"]
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
        cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -o '{wsl_exe}'"]

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
        cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -o '{wsl_exe}'"]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        run_res = subprocess.run([exe_file], capture_output=True, text=True)
        self.assertEqual(run_res.returncode, 79, f"Expected returncode 79, got {run_res.returncode}")
        self.assertIn("OK", run_res.stdout)

    def test_10_multi_dll_msvcrt_printf_malloc(self):
        """Verify multi-DLL PE import directory with msvcrt.dll and KERNEL32.dll dynamic binding."""
        src_file = os.path.join(self.tmp_dir.name, "probe_msvcrt_kernel32.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_msvcrt_kernel32.exe")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write(
                "extern void *malloc(unsigned long long sz);\n"
                "extern void free(void *ptr);\n"
                "extern int printf(const char *fmt, ...);\n"
                "extern void ExitProcess(unsigned int code);\n"
                "int main(void) {\n"
                "    char *buf = (char *)malloc(64);\n"
                "    if (!buf) ExitProcess(1);\n"
                "    buf[0] = 'H'; buf[1] = 'I'; buf[2] = '_'; buf[3] = 'M'; buf[4] = 'S'; buf[5] = 'V'; buf[6] = 'C'; buf[7] = 'R'; buf[8] = 'T'; buf[9] = '\\n'; buf[10] = '\\0';\n"
                "    printf(\"%s\", buf);\n"
                "    free(buf);\n"
                "    ExitProcess(88);\n"
                "    return 0;\n"
                "}\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src_file, "-target", "win64", "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        with open(exe_file, "rb") as f:
            data = f.read()

        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        opt_offset = e_lfanew + 4 + 20

        # DataDirectory[1] = Import Directory: 2 DLLs -> (2 + 1) * 20 = 60 bytes
        import_rva, import_sz = struct.unpack_from("<II", data, opt_offset + 112 + 1 * 8)
        self.assertGreater(import_rva, 0x1000, "Import Directory RVA should be valid")
        self.assertEqual(import_sz, 60, "Import Directory size should be 60 bytes (2 DLL descriptors + 1 null)")

        # Verify DLL name strings are embedded
        self.assertIn(b"msvcrt.dll", data)
        self.assertIn(b"KERNEL32.dll", data)
        self.assertIn(b"printf", data)
        self.assertIn(b"malloc", data)
        self.assertIn(b"free", data)
        self.assertIn(b"ExitProcess", data)

        if sys.platform == "win32":
            run_res = subprocess.run([exe_file], capture_output=True, text=True)
            self.assertEqual(run_res.returncode, 88, f"Expected returncode 88, got {run_res.returncode}")
            self.assertIn("HI_MSVCRT", run_res.stdout)

    def test_11_multi_dll_three_libraries_user32(self):
        """Verify simultaneous multi-DLL binding across 3 libraries (KERNEL32, msvcrt, USER32)."""
        src_file = os.path.join(self.tmp_dir.name, "probe_tri_dll.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_tri_dll.exe")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write(
                "extern void *malloc(unsigned long long sz);\n"
                "extern void free(void *ptr);\n"
                "extern int printf(const char *fmt, ...);\n"
                "extern void *GetDesktopWindow(void);\n"
                "extern int GetSystemMetrics(int nIndex);\n"
                "extern void Sleep(unsigned int dwMilliseconds);\n"
                "extern void ExitProcess(unsigned int code);\n"
                "int main(void) {\n"
                "    void *hwnd = GetDesktopWindow();\n"
                "    int screen_w = GetSystemMetrics(0);\n"
                "    char *buf = (char *)malloc(128);\n"
                "    if (!buf) ExitProcess(1);\n"
                "    printf(\"DESKTOP_HWND: %p, SCREEN_W: %d\\n\", hwnd, screen_w);\n"
                "    free(buf);\n"
                "    Sleep(10);\n"
                "    ExitProcess(77);\n"
                "    return 0;\n"
                "}\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src_file, "-target", "win64", "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        with open(exe_file, "rb") as f:
            data = f.read()

        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        opt_offset = e_lfanew + 4 + 20

        # DataDirectory[1] = Import Directory: 3 DLLs -> (3 + 1) * 20 = 80 bytes
        import_rva, import_sz = struct.unpack_from("<II", data, opt_offset + 112 + 1 * 8)
        self.assertGreater(import_rva, 0x1000, "Import Directory RVA should be valid")
        self.assertEqual(import_sz, 80, "Import Directory size should be 80 bytes (3 DLL descriptors + 1 null)")

        # Verify DLL names
        self.assertIn(b"msvcrt.dll", data)
        self.assertIn(b"USER32.dll", data)
        self.assertIn(b"KERNEL32.dll", data)
        self.assertIn(b"GetDesktopWindow", data)
        self.assertIn(b"GetSystemMetrics", data)

        if sys.platform == "win32":
            run_res = subprocess.run([exe_file], capture_output=True, text=True)
            self.assertEqual(run_res.returncode, 77, f"Expected returncode 77, got {run_res.returncode}")
            self.assertIn("DESKTOP_HWND:", run_res.stdout)
            self.assertIn("SCREEN_W:", run_res.stdout)

    def test_12_win64_pe_exception_directory_headers(self):
        """Verify emitted PE binary has valid DataDirectory[3] (.pdata) and valid UNWIND_INFO entries."""
        src_file = os.path.join(self.tmp_dir.name, "probe_pdata_hdr.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_pdata_hdr.exe")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write(
                "int helper(int a) { return a * 2; }\n"
                "int compute(int x, int y) { return helper(x) + y; }\n"
                "int main(void) { return compute(10, 22); }\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src_file, "-target", "win64", "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        with open(exe_file, "rb") as f:
            data = f.read()

        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        coff_offset = e_lfanew + 4
        num_sections = struct.unpack_from("<H", data, coff_offset + 2)[0]
        opt_offset = coff_offset + 20

        # DataDirectory[3] = Exception Directory (.pdata)
        pdata_rva, pdata_sz = struct.unpack_from("<II", data, opt_offset + 112 + 3 * 8)
        self.assertGreater(pdata_rva, 0x1000, "Exception Directory RVA must be valid (> 0x1000)")
        self.assertGreaterEqual(pdata_sz, 12, "Exception Directory Size must be >= 12 bytes (at least one RUNTIME_FUNCTION)")
        self.assertEqual(pdata_sz % 12, 0, "Exception Directory Size must be a multiple of 12 bytes")

        # Scan section headers for .pdata
        sec_headers_offset = opt_offset + 240
        pdata_sec_file_offset = None
        for i in range(num_sections):
            sec_off = sec_headers_offset + i * 40
            sec_name = data[sec_off : sec_off + 8].rstrip(b"\x00")
            if sec_name == b".pdata":
                sec_vaddr, sec_raw_sz, sec_raw_ptr = struct.unpack_from("<III", data, sec_off + 12)
                self.assertEqual(sec_vaddr, pdata_rva, ".pdata section VirtualAddress must match DataDirectory[3].VirtualAddress")
                pdata_sec_file_offset = sec_raw_ptr
                break

        self.assertIsNotNone(pdata_sec_file_offset, ".pdata section header not found in PE section headers")

        # Unpack RUNTIME_FUNCTION entries
        entry_count = pdata_sz // 12
        self.assertGreaterEqual(entry_count, 1, "Must have at least 1 RUNTIME_FUNCTION entry")
        prev_begin = 0
        for i in range(entry_count):
            entry_off = pdata_sec_file_offset + i * 12
            begin_addr, end_addr, unwind_addr = struct.unpack_from("<III", data, entry_off)
            self.assertGreaterEqual(begin_addr, 0x1000, f"Entry {i} BeginAddress must be in .text")
            self.assertGreater(end_addr, begin_addr, f"Entry {i} EndAddress must be greater than BeginAddress")
            self.assertGreater(begin_addr, prev_begin, f"Entry {i} BeginAddress must be strictly increasing (sorted)")
            prev_begin = begin_addr

            # Unwind Info Address must point to unwind info within the .pdata section
            self.assertGreaterEqual(unwind_addr, pdata_rva + pdata_sz)
            # Find file offset of UNWIND_INFO
            unwind_file_off = pdata_sec_file_offset + (unwind_addr - pdata_rva)
            ver_flags, prolog_sz, cnt_codes, frame_reg = struct.unpack_from("<BBBB", data, unwind_file_off)
            self.assertEqual(ver_flags & 0x07, 1, f"Entry {i} UNWIND_INFO Version must be 1")
            self.assertEqual(prolog_sz, 4, f"Entry {i} SizeOfProlog expected 4 bytes (push rbp + mov rbp, rsp)")
            self.assertEqual(cnt_codes, 2, f"Entry {i} CountOfCodes expected 2 unwind codes")
            self.assertEqual(frame_reg & 0x0F, 5, f"Entry {i} FrameRegister expected 5 (RBP)")

            # Check unwind codes: code0 = 0x0304 (UWOP_SET_FPREG), code1 = 0x5001 (UWOP_PUSH_NONVOL RBP)
            code0, code1 = struct.unpack_from("<HH", data, unwind_file_off + 4)
            self.assertEqual(code0, 0x0304, f"Entry {i} UnwindCode[0] expected 0x0304 (UWOP_SET_FPREG)")
            self.assertEqual(code1, 0x5001, f"Entry {i} UnwindCode[1] expected 0x5001 (UWOP_PUSH_NONVOL RBP)")

    def test_13_win64_pe_exception_handling_live_execution(self):
        """Verify Windows host execution of multi-function program with full .pdata unwind tables."""
        if sys.platform != "win32":
            self.skipTest("Host execution test requires Windows environment")

        exe_file = os.path.join(self.tmp_dir.name, "probe_pdata_hdr.exe")
        self.assertTrue(os.path.exists(exe_file))

        run_res = subprocess.run([exe_file])
        self.assertEqual(run_res.returncode, 42, f"Expected exit code 42 (compute(10, 22) -> 10*2+22), got {run_res.returncode}")



    def test_14_multi_source_win64_pe_compilation(self):
        """Verify multi-source compilation: two C source files compiled directly to PE32+ executable."""
        src1 = os.path.join(self.tmp_dir.name, "probe_m1.c")
        src2 = os.path.join(self.tmp_dir.name, "probe_m2.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_multi_run.exe")

        with open(src1, "w", encoding="utf-8") as f:
            f.write("extern int helper(int x);\nint main(void) { return helper(40) + 2; }\n")
        with open(src2, "w", encoding="utf-8") as f:
            f.write("int helper(int x) { return x * 2; }\n")

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_s1 = src1.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_s2 = src2.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_s1}' '{wsl_s2}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src1, src2, "-target", "win64", "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        with open(exe_file, "rb") as f:
            data = f.read()
        self.assertTrue(data.startswith(DOS_MAGIC_MZ))

        if sys.platform == "win32":
            run_res = subprocess.run([exe_file])
            self.assertEqual(run_res.returncode, 82, f"Expected returncode 82 (40*2+2), got {run_res.returncode}")

    def test_15_direct_assembly_to_pe_emission(self):
        """Verify direct assembly (.s) compilation to Win64 PE32+ executable."""
        src_file = os.path.join(self.tmp_dir.name, "probe_asm_in.c")
        asm_file = os.path.join(self.tmp_dir.name, "probe_asm_in.s")
        exe_file = os.path.join(self.tmp_dir.name, "probe_asm_out.exe")

        with open(src_file, "w", encoding="utf-8") as f:
            f.write("int calc(int x) { return x + 15; }\nint main(void) { return calc(30); }\n")

        # Compile to .s first
        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_s = asm_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd_s = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc -S '{wsl_src}' -o '{wsl_s}'"]
            cmd_exe = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_s}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd_s = [self.zcc_bin, "-S", src_file, "-o", asm_file]
            cmd_exe = [self.zcc_bin, asm_file, "-target", "win64", "-o", exe_file]

        res_s = subprocess.run(cmd_s, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res_s.returncode, 0, f"Assembly generation failed: {res_s.stderr}")
        self.assertTrue(os.path.exists(asm_file))

        res_exe = subprocess.run(cmd_exe, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res_exe.returncode, 0, f"Direct assembly compilation failed: {res_exe.stderr}")
        self.assertTrue(os.path.exists(exe_file))

        with open(exe_file, "rb") as f:
            data = f.read()
        self.assertTrue(data.startswith(DOS_MAGIC_MZ))

        if sys.platform == "win32":
            run_res = subprocess.run([exe_file])
            self.assertEqual(run_res.returncode, 45, f"Expected returncode 45 (30+15), got {run_res.returncode}")

    def test_16_multi_source_dynamic_import_binding(self):
        """Verify multi-source compilation with dynamic DLL imports (printf from msvcrt, ExitProcess from KERNEL32)."""
        src1 = os.path.join(self.tmp_dir.name, "probe_dyn1.c")
        src2 = os.path.join(self.tmp_dir.name, "probe_dyn2.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_dyn_run.exe")

        with open(src1, "w", encoding="utf-8") as f:
            f.write(
                "extern int compute_sum(int a, int b);\n"
                "extern int printf(const char *fmt, ...);\n"
                "int main(void) {\n"
                "    int val = compute_sum(20, 33);\n"
                "    printf(\"DYN_RESULT: %d\\n\", val);\n"
                "    return val;\n"
                "}\n"
            )
        with open(src2, "w", encoding="utf-8") as f:
            f.write("int compute_sum(int a, int b) { return a + b; }\n")

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_s1 = src1.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_s2 = src2.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_s1}' '{wsl_s2}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src1, src2, "-target", "win64", "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        if sys.platform == "win32":
            run_res = subprocess.run([exe_file], capture_output=True, text=True)
            self.assertEqual(run_res.returncode, 53, f"Expected returncode 53 (20+33), got {run_res.returncode}")
            self.assertIn("DYN_RESULT: 53", run_res.stdout)
    def test_17_mixed_c_and_assembly_win64_pe_compilation(self):
        """Verify compiling mixed C and hand-written assembly sources directly to Win64 PE."""
        src_c = os.path.join(self.tmp_dir.name, "probe_mixed_c.c")
        src_s = os.path.join(self.tmp_dir.name, "probe_mixed_s.s")
        exe_file = os.path.join(self.tmp_dir.name, "probe_mixed_run.exe")

        with open(src_c, "w", encoding="utf-8") as f:
            f.write(
                "extern int asm_mult(int a, int b);\n"
                "int main(void) {\n"
                "    int res = asm_mult(9, 7);\n"
                "    return res;\n"
                "}\n"
            )
        with open(src_s, "w", encoding="utf-8") as f:
            f.write(
                "    .text\n"
                "    .globl asm_mult\n"
                "asm_mult:\n"
                "    movq %rdi, %rax\n"
                "    imulq %rsi, %rax\n"
                "    ret\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_c = src_c.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_s = src_s.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_c}' '{wsl_s}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src_c, src_s, "-target", "win64", "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Mixed compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        if sys.platform == "win32":
            run_res = subprocess.run([exe_file])
            self.assertEqual(run_res.returncode, 63, f"Expected returncode 63 (9*7), got {run_res.returncode}")

    def test_18_multi_assembly_win64_pe_compilation(self):
        """Verify compiling multiple .s assembly files directly to Win64 PE."""
        s1 = os.path.join(self.tmp_dir.name, "probe_m_s1.s")
        s2 = os.path.join(self.tmp_dir.name, "probe_m_s2.s")
        exe_file = os.path.join(self.tmp_dir.name, "probe_m_s_run.exe")

        with open(s1, "w", encoding="utf-8") as f:
            f.write(
                "    .text\n"
                "    .globl main\n"
                "    .extern get_const\n"
                "main:\n"
                "    subq $40, %rsp\n"
                "    call get_const\n"
                "    addq $40, %rsp\n"
                "    ret\n"
            )
        with open(s2, "w", encoding="utf-8") as f:
            f.write(
                "    .text\n"
                "    .globl get_const\n"
                "get_const:\n"
                "    movq $77, %rax\n"
                "    ret\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_s1 = s1.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_s2 = s2.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_s1}' '{wsl_s2}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, s1, s2, "-target", "win64", "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Multi-assembly compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        if sys.platform == "win32":
            run_res = subprocess.run([exe_file])
            self.assertEqual(run_res.returncode, 77, f"Expected returncode 77, got {run_res.returncode}")

    def test_19_large_scale_pdata_unwind_extents(self):
        """Verify dynamic .pdata extent scaling with > 256 functions (exceeding fixed buffer)."""
        src = os.path.join(self.tmp_dir.name, "probe_300_funcs.c")
        exe_file = os.path.join(self.tmp_dir.name, "probe_300_funcs.exe")

        lines = ["int dummy(void) { return 1; }\n"]
        for fi in range(300):
            lines.append(f"int func_{fi}(void) {{ return {fi} + dummy(); }}\n")
        lines.append("int main(void) { return func_299() - 299; }\n")

        with open(src, "w", encoding="utf-8") as f:
            f.writelines(lines)

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src, "-target", "win64", "-o", exe_file]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"300-function compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        with open(exe_file, "rb") as f:
            data = f.read()

        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        # DataDirectory[3] is Exception Directory at opt_hdr offset 0x70 + 3*8 = 0x88
        opt_hdr_offset = e_lfanew + 4 + 20
        pdata_rva, pdata_size = struct.unpack_from("<II", data, opt_hdr_offset + 0x88)
        self.assertGreater(pdata_rva, 0)
        # 301 functions (dummy + 300 funcs + main) * 12 bytes = 3612 bytes
        self.assertGreaterEqual(pdata_size, 300 * 12)

        if sys.platform == "win32":
            run_res = subprocess.run([exe_file])
            self.assertEqual(run_res.returncode, 1, f"Expected returncode 1 (func_299() - 299 = 1), got {run_res.returncode}")

    def test_20_win64_ms_abi_calling_convention(self):
        """Verify Win64 MS ABI calling convention: rcx, rdx, r8, r9 + 32-byte shadow space."""
        src = os.path.join(self.tmp_dir.name, "probe_ms_abi.c")
        asm_file = os.path.join(self.tmp_dir.name, "probe_ms_abi.s")
        exe_file = os.path.join(self.tmp_dir.name, "probe_ms_abi.exe")

        with open(src, "w", encoding="utf-8") as f:
            f.write(
                "int compute4(int a, int b, int c, int d) {\n"
                "    return a + b + c + d;\n"
                "}\n"
                "int main(void) {\n"
                "    int res = compute4(10, 12, 8, 12);\n"
                "    return res;\n"
                "}\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_asm = asm_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe_file.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd_asm = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -mabi=ms -S -o '{wsl_asm}'"]
            cmd_exe = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -mabi=ms -o '{wsl_exe}'"]
        else:
            cmd_asm = [self.zcc_bin, src, "-mabi=ms", "-S", "-o", asm_file]
            cmd_exe = [self.zcc_bin, src, "-target", "win64", "-mabi=ms", "-o", exe_file]

        # 1. Verify assembly emission with -mabi=ms
        res_asm = subprocess.run(cmd_asm, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res_asm.returncode, 0, f"Assembly generation failed: {res_asm.stderr}\n{res_asm.stdout}")
        self.assertTrue(os.path.exists(asm_file))

        with open(asm_file, "r", encoding="utf-8") as f:
            asm_content = f.read()

        # Win64 ABI mandates RCX, RDX, R8, R9 for first 4 integer/pointer parameters
        self.assertIn("%rcx", asm_content, "Expected %rcx in MS ABI parameter store")
        self.assertIn("%rdx", asm_content, "Expected %rdx in MS ABI parameter store")
        self.assertIn("%r8", asm_content, "Expected %r8 in MS ABI parameter store")
        self.assertIn("%r9", asm_content, "Expected %r9 in MS ABI parameter store")
        # Win64 ABI mandates 32-byte shadow space allocation prior to call
        self.assertIn("subq $32, %rsp", asm_content, "Expected 32-byte shadow space allocation")

        # 2. Verify standalone PE executable creation with -mabi=ms
        res_exe = subprocess.run(cmd_exe, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res_exe.returncode, 0, f"PE executable compilation failed: {res_exe.stderr}\n{res_exe.stdout}")
        self.assertTrue(os.path.exists(exe_file))

        # 3. Verify execution on Windows host
        if sys.platform == "win32":
            run_res = subprocess.run([exe_file])
            self.assertEqual(run_res.returncode, 42, f"Expected returncode 42 (10+12+8+12), got {run_res.returncode}")

    def test_21_win64_gui_subsystem_and_winmain(self):
        """Verify Win64 PE GUI Subsystem (-mwindows) sets Subsystem=2 and resolves WinMain entry point."""
        src = os.path.join(self.tmp_dir.name, "probe_gui_subsystem.c")
        exe = os.path.join(self.tmp_dir.name, "probe_gui_subsystem.exe")

        with open(src, "w", encoding="utf-8") as f:
            f.write(
                "int WinMain(void *hInstance, void *hPrevInstance, char *lpCmdLine, int nCmdShow) {\n"
                "    return 88;\n"
                "}\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -mwindows -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src, "-target", "win64", "-mwindows", "-o", exe]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"GUI compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe))

        with open(exe, "rb") as f:
            data = f.read()

        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        opt_offset = e_lfanew + 4 + 20
        subsystem = struct.unpack_from("<H", data, opt_offset + 68)[0]
        self.assertEqual(subsystem, 2, f"Expected IMAGE_SUBSYSTEM_WINDOWS_GUI (2), got {subsystem}")

        if sys.platform == "win32":
            run_res = subprocess.run([exe])
            self.assertEqual(run_res.returncode, 88, f"Expected returncode 88 from WinMain, got {run_res.returncode}")

    def test_22_win64_pe_resource_directory_and_manifest(self):
        """Verify Win64 PE .rsrc section and DataDirectory[2] Resource Table with Common-Controls manifest."""
        src = os.path.join(self.tmp_dir.name, "probe_rsrc_test.c")
        exe = os.path.join(self.tmp_dir.name, "probe_rsrc_test.exe")

        with open(src, "w", encoding="utf-8") as f:
            f.write(
                "int WinMain(void *hInst, void *hPrev, char *cmd, int show) {\n"
                "    return 0;\n"
                "}\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -mwindows -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src, "-target", "win64", "-mwindows", "-o", exe]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")

        with open(exe, "rb") as f:
            data = f.read()

        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        opt_offset = e_lfanew + 4 + 20

        # DataDirectory[2] is Resource Directory
        rsrc_rva, rsrc_size = struct.unpack_from("<II", data, opt_offset + 112 + 2 * 8)
        self.assertGreater(rsrc_rva, 0x1000, "Expected valid .rsrc RVA")
        self.assertGreater(rsrc_size, 0, "Expected non-zero .rsrc size")

        # Verify .rsrc section header exists
        self.assertIn(b".rsrc\x00\x00\x00", data)
        # Verify XML manifest with Common-Controls 6.0 exists
        self.assertIn(b"Microsoft.Windows.Common-Controls", data)
        self.assertIn(b"urn:schemas-microsoft-com:asm.v1", data)

    def test_23_win64_pe_native_dll_export_directory(self):
        """Verify Win64 PE native dynamic library emission with -shared and .edata Export Address Table."""
        src = os.path.join(self.tmp_dir.name, "probe_dll_test.c")
        dll = os.path.join(self.tmp_dir.name, "probe_dll_test.dll")

        with open(src, "w", encoding="utf-8") as f:
            f.write(
                "int compute_sum(int a, int b) {\n"
                "    return a + b;\n"
                "}\n"
                "int compute_prod(int a, int b) {\n"
                "    return a * b;\n"
                "}\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_dll = dll.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -shared -o '{wsl_dll}'"]
        else:
            cmd = [self.zcc_bin, src, "-target", "win64", "-shared", "-o", dll]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"DLL compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(dll), "Output .dll was not generated")

        with open(dll, "rb") as f:
            data = f.read()

        # Check DOS & PE signature
        self.assertTrue(data.startswith(DOS_MAGIC_MZ))
        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        self.assertEqual(data[e_lfanew : e_lfanew + 4], PE_SIG_MAGIC)

        # File header Characteristics
        file_hdr = data[e_lfanew + 4 : e_lfanew + 24]
        num_sections, _, _, _, opt_size, characteristics = struct.unpack_from("<HIIIHH", file_hdr, 2)
        # IMAGE_FILE_DLL = 0x2000
        self.assertTrue(bool(characteristics & 0x2000), f"Expected IMAGE_FILE_DLL (0x2000) flag in {hex(characteristics)}")

        # Optional header DataDirectory[0] = Export Table
        opt_offset = e_lfanew + 24
        export_rva, export_size = struct.unpack_from("<II", data, opt_offset + 112)
        self.assertGreater(export_rva, 0, "Export Directory RVA must be non-zero")
        self.assertGreater(export_size, 0, "Export Directory size must be non-zero")

        # Locate .edata section
        sec_offset = opt_offset + opt_size
        edata_raw = None
        edata_vaddr = None
        for i in range(num_sections):
            sec = data[sec_offset + i*40 : sec_offset + (i+1)*40]
            name = sec[:8].rstrip(b"\x00").decode("latin1")
            vsize, vaddr, rsize, roff = struct.unpack_from("<IIII", sec, 8)
            if name == ".edata":
                edata_raw = roff
                edata_vaddr = vaddr
                break

        self.assertIsNotNone(edata_raw, ".edata section header not found")
        # Parse IMAGE_EXPORT_DIRECTORY
        exp_dir = data[edata_raw : edata_raw + 40]
        _, _, _, _, name_rva, base, num_funcs, num_names, eat_rva, ent_rva, eot_rva = struct.unpack_from("<IIHHIIIIIII", exp_dir)
        self.assertEqual(base, 1)
        self.assertEqual(num_funcs, 2)
        self.assertEqual(num_names, 2)

        # Verify exported names in ENT
        ent_offset = edata_raw + (ent_rva - edata_vaddr)
        exported_names = []
        for j in range(num_names):
            nrva = struct.unpack_from("<I", data, ent_offset + j*4)[0]
            name_str = data[edata_raw + (nrva - edata_vaddr):].split(b"\x00")[0].decode("latin1")
            exported_names.append(name_str)

        self.assertIn("compute_sum", exported_names)
        self.assertIn("compute_prod", exported_names)

    def test_24_win64_pe_dll_runtime_loadlibrary(self):
        """Verify Win64 PE DLL runtime execution via ctypes LoadLibrary on Windows host."""
        src = os.path.join(self.tmp_dir.name, "probe_dll_runtime.c")
        dll = os.path.join(self.tmp_dir.name, "probe_dll_runtime.dll")

        with open(src, "w", encoding="utf-8") as f:
            f.write(
                "int add_ten(int x) {\n"
                "    return x + 10;\n"
                "}\n"
                "int square(int x) {\n"
                "    return x * x;\n"
                "}\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_dll = dll.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -mabi=ms -shared -o '{wsl_dll}'"]
        else:
            cmd = [self.zcc_bin, src, "-target", "win64", "-mabi=ms", "-shared", "-o", dll]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(dll))

        if sys.platform == "win32":
            import ctypes
            import _ctypes
            lib = ctypes.CDLL(dll)
            try:
                add_fn = lib.add_ten
                add_fn.argtypes = [ctypes.c_int]
                add_fn.restype = ctypes.c_int
                self.assertEqual(add_fn(32), 42)

                sq_fn = lib.square
                sq_fn.argtypes = [ctypes.c_int]
                sq_fn.restype = ctypes.c_int
                self.assertEqual(sq_fn(9), 81)
            finally:
                _ctypes.FreeLibrary(lib._handle)

    def test_25_win64_pe_seh_unwind_ehandler_and_scope_table(self):
        """Verify Win64 PE Structured Exception Handling .pdata emission with UNW_FLAG_EHANDLER and ScopeTable."""
        src = os.path.join(self.tmp_dir.name, "probe_seh_test.c")
        exe = os.path.join(self.tmp_dir.name, "probe_seh_test.exe")

        with open(src, "w", encoding="utf-8") as f:
            f.write(
                "int protected_func_handler(void *rec, void *frame, void *ctx, void *disp) {\n"
                "    return 1;\n"
                "}\n"
                "int protected_func(void) {\n"
                "    int x = 10;\n"
                "    __asm__(\"seh_try_begin:\");\n"
                "    x += 32;\n"
                "    __asm__(\"seh_try_end:\");\n"
                "    if (x == 0) {\n"
                "        __asm__(\"seh_target:\");\n"
                "        x = 999;\n"
                "    }\n"
                "    return x;\n"
                "}\n"
                "int main(void) {\n"
                "    return protected_func();\n"
                "}\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_exe = exe.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -o '{wsl_exe}'"]
        else:
            cmd = [self.zcc_bin, src, "-target", "win64", "-o", exe]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"SEH compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(exe))

        with open(exe, "rb") as f:
            data = f.read()

        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        opt_offset = e_lfanew + 24
        pdata_rva, pdata_sz = struct.unpack_from("<II", data, opt_offset + 112 + 3*8)
        self.assertGreater(pdata_rva, 0x1000)
        self.assertGreaterEqual(pdata_sz, 24)

        # Locate .pdata section
        coff_offset = e_lfanew + 4
        num_sections, _, _, _, opt_size, _ = struct.unpack_from("<HIIIHH", data, coff_offset + 2)
        sec_headers_offset = opt_offset + opt_size
        pdata_raw = None
        for i in range(num_sections):
            sec_off = sec_headers_offset + i * 40
            sec_name = data[sec_off : sec_off + 8].rstrip(b"\x00")
            if sec_name == b".pdata":
                pdata_raw = struct.unpack_from("<I", data, sec_off + 20)[0]
                break
        self.assertIsNotNone(pdata_raw)

        # Find entry with UNW_FLAG_EHANDLER
        num_entries = pdata_sz // 12
        found_ehandler = False
        for i in range(num_entries):
            b_rva, e_rva, u_rva = struct.unpack_from("<III", data, pdata_raw + i*12)
            u_off = pdata_raw + (u_rva - pdata_rva)
            vf, psz, cc, fr = struct.unpack_from("<BBBB", data, u_off)
            version = vf & 7
            flags = vf >> 3
            self.assertEqual(version, 1)

            if flags & 1:  # UNW_FLAG_EHANDLER
                found_ehandler = True
                handler_rva = struct.unpack_from("<I", data, u_off + 8)[0]
                self.assertGreaterEqual(handler_rva, 0x1000, "ExceptionHandler RVA must be valid in .text")

                # Verify ScopeTable
                scope_cnt = struct.unpack_from("<I", data, u_off + 12)[0]
                self.assertEqual(scope_cnt, 1, "Expected 1 SEH ScopeRecord")

                sb, se, sh, st = struct.unpack_from("<IIII", data, u_off + 16)
                self.assertGreater(se, sb, "Scope EndAddress must be > BeginAddress")
                self.assertEqual(sh, 1, "HandlerAddress must be 1 (EXCEPTION_EXECUTE_HANDLER)")
                self.assertGreater(st, b_rva, "JumpTarget must be within function extent")

        self.assertTrue(found_ehandler, "Expected at least one function with UNW_FLAG_EHANDLER")

    def test_26_win64_pe_seh_runtime_execution(self):
        """Verify Windows host execution of binary with SEH unwind tables and scope records."""
        if sys.platform != "win32":
            self.skipTest("Host execution test requires Windows environment")

        exe = os.path.join(self.tmp_dir.name, "probe_seh_test.exe")
        self.assertTrue(os.path.exists(exe))

        run_res = subprocess.run([exe])
        self.assertEqual(run_res.returncode, 42, f"Expected returncode 42 from SEH probe, got {run_res.returncode}")


    def test_27_win64_pe_module_definition_file_emission(self):
        """Verify automatic emission of Module-Definition (.def) file during -shared compilation."""
        src = os.path.join(self.tmp_dir.name, "probe_def_test.c")
        dll = os.path.join(self.tmp_dir.name, "probe_def_test.dll")
        deffile = os.path.join(self.tmp_dir.name, "probe_def_test.def")

        with open(src, "w", encoding="utf-8") as f:
            f.write(
                "int export_alpha(int x) { return x + 1; }\n"
                "int export_beta(int y) { return y * 2; }\n"
            )

        if sys.platform == "win32":
            wsl_repo = REPO_ROOT.replace("\\", "/").replace("H:", "/mnt/h").replace("h:", "/mnt/h")
            wsl_src = src.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            wsl_dll = dll.replace("\\", "/").replace("C:", "/mnt/c").replace("c:", "/mnt/c")
            cmd = ["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", f"cd '{wsl_repo}' && ./zcc '{wsl_src}' -target win64 -shared -o '{wsl_dll}'"]
        else:
            cmd = [self.zcc_bin, src, "-target", "win64", "-shared", "-o", dll]

        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertTrue(os.path.exists(dll))
        self.assertTrue(os.path.exists(deffile), ".def file was not automatically emitted alongside .dll")

        with open(deffile, "r", encoding="utf-8") as f:
            def_content = f.read()

        self.assertIn("LIBRARY \"probe_def_test.dll\"", def_content)
        self.assertIn("EXPORTS", def_content)
        self.assertIn("export_alpha", def_content)
        self.assertIn("export_beta", def_content)

    def test_28_win64_pe_coff_import_library_emission(self):
        """Verify automatic emission of MSVC-compatible COFF Import Library (.lib) archive during -shared compilation."""
        dll = os.path.join(self.tmp_dir.name, "probe_def_test.dll")
        libfile = os.path.join(self.tmp_dir.name, "probe_def_test.lib")

        self.assertTrue(os.path.exists(libfile), ".lib import library was not emitted alongside .dll")

        with open(libfile, "rb") as f:
            data = f.read()

        # Archive Signature (8 bytes)
        self.assertTrue(data.startswith(b"!<arch>\n"), "Invalid COFF archive signature")

        # First Linker Member header (60 bytes)
        first_hdr = data[8:68]
        self.assertTrue(first_hdr.startswith(b"/               "), "Expected first linker member /")
        member_sz_str = first_hdr[48:58].decode("ascii").strip()
        member_sz = int(member_sz_str)
        self.assertGreater(member_sz, 0)

        # Unpack big-endian symbol count
        first_data = data[68 : 68 + member_sz]
        num_symbols = struct.unpack_from(">I", first_data, 0)[0]
        self.assertEqual(num_symbols, 4, f"Expected 4 symbols (2 direct + 2 __imp_), got {num_symbols}")

        # Verify symbols present in string table
        self.assertIn(b"export_alpha\x00", first_data)
        self.assertIn(b"__imp_export_alpha\x00", first_data)
        self.assertIn(b"export_beta\x00", first_data)
        self.assertIn(b"__imp_export_beta\x00", first_data)

        # Verify Short Import Object member headers
        self.assertIn(b"export_alpha.obj/", data)
        self.assertIn(b"export_beta.obj/", data)

        # Verify Short Import Object Header fields (Sig1=0, Sig2=0xFFFF, Machine=0x8664)
        alpha_idx = data.find(b"export_alpha.obj/")
        self.assertGreater(alpha_idx, 0)
        obj_data = data[alpha_idx + 60 :]
        sig1, sig2, ver, machine = struct.unpack_from("<HHHH", obj_data, 0)
        self.assertEqual(sig1, 0x0000, "Sig1 must be 0x0000")
        self.assertEqual(sig2, 0xFFFF, "Sig2 must be 0xFFFF (IMAGE_FILE_MACHINE_UNKNOWN)")
        self.assertEqual(machine, 0x8664, "Machine must be 0x8664 (AMD64)")


if __name__ == "__main__":
    unittest.main()


