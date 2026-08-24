import os
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")
NODE_BIN = "/usr/bin/node"

def run_wasm_code(c_source: str) -> int:
    with tempfile.NamedTemporaryFile(suffix=".c", mode="w", delete=False) as f_c:
        f_c.write(c_source)
        c_path = f_c.name
    wasm_path = c_path + ".wasm"
    runner_js = c_path + "_runner.js"

    try:
        comp = subprocess.run(
            [ZCC_BIN, "--target=wasm32", c_path, "-o", wasm_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        if comp.returncode != 0:
            raise RuntimeError(f"ZCC compilation failed (rc={comp.returncode}):\n{comp.stderr}\n{comp.stdout}")

        js_code = f"""
const fs = require('fs');
const wasmBuffer = fs.readFileSync('{wasm_path}');
WebAssembly.instantiate(wasmBuffer, {{}}).then(mod => {{
    const result = mod.instance.exports.main();
    console.log(result);
}}).catch(err => {{
    console.error(err);
    process.exit(1);
}});
"""
        with open(runner_js, "w") as f_js:
            f_js.write(js_code)

        run = subprocess.run(
            [NODE_BIN, runner_js],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        if run.returncode != 0:
            raise RuntimeError(f"Node WASM execution failed (rc={run.returncode}):\n{run.stderr}\n{run.stdout}")

        return int(run.stdout.strip())
    finally:
        for p in (c_path, wasm_path, runner_js):
            if os.path.exists(p):
                os.remove(p)

class TestWASMAdvancedCapabilities(unittest.TestCase):

    # -------------------------------------------------------------
    # 1. Dynamic / Heap Allocator (malloc / free)
    # -------------------------------------------------------------
    def test_01_malloc_store_and_read(self):
        code = """
void *malloc(int size);
void free(void *ptr);

int main() {
    int *p;
    p = (int *)malloc(16);
    p[0] = 100;
    p[1] = 200;
    p[2] = 300;
    p[3] = 400;
    int sum;
    sum = p[0] + p[1] + p[2] + p[3];
    free(p);
    return sum;
}
"""
        self.assertEqual(run_wasm_code(code), 1000)

    def test_02_multiple_malloc_allocations(self):
        code = """
void *malloc(int size);
void free(void *ptr);

int main() {
    int *a;
    int *b;
    a = (int *)malloc(32);
    b = (int *)malloc(32);
    a[0] = 42;
    b[0] = 58;
    int res;
    res = a[0] + b[0];
    free(b);
    free(a);
    return res;
}
"""
        self.assertEqual(run_wasm_code(code), 100)

    # -------------------------------------------------------------
    # 2. Float / Double Linear Memory Operations & Arithmetic
    # -------------------------------------------------------------
    def test_03_float_arithmetic_and_cast(self):
        code = """
int main() {
    float f;
    f = 3.5f;
    f = f * 4.0f;
    return (int)f;
}
"""
        self.assertEqual(run_wasm_code(code), 14)

    def test_04_double_arithmetic_and_cast(self):
        code = """
int main() {
    double d;
    d = 10.25;
    d = d * 4.0;
    return (int)d;
}
"""
        self.assertEqual(run_wasm_code(code), 41)

    def test_05_float_array_load_store(self):
        code = """
int main() {
    float a[3];
    a[0] = 1.5f;
    a[1] = 2.5f;
    a[2] = 3.5f;
    float sum;
    sum = a[0] + a[1] + a[2];
    return (int)sum;
}
"""
        self.assertEqual(run_wasm_code(code), 7)

    def test_06_struct_with_floats(self):
        code = """
struct Vec2 {
    float x;
    float y;
};
int main() {
    struct Vec2 v;
    v.x = 12.5f;
    v.y = 27.5f;
    float res;
    res = v.x + v.y;
    return (int)res;
}
"""
        self.assertEqual(run_wasm_code(code), 40)

    def test_07_float_comparisons(self):
        code = """
int main() {
    float a;
    float b;
    a = 5.5f;
    b = 10.5f;
    if (a < b && b > 8.0f) {
        return 1;
    }
    return 0;
}
"""
        self.assertEqual(run_wasm_code(code), 1)

    # -------------------------------------------------------------
    # 3. Indirect Calls (call_indirect) & Function Pointers
    # -------------------------------------------------------------
    def test_08_function_pointer_simple(self):
        code = """
int add5(int x) {
    return x + 5;
}

int main() {
    int (*fn)(int);
    fn = add5;
    return fn(20);
}
"""
        self.assertEqual(run_wasm_code(code), 25)

    def test_09_function_pointer_selector(self):
        code = """
int mul2(int x) { return x * 2; }
int mul3(int x) { return x * 3; }

int apply(int (*f)(int), int val) {
    return f(val);
}

int main() {
    int a;
    int b;
    a = apply(mul2, 10);
    b = apply(mul3, 10);
    return a + b;
}
"""
        self.assertEqual(run_wasm_code(code), 50)

    def test_10_function_pointer_in_struct(self):
        code = """
int double_val(int x) { return x * 2; }

struct Op {
    int (*execute)(int);
    int value;
};

int main() {
    struct Op op;
    op.execute = double_val;
    op.value = 21;
    return op.execute(op.value);
}
"""
        self.assertEqual(run_wasm_code(code), 42)

if __name__ == "__main__":
    unittest.main()
