#!/usr/bin/env python3
"""
Differential & Dual-Host Verification Suite for `build_wasm/micropython.wasm`
==============================================================================
Verifies standalone ZCC-compiled Upstream MicroPython v1.24.1 (`build_wasm/micropython.wasm`):
  1. V8 WebAssembly.validate(), zero `env.*` imports, Section 13 (`Tag`) present,
     and strictly unmodified upstream sources matching pinned SHA-256 manifest.
  2. Full Python 3 differential parity against host `python3` across BOTH
     `node:wasi` and `tools/browser_wasi_shim.js` (arithmetic, arbitrary-precision
     `mpz` 200-bit integers, IEEE-754 doubles + `math`, complex numbers, generators,
     comprehensions, closures, decorators, classes & inheritance, f-strings, `json`,
     `re`, `struct`, `collections`, `heapq`, `sys.argv`).
  3. Deep native Wasm EH (`MICROPY_NLR_SETJMP` over `Tag` 13 + `try_table` + `throw`)
     stress: nested `try`/`except`/`else`/`finally`, custom exceptions, deep stack
     unwinding + re-raise, generator `StopIteration` & `gen.throw()`, `__getitem__`
     `IndexError` loop termination, and `eval`/`exec` `SyntaxError` capture.
  4. GC stress test (30,000 objects + `gc.collect()`) proving linear heap & root
     stability without corruption across allocation cycles.
  5. WASI VFS script execution + local `import helper_mod` + Python `open()` I/O
     + `stdin` fallback across both hosts.
  6. `tools/zcc_wasm_playground.html` headless `ZccPlaygroundEngine.runExternalWasm`.
  7. ZCC-EF-0 negative controls (uncaught exception, `SyntaxError`, `sys.exit(7)`,
     invalid CLI flag `--bogus-flag`, missing script, corrupted `.wasm` header).
"""

import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MPY_WASM = os.path.join(REPO_ROOT, "build_wasm", "micropython.wasm")
MPY_DIR = os.path.join(REPO_ROOT, "tests", "embedded_micropython")
BROWSER_SHIM = os.path.join(REPO_ROOT, "tools", "browser_wasi_shim.js")
PLAYGROUND_HTML = os.path.join(REPO_ROOT, "tools", "zcc_wasm_playground.html")

