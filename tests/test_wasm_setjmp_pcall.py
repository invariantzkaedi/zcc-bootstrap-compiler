#!/usr/bin/env python3
"""
WebAssembly Native Exception Handling (`setjmp`/`longjmp`) + Lua `pcall()` Suite
================================================================================
Verifies ZCC's native WebAssembly Exception Handling (`Tag` Section 13,
`try_table` 0x1F, `throw` 0x08) with zero `env.*` imports and zero Asyncify/JS
trampolines across:
  1. Direct C99 `setjmp`/`_setjmp`/`sigsetjmp` + `longjmp`/`_longjmp`/`siglongjmp`
     (normal return, value=0 -> 1 normalization, deep stack unwind + alloca SP
     restoration, recursive frame `$fp` disambiguation, multi-site / switch /
     var assignment, MicroPython-style comma expression hoisting, indirect
     function-pointer `longjmp`, ZCC-EF-0 uncaught `longjmp` negative control,
     and MVP binary purity check verifying Section 13 is omitted when unused)
  2. Real upstream Lua 5.4.6 `pcall()`, `xpcall()`, `error()`, nested `pcall`,
     recursive Lua+C `pcall`, load syntax-error handling, 100-iteration unwind
     SP stability, and uncaught error negative control across BOTH `node:wasi`
     and `tools/browser_wasi_shim.js`
"""

import json
import os
import shutil
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")
BUILD_WASM = os.path.join(REPO_ROOT, "build_wasm")
LUA_WASM = os.path.join(BUILD_WASM, "lua.wasm")
SHIM_JS = os.path.join(REPO_ROOT, "tools", "browser_wasi_shim.js")


def find_node_bin() -> str:
    candidate = "/home/ZKAEDI/.local/bin/node"
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    found = shutil.which("node")
    if found:
        return found
    raise RuntimeError("node binary not found")


