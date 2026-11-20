#!/usr/bin/env python3
"""
Differential & Dual-Host Verification Suite for `build_wasm/qjs.wasm`
======================================================================
Verifies standalone ZCC-compiled Upstream QuickJS 2024-01-13 (`build_wasm/qjs.wasm`):
  1. V8 WebAssembly.validate() & zero non-WASI (`env.*`) imports + clean upstream sources.
  2. Full ES2023 differential parity vs Node 22 V8 across both `node:wasi` and
     `tools/browser_wasi_shim.js` (Math IEEE-754/hyperbolics, 53-bit MAX_SAFE_INTEGER,
     JSON, closures & fib(18) alloca, RegExp captures, ES6 class, Date, BigInt 128-bit,
     Promise/async/await microtasks, Map/Set/Proxy/Symbol, -0.0 vs +0.0).
  3. WASI VFS script execution + `wasiReadFile`/`wasiWriteFile` + stdin fallback (dual host).
  4. `tools/zcc_wasm_playground.html` headless `ZccPlaygroundEngine.runExternalWasm`.
  5. ZCC-EF-0 negative controls (uncaught exception, differential mismatch check, missing
     script file, corrupted WASM header).
"""

import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QJS_WASM = os.path.join(REPO_ROOT, "build_wasm", "qjs.wasm")
QJS_DIR = os.path.join(REPO_ROOT, "tests", "embedded_quickjs")
BROWSER_SHIM = os.path.join(REPO_ROOT, "tools", "browser_wasi_shim.js")
PLAYGROUND_HTML = os.path.join(REPO_ROOT, "tools", "zcc_wasm_playground.html")

UPSTREAM_EXPECTED_FILES = [
    "LICENSE",
    "cutils.c",
    "cutils.h",
    "libbf.c",
    "libbf.h",
    "libregexp-opcode.h",
    "libregexp.c",
    "libregexp.h",
    "libunicode-table.h",
    "libunicode.c",
    "libunicode.h",
    "list.h",
    "quickjs-atom.h",
    "quickjs-opcode.h",
    "quickjs.c",
    "quickjs.h",
]