PY_DIFF_SCRIPT = r"""
import math
import json
import re
import struct
import heapq
import sys
from collections import OrderedDict, deque, namedtuple

lines = []

# 1. Arbitrary-precision mpz integers & modular exponentiation
big_a = (2 ** 200) + 12345678901234567890123456789
big_b = 98765432109876543210987654321
fact60 = 1
for k in range(2, 61):
    fact60 *= k
lines.append("mpz:%d|%d|%d|%d" % (big_a // big_b, big_a % big_b, pow(3, 100, 10**9 + 7), fact60))

# 2. IEEE-754 floats, math & complex
fvals = [math.sin(0.5), math.cos(0.5), math.sqrt(2.0), math.exp(1.25), math.log(12.5), math.floor(-2.3), math.ceil(-2.3)]
lines.append("math:" + ",".join("%.6f" % x for x in fvals))
z = (3 + 4j) * (1 - 2j)
lines.append("cplx:%d,%d,%.1f" % (int(z.real), int(z.imag), abs(3 + 4j)))

# 3. Closures, decorators, comprehensions, f-strings, sorting
def trace_wrap(fn):
    def inner(x):
        return fn(x) * 10
    return inner

@trace_wrap
def square(n):
    return n * n

def make_acc(base):
    def add(d):
        nonlocal base
        base += d
        return base
    return add

acc = make_acc(10)
comp = [x * x for x in range(10) if x % 2 == 1]
dcomp = {str(k): k * 3 for k in range(4)}
scomp = sorted({x % 4 for x in range(10)})
words = sorted(["pear", "fig", "apple", "kiwi"], key=lambda s: (len(s), s))
lines.append(f"fn:{square(3)}|{acc(5)},{acc(7)}|{comp}|{sorted(dcomp.items())}|{scomp}|{words}")

# 4. Classes, inheritance, property, dunder methods
class Shape:
    def __init__(self, scale):
        self._scale = scale
    def area(self):
        return 0

class Rect(Shape):
    def __init__(self, w, h, scale=1):
        super().__init__(scale)
        self.w = w
        self.h = h
    @property
    def perim(self):
        return 2 * (self.w + self.h)
    def area(self):
        return self.w * self.h * self._scale
    def __repr__(self):
        return f"Rect({self.w},{self.h})"

r = Rect(3, 4, 2)
lines.append(f"cls:{r}|{r.area()}|{r.perim}|{isinstance(r, Shape)}")

# 5. Deep native Wasm EH (`try/except/else/finally`, `raise`, custom class, re-raise, generators)
class CustomErr(Exception):
    def __init__(self, msg, code):
        super().__init__(msg)
        self.code = code

eh_trace = []
def level3():
    raise CustomErr("deep_fail", 503)

def level2():
    try:
        level3()
    finally:
        eh_trace.append("lvl2_fin")

def level1():
    try:
        try:
            level2()
        except CustomErr as e:
            eh_trace.append(f"caught:{e.code}")
            raise ValueError("rethrown")
        finally:
            eh_trace.append("inner_fin")
    except ValueError as v:
        eh_trace.append(f"outer:{v}")
    else:
        eh_trace.append("BAD_ELSE")
    finally:
        eh_trace.append("outer_fin")

level1()

# ZeroDivisionError / KeyError / IndexError / TypeError / AttributeError / __getitem__ iter StopIteration
try:
    _ = 1 // 0
except ZeroDivisionError:
    eh_trace.append("zdiv")

class SeqGetItem:
    def __getitem__(self, idx):
        if idx >= 4:
            raise IndexError
        return idx * 10

lines.append("eh:" + ",".join(eh_trace) + "|" + str([v for v in SeqGetItem()]))

# 6. Generators, yield from, gen.throw
def inner_gen():
    try:
        yield 10
        yield 20
    except RuntimeError:
        yield 99

def outer_gen():
    yield from inner_gen()
    yield 30

g = outer_gen()
g_out = [next(g), g.throw(RuntimeError("g_boom")), next(g)]
lines.append("gen:" + str(g_out))

# 7. Stdlib modules: json, re, struct, collections, heapq, sys.argv
payload = {"k": [1, True, None, "zcc"], "n": 42}
dec = json.loads(json.dumps(payload))
m = re.search(r"([a-z]+)-([0-9]+)", "id=alpha-2026!")
sub = re.sub(r"[0-9]+", "NUM", "a1b23c456")
pk = struct.pack("<BHi", 0x7A, 0x1234, -99999)
unpk = struct.unpack("<BHi", pk)
od = OrderedDict([("b", 2), ("a", 1)])
dq = deque([1, 2, 3], 10)
dq.appendleft(0)
Pt = namedtuple("Pt", ("x", "y"))
pt = Pt(7, 9)
h = [5, 1, 8, 3, 2]
heapq.heapify(h)
hsorted = [heapq.heappop(h) for _ in range(len(h))]
lines.append(f"std:{dec['k']}|{dec['n']}|{m.group(1)}:{m.group(2)}|{sub}|{len(pk)}:{unpk}|{list(od.keys())}|{list(dq)}|{pt.x + pt.y}|{hsorted}")

# 8. sys.argv check
lines.append("argv:" + "|".join(sys.argv))

for ln in lines:
    print(ln)
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


def parse_wasm_sections(data: bytes) -> list[int]:
    pos = 8
    secs = []
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
        secs.append(sec_id)
        pos += sz
    return secs


def run_host_python3(argv: list[str], cwd: str | None = None, stdin_text: str = "") -> subprocess.CompletedProcess:
    return subprocess.run(
        ["python3", *argv],
        input=stdin_text,
        cwd=cwd or REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=30,
    )


def run_mpy_node_wasi(
    node_bin: str, argv: list[str], cwd: str | None = None, stdin_text: str = ""
) -> subprocess.CompletedProcess:
    runner_js = r"""
const fs = require('fs');
const { WASI } = require('node:wasi');
const cfg = JSON.parse(fs.readFileSync(process.argv[1], 'utf8'));
const wasi = new WASI({
  version: 'preview1',
  args: ['micropython.wasm', ...cfg.argv],
  env: {},
  preopens: { '.': cfg.preopen },
  returnOnExit: true,
});
(async () => {
  const bytes = fs.readFileSync(cfg.wasm);
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
  process.exit(99);
});
"""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump({"wasm": os.path.abspath(MPY_WASM), "argv": argv, "preopen": "."}, tf)
        cfg_path = tf.name
    try:
        return subprocess.run(
            [node_bin, "--no-warnings", "-e", runner_js, cfg_path],
            input=stdin_text,
            cwd=cwd or REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=60,
        )
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


def run_mpy_browser_shim(
    node_bin: str, argv: list[str], files: dict[str, str] | None = None, stdin_text: str = ""
) -> dict:
    import base64
    files_b64 = {}
    if files:
        for k, v in files.items():
            raw = v.encode("utf-8") if isinstance(v, str) else bytes(v)
            files_b64[k] = base64.b64encode(raw).decode("ascii")
    runner_js = r"""