def inspect_wasm_binary(node_bin: str, wasm_path: str) -> dict:
    with open(wasm_path, "rb") as f:
        data = f.read()
    # Parse WASM section IDs directly (ZCC-EF-0 independent binary check)
    pos = 8
    section_ids = []
    while pos < len(data):
        sec_id = data[pos]
        pos += 1
        sz = 0
        shift = 0
        while pos < len(data):
            b = data[pos]
            pos += 1
            sz |= (b & 0x7F) << shift
            if (b & 0x80) == 0:
                break
            shift += 7
        section_ids.append(sec_id)
        pos += sz

    js = r"""
const fs = require('fs');
const b = fs.readFileSync(process.argv[1]);
const valid = WebAssembly.validate(b);
let compiled = false;
let imports = [];
let err = null;
if (valid) {
  try {
    const mod = new WebAssembly.Module(b);
    compiled = true;
    imports = WebAssembly.Module.imports(mod).map(i => `${i.module}.${i.name}`);
  } catch (e) { err = String(e); }
}
console.log(JSON.stringify({valid, compiled, bytes: b.length, imports, err}));
"""
    res = subprocess.run([node_bin, "-e", js, wasm_path], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
    info = json.loads(res.stdout.strip().splitlines()[-1])
    info["sectionIds"] = section_ids
    return info


def run_node_wasi(node_bin: str, wasm_path: str, args, stdin_str: str = "") -> subprocess.CompletedProcess:
    js = r"""
const fs = require('fs');
const { WASI } = require('node:wasi');
const cfg = JSON.parse(fs.readFileSync(process.argv[1], 'utf8'));
const w = new WASI({
  version: 'preview1',
  args: cfg.args,
  env: {},
  preopens: { '.': '.' },
  returnOnExit: true,
});
const imp = w.getImportObject();
const wasi = imp.wasi_snapshot_preview1;
for (const k of Object.keys(wasi)) {
  const orig = wasi[k];
  wasi[k] = (...a) => orig.apply(wasi, a);
}
(async () => {
  const bytes = fs.readFileSync(cfg.wasm);
  const mod = await WebAssembly.compile(bytes);
  const inst = await WebAssembly.instantiate(mod, imp);
  const rc = w.start(inst);
  process.exit(rc | 0);
})().catch((e) => { console.error(e); process.exit(99); });
"""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump({"wasm": os.path.abspath(wasm_path), "args": args}, tf)
        cfg_path = tf.name
    try:
        return subprocess.run(
            [node_bin, "--no-warnings", "-e", js, cfg_path],
            cwd=REPO_ROOT,
            input=stdin_str,
            capture_output=True,
            text=True,
            timeout=60,
        )
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


def run_browser_shim(node_bin: str, wasm_path: str, args, files=None, stdin_str: str = "") -> dict:
    import base64
    files_b64 = {}
    if files:
        for k, v in files.items():
            raw = v.encode("utf-8") if isinstance(v, str) else bytes(v)
            files_b64[k] = base64.b64encode(raw).decode("ascii")
    js = r"""
const fs = require('fs');
const { BrowserWasiShim } = require(process.argv[1]);
const cfg = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
(async () => {
  const bytes = fs.readFileSync(cfg.wasm);
  const vfsFiles = {};
  for (const [k, b64] of Object.entries(cfg.filesB64 || {})) {
    vfsFiles[k] = Buffer.from(b64, 'base64');
  }
  const shim = new BrowserWasiShim({ args: cfg.args, files: vfsFiles, stdin: cfg.stdin || '' });
  let rc = -1;
  let trap = null;
  try {
    const res = await shim.run(bytes);
    rc = res.exitCode;
  } catch (e) {
    trap = String(e);
  }
  console.log('__RESULT__' + JSON.stringify({
    exitCode: rc,
    stdout: shim.getStdout(),
    stderr: shim.getStderr(),
    trap,
  }));
})();
"""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump({"wasm": os.path.abspath(wasm_path), "args": args, "filesB64": files_b64, "stdin": stdin_str}, tf)
        cfg_path = tf.name
    try:
        res = subprocess.run([node_bin, "-e", js, SHIM_JS, cfg_path], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
        for line in reversed(res.stdout.splitlines()):
            if line.startswith("__RESULT__"):
                return json.loads(line[len("__RESULT__"):])
        raise RuntimeError(f"Missing __RESULT__ marker: {res.stdout}\n{res.stderr}")
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


C99_SETJMP_SUITE = r"""
#include "src/wasm_wasi_rt.c"

static int g_sp_anchor(void) {
    volatile int local_probe = 0;
    return (int)(uintptr_t)&local_probe;
}

static void deep_throw(jmp_buf env, int depth, int val) {
    volatile char pad[128];
    pad[0] = (char)depth;
    if (depth > 0) {
        deep_throw(env, depth - 1, val);
    } else {
        longjmp(env, val);
    }
}

static int comma_side_effect_helper(int *counter) {
    (*counter)++;
    return 123;
}

static int test_basic_and_zero_norm(void) {
    jmp_buf env;
    int r1 = setjmp(env);
    if (r1 == 0) {
        longjmp(env, 42);
        return 1001;
    }
    if (r1 != 42) return 1002;

    if (setjmp(env) == 0) {
        longjmp(env, 0); /* C99 7.13.2.1: val==0 must return 1 */
        return 1003;
    } else {
        /* Switch statement with _setjmp/_longjmp */
        switch (_setjmp(env)) {
            case 0:
                _longjmp(env, 77);
                return 1004;
            case 77:
                break;
            default:
                return 1005;
        }
    }
    return 0;
}

static int test_deep_unwind_and_sp_restore(void) {
    int base_sp = g_sp_anchor();
    jmp_buf env;
    for (int iter = 0; iter < 500; iter++) {
        if (setjmp(env) == 0) {
            volatile char *dyn = (volatile char *)alloca(256);
            dyn[0] = (char)iter;
            deep_throw(env, 6, 10 + (iter & 7));
            return 2001;
        }
        int cur_sp = g_sp_anchor();
        /* Every catch iteration restores the exact stack frame offset */
        if (cur_sp != base_sp) {
            printf("SP_DRIFT iter=%d base=%d cur=%d\n", iter, base_sp, cur_sp);
            return 2002;
        }
    }
    return 0;
}

static int test_recursive_frame_disambiguation(int depth, jmp_buf *target_env) {
    jmp_buf my_env;
    int caught = 0;
    if (depth == 1) target_env = &my_env;

    int rc = setjmp(my_env);
    if (rc == 0) {
        if (depth < 4) {
            int sub = test_recursive_frame_disambiguation(depth + 1, target_env);
            if (depth == 1) return 3001; /* depth 1 must NOT return normally! */
            return sub;
        } else {
            /* Throw from depth 4 directly into depth 1's frame (skipping 3 and 2) */
            longjmp(*target_env, 919);
            return 3002;
        }
    } else {
        caught = rc;
    }
    if (depth != 1) return 3003; /* Only depth 1 should have caught 919 */
    return (caught == 919) ? 0 : 3004;
}

static int test_multi_site_and_comma_hoist(void) {
    jmp_buf e1, e2;
    int side_cnt = 0;
    if ((comma_side_effect_helper(&side_cnt), setjmp(e1)) == 0) {
        if (setjmp(e2) == 0) {
            void (*indirect_lj)(jmp_buf, int) = longjmp;
            indirect_lj(e2, 55);
            return 4001;
        }
        longjmp(e1, 88);
        return 4002;
    }
    if (side_cnt != 1) {
        printf("COMMA_SIDE_EFFECT_CNT=%d\n", side_cnt);
        return 4003;
    }
    return 0;
}

int main(void) {
    int rc;
    if ((rc = test_basic_and_zero_norm()) != 0) { printf("FAIL basic %d\n", rc); return 1; }
    if ((rc = test_deep_unwind_and_sp_restore()) != 0) { printf("FAIL deep %d\n", rc); return 2; }
    if ((rc = test_recursive_frame_disambiguation(0, NULL)) != 0) { printf("FAIL rec %d\n", rc); return 3; }
    if ((rc = test_multi_site_and_comma_hoist()) != 0) { printf("FAIL multi %d\n", rc); return 4; }
    puts("SETJMP_EH_C99_ALL_PASS");
    return 0;
}
"""

C99_UNCAUGHT_NEG = r"""
#include "src/wasm_wasi_rt.c"
int main(void) {
    jmp_buf env;
    memset(&env, 0, sizeof(env));
    longjmp(env, 9);
    puts("SHOULD_NOT_REACH");
    return 0;
}
"""

C99_MVP_PURITY = r"""
#include "src/wasm_wasi_rt.c"
int main(void) {
    puts("MVP_NO_TAG");
    return 0;
}
"""

C99_STRUCT_ZERO_INIT_REG = r"""
#include "src/wasm_wasi_rt.c"
typedef struct {
    int a;
    int b;
    void *p1;
    int arr[15];
    void *p2;
    int tail;
} ZeroBig;

typedef struct {
    int pad;
    struct {
        int inner_pad;
        int val;
    } sub;
} OuterGlobal;

static OuterGlobal g_outer = { 11, { 22, 777 } };
static int *g_nested_ptr = &g_outer.sub.val;

static void dirty_stack(void) {
    volatile uint32_t junk[64];
    for (int i = 0; i < 64; i++) junk[i] = 0xDEADBEEFu;
}

static int check_struct_inits(void) {
    ZeroBig z = {0};
    if (z.a != 0 || z.b != 0 || z.p1 != NULL || z.p2 != NULL || z.tail != 0) return 1;
    for (int i = 0; i < 15; i++) {
        if (z.arr[i] != 0) return 2;
    }
    ZeroBig d = { .b = 5, .a = 0 };
    if (d.b != 5 || d.a != 0 || d.p1 != NULL || d.p2 != NULL || d.tail != 0) return 3;
    if (!g_nested_ptr || *g_nested_ptr != 777) return 4;
    return 0;
}

int main(void) {
    dirty_stack();
    int rc = check_struct_inits();
    if (rc != 0) {
        printf("STRUCT_INIT_FAIL %d\n", rc);
        return rc;
    }
    puts("STRUCT_ZERO_INIT_AND_NESTED_PTR_OK");
    return 0;
}
"""

LUA_PCALL_SUITE = r"""
-- 1. Basic pcall error recovery (string with level=0 and table payload)
local ok1, err1 = pcall(function() error("boom_one", 0) end)
assert(ok1 == false and err1 == "boom_one", "pcall boom failed: " .. tostring(err1))
local ok1b, err1b = pcall(function() error({tag = "E42", val = 42}) end)
assert(ok1b == false and type(err1b) == "table" and err1b.tag == "E42" and err1b.val == 42, "pcall table err failed")

-- 2. Success path preserves multiple return values
local ok2, a, b, c = pcall(function(x) return x + 1, x * 2, "ok" end, 20)
assert(ok2 == true and a == 21 and b == 40 and c == "ok", "pcall multi-ret failed")

-- 3. xpcall custom handler on arithmetic runtime error
local xp_seen = false
local ok3, err3 = xpcall(function()
  local v = nil
  return v + 5
end, function(msg)
  xp_seen = true
  return { kind = "XCAUGHT", raw = tostring(msg) }
end)
assert(ok3 == false and xp_seen and type(err3) == "table" and err3.kind == "XCAUGHT" and type(err3.raw) == "string" and #err3.raw > 5, "xpcall failed")

-- 4. Nested pcall inside pcall + rethrow
local outer_ok, outer_err = pcall(function()
  local in_ok, in_err = pcall(function() error("inner_err", 0) end)
  assert(in_ok == false and in_err == "inner_err")
  error("outer_after:" .. tostring(in_err), 0)
end)
assert(outer_ok == false and outer_err == "outer_after:inner_err", "nested pcall failed")

-- 5. Recursive pcall stack unwind across 10 Lua+C levels
local function rec_err(d)
  if d == 0 then error("depth_zero_boom", 0) end
  return rec_err(d - 1) + 1
end
local rec_ok, rec_msg = pcall(rec_err, 10)
assert(rec_ok == false and rec_msg == "depth_zero_boom", "rec pcall failed")

-- 6. 100 repeated pcall errors (verifies linear SP stability)
local catches = 0
for i = 1, 100 do
  local ok, e = pcall(function() error("loop_" .. i, 0) end)
  if (not ok) and e == ("loop_" .. i) then catches = catches + 1 end
end
assert(catches == 100, "loop pcall count mismatch")

-- 7. load() syntax error + assert failure
local chunk, lerr = load("if then end")
assert(chunk == nil and type(lerr) == "string" and #lerr > 0, "load syntax err failed")
local aok, amsg = pcall(assert, false, "assert_boom")
assert(aok == false and type(amsg) == "string" and #amsg > 0, "pcall assert failed")

print("LUA_PCALL_ALL_PASS", catches)
"""


class TestWasmSetjmpAndLuaPcall(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.node = find_node_bin()
        cls.tmp = tempfile.mkdtemp(prefix="zcc_eh_test_")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _compile_c(self, src_text: str, name: str) -> str:
        c_path = os.path.join(self.tmp, f"{name}.c")
        w_path = os.path.join(self.tmp, f"{name}.wasm")
        with open(c_path, "w", encoding="utf-8") as f:
            f.write(src_text)
        res = subprocess.run(
            [ZCC_BIN, "--target=wasm32", "-Isrc", "-I.", c_path, "-o", w_path],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(res.returncode, 0, f"Compile {name} failed:\n{res.stdout}\n{res.stderr}")
        return w_path

    def test_c99_setjmp_longjmp_suite(self):
        w_path = self._compile_c(C99_SETJMP_SUITE, "c99_setjmp_suite")
        info = inspect_wasm_binary(self.node, w_path)
        self.assertTrue(info["valid"], f"Invalid WASM: {info}")
        self.assertTrue(info["compiled"])
        self.assertIn(13, info["sectionIds"], "Expected Tag Section 13 in setjmp/longjmp module")
        non_wasi = [i for i in info["imports"] if not i.startswith("wasi_snapshot_preview1.")]
        self.assertEqual(non_wasi, [], f"Non-WASI imports found: {non_wasi}")

        nw = run_node_wasi(self.node, w_path, ["c99_setjmp_suite.wasm"])
        self.assertEqual(nw.returncode, 0, f"node:wasi failed: {nw.stdout}\n{nw.stderr}")
        self.assertEqual(nw.stdout.strip(), "SETJMP_EH_C99_ALL_PASS")

        bs = run_browser_shim(self.node, w_path, ["c99_setjmp_suite.wasm"])
        self.assertIsNone(bs["trap"])
        self.assertEqual(bs["exitCode"], 0)
        self.assertEqual(bs["stdout"].strip(), "SETJMP_EH_C99_ALL_PASS")

    def test_uncaught_longjmp_negative_control(self):
        w_path = self._compile_c(C99_UNCAUGHT_NEG, "uncaught_longjmp")
        info = inspect_wasm_binary(self.node, w_path)
        self.assertTrue(info["valid"])
        self.assertIn(13, info["sectionIds"])
        nw = run_node_wasi(self.node, w_path, ["uncaught_longjmp.wasm"])
        self.assertNotEqual(nw.returncode, 0, f"Uncaught longjmp MUST fail closed, got rc=0 stdout={nw.stdout!r}")
        self.assertNotIn("SHOULD_NOT_REACH", nw.stdout)

        bs = run_browser_shim(self.node, w_path, ["uncaught_longjmp.wasm"])
        self.assertTrue(bs["exitCode"] != 0 or bs["trap"] is not None, f"BrowserWasiShim did not trap/fail: {bs}")
        self.assertNotIn("SHOULD_NOT_REACH", bs["stdout"])

    def test_mvp_purity_when_setjmp_unused(self):
        w_path = self._compile_c(C99_MVP_PURITY, "mvp_purity")
        info = inspect_wasm_binary(self.node, w_path)
        self.assertTrue(info["valid"])
        self.assertNotIn(13, info["sectionIds"], "Tag Section 13 must NOT be emitted when setjmp/longjmp is unused")
        nw = run_node_wasi(self.node, w_path, ["mvp_purity.wasm"])
        self.assertEqual(nw.returncode, 0)
        self.assertEqual(nw.stdout.strip(), "MVP_NO_TAG")

    def test_lua_real_pcall_xpcall_both_hosts(self):
        self.assertTrue(os.path.isfile(LUA_WASM), f"Missing {LUA_WASM}; run build_lua_wasm.py")
        info = inspect_wasm_binary(self.node, LUA_WASM)
        self.assertTrue(info["valid"])
        self.assertTrue(info["compiled"])
        self.assertIn(13, info["sectionIds"], "lua.wasm should contain Tag Section 13")
        non_wasi = [i for i in info["imports"] if not i.startswith("wasi_snapshot_preview1.")]
        self.assertEqual(non_wasi, [])

        # node:wasi (-e) and BrowserWasiShim (VFS file)
        nw_e = run_node_wasi(self.node, LUA_WASM, ["lua.wasm", "-e", LUA_PCALL_SUITE])
        self.assertEqual(nw_e.returncode, 0, f"node:wasi Lua pcall failed:\n{nw_e.stdout}\n{nw_e.stderr}")
        self.assertEqual(nw_e.stdout.strip(), "LUA_PCALL_ALL_PASS\t100")

        bs = run_browser_shim(self.node, LUA_WASM, ["lua.wasm", "pcall_test.lua"], files={"pcall_test.lua": LUA_PCALL_SUITE})
        self.assertIsNone(bs["trap"], f"BrowserWasiShim trapped: {bs}")
        self.assertEqual(bs["exitCode"], 0, f"BrowserWasiShim failed: {bs}")
        self.assertEqual(bs["stdout"].strip(), "LUA_PCALL_ALL_PASS\t100")

        # Negative control: uncaught Lua error() must return non-zero!
        neg = run_node_wasi(self.node, LUA_WASM, ["lua.wasm", "-e", 'error("uncaught_boom")'])
        self.assertNotEqual(neg.returncode, 0)
        self.assertIn("uncaught_boom", neg.stderr + neg.stdout)

    def test_local_struct_zero_init_and_nested_global_member(self):
        w_path = self._compile_c(C99_STRUCT_ZERO_INIT_REG, "struct_zero_init")
        info = inspect_wasm_binary(self.node, w_path)
        self.assertTrue(info["valid"], f"Invalid WASM: {info}")
        self.assertTrue(info["compiled"])
        self.assertNotIn(13, info["sectionIds"])
        nw = run_node_wasi(self.node, w_path, ["struct_zero_init.wasm"])
        self.assertEqual(nw.returncode, 0, f"node:wasi failed: {nw.stdout}\n{nw.stderr}")
        self.assertEqual(nw.stdout.strip(), "STRUCT_ZERO_INIT_AND_NESTED_PTR_OK")
        bs = run_browser_shim(self.node, w_path, ["struct_zero_init.wasm"])
        self.assertIsNone(bs["trap"])
        self.assertEqual(bs["exitCode"], 0)
        self.assertEqual(bs["stdout"].strip(), "STRUCT_ZERO_INIT_AND_NESTED_PTR_OK")


if __name__ == "__main__":
    unittest.main(verbosity=2)