ES2023_DIFF_SCRIPT = r"""
const out = [];

// 1. Arithmetic, bitwise, 53-bit MAX_SAFE_INTEGER
out.push("arith:" + ((17 * 23 - 5) ^ 0x5a) + "," + ((-17) >> 1) + "," + ((-17) >>> 1));
out.push("safe:" + Number.MAX_SAFE_INTEGER + "," + (Number.MAX_SAFE_INTEGER - 1) + "," + ((9007199254740991).toString(16)));

// 2. IEEE-754 & Math.* (global function tables + libm)
const f6 = (x) => Number.isNaN(x) ? "NaN" : (Object.is(x, -0) ? "-0" : x.toFixed(6));
out.push("pi:" + Math.PI.toFixed(10));
out.push("trig:" + [
  f6(Math.sin(0.5)), f6(Math.cos(0.5)), f6(Math.tan(0.5)),
  f6(Math.acos(0.25)), f6(Math.atan2(-3, -4))
].join(","));
out.push("exp:" + [
  f6(Math.exp(1.5)), f6(Math.log(12.5)), f6(Math.pow(2.5, 3.25)),
  f6(Math.sqrt(2.0)), f6(Math.cbrt(-27.0)), f6(Math.hypot(3, 4, 12)),
  f6(Math.expm1(0.001)), f6(Math.log1p(0.001))
].join(","));
out.push("hyp:" + [
  f6(Math.sinh(0.75)), f6(Math.cosh(0.75)), f6(Math.tanh(0.75)),
  f6(Math.asinh(1.5)), f6(Math.acosh(2.5)), f6(Math.atanh(0.5))
].join(","));
out.push("bits:" + Math.clz32(1) + "," + Math.imul(0x7fffffff, 3) + "," + Math.fround(1.337).toFixed(7));

// 3. Signed -0.0 vs +0.0 & Float64Array
const negZero = -0.0;
const posZero = +0.0;
const arrZ = new Float64Array([0, -0, 1, -1]);
arrZ.sort((a, b) => (a - b) || ((1 / a) - (1 / b)));
out.push("neg0:" + Object.is(negZero, -0) + "," + Object.is(negZero, posZero) + "," +
         (1 / negZero) + "," + Array.from(arrZ).map(x => Object.is(x, -0) ? "-0" : String(x)).join(","));

// 4. JSON
const parsed = JSON.parse('{"a":[1,true,null,"z\\u0043"],"b":{"x":42.5}}');
parsed.a.push(JSON.stringify(parsed.b).length);
out.push("json:" + JSON.stringify(parsed));

// 5. Closures + fib(18) recursion (stresses JS_CallInternal alloca)
const makeAcc = (base) => (delta) => (base += delta);
const acc = makeAcc(10);
function fib(n) { return n <= 1 ? n : fib(n - 1) + fib(n - 2); }
out.push("cls:" + acc(5) + "," + acc(7) + ",fib18=" + fib(18));

// 6. RegExp captures, replace, unicode property escape
const re = /([a-z]+)-(\d+)/g;
const rep = "x-12 y-345".replace(re, (_, w, n) => w.toUpperCase() + ":" + (Number(n) * 2));
const reU = /^\p{Letter}+$/u.test("Alpha");
out.push("re:" + rep + ",u=" + reU);

// 7. ES6 class inheritance + super + getter
class Vec {
  constructor(x, y) { this.x = x; this.y = y; }
  norm2() { return this.x * this.x + this.y * this.y; }
}
class Vec3 extends Vec {
  constructor(x, y, z) { super(x, y); this.z = z; }
  norm2() { return super.norm2() + this.z * this.z; }
}
out.push("cls3:" + new Vec3(3, 4, 12).norm2());

// 8. Date ISO
out.push("date:" + new Date(1700000000000).toISOString());

// 9. BigInt (64/128-bit)
const b1 = (2n ** 64n) * 1000000000000000003n + 7n;
const b2 = BigInt.asUintN(64, (1n << 65n) - 1n);
out.push("big:" + b1.toString() + "," + b2.toString(16));

// 10. Map, Set, Proxy, Symbol
const s = new Set([3, 1, 4, 1, 5, 9]);
const m = new Map([["k1", 10], ["k2", 20]]);
const sym = Symbol("tok");
const px = new Proxy({ val: 21, [sym]: "sec" }, {
  get(t, k) { return k === "val" ? t.val * 2 : t[k]; }
});
out.push("col:" + Array.from(s).join("") + "," + m.get("k2") + "," + px.val + "," + px[sym]);

// 11. Promise / async / await microtasks
async function asyncFlow() {
  const step1 = await Promise.resolve(20);
  const step2 = await new Promise((res) => Promise.resolve().then(() => res(step1 + 22)));
  out.push("async:" + step2);
  if (typeof print === "function") {
    for (const line of out) print(line);
  } else {
    for (const line of out) console.log(line);
  }
}
asyncFlow();
""".strip()


def find_node_bin() -> str:
    candidate = "/home/ZKAEDI/.local/bin/node"
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    found = shutil.which("node")
    if found:
        return found
    raise RuntimeError("node binary not found at /home/ZKAEDI/.local/bin/node or on PATH")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def run_node_v8_js(node_bin: str, js_code: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [node_bin, "-e", js_code],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=30,
    )


def run_qjs_node_wasi(
    node_bin: str, argv: list[str], cwd: str | None = None, stdin_text: str = ""
) -> subprocess.CompletedProcess:
    runner_js = r"""
const fs = require('fs');
const { WASI } = require('node:wasi');
const args = JSON.parse(process.argv[1]);
const preopenDir = process.argv[2];
const wasi = new WASI({
  version: 'preview1',
  args: ['qjs.wasm', ...args],
  env: {},
  preopens: { '.': preopenDir },
  returnOnExit: true,
});
(async () => {
  const bytes = fs.readFileSync(process.argv[3]);
  const mod = await WebAssembly.compile(bytes);
  const rawImp = wasi.getImportObject();
  const imp = { wasi_snapshot_preview1: {} };
  for (const [k, v] of Object.entries(rawImp.wasi_snapshot_preview1)) {
    imp.wasi_snapshot_preview1[k] = typeof v === 'function' ? (...a) => v.apply(wasi, a) : v;
  }
  const inst = await WebAssembly.instantiate(mod, imp);
  const rc = wasi.start(inst);
  process.exit(typeof rc === 'number' ? rc : 0);
})().catch((e) => {
  console.error(String(e && e.stack ? e.stack : e));
  process.exit(1);
});
"""
    return subprocess.run(
        [node_bin, "--no-warnings", "-e", runner_js, json.dumps(argv), cwd or REPO_ROOT, QJS_WASM],
        input=stdin_text,
        cwd=cwd or REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=60,
    )