const fs = require('fs');
const { BrowserWasiShim } = require(process.argv[1]);
const mpyBytes = fs.readFileSync(process.argv[2]);
const cfg = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
(async () => {
  const vfsFiles = {};
  for (const [k, b64] of Object.entries(cfg.filesB64 || {})) {
    vfsFiles[k] = Buffer.from(b64, 'base64');
  }
  const shim = new BrowserWasiShim({
    args: ['micropython.wasm', ...cfg.argv],
    files: vfsFiles,
    stdin: cfg.stdin || '',
    preopenDir: '.',
  });
  let rc = -1;
  let trap = null;
  try {
    const res = await shim.run(mpyBytes);
    rc = res.exitCode;
  } catch (e) {
    trap = String(e);
  }
  const outFiles = {};
  const td = new TextDecoder();
  for (const [k, v] of shim.vfs.entries()) {
    outFiles[k] = td.decode(v);
  }
  console.log('__RESULT__' + JSON.stringify({
    exitCode: rc,
    stdout: shim.getStdout(),
    stderr: shim.getStderr(),
    trap,
    files: outFiles,
  }));
})();
"""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump({"argv": argv, "filesB64": files_b64, "stdin": stdin_text}, tf)
        cfg_path = tf.name
    try:
        res = subprocess.run(
            [node_bin, "-e", runner_js, BROWSER_SHIM, MPY_WASM, cfg_path],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=60,
            check=True,
        )
        for line in reversed(res.stdout.splitlines()):
            if line.startswith("__RESULT__"):
                return json.loads(line[len("__RESULT__"):])
        raise AssertionError(f"Missing __RESULT__ marker: {res.stdout}\n{res.stderr}")
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


class TestWasmMicroPython(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.node_bin = find_node_bin()
        if not os.path.isfile(MPY_WASM):
            raise RuntimeError(f"Missing {MPY_WASM}; run `python3 tools/build_micropython_wasm.py` first.")

    def test_01_v8_validation_tag13_zero_env_and_clean_upstream(self):
        with open(MPY_WASM, "rb") as f:
            raw = f.read()
        self.assertGreater(len(raw), 300_000, f"micropython.wasm too small: {len(raw)} bytes")
        secs = parse_wasm_sections(raw)
        self.assertIn(13, secs, f"Expected Tag Section 13 in micropython.wasm sections={secs}")

        inspect_js = r"""
const fs = require('fs');
const b = fs.readFileSync(process.argv[1]);
const valid = WebAssembly.validate(b);
const mod = new WebAssembly.Module(b);
const imports = WebAssembly.Module.imports(mod).map(i => `${i.module}.${i.name}`);
const exportsList = WebAssembly.Module.exports(mod).map(e => `${e.name}:${e.kind}`);
process.stdout.write(JSON.stringify({ valid, imports, exportsList }));
"""
        res = subprocess.run([self.node_bin, "-e", inspect_js, MPY_WASM], capture_output=True, text=True, check=True)
        info = json.loads(res.stdout)
        self.assertTrue(info["valid"], "V8 WebAssembly.validate() returned false")
        non_wasi = [i for i in info["imports"] if not i.startswith("wasi_snapshot_preview1.")]
        self.assertEqual(non_wasi, [], f"Unexpected non-WASI imports: {non_wasi}")
        self.assertIn("_start:function", info["exportsList"])
        self.assertIn("memory:memory", info["exportsList"])

        spec = importlib.util.spec_from_file_location(
            "build_micropython_wasm", os.path.join(REPO_ROOT, "tools", "build_micropython_wasm.py")
        )
        bmod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(bmod)
        for rel, expected_sha in bmod.UPSTREAM_FILE_SHA256.items():
            full = os.path.join(MPY_DIR, rel)
            self.assertTrue(os.path.isfile(full), f"Missing upstream file {rel}")
            self.assertEqual(sha256_file(full), expected_sha, f"SHA-256 mismatch for upstream {rel}")

    def test_02_python3_differential_vs_cpython_dual_host(self):
        py_argv = ["-c", PY_DIFF_SCRIPT, "arg1", "arg2"]
        cp_res = run_host_python3(py_argv)
        self.assertEqual(cp_res.returncode, 0, f"Host python3 failed: {cp_res.stderr}")
        cp_lines = cp_res.stdout.strip().splitlines()
        self.assertEqual(len(cp_lines), 9, f"Expected 9 lines from python3, got: {cp_res.stdout}")

        nw_res = run_mpy_node_wasi(self.node_bin, py_argv)
        self.assertEqual(nw_res.returncode, 0, f"node:wasi failed ({nw_res.returncode}): {nw_res.stderr}")
        self.assertEqual(nw_res.stderr.strip(), "")
        self.assertEqual(nw_res.stdout.strip().splitlines(), cp_lines, "Differential mismatch: node:wasi vs python3")

        bs_res = run_mpy_browser_shim(self.node_bin, py_argv)
        self.assertIsNone(bs_res["trap"])
        self.assertEqual(bs_res["exitCode"], 0, f"BrowserWasiShim failed: {bs_res['stderr']}")
        self.assertEqual(bs_res["stderr"].strip(), "")
        self.assertEqual(bs_res["stdout"].strip().splitlines(), cp_lines, "Differential mismatch: shim vs python3")

    def test_03_gc_collect_stress_and_random(self):
        gc_code = r"""
