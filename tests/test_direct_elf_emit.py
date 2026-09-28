#!/usr/bin/env python3
"""
Test Suite: Hardened Autonomous Direct-to-Binary & Direct-to-Object Emission (src/elf_emit.c / src/zld.c)
Validates that ZCC compiles C source to native ELF64 relocatables and static executables
autonomously without external GNU/LLVM (as/ld/gcc) toolchain dependencies.
Covers multi-TU linking, static archive linking, base derivation, -S flag preservation,
and undefined symbol diagnostic verification.
"""

import os
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")

class TestDirectElfEmit(unittest.TestCase):
    def setUp(self):
        self.assertTrue(os.path.isfile(ZCC_BIN), f"zcc binary not found at {ZCC_BIN}")
        self.temp_dir = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.temp_dir.cleanup()

    def test_01_compile_only_object_emission(self):
        """Test that ./zcc input.c -c -o input.o emits a valid ELF64 relocatable object autonomously."""
        src_path = os.path.join(self.temp_dir.name, "test_ret.c")
        obj_path = os.path.join(self.temp_dir.name, "test_ret.o")
        with open(src_path, "w") as f:
            f.write("int main(void) { return 42; }\n")

        res = subprocess.run([ZCC_BIN, src_path, "-c", "-o", obj_path], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        self.assertNotIn("[Phase 6] GCC Assembly/Linker Binding", res.stdout, "Should not invoke GCC Phase 6")
        self.assertTrue(os.path.isfile(obj_path), "Target .o was not created")

        file_res = subprocess.run(["file", obj_path], capture_output=True, text=True)
        self.assertIn("ELF 64-bit", file_res.stdout)
        self.assertIn("relocatable", file_res.stdout)

    def test_02_object_linking_to_executable(self):
        """Test that ./zcc input.o -o exec links native object into static ELF64 executable autonomously."""
        src_path = os.path.join(self.temp_dir.name, "test_calc.c")
        obj_path = os.path.join(self.temp_dir.name, "test_calc.o")
        exe_path = os.path.join(self.temp_dir.name, "test_calc_exe")
        with open(src_path, "w") as f:
            f.write("""
int square(int x) { return x * x; }
int main(void) {
    int a = 6;
    return square(a) + 6; // 36 + 6 = 42
}
""")

        # Step 1: -c -o
        res1 = subprocess.run([ZCC_BIN, src_path, "-c", "-o", obj_path], capture_output=True, text=True)
        self.assertEqual(res1.returncode, 0, f"Object emission failed: {res1.stderr}")

        # Step 2: link .o to binary
        res2 = subprocess.run([ZCC_BIN, obj_path, "-o", exe_path], capture_output=True, text=True)
        self.assertEqual(res2.returncode, 0, f"Linking failed: {res2.stderr}\n{res2.stdout}")
        self.assertTrue(os.path.isfile(exe_path), "Target binary was not created")

        # Step 3: execute directly on WSL without external loader
        run_res = subprocess.run([exe_path], capture_output=True)
        self.assertEqual(run_res.returncode, 42, f"Execution returned {run_res.returncode}, expected 42")

    def test_03_end_to_end_direct_binary(self):
        """Test that ./zcc input.c -o exec compiles and links directly to ELF64 executable."""
        src_path = os.path.join(self.temp_dir.name, "test_e2e.c")
        exe_path = os.path.join(self.temp_dir.name, "test_e2e_exe")
        with open(src_path, "w") as f:
            f.write("""
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}
int main(void) {
    return fib(9); // fib(9) = 34
}
""")

        res = subprocess.run([ZCC_BIN, src_path, "-o", exe_path], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Direct binary compilation failed: {res.stderr}\n{res.stdout}")
        self.assertNotIn("[Phase 6] GCC Assembly/Linker Binding", res.stdout)
        self.assertTrue(os.path.isfile(exe_path))

        run_res = subprocess.run([exe_path], capture_output=True)
        self.assertEqual(run_res.returncode, 34, f"Execution returned {run_res.returncode}, expected 34")

    def test_04_globals_and_pointers(self):
        """Test global data and pointer arithmetic under autonomous emission."""
        src_path = os.path.join(self.temp_dir.name, "test_globals.c")
        exe_path = os.path.join(self.temp_dir.name, "test_globals_exe")
        with open(src_path, "w") as f:
            f.write("""
int g_val = 100;
int g_arr[4] = { 5, 10, 15, 20 };

int main(void) {
    int sum = 0;
    int *p = g_arr;
    for (int i = 0; i < 4; i++) {
        sum += *(p + i);
    }
    return g_val - sum; // 100 - 50 = 50
}
""")

        res = subprocess.run([ZCC_BIN, src_path, "-o", exe_path], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        run_res = subprocess.run([exe_path], capture_output=True)
        self.assertEqual(run_res.returncode, 50, f"Execution returned {run_res.returncode}, expected 50")

    def test_05_multi_tu_c_and_obj_linking(self):
        """Test autonomous compilation of .c and linking with an external .o."""
        helper_src = os.path.join(self.temp_dir.name, "helper.c")
        helper_obj = os.path.join(self.temp_dir.name, "helper.o")
        main_src = os.path.join(self.temp_dir.name, "main.c")
        exe_path = os.path.join(self.temp_dir.name, "multi_tu_exe")

        with open(helper_src, "w") as f:
            f.write("int get_secret(void) { return 77; }\n")
        with open(main_src, "w") as f:
            f.write("int get_secret(void); int main(void) { return get_secret() + 1; }\n")

        # Compile helper.o
        res1 = subprocess.run([ZCC_BIN, helper_src, "-c", "-o", helper_obj], capture_output=True, text=True)
        self.assertEqual(res1.returncode, 0, f"Helper compilation failed: {res1.stderr}")

        # Compile main.c and link with helper.o
        res2 = subprocess.run([ZCC_BIN, main_src, helper_obj, "-o", exe_path], capture_output=True, text=True)
        self.assertEqual(res2.returncode, 0, f"Multi-TU linking failed: {res2.stderr}\n{res2.stdout}")

        run_res = subprocess.run([exe_path], capture_output=True)
        self.assertEqual(run_res.returncode, 78, f"Execution returned {run_res.returncode}, expected 78")

    def test_06_pure_multi_obj_linking(self):
        """Test linking multiple .o files directly without any .c file."""
        mod1_src = os.path.join(self.temp_dir.name, "mod1.c")
        mod1_obj = os.path.join(self.temp_dir.name, "mod1.o")
        mod2_src = os.path.join(self.temp_dir.name, "mod2.c")
        mod2_obj = os.path.join(self.temp_dir.name, "mod2.o")
        exe_path = os.path.join(self.temp_dir.name, "pure_objs_exe")

        with open(mod1_src, "w") as f:
            f.write("int add5(int x) { return x + 5; }\n")
        with open(mod2_src, "w") as f:
            f.write("int add5(int); int main(void) { return add5(10); }\n")

        self.assertEqual(subprocess.run([ZCC_BIN, mod1_src, "-c", "-o", mod1_obj]).returncode, 0)
        self.assertEqual(subprocess.run([ZCC_BIN, mod2_src, "-c", "-o", mod2_obj]).returncode, 0)

        res = subprocess.run([ZCC_BIN, mod1_obj, mod2_obj, "-o", exe_path], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Object link failed: {res.stderr}\n{res.stdout}")
        run_res = subprocess.run([exe_path], capture_output=True)
        self.assertEqual(run_res.returncode, 15, f"Execution returned {run_res.returncode}, expected 15")

    def test_07_archive_static_linking(self):
        """Test linking against a static library archive (.a)."""
        lib_src = os.path.join(self.temp_dir.name, "libmath.c")
        lib_obj = os.path.join(self.temp_dir.name, "libmath.o")
        lib_a = os.path.join(self.temp_dir.name, "libmath.a")
        main_src = os.path.join(self.temp_dir.name, "app.c")
        exe_path = os.path.join(self.temp_dir.name, "app_exe")

        with open(lib_src, "w") as f:
            f.write("int compute_val(void) { return 21; }\n")
        with open(main_src, "w") as f:
            f.write("int compute_val(void); int main(void) { return compute_val() * 2; }\n")

        self.assertEqual(subprocess.run([ZCC_BIN, lib_src, "-c", "-o", lib_obj]).returncode, 0)
        ar_res = subprocess.run(["ar", "rcs", lib_a, lib_obj], capture_output=True)
        self.assertEqual(ar_res.returncode, 0, "ar failed")

        res = subprocess.run([ZCC_BIN, main_src, lib_a, "-o", exe_path], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Archive link failed: {res.stderr}\n{res.stdout}")
        run_res = subprocess.run([exe_path], capture_output=True)
        self.assertEqual(run_res.returncode, 42, f"Execution returned {run_res.returncode}, expected 42")

    def test_08_undefined_symbol_fails_cleanly(self):
        """Test that unresolved symbols cause zld to report an error and exit non-zero."""
        bad_src = os.path.join(self.temp_dir.name, "bad.c")
        bad_exe = os.path.join(self.temp_dir.name, "bad_exe")
        with open(bad_src, "w") as f:
            f.write("int non_existent_fn(void); int main(void) { return non_existent_fn(); }\n")

        res = subprocess.run([ZCC_BIN, bad_src, "-o", bad_exe], capture_output=True, text=True)
        self.assertNotEqual(res.returncode, 0, "Should have failed on undefined symbol")
        self.assertIn("undefined symbol", res.stderr + res.stdout)

    def test_09_base_derived_object_filename(self):
        """Test that ./zcc sub/dir/foo.c -c produces foo.o in the current working directory."""
        subdir = os.path.join(self.temp_dir.name, "subdir")
        os.makedirs(subdir, exist_ok=True)
        src_path = os.path.join(subdir, "deep.c")
        with open(src_path, "w") as f:
            f.write("int deep_val(void) { return 1; }\n")

        cwd = self.temp_dir.name
        res = subprocess.run([ZCC_BIN, src_path, "-c"], cwd=cwd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}\n{res.stdout}")
        expected_o = os.path.join(cwd, "deep.o")
        self.assertTrue(os.path.isfile(expected_o), f"Expected {expected_o} in CWD")

    def test_10_escape_hatch_and_text_asm(self):
        """Verify that -S and --use-system-as preserve legacy behavior."""
        src_path = os.path.join(self.temp_dir.name, "test_esc.c")
        with open(src_path, "w") as f:
            f.write("int main(void) { return 0; }\n")

        # -S produces text assembly and does not link
        cwd = self.temp_dir.name
        res_s = subprocess.run([ZCC_BIN, src_path, "-S"], cwd=cwd, capture_output=True, text=True)
        self.assertEqual(res_s.returncode, 0)
        expected_s = os.path.join(cwd, "test_esc.s")
        self.assertTrue(os.path.isfile(expected_s))
        with open(expected_s, "r") as f:
            content = f.read()
            self.assertIn(".globl main", content)

        # --use-system-as routes to GCC Phase 6
        obj_sys = os.path.join(self.temp_dir.name, "test_sys.o")
        res_sys = subprocess.run([ZCC_BIN, "--use-system-as", src_path, "-c", "-o", obj_sys], capture_output=True, text=True)
        self.assertEqual(res_sys.returncode, 0)
        self.assertIn("[Phase 6] GCC Assembly/Linker Binding", res_sys.stdout)

if __name__ == "__main__":
    unittest.main()