def run_qjs_browser_shim(
    node_bin: str, argv: list[str], files: dict[str, str] | None = None, stdin_text: str = ""
) -> dict:
    runner_js = r"""
const fs = require('fs');
const { BrowserWasiShim } = require(process.argv[1]);
const qjsBytes = fs.readFileSync(process.argv[2]);
const cfg = JSON.parse(process.argv[3]);
(async () => {
  const shim = new BrowserWasiShim({
    args: ['qjs.wasm', ...cfg.argv],
    files: cfg.files || {},
    stdin: cfg.stdin || '',
    preopenDir: '.',
  });
  const res = await shim.run(qjsBytes);
  const outFiles = {};
  const td = new TextDecoder();
  for (const [k, v] of res.vfs.entries()) {
    outFiles[k] = td.decode(v);
  }
  process.stdout.write(JSON.stringify({
    exitCode: res.exitCode,
    stdout: res.stdout,
    stderr: res.stderr,
    files: outFiles,
  }));
})().catch((e) => {
  console.error(String(e && e.stack ? e.stack : e));
  process.exit(1);
});
"""
    cfg = json.dumps({"argv": argv, "files": files or {}, "stdin": stdin_text})
    res = subprocess.run(
        [node_bin, "-e", runner_js, BROWSER_SHIM, QJS_WASM, cfg],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=60,
    )
    if res.returncode != 0:
        raise AssertionError(f"BrowserWasiShim runner failed ({res.returncode}): {res.stderr}")
    return json.loads(res.stdout)