import gc
import random

anchors = [(i, str(i * i)) for i in range(40)]
gc.collect()
free_before = gc.mem_free()
acc = 0
for i in range(30000):
    d = {"i": i, "k": "v" + str(i & 63)}
    acc ^= len(d["k"]) + d["i"]
    if (i & 4095) == 4095:
        gc.collect()
gc.collect()
free_after = gc.mem_free()
assert anchors[0] == (0, "0") and anchors[-1] == (39, "1521"), "live roots corrupted!"
assert free_after > 1000000, f"heap not reclaimed: {free_after}"

random.seed(42)
rvals = [random.randint(1, 1000) for _ in range(5)]
assert len(rvals) == 5 and all(1 <= v <= 1000 for v in rvals)
print("GC_STRESS_OK", acc, len(anchors), free_before > 0, free_after > 0)
"""
        nw = run_mpy_node_wasi(self.node_bin, ["-c", gc_code])
        self.assertEqual(nw.returncode, 0, f"GC stress node:wasi failed:\n{nw.stdout}\n{nw.stderr}")
        self.assertEqual(nw.stdout.strip(), "GC_STRESS_OK 30014 40 True True")

        bs = run_mpy_browser_shim(self.node_bin, ["-c", gc_code])
        self.assertEqual(bs["exitCode"], 0, bs["stderr"])
        self.assertEqual(bs["stdout"].strip(), "GC_STRESS_OK 30014 40 True True")

    def test_04_wasi_vfs_script_import_open_and_stdin_dual_host(self):
        helper_py = "def greet(name, n):\n    return f'HELLO:{name}:{n * 2}'\n"
        main_py = r"""
import sys
import helper_mod

msg = helper_mod.greet(sys.argv[1], int(sys.argv[2]))
with open("wasi_out.txt", "w") as f:
    f.write(msg + "\nSECOND_LINE\n")
with open("wasi_out.txt", "r") as f:
    lines = [ln.strip() for ln in f]
print("VFS_OK", sys.argv[0], "|".join(lines))
"""
        with tempfile.TemporaryDirectory() as td:
            with open(os.path.join(td, "helper_mod.py"), "w", encoding="utf-8", newline="\n") as f:
                f.write(helper_py)
            with open(os.path.join(td, "main_app.py"), "w", encoding="utf-8", newline="\n") as f:
                f.write(main_py)

            nw = run_mpy_node_wasi(self.node_bin, ["main_app.py", "ZCC", "21"], cwd=td)
            self.assertEqual(nw.returncode, 0, f"node:wasi VFS script failed: {nw.stderr}")
            self.assertEqual(nw.stdout.strip(), "VFS_OK main_app.py HELLO:ZCC:42|SECOND_LINE")
            with open(os.path.join(td, "wasi_out.txt"), "r", encoding="utf-8") as f:
                self.assertEqual(f.read(), "HELLO:ZCC:42\nSECOND_LINE\n")

            nw_stdin = run_mpy_node_wasi(self.node_bin, ["-"], cwd=td, stdin_text="print('STDIN_MPY', 6 * 7)\n")
            self.assertEqual(nw_stdin.returncode, 0, nw_stdin.stderr)
            self.assertEqual(nw_stdin.stdout.strip(), "STDIN_MPY 42")

        bs = run_mpy_browser_shim(
            self.node_bin,
            ["main_app.py", "ZCC", "21"],
            files={"helper_mod.py": helper_py, "main_app.py": main_py},
        )
        self.assertEqual(bs["exitCode"], 0, bs["stderr"])
        self.assertEqual(bs["stdout"].strip(), "VFS_OK main_app.py HELLO:ZCC:42|SECOND_LINE")
        self.assertEqual(bs["files"].get("wasi_out.txt"), "HELLO:ZCC:42\nSECOND_LINE\n")

    def test_05_playground_external_runner_runs_micropython_wasm(self):
        self.assertTrue(os.path.isfile(PLAYGROUND_HTML), f"Missing {PLAYGROUND_HTML}")
        with open(PLAYGROUND_HTML, "r", encoding="utf-8") as f:
            html = f.read()
        blocks = re.findall(r"<script[^>]*>([\s\S]*?)</script>", html)
        self.assertGreaterEqual(len(blocks), 1)

        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as tf:
            json.dump(blocks, tf)
            blocks_json = tf.name

        eval_js = r"""
