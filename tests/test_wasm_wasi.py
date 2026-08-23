import os
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")
NODE_BIN = "/usr/bin/node"

def run_wasi_program(c_source: str, stdin_input: str = "") -> tuple[int, str, str]:
    with tempfile.NamedTemporaryFile(suffix=".c", mode="w", delete=False) as f_c:
        f_c.write(c_source)
        c_path = f_c.name
    wasm_path = c_path + ".wasm"
    runner_js = c_path + "_runner.js"

    try:
        # Compile C source to WASM using ZCC
        comp = subprocess.run(
            [ZCC_BIN, "--target=wasm32", c_path, "-o", wasm_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        if comp.returncode != 0:
            raise RuntimeError(f"ZCC compilation failed (rc={comp.returncode}):\n{comp.stderr}\n{comp.stdout}")

        # Node.js WASI Preview1 Runner
        js_code = f"""
const fs = require('fs');
const {{ WASI }} = require('wasi');

const wasi = new WASI({{
    version: 'preview1',
    args: ['program.wasm'],
    env: process.env,
    preopens: {{}}
}});

const wasmBuffer = fs.readFileSync('{wasm_path}');

// 1. Independent V8 WebAssembly Validation
const isValid = WebAssembly.validate(wasmBuffer);
if (!isValid) {{
    console.error("WASM Binary Validation Failed!");
    process.exit(254);
}}

// 2. Instantiate with WASI Preview1 host imports
WebAssembly.instantiate(wasmBuffer, {{
    wasi_snapshot_preview1: wasi.wasiImport
}}).then(mod => {{
    if (mod.instance.exports._start) {{
        const exitCode = wasi.start(mod.instance);
        process.exit(exitCode !== undefined ? exitCode : 0);
    }} else if (mod.instance.exports.main) {{
        try {{ wasi.initialize(mod.instance); }} catch(e) {{}}
        const res = mod.instance.exports.main();
        process.exit(res);
    }}
}}).catch(err => {{
    if (err && typeof err.code === 'number') {{
        process.exit(err.code);
    }}
    console.error(err);
    process.exit(255);
}});
"""
        with open(runner_js, "w") as f_js:
            f_js.write(js_code)

        run = subprocess.run(
            [NODE_BIN, runner_js],
            input=stdin_input,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return run.returncode, run.stdout, run.stderr
    finally:
        for p in (c_path, wasm_path, runner_js):
            if os.path.exists(p):
                os.remove(p)

class TestWASIPreview1(unittest.TestCase):

    # -------------------------------------------------------------
    # 1. Basic stdout with puts
    # -------------------------------------------------------------
    def test_01_puts_stdout(self):
        code = """
int puts(const char *s);
int main(void) {
    puts("Hello from ZCC WASM");
    return 0;
}
"""
        rc, out, err = run_wasi_program(code)
        self.assertEqual(rc, 0)
        self.assertEqual(out, "Hello from ZCC WASM\n")

    # -------------------------------------------------------------
    # 2. Sequential puts
    # -------------------------------------------------------------
    def test_02_sequential_puts(self):
        code = """
int puts(const char *s);
int main(void) {
    puts("Line 1");
    puts("Line 2");
    puts("Line 3");
    return 0;
}
"""
        rc, out, err = run_wasi_program(code)
        self.assertEqual(rc, 0)
        self.assertEqual(out, "Line 1\nLine 2\nLine 3\n")

    # -------------------------------------------------------------
    # 3. Empty string puts
    # -------------------------------------------------------------
    def test_03_empty_string_puts(self):
        code = """
int puts(const char *s);
int main(void) {
    puts("");
    return 0;
}
"""
        rc, out, err = run_wasi_program(code)
        self.assertEqual(rc, 0)
        self.assertEqual(out, "\n")

    # -------------------------------------------------------------
    # 4. Long string puts
    # -------------------------------------------------------------
    def test_04_long_string_puts(self):
        long_str = "The quick brown fox jumps over the lazy dog. 0123456789! WebAssembly Preview1 I/O in ZCC Compiler."
        code = f"""
int puts(const char *s);
int main(void) {{
    puts("{long_str}");
    return 0;
}}
"""
        rc, out, err = run_wasi_program(code)
        self.assertEqual(rc, 0)
        self.assertEqual(out, long_str + "\n")

    # -------------------------------------------------------------
    # 5. putchar stdout
    # -------------------------------------------------------------
    def test_05_putchar_stdout(self):
        code = """
int putchar(int c);
int main(void) {
    putchar('Z');
    putchar('C');
    putchar('C');
    putchar('\\n');
    return 0;
}
"""
        rc, out, err = run_wasi_program(code)
        self.assertEqual(rc, 0)
        self.assertEqual(out, "ZCC\n")

    # -------------------------------------------------------------
    # 6. write() syscall wrapper to stdout (fd = 1)
    # -------------------------------------------------------------
    def test_06_write_stdout(self):
        code = """
int write(int fd, const void *buf, int count);
int main(void) {
    int n = write(1, "WASI Preview1\\n", 14);
    return n == 14 ? 0 : 1;
}
"""
        rc, out, err = run_wasi_program(code)
        self.assertEqual(rc, 0)
        self.assertEqual(out, "WASI Preview1\n")

    # -------------------------------------------------------------
    # 7. write() syscall wrapper to stderr (fd = 2)
    # -------------------------------------------------------------
    def test_07_write_stderr(self):
        code = """
int write(int fd, const void *buf, int count);
int main(void) {
    write(2, "Error Log Message\\n", 18);
    return 0;
}
"""
        rc, out, err = run_wasi_program(code)
        self.assertEqual(rc, 0)
        self.assertIn("Error Log Message\n", err)

    # -------------------------------------------------------------
    # 8. exit() syscall with successful code 0
    # -------------------------------------------------------------
    def test_08_exit_zero(self):
        code = """
int puts(const char *s);
void exit(int code);
int main(void) {
    puts("Exiting immediately with 0");
    exit(0);
    return 1;
}
"""
        rc, out, err = run_wasi_program(code)
        self.assertEqual(rc, 0)
        self.assertEqual(out, "Exiting immediately with 0\n")

    # -------------------------------------------------------------
    # 9. exit() syscall with non-zero exit code
    # -------------------------------------------------------------
    def test_09_exit_nonzero(self):
        code = """
int puts(const char *s);
void exit(int code);
int main(void) {
    puts("Exiting with 42");
    exit(42);
    return 0;
}
"""
        rc, out, err = run_wasi_program(code)
        self.assertEqual(rc, 42)
        self.assertEqual(out, "Exiting with 42\n")

    # -------------------------------------------------------------
    # 10. read() syscall from stdin (fd = 0)
    # -------------------------------------------------------------
    def test_10_read_stdin(self):
        code = """
int read(int fd, void *buf, int count);
int puts(const char *s);
int main(void) {
    char buf[32];
    int n = read(0, buf, 5);
    buf[n] = '\\0';
    puts(buf);
    return 0;
}
"""
        rc, out, err = run_wasi_program(code, stdin_input="HELLO WORLD")
        self.assertEqual(rc, 0)
        self.assertEqual(out, "HELLO\n")

    # -------------------------------------------------------------
    # 11. Multiple user functions with WASI imports present
    # -------------------------------------------------------------
    def test_11_multiple_functions_with_imports(self):
        code = """
int puts(const char *s);

int add(int a, int b) {
    return a + b;
}

int mul(int a, int b) {
    return a * b;
}

int compute(int x, int y) {
    int s = add(x, y);
    int p = mul(s, 2);
    return p;
}

int main(void) {
    int res = compute(10, 5);
    if (res == 30) {
        puts("COMPUTE PASS");
        return 0;
    } else {
        puts("COMPUTE FAIL");
        return 1;
    }
}
"""
        rc, out, err = run_wasi_program(code)
        self.assertEqual(rc, 0)
        self.assertEqual(out, "COMPUTE PASS\n")

    # -------------------------------------------------------------
    # 12. Function pointers / indirect dispatch with WASI imports
    # -------------------------------------------------------------
    def test_12_function_pointers_with_wasi(self):
        code = """
int puts(const char *s);
typedef int (*op_fn)(int, int);

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

int apply(op_fn fn, int a, int b) {
    return fn(a, b);
}

int main(void) {
    int r1 = apply(add, 20, 10);
    int r2 = apply(sub, 20, 10);
    if (r1 == 30 && r2 == 10) {
        puts("FPTR DISPATCH OK");
        return 0;
    }
    puts("FPTR DISPATCH FAIL");
    return 1;
}
"""
        rc, out, err = run_wasi_program(code)
        self.assertEqual(rc, 0)
        self.assertEqual(out, "FPTR DISPATCH OK\n")

if __name__ == "__main__":
    unittest.main()