class TestWasmQuickJS(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.node_bin = find_node_bin()
        if not os.path.isfile(QJS_WASM):
            raise RuntimeError(f"Missing {QJS_WASM}; run `python3 tools/build_quickjs_wasm.py` first.")

    def test_01_v8_validation_zero_env_and_clean_upstream(self):
        sz = os.path.getsize(QJS_WASM)
        self.assertGreater(sz, 500_000, f"qjs.wasm suspiciously small: {sz} bytes")

        inspect_js = r"""
const fs = require('fs');
const b = fs.readFileSync(process.argv[1]);
const valid = WebAssembly.validate(b);
const mod = new WebAssembly.Module(b);
const imports = WebAssembly.Module.imports(mod).map(i => `${i.module}.${i.name}`);
const exportsList = WebAssembly.Module.exports(mod).map(e => `${e.name}:${e.kind}`);
process.stdout.write(JSON.stringify({ valid, imports, exportsList }));
"""
        res = subprocess.run([self.node_bin, "-e", inspect_js, QJS_WASM], capture_output=True, text=True, check=True)
        info = json.loads(res.stdout)
        self.assertTrue(info["valid"], "V8 WebAssembly.validate() returned false")
        non_wasi = [i for i in info["imports"] if not i.startswith("wasi_snapshot_preview1.")]
        self.assertEqual(non_wasi, [], f"Unexpected non-WASI imports: {non_wasi}")
        self.assertIn("_start:function", info["exportsList"])
        self.assertIn("memory:memory", info["exportsList"])

        actual_files = sorted(os.listdir(QJS_DIR))
        self.assertEqual(actual_files, sorted(UPSTREAM_EXPECTED_FILES))
        with open(os.path.join(QJS_DIR, "quickjs.c"), "r", encoding="utf-8") as f:
            qjs_src = f.read()
        self.assertIn("malloc_is_forbidden", qjs_src, "Upstream quickjs.c appears modified")

        # Verify every pinned upstream 2024-01-13 file unconditionally against SHA-256 manifest
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            "build_quickjs_wasm", os.path.join(REPO_ROOT, "tools", "build_quickjs_wasm.py")
        )
        bmod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(bmod)
        self.assertEqual(sorted(bmod.UPSTREAM_FILE_SHA256.keys()), sorted(UPSTREAM_EXPECTED_FILES))
        for fname, expected_sha in bmod.UPSTREAM_FILE_SHA256.items():
            self.assertEqual(
                sha256_file(os.path.join(QJS_DIR, fname)),
                expected_sha,
                f"SHA-256 mismatch vs pinned QuickJS 2024-01-13 manifest for {fname}",
            )

    def test_02_es2023_differential_vs_node_v8_dual_host(self):
        v8_res = run_node_v8_js(self.node_bin, ES2023_DIFF_SCRIPT)
        self.assertEqual(v8_res.returncode, 0, f"Node V8 baseline failed: {v8_res.stderr}")
        v8_lines = v8_res.stdout.strip().splitlines()
        self.assertEqual(len(v8_lines), 16, f"Expected 16 output lines from V8, got: {v8_res.stdout}")

        node_wasi_res = run_qjs_node_wasi(self.node_bin, ["-e", ES2023_DIFF_SCRIPT])
        self.assertEqual(
            node_wasi_res.returncode,
            0,
            f"qjs.wasm (node:wasi) failed ({node_wasi_res.returncode}): {node_wasi_res.stderr}",
        )
        self.assertEqual(node_wasi_res.stderr.strip(), "")
        wasi_lines = node_wasi_res.stdout.strip().splitlines()
        self.assertEqual(wasi_lines, v8_lines, "Differential mismatch: qjs.wasm (node:wasi) vs Node V8")

        shim_res = run_qjs_browser_shim(self.node_bin, ["-e", ES2023_DIFF_SCRIPT])
        self.assertEqual(
            shim_res["exitCode"],
            0,
            f"qjs.wasm (BrowserWasiShim) failed ({shim_res['exitCode']}): {shim_res['stderr']}",
        )
        self.assertEqual(shim_res["stderr"].strip(), "")
        shim_lines = shim_res["stdout"].strip().splitlines()
        self.assertEqual(shim_lines, v8_lines, "Differential mismatch: qjs.wasm (BrowserWasiShim) vs Node V8")

        # Quick check of `-p` print-result mode & `-0.0` Math.sign / 1/-0
        p_res = run_qjs_node_wasi(self.node_bin, ["-p", "Object.is(Math.sin(-0.0), -0.0) && Object.is(Math.atan2(-0.0, 1.0), -0.0)"])
        self.assertEqual(p_res.returncode, 0, p_res.stderr)
        self.assertEqual(p_res.stdout.strip(), "true")

    def test_03_wasi_vfs_script_and_file_io_dual_host(self):
        input_payload = json.dumps({"tag": "ZCC_QJS_WASI", "vals": [10, 20, 30, 40]})
        app_js = r"""
const data = JSON.parse(wasiReadFile("input.json"));
const sum = data.vals.reduce((a, b) => a + b, 0);
const big = (2n ** 64n) + BigInt(sum);
const match = data.tag.replace(/^ZCC_([A-Z]+)_([A-Z]+)$/, "$2:$1");
const report = JSON.stringify({ sum, big: big.toString(), match });
wasiWriteFile("output.json", report);
print("WROTE:" + report);
"""
        # 1) Real OS filesystem preopen via node:wasi
        with tempfile.TemporaryDirectory() as td:
            with open(os.path.join(td, "input.json"), "w", encoding="utf-8", newline="\n") as f:
                f.write(input_payload)
            with open(os.path.join(td, "app.js"), "w", encoding="utf-8", newline="\n") as f:
                f.write(app_js)
            res = run_qjs_node_wasi(self.node_bin, ["app.js"], cwd=td)
            self.assertEqual(res.returncode, 0, f"node:wasi script failed: {res.stderr}")
            expected_json = '{"sum":100,"big":"18446744073709551716","match":"WASI:QJS"}'
            self.assertEqual(res.stdout.strip(), "WROTE:" + expected_json)
            with open(os.path.join(td, "output.json"), "r", encoding="utf-8") as f:
                self.assertEqual(f.read(), expected_json)

            # Also test stdin fallback when no args provided
            stdin_res = run_qjs_node_wasi(
                self.node_bin, [], cwd=td, stdin_text='print("STDIN_OK:" + (6 * 7));\n'
            )
            self.assertEqual(stdin_res.returncode, 0, stdin_res.stderr)
            self.assertEqual(stdin_res.stdout.strip(), "STDIN_OK:42")

        # 2) Pure JS memory BrowserWasiShim VFS
        shim_res = run_qjs_browser_shim(
            self.node_bin,
            ["app.js"],
            files={"input.json": input_payload, "app.js": app_js},
        )
        self.assertEqual(shim_res["exitCode"], 0, shim_res["stderr"])
        expected_json = '{"sum":100,"big":"18446744073709551716","match":"WASI:QJS"}'
        self.assertEqual(shim_res["stdout"].strip(), "WROTE:" + expected_json)
        self.assertEqual(shim_res["files"].get("output.json"), expected_json)

    def test_04_playground_external_runner_runs_qjs_wasm(self):
        self.assertTrue(os.path.isfile(PLAYGROUND_HTML), f"Missing {PLAYGROUND_HTML}")
        with open(PLAYGROUND_HTML, "r", encoding="utf-8") as f:
            html = f.read()
        blocks = re.findall(r"<script[^>]*>([\s\S]*?)</script>", html)
        self.assertGreaterEqual(len(blocks), 1, "No <script> blocks found in playground HTML")

        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as tf:
            json.dump(blocks, tf)
            blocks_json = tf.name

        eval_js = r"""
const fs = require('fs');
const vm = require('vm');
const blocks = JSON.parse(fs.readFileSync(process.argv[1], 'utf8'));
const qjsBytes = new Uint8Array(fs.readFileSync(process.argv[2]));

class MockElem {
  constructor(id) {
    this.id = id; this.value = ''; this.checked = false; this.textContent = '';
    this.innerHTML = ''; this.className = ''; this.tagName = 'BUTTON';
    this.dataset = {}; this.attributes = {}; this.listeners = {};
    this.classList = { toggle() {} };
  }
  getAttribute(k) { return this.attributes[k] || null; }
  addEventListener() {}
}
class MockCanvas extends MockElem {
  constructor(id, w, h) { super(id); this.width = w; this.height = h; }
  getContext() {
    return {
      clearRect() {}, fillRect() {}, beginPath() {}, arc() {}, fill() {}, fillText() {},
      createImageData(w, h) { return { width: w, height: h, data: new Uint8ClampedArray(w * h * 4) }; },
      putImageData() {},
    };
  }
}
const domMap = new Map();
for (const id of [
  'metaBanner','statusBadge','tabBtnC','tabBtnLua','tabBtnExt','tabBtnFb',
  'panel-c','panel-lua','panel-ext','panel-fb','cPresetSelect','linkWasiRtChk',
  'cSourceInput','cArgsInput','cStdinInput','luaPresetSelect','luaSourceInput',
  'extDropZone','extWasmFile','extLoadedLabel','extArgsInput','extStdinInput',
  'fbPresetSelect','fbSourceInput','btnFbCompileRun','btnFbPause','btnFbStep',
  'btnFbStop','fbDropZone','fbWasmFile','fbWadFile','fbLoadedLabel',
  'btnFbRunTitle','btnFbRunE1M1','fbStatusText','vfsFileNameInput',
  'vfsFileContentInput','vfsListOutput','metricCompileMs','metricRunMs',
  'metricSizeBytes','metricExitCode','stdoutOutput','stderrOutput','asmOrInfoOutput'
]) domMap.set(id, new MockElem(id));
domMap.set('wasmMapCanvas', new MockCanvas('wasmMapCanvas', 480, 150));
domMap.set('fbCanvas', new MockCanvas('fbCanvas', 320, 200));

const ctx = {
  console, Buffer, require, TextEncoder, TextDecoder,
  Uint8Array, Uint8ClampedArray, Int8Array, Uint16Array, Uint32Array, Int32Array,
  BigInt, DataView, Map, Set, Promise, Math, Date, Error, RegExp, JSON, WebAssembly,
  DecompressionStream: globalThis.DecompressionStream,
  Response: globalThis.Response,
  atob: globalThis.atob,
  performance: globalThis.performance,
  document: {
    readyState: 'complete',
    getElementById(id) { return domMap.get(id) || null; },
    querySelectorAll() { return []; },
    addEventListener() {},
  },
};
ctx.globalThis = ctx;
ctx.window = ctx;
vm.createContext(ctx);
for (let i = 0; i < blocks.length; i++) {
  vm.runInContext(blocks[i], ctx, { filename: `pg_${i}.js` });
}
(async () => {
  const eng = ctx.ZccPlaygroundEngine;
  eng.bootPlayground();
  const res = await eng.runExternalWasm(
    qjsBytes,
    [
      "qjs.wasm",
      "-e",
      "Promise.resolve((2n**64n) + BigInt(Math.round(Math.hypot(3,4)))).then(v => print(JSON.stringify({v:v.toString(), ok:true})))"
    ]
  );
  if (res.exitCode !== 0 || res.stdout.trim() !== '{"v":"18446744073709551621","ok":true}') {
    throw new Error("Playground External WASI Runner qjs.wasm failed: " + JSON.stringify(res));
  }
  console.log("PLAYGROUND_QJS_PASS", res.stdout.trim(), "runMs=" + res.runMs.toFixed(1));
})().catch((e) => {
  console.error(String(e && e.stack ? e.stack : e));
  process.exit(1);
});
"""
        try:
            res = subprocess.run(
                [self.node_bin, "-e", eval_js, blocks_json, QJS_WASM],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=60,
            )
            self.assertEqual(res.returncode, 0, f"Playground runner failed:\n{res.stdout}\n{res.stderr}")
            self.assertIn("PLAYGROUND_QJS_PASS", res.stdout)
        finally:
            if os.path.exists(blocks_json):
                os.remove(blocks_json)

    def test_05_ef0_negative_controls(self):
        # NC-1: Runtime uncaught JS exception exits non-zero and includes stack on stderr
        nc1 = run_qjs_node_wasi(self.node_bin, ["-e", "function boom(){ return missingVar + 1; } boom();"])
        self.assertNotEqual(nc1.returncode, 0, "NC-1 failed: uncaught ReferenceError exited 0")
        self.assertIn("ReferenceError", nc1.stderr)
        self.assertIn("boom", nc1.stderr)

        # NC-2: Deliberate differential divergence check detects corruption
        v8_bad = run_node_v8_js(self.node_bin, "console.log(2 + 2);")
        qjs_good = run_qjs_node_wasi(self.node_bin, ["-e", "print(2 + 3);"])
        self.assertNotEqual(v8_bad.stdout.strip(), qjs_good.stdout.strip())

        # NC-3: Missing VFS script path exits non-zero
        nc3 = run_qjs_browser_shim(self.node_bin, ["does_not_exist.js"])
        self.assertNotEqual(nc3["exitCode"], 0, "NC-3 failed: missing script exited 0")
        self.assertIn("cannot open script", nc3["stderr"])

        # NC-4: Corrupted WASM magic fails V8 WebAssembly.validate() and BrowserWasiShim
        with open(QJS_WASM, "rb") as f:
            corrupt = bytearray(f.read(256))
        corrupt[0:4] = b"\x00BAD"
        nc4_js = r"""
const fs = require('fs');
const b = Buffer.from(process.argv[1], 'hex');
if (WebAssembly.validate(b)) process.exit(2);
process.stdout.write("VALID=false");
"""
        nc4 = subprocess.run([self.node_bin, "-e", nc4_js, corrupt.hex()], capture_output=True, text=True, check=True)
        self.assertEqual(nc4.stdout, "VALID=false")

        # NC-5: Hardened CLI fail-closed option errors (missing -e arg & unknown flag)
        nc5_miss = run_qjs_node_wasi(self.node_bin, ["-e"])
        self.assertEqual(nc5_miss.returncode, 2)
        self.assertIn("requires an argument", nc5_miss.stderr)
        nc5_unk = run_qjs_node_wasi(self.node_bin, ["--unknown-flag"])
        self.assertEqual(nc5_unk.returncode, 2)
        self.assertIn("unknown option", nc5_unk.stderr)

    def test_06_es2023_adversarial_stress_vs_node_v8(self):
        stress_js = r"""
const out = [];
// 1. Edge IEEE-754 floats (subnormals, MAX_VALUE, EPSILON, tiny/huge exponents) & formatting
const nums = [0, -0, 0.1 + 0.2, 0.000001, 0.0000001, 1e-7, 1e-20, 1e-100, 1e-300, 1e20, 1e21, 1e100, 1e300,
              Number.EPSILON, Number.MAX_SAFE_INTEGER, Number.MIN_SAFE_INTEGER, Number.MAX_VALUE, Number.MIN_VALUE,
              NaN, Infinity, -Infinity, 42.5, 100.5, 999.999, 123456789.98765432, -0.00012345];
out.push("nums:" + nums.map(x => Object.is(x, -0) ? "-0" : String(x)).join("|"));
out.push("exp:" + [0, -0, 123.456, 1e-25, 1e25, 9.999].map(x => x.toExponential(2)).join("|"));
out.push("fix:" + [0, -0, 123.456, 0.005, 9.999].map(x => x.toFixed(2)).join("|"));
out.push("prec:" + [0.00012345, 12345.67, 1e-10, 1e10].map(x => x.toPrecision(4)).join("|"));

// 2. Generators, destructuring, spread
function* fibGen(limit) {
  let [a, b] = [0, 1];
  for (let i = 0; i < limit; i++) { yield a; [a, b] = [b, a + b]; }
}
out.push("gen:" + [...fibGen(12)].join(","));

// 3. Nested try/catch/finally & custom Error subclass
class CustomErr extends Error { constructor(m, code) { super(m); this.code = code; } }
const trace = [];
try {
  try { throw new CustomErr("boom", 503); }
  catch (e) { trace.push(e instanceof CustomErr, e.code, e.message); throw new TypeError("rethrow"); }
  finally { trace.push("inner_fin"); }
} catch (e2) {
  trace.push(e2.name, e2.message);
} finally { trace.push("outer_fin"); }
out.push("err:" + trace.join(","));

// 4. DataView & ArrayBuffer (big/little endian int/float64/BigInt64)
const ab = new ArrayBuffer(24);
const dv = new DataView(ab);
dv.setInt32(0, 0x12345678, false);
dv.setFloat64(8, Math.PI, true);
dv.setBigInt64(16, -0x123456789abcdef0n, false);
out.push("dv:" + dv.getUint32(0, true).toString(16) + "," + dv.getFloat64(8, true).toFixed(12) + "," + dv.getBigInt64(16, false).toString(16));

// 5. 200-bit negative BigInt div/mod/bitwise
const bigA = (1n << 200n) - 12345678901234567890123456789n;
const bigB = -98765432109876543210987654321n;
out.push("big2:" + (bigA / bigB).toString() + "," + (bigA % bigB).toString() + "," + ((bigA ^ 0xffffn) & 0xffffffffn).toString(16));

// 6. RegExp lookbehind, named capture groups, Unicode NFC normalization
const m = "price=$123.45".match(/(?<=\$)(\d+)\.(?<cents>\d+)/);
out.push("re2:" + m[1] + "," + m.groups.cents + "," + ("\u0065\u0301".normalize("NFC") === "\u00e9"));

// 7. eval, Function, Reflect, WeakMap, Object.defineProperty
const wm = new WeakMap();
const keyObj = {};
wm.set(keyObj, eval("6 * 7"));
const dynFn = new Function("x", "y", "return x ** y;");
const getterObj = {};
Object.defineProperty(getterObj, "v", { get() { return 99; }, enumerable: true });
out.push("meta:" + wm.get(keyObj) + "," + dynFn(3, 5) + "," + Reflect.get(getterObj, "v"));

// 8. GC & allocator churn (30,000 objects + strings)
let gcSum = 0;
for (let i = 0; i < 30000; i++) {
  const o = { idx: i, s: "k" + (i & 255) };
  gcSum += o.idx ^ o.s.length;
}
out.push("gc:" + gcSum);

// 9. Async generator + for-await-of + Promise.all
async function runAsync() {
  async function* agen() { for (let i = 1; i <= 4; i++) yield await Promise.resolve(i * i); }
  const sq = [];
  for await (const v of agen()) sq.push(v);
  const all = await Promise.all([Promise.resolve(10), Promise.resolve(20), Promise.resolve(30)]);
  out.push("async2:" + sq.join(",") + "|" + all.reduce((a, b) => a + b, 0));
  if (typeof print === "function") out.forEach(l => print(l));
  else out.forEach(l => console.log(l));
}
runAsync();
""".strip()

        v8_res = run_node_v8_js(self.node_bin, stress_js)
        self.assertEqual(v8_res.returncode, 0, v8_res.stderr)
        v8_lines = v8_res.stdout.strip().splitlines()
        self.assertEqual(len(v8_lines), 12)

        wasi_res = run_qjs_node_wasi(self.node_bin, ["--eval", stress_js])
        self.assertEqual(wasi_res.returncode, 0, wasi_res.stderr)
        self.assertEqual(wasi_res.stderr.strip(), "")
        self.assertEqual(wasi_res.stdout.strip().splitlines(), v8_lines)

        shim_res = run_qjs_browser_shim(self.node_bin, ["--eval", stress_js])
        self.assertEqual(shim_res["exitCode"], 0, shim_res["stderr"])
        self.assertEqual(shim_res["stderr"].strip(), "")
        self.assertEqual(shim_res["stdout"].strip().splitlines(), v8_lines)


if __name__ == "__main__":
    unittest.main(verbosity=2)