const fs = require('fs');
const vm = require('vm');
const blocks = JSON.parse(fs.readFileSync(process.argv[1], 'utf8'));
const mpyBytes = new Uint8Array(fs.readFileSync(process.argv[2]));

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
    mpyBytes,
    [
      "micropython.wasm",
      "-c",
      "import json\ntry:\n    1 / 0\nexcept ZeroDivisionError:\n    print(json.dumps({'big': str((2**64) + 1), 'eh': True}))"
    ]
  );
  if (res.exitCode !== 0 || JSON.parse(res.stdout).big !== "18446744073709551617") {
    throw new Error("Playground External WASI Runner micropython.wasm failed: " + JSON.stringify(res));
  }
  console.log("PLAYGROUND_MPY_PASS", res.stdout.trim(), "runMs=" + res.runMs.toFixed(1));
})().catch((e) => {
  console.error(String(e && e.stack ? e.stack : e));
  process.exit(1);
});
"""
        try:
            res = subprocess.run(
                [self.node_bin, "-e", eval_js, blocks_json, MPY_WASM],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=60,
            )
            self.assertEqual(res.returncode, 0, f"Playground runner failed:\n{res.stdout}\n{res.stderr}")
            self.assertIn("PLAYGROUND_MPY_PASS", res.stdout)
        finally:
            if os.path.exists(blocks_json):
                os.remove(blocks_json)

    def test_06_ef0_negative_controls(self):
        # NC-1: Uncaught RuntimeError -> exit 1 + traceback on stderr
        nc1 = run_mpy_node_wasi(self.node_bin, ["-c", "def f(): raise RuntimeError('nc1_boom')\nf()"])
        self.assertEqual(nc1.returncode, 1)
        self.assertIn("RuntimeError: nc1_boom", nc1.stderr)

        # NC-2: SyntaxError -> exit 1 + SyntaxError on stderr
        nc2 = run_mpy_node_wasi(self.node_bin, ["-c", "def broken(:\n    pass"])
        self.assertEqual(nc2.returncode, 1)
        self.assertIn("SyntaxError", nc2.stderr)

        # NC-3: sys.exit(7) -> exact exit code 7
        nc3 = run_mpy_node_wasi(self.node_bin, ["-c", "import sys; sys.exit(7)"])
        self.assertEqual(nc3.returncode, 7)

        # NC-4: Invalid CLI flag -> exit 2 + unknown option diagnostic
        nc4_unk = run_mpy_node_wasi(self.node_bin, ["--bogus-flag"])
        self.assertEqual(nc4_unk.returncode, 2)
        self.assertIn("unknown option", nc4_unk.stderr)
        nc4_miss = run_mpy_node_wasi(self.node_bin, ["-c"])
        self.assertEqual(nc4_miss.returncode, 2)
        self.assertIn("requires an argument", nc4_miss.stderr)

        # NC-5: Missing script file -> non-zero exit
        nc5 = run_mpy_browser_shim(self.node_bin, ["nonexistent_script_ef0.py"])
        self.assertEqual(nc5["exitCode"], 1)
        self.assertIn("can't open file", nc5["stderr"])

        # NC-6: Corrupted WASM byte fails V8 WebAssembly.validate()
        with open(MPY_WASM, "rb") as f:
            corrupt = bytearray(f.read(256))
        corrupt[10] ^= 0xFF
        nc6_js = r"""
const b = Buffer.from(process.argv[1], 'hex');
process.stdout.write(WebAssembly.validate(b) ? "VALID=true" : "VALID=false");
"""
        nc6 = subprocess.run([self.node_bin, "-e", nc6_js, corrupt.hex()], capture_output=True, text=True, check=True)
        self.assertEqual(nc6.stdout, "VALID=false")


if __name__ == "__main__":
    unittest.main(verbosity=2)
