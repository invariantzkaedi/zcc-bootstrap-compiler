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
        # Compile C source to WASM using ZCC
        comp = subprocess.run(
            [ZCC_BIN, "--target=wasm32", c_path, "-o", wasm_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        if comp.returncode != 0:
            raise RuntimeError(f"ZCC compilation failed (rc={comp.returncode}):\n{comp.stderr}\n{comp.stdout}")

        # Node.js runner to execute the WASM module
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

class TestWASMMemorySemantics(unittest.TestCase):

    def test_01_pointer_address_and_deref(self):
        code = """
int main() {
    int x;
    int *p;
    x = 7;
    p = &x;
    return *p;
}
"""
        self.assertEqual(run_wasm_code(code), 7)

    def test_02_pointer_store_and_readback(self):
        code = """
int main() {
    int x;
    int *p;
    x = 7;
    p = &x;
    *p = 19;
    return x;
}
"""
        self.assertEqual(run_wasm_code(code), 19)

    def test_03_array_indexing(self):
        code = """
int main() {
    int a[4];
    a[0] = 11;
    a[1] = 22;
    a[2] = 33;
    a[3] = 44;
    return a[2];
}
"""
        self.assertEqual(run_wasm_code(code), 33)

    def test_04_signed_char_sign_extension(self):
        code = """
int main() {
    char c;
    c = -1;
    return c;
}
"""
        self.assertEqual(run_wasm_code(code), -1)

    def test_05_unsigned_char_zero_extension(self):
        code = """
int main() {
    unsigned char c;
    c = 255;
    return c;
}
"""
        self.assertEqual(run_wasm_code(code), 255)

    def test_06_signed_short_sign_extension(self):
        code = """
int main() {
    short s;
    s = -1000;
    return s;
}
"""
        self.assertEqual(run_wasm_code(code), -1000)

    def test_07_unsigned_short_zero_extension(self):
        code = """
int main() {
    unsigned short s;
    s = 60000;
    return s;
}
"""
        self.assertEqual(run_wasm_code(code), 60000)

    def test_08_struct_member_direct_access(self):
        code = """
struct P {
    int x;
    int y;
};
int main() {
    struct P p;
    p.x = 10;
    p.y = 42;
    return p.y;
}
"""
        self.assertEqual(run_wasm_code(code), 42)

    def test_09_struct_pointer_member_access(self):
        code = """
struct P {
    int x;
    int y;
};
int main() {
    struct P p;
    struct P *ptr;
    ptr = &p;
    ptr->x = 12;
    ptr->y = 99;
    return ptr->y;
}
"""
        self.assertEqual(run_wasm_code(code), 99)

    def test_10_pointer_arithmetic(self):
        code = """
int main() {
    int a[4];
    int *p;
    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    a[3] = 40;
    p = a;
    *(p + 3) = 88;
    return a[3];
}
"""
        self.assertEqual(run_wasm_code(code), 88)

    def test_11_compound_assign_on_deref(self):
        code = """
int main() {
    int x;
    int *p;
    x = 10;
    p = &x;
    *p += 5;
    return x;
}
"""
        self.assertEqual(run_wasm_code(code), 15)

    def test_12_compound_assign_on_array(self):
        code = """
int main() {
    int a[3];
    a[1] = 20;
    a[1] *= 3;
    return a[1];
}
"""
        self.assertEqual(run_wasm_code(code), 60)

    def test_13_global_variable_load_store(self):
        code = """
int g = 40;
int main() {
    g += 2;
    return g;
}
"""
        self.assertEqual(run_wasm_code(code), 42)

if __name__ == "__main__":
    unittest.main()
