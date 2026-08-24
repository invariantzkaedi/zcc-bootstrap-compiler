import os
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")
NODE_BIN = "/usr/bin/node"

def run_wasm_code(c_source: str, entry_fn: str = "main") -> int:
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

// 1. Independent V8 WebAssembly Validation
const isValid = WebAssembly.validate(wasmBuffer);
if (!isValid) {{
    console.error("WASM Binary Validation Failed!");
    process.exit(2);
}}

// 2. Instantiate and Execute
WebAssembly.instantiate(wasmBuffer, {{}}).then(mod => {{
    const result = mod.instance.exports.{entry_fn}();
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

class TestWASMFloatingPoint(unittest.TestCase):

    # -------------------------------------------------------------
    # 1. Float function parameters and return values (f32)
    # -------------------------------------------------------------
    def test_01_f32_params_and_return(self):
        code = """
float add(float a, float b) {
    return a + b;
}

int main(void) {
    float x = add(1.5f, 2.25f);
    return x == 3.75f ? 0 : 1;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 2. Double function parameters and return values (f64)
    # -------------------------------------------------------------
    def test_02_f64_params_and_return(self):
        code = """
double mul(double a, double b) {
    return a * b;
}

int main(void) {
    double x = mul(1.5, 4.0);
    return x == 6.0 ? 0 : 1;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 3. Float array load/store
    # -------------------------------------------------------------
    def test_03_float_array_load_store(self):
        code = """
int main(void) {
    float a[3];
    a[1] = 7.5f;
    return a[1] == 7.5f ? 0 : 1;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 4. Double pointer & compound assignment (*p += 0.75)
    # -------------------------------------------------------------
    def test_04_double_pointer_compound_assign(self):
        code = """
int main(void) {
    double x = 9.25;
    double *p = &x;
    *p += 0.75;
    return x == 10.0 ? 0 : 1;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 5. Mixed arithmetic conversions (int + double -> double)
    # -------------------------------------------------------------
    def test_05_mixed_arithmetic_int_double(self):
        code = """
int main(void) {
    int i = 2;
    double d = 0.5;
    double r = i + d;
    return r == 2.5 ? 0 : 1;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    def test_06_mixed_arithmetic_int_float(self):
        code = """
int main(void) {
    int i = 5;
    float f = 2.5f;
    float r = i * f;
    return r == 12.5f ? 0 : 1;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    def test_07_mixed_arithmetic_float_double(self):
        code = """
int main(void) {
    float f = 1.25f;
    double d = 2.75;
    double r = f + d;
    return r == 4.0 ? 0 : 1;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 6. Struct with floats & mixed structs
    # -------------------------------------------------------------
    def test_08_struct_with_floats(self):
        code = """
struct Vec {
    float x;
    float y;
};

int main(void) {
    struct Vec v;
    v.x = 1.25f;
    v.y = 2.75f;
    return v.x + v.y == 4.0f ? 0 : 1;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    def test_09_struct_mixed_int_double(self):
        code = """
struct Item {
    int id;
    float weight;
    double price;
};

int main(void) {
    struct Item item;
    item.id = 101;
    item.weight = 3.5f;
    item.price = 19.5;
    double total = item.weight * item.price;
    return total == 68.25 ? 0 : 1;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 7. Comparisons and boolean results
    # -------------------------------------------------------------
    def test_10_float_comparisons_matrix(self):
        code = """
int main(void) {
    float a = 3.5f;
    float b = 7.0f;
    if (!(a < b)) return 1;
    if (!(a <= b)) return 2;
    if (!(b > a)) return 3;
    if (!(b >= a)) return 4;
    if (!(a != b)) return 5;
    if (a == b) return 6;
    return 0;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    def test_11_double_comparisons_matrix(self):
        code = """
int main(void) {
    double a = -10.5;
    double b = 10.5;
    if (!(a < b)) return 1;
    if (!(b > a)) return 2;
    if (a == b) return 3;
    if (!(a != b)) return 4;
    return 0;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 8. Explicit Casts and Conversions
    # -------------------------------------------------------------
    def test_12_explicit_casts(self):
        code = """
int main(void) {
    int i = (int)3.75f;
    float f = (float)10;
    double d = (double)42;
    float demoted = (float)d;
    double promoted = (double)f;
    if (i != 3) return 1;
    if (f != 10.0f) return 2;
    if (d != 42.0) return 3;
    if (demoted != 42.0f) return 4;
    if (promoted != 10.0) return 5;
    return 0;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 9. Compound assignments
    # -------------------------------------------------------------
    def test_13_compound_assignments_float_double(self):
        code = """
int main(void) {
    float f = 10.0f;
    f += 5.5f;
    f -= 2.5f;
    f *= 2.0f;
    f /= 13.0f;

    double d = 100.0;
    d += 50.0;
    d -= 25.0;
    d *= 2.0;
    d /= 25.0;

    if (f != 2.0f) return 1;
    if (d != 10.0) return 2;
    return 0;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 10. Unary Negation
    # -------------------------------------------------------------
    def test_14_unary_negation(self):
        code = """
int main(void) {
    float a = 15.5f;
    float neg_a = -a;
    double b = -42.25;
    double pos_b = -b;

    if (neg_a != -15.5f) return 1;
    if (pos_b != 42.25) return 2;
    return 0;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 11. Edge values: Signed zero & precision
    # -------------------------------------------------------------
    def test_15_signed_zero_and_precision(self):
        code = """
int main(void) {
    float pz = 0.0f;
    float nz = -0.0f;
    double eps = 1e-10;
    if (pz != nz) return 1;
    if (eps <= 0.0) return 2;
    return 0;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 12. NaN generation and non-equality
    # -------------------------------------------------------------
    def test_16_nan_properties(self):
        code = """
int main(void) {
    float zero = 0.0f;
    float nan_val = zero / zero;
    /* In IEEE-754 / WASM, NaN != NaN is always true, and NaN == NaN is false */
    if (nan_val == nan_val) return 1;
    if (!(nan_val != nan_val)) return 2;
    return 0;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 13. Infinity generation and comparisons
    # -------------------------------------------------------------
    def test_17_infinity_properties(self):
        code = """
int main(void) {
    float zero = 0.0f;
    float pos_inf = 1.0f / zero;
    float neg_inf = -1.0f / zero;
    if (!(pos_inf > 1000000.0f)) return 1;
    if (!(neg_inf < -1000000.0f)) return 2;
    if (pos_inf <= neg_inf) return 3;
    return 0;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 14. Mixed multi-parameter functions
    # -------------------------------------------------------------
    def test_18_mixed_param_function(self):
        code = """
double calculate(int count, float rate, double base) {
    return (double)count * (double)rate + base;
}

int main(void) {
    double res = calculate(4, 2.5f, 10.0);
    return res == 20.0 ? 0 : 1;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 15. Array of structs
    # -------------------------------------------------------------
    def test_19_array_of_structs(self):
        code = """
struct Point {
    float x;
    float y;
};

int main(void) {
    struct Point pts[3];
    pts[0].x = 1.0f;
    pts[0].y = 2.0f;
    pts[1].x = 3.0f;
    pts[1].y = 4.0f;
    pts[2].x = 5.0f;
    pts[2].y = 6.0f;

    float sum_x = pts[0].x + pts[1].x + pts[2].x;
    float sum_y = pts[0].y + pts[1].y + pts[2].y;

    if (sum_x != 9.0f) return 1;
    if (sum_y != 12.0f) return 2;
    return 0;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

    # -------------------------------------------------------------
    # 16. Nested expressions and operator precedence
    # -------------------------------------------------------------
    def test_20_nested_expressions_precedence(self):
        code = """
int main(void) {
    double a = 2.0;
    double b = 3.0;
    double c = 4.0;
    double d = 5.0;
    double e = 10.0;

    double res = (a + b) * (d - c) + e / a;
    /* (2 + 3) * (5 - 4) + 10 / 2 = 5 * 1 + 5 = 10 */
    return res == 10.0 ? 0 : 1;
}
"""
        self.assertEqual(run_wasm_code(code), 0)

if __name__ == "__main__":
    unittest.main()
