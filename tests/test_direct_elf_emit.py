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

    def test_11_freestanding_stdio_math_and_exp_suite(self):
        """Verify freestanding FILE* I/O, vsnprintf/snprintf, SSE2 rt_math, and exp1..exp5 with zero binutils."""
        src_path = os.path.join(self.temp_dir.name, "test_rt_full.c")
        exe_path = os.path.join(self.temp_dir.name, "test_rt_full_exe")
        data_file = os.path.join(self.temp_dir.name, "io_test.bin")

        code = f"""#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>

int remove(const char *path);
int vsnprintf(char *str, unsigned long size, const char *fmt, va_list ap);
int snprintf(char *str, unsigned long size, const char *fmt, ...);
double fmin(double a, double b);
double fmax(double a, double b);
float fminf(float a, float b);
float fmaxf(float a, float b);
float floorf(float x);
float fmodf(float x, float y);
float sinf(float x);
float cosf(float x);
float tanf(float x);
float expf(float x);
float fabsf(float x);

static int my_vsn(char *buf, unsigned long sz, const char *fmt, ...) {{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sz, fmt, ap);
    va_end(ap);
    return n;
}}

int main(void) {{
    char b1[64], b2[64];
    int n1 = my_vsn(b1, sizeof(b1), "v=%d|%.2f|%s", 17, 2.75, "rt");
    int n2 = snprintf(b2, sizeof(b2), "s=%x|%.1e", 0xAB, 125.0);
    if (n1 != 12 || n2 <= 0) return 10;

    const char *path = "{data_file.replace(chr(92), "/")}";
    FILE *wf = fopen(path, "wb");
    if (!wf) return 11;
    if (fwrite(b1, 1, (size_t)n1, wf) != (size_t)n1) return 12;
    fputc('!', wf);
    fflush(wf);
    fclose(wf);

    FILE *rf = fopen(path, "rb");
    if (!rf) return 13;
    fseek(rf, 0, SEEK_END);
    long fsz = ftell(rf);
    if (fsz != n1 + 1) return 14;
    fseek(rf, 0, SEEK_SET);
    char rbuf[64];
    if (fread(rbuf, 1, (size_t)fsz, rf) != (size_t)fsz) return 15;
    rbuf[fsz] = 0;
    fclose(rf);
    if (remove(path) != 0) return 16;

    srand(42);
    int rval = rand();
    if (rval <= 0) return 17;

    if (fabsf(sqrtf(9.0f) - 3.0f) > 1e-4f) return 18;
    if (fabs(sqrt(16.0) - 4.0) > 1e-9) return 19;
    if (fabs(floor(-1.25) - (-2.0)) > 1e-9) return 20;
    if (fabsf(floorf(-1.25f) - (-2.0f)) > 1e-5f) return 21;
    if (fabsf(fmodf(5.5f, 2.0f) - 1.5f) > 1e-5f) return 22;
    if (fabsf(fminf(3.0f, -2.0f) - (-2.0f)) > 1e-5f) return 23;
    if (fabsf(fmaxf(3.0f, -2.0f) - 3.0f) > 1e-5f) return 24;
    if (fabs(fmin(5.0, 2.0) - 2.0) > 1e-9) return 25;
    if (fabs(fmax(5.0, 2.0) - 5.0) > 1e-9) return 26;
    if (fabsf(sinf(0.5f) - 0.4794255f) > 1e-4f) return 27;
    if (fabsf(cosf(0.5f) - 0.8775826f) > 1e-4f) return 28;
    if (fabsf(tanf(0.5f) - 0.5463025f) > 2e-4f) return 29;
    if (fabsf(expf(1.0f) - 2.7182818f) > 2e-4f) return 30;

    fprintf(stderr, "FS_ERR:%s|%s\\n", b2, rbuf);
    printf("FREESTANDING_STDIO_MATH_OK:%s\\n", rbuf);
    return 0;
}}
"""
        with open(src_path, "w") as f:
            f.write(code)

        res = subprocess.run([ZCC_BIN, src_path, "-o", exe_path], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Freestanding unit compile failed: {res.stderr}\n{res.stdout}")
        self.assertNotIn("[Phase 6] GCC Assembly/Linker Binding", res.stdout)

        run_res = subprocess.run([exe_path], capture_output=True, text=True)
        self.assertEqual(run_res.returncode, 0, f"Freestanding unit run failed ({run_res.returncode}): {run_res.stderr}")
        self.assertIn("FREESTANDING_STDIO_MATH_OK:v=17|2.75|rt!", run_res.stdout)
        self.assertIn("FS_ERR:s=ab|125.0|", run_res.stderr)

        exps = [
            ("exp1_raytracer_simd.c", b"P6\n640 480\n255\n", 921615, None),
            ("exp2_voxel_engine.c", b"P6\n640 480\n255\n", 921615, b"16\xc3\x9732\xc3\x9716 voxels"),
            ("exp3_audio_visualizer.c", b"P6\n640 480\n255\n", 921615, None),
            ("exp4_vr_stereo.c", b"P6\n1280 480\n255\n", 1843216, None),
            ("exp5_physics_engine.c", b"P6\n640 480\n255\n", 921615, None),
        ]
        for fname, exp_hdr, exp_sz, exp_err in exps:
            c_path = os.path.join(REPO_ROOT, fname)
            bin_path = os.path.join(self.temp_dir.name, fname[:-2])
            cres = subprocess.run([ZCC_BIN, c_path, "-o", bin_path], capture_output=True, text=True)
            self.assertEqual(cres.returncode, 0, f"{fname} compile failed: {cres.stderr}\n{cres.stdout}")
            self.assertNotIn("[Phase 6] GCC Assembly/Linker Binding", cres.stdout)
            rres = subprocess.run([bin_path], capture_output=True)
            self.assertEqual(rres.returncode, 0, f"{fname} run failed ({rres.returncode}): {rres.stderr!r}")
            self.assertEqual(len(rres.stdout), exp_sz, f"{fname} unexpected PPM size")
            self.assertTrue(rres.stdout.startswith(exp_hdr), f"{fname} unexpected PPM header: {rres.stdout[:24]!r}")
            if exp_err is not None:
                self.assertIn(exp_err, rres.stderr)

    def test_12_freestanding_argc_env_stdlib_str_and_nested_zero_init(self):
        """Verify nested Struct s = {0}; init, _start(argc,argv,envp), getenv, calloc/realloc, qsort/bsearch, strtol, str* ext, and sscanf."""
        src_path = os.path.join(self.temp_dir.name, "test_rt12.c")
        exe_path = os.path.join(self.temp_dir.name, "test_rt12_exe")

        code = """#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *getenv(const char *name);
void *calloc(unsigned long nmemb, unsigned long size);
void *realloc(void *ptr, unsigned long size);
int atoi(const char *nptr);
long atol(const char *nptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
int abs(int j);
long labs(long j);
void qsort(void *base, unsigned long nmemb, unsigned long size, int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, unsigned long nmemb, unsigned long size, int (*compar)(const void *, const void *));
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
void *memchr(const void *s, int c, unsigned long n);
char *strdup(const char *s);
char *strcat(char *dst, const char *src);
char *strncat(char *dst, const char *src, unsigned long n);
int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int tolower(int c);
int toupper(int c);
int sscanf(const char *str, const char *fmt, ...);

struct Inner {
    int a;
    long long b;
};

union ValU {
    int i;
    double d;
};

struct Outer {
    struct Inner in;
    int arr[4];
    union ValU u;
    int tail;
};

struct Item {
    int key;
    long long val;
};

static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

static int cmp_item(const void *a, const void *b) {
    const struct Item *ia = (const struct Item *)a;
    const struct Item *ib = (const struct Item *)b;
    return (ia->key > ib->key) - (ia->key < ib->key);
}

int main(int argc, char **argv, char **envp) {
    struct Outer x = {0};
    if (x.in.a != 0 || x.in.b != 0 || x.arr[0] != 0 || x.arr[3] != 0 || x.u.i != 0 || x.tail != 0)
        return 10;

    struct Outer y = { 77 };
    if (y.in.a != 77 || y.in.b != 0 || y.arr[2] != 0 || y.tail != 0)
        return 11;

    struct Outer w = { { 11, 22 }, { 1, 2, 3, 4 }, { 99 }, 55 };
    if (w.in.a != 11 || w.in.b != 22 || w.arr[0] != 1 || w.arr[3] != 4 || w.u.i != 99 || w.tail != 55)
        return 12;

    if (argc != 4) return 13;
    if (strcmp(argv[1], "alpha") != 0 || strcmp(argv[2], "beta") != 0 || strcmp(argv[3], "gamma") != 0)
        return 14;
    if (!envp || !envp[0]) return 15;
    char *tok = getenv("ZCC_SOVEREIGN_TOKEN");
    if (!tok || strcmp(tok, "OMEGA_42") != 0) return 16;

    int *z = (int *)calloc(16, sizeof(int));
    if (!z) return 17;
    for (int i = 0; i < 16; i++) {
        if (z[i] != 0) return 18;
        z[i] = 100 + i;
    }
    int *z2 = (int *)realloc(z, 32 * sizeof(int));
    if (!z2 || z2[0] != 100 || z2[15] != 115) return 19;

    if (atoi(" -123") != -123) return 20;
    if (atol("9876543210") != 9876543210L) return 21;
    char *ep = 0;
    if (strtol("  -0x2a_ok", &ep, 0) != -42 || !ep || strcmp(ep, "_ok") != 0) return 22;
    if (strtoul("ffZ", &ep, 16) != 255 || *ep != 'Z') return 23;
    if (abs(-77) != 77 || labs(-9876543210L) != 9876543210L) return 24;

    int nums[6] = { 42, -5, 19, 0, 7, 19 };
    qsort(nums, 6, sizeof(int), cmp_int);
    if (nums[0] != -5 || nums[1] != 0 || nums[2] != 7 || nums[3] != 19 || nums[4] != 19 || nums[5] != 42)
        return 25;
    int sk = 7;
    int *found_i = (int *)bsearch(&sk, nums, 6, sizeof(int), cmp_int);
    if (!found_i || *found_i != 7) return 26;

    struct Item items[4] = { {30, 300}, {10, 100}, {40, 400}, {20, 200} };
    qsort(items, 4, sizeof(struct Item), cmp_item);
    if (items[0].key != 10 || items[1].val != 200 || items[3].key != 40) return 27;
    struct Item ik = { 20, 0 };
    struct Item *found_it = (struct Item *)bsearch(&ik, items, 4, sizeof(struct Item), cmp_item);
    if (!found_it || found_it->val != 200) return 28;

    const char *hay = "zcc_sovereign_runtime_zcc";
    if (!strchr(hay, 'v') || strchr(hay, 'v') - hay != 6) return 29;
    if (!strchr(hay, 0) || *strchr(hay, 0) != 0) return 30;
    if (!strrchr(hay, 'z') || strrchr(hay, 'z') - hay != 22) return 31;
    if (!strstr(hay, "sovereign") || strstr(hay, "sovereign") - hay != 4) return 32;
    if (memchr("ab\\0xy", 'x', 5) == 0) return 33;

    char *dup = strdup("hello");
    if (!dup || strcmp(dup, "hello") != 0) return 34;
    char sbuf[32] = "foo";
    strcat(sbuf, "_bar");
    strncat(sbuf, "_baz123", 4);
    if (strcmp(sbuf, "foo_bar_baz") != 0) return 35;

    if (!isspace(' ') || !isdigit('9') || !isalpha('Q') || tolower('Z') != 'z' || toupper('a') != 'A')
        return 36;

    int sa = 0, sc = 0;
    long sb = 0;
    char sstr[32] = {0};
    int sn = sscanf("42 -17 0x2a hello!", "%d %ld %x %31s", &sa, &sb, &sc, sstr);
    if (sn != 4 || sa != 42 || sb != -17 || sc != 42 || strcmp(sstr, "hello!") != 0)
        return 37;

    printf("RT12_PASS:%s|%s|%d\\n", tok, sbuf, sc);
    return 0;
}
"""
        with open(src_path, "w") as f:
            f.write(code)

        res = subprocess.run([ZCC_BIN, src_path, "-o", exe_path], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Compile failed: {res.stderr}\n{res.stdout}")
        self.assertNotIn("[Phase 6] GCC Assembly/Linker Binding", res.stdout)

        env = os.environ.copy()
        env["ZCC_SOVEREIGN_TOKEN"] = "OMEGA_42"
        run_res = subprocess.run([exe_path, "alpha", "beta", "gamma"], env=env, capture_output=True, text=True)
        self.assertEqual(run_res.returncode, 0, f"Runtime failed (rc={run_res.returncode}): stdout={run_res.stdout!r} stderr={run_res.stderr!r}")
        self.assertIn("RT12_PASS:OMEGA_42|foo_bar_baz|42", run_res.stdout)

if __name__ == "__main__":
    unittest.main()
