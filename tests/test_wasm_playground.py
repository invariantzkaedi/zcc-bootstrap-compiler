#!/usr/bin/env python3
"""
Zero-Server In-Browser ZCC Playground Test & Rule EF-7 Verification Suite
==========================================================================
Enforces all 6 mandatory UI & HTML Upgrade Verification gates (Rule EF-7):
  1. AST Syntax Parse Gate (`new vm.Script` on all embedded `<script>` blocks)
  2. Zero-Dimension Canvas Resilience (HTML dimensions, <50px guard, Math.max(1,...))
  3. Global Animation Clock Fault-Isolation (`try...catch` + rAF loop)
  4. Symbol & DOM ID Integrity Audit (every `onclick`/`onchange` & `el(...)` exists)
  5. Same-Turn Headless Node 22 Evaluation (`ZccPlaygroundEngine` end-to-end across C bare,
     C + `wasm_wasi_rt.c` VFS, `-S` x86-64 asm, embedded `lua.wasm`, external
     `sqlite3.wasm`, dual-layer single-fire click check, and invalid-C negative control)
  6. Anti-Dead-Click Invariant (dual-layer bindings, `z-index: 1000`, `pointer-events: auto`,
     `pointer-events: none !important` on backdrop overlay, `user-select: auto`)
"""

import html.parser
import os
import re
import shutil
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLAYGROUND_HTML = os.path.join(REPO_ROOT, "tools", "zcc_wasm_playground.html")
BUILD_PG_PY = os.path.join(REPO_ROOT, "tools", "build_wasm_playground.py")
SQLITE_WASM = os.path.join(REPO_ROOT, "build_wasm", "sqlite3.wasm")


def find_node_bin() -> str:
    found = shutil.which("node")
    if found:
        return found
    candidate = "/home/ZKAEDI/.local/bin/node"
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    raise RuntimeError("node binary not found on PATH or at /home/ZKAEDI/.local/bin/node")


class PlaygroundHtmlInspector(html.parser.HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.handlers = []
        self.data_actions = []
        self.scripts = []
        self.canvases = []
        self.external_urls = []
        self._in_script = False
        self._cur_script = []

    def handle_starttag(self, tag, attrs):
        attr_map = dict(attrs)
        if "id" in attr_map:
            self.ids.add(attr_map["id"])
        if tag == "canvas":
            self.canvases.append(attr_map)
        if tag in ("script", "link"):
            url = attr_map.get("src") or attr_map.get("href") or ""
            if url.startswith("http://") or url.startswith("https://"):
                self.external_urls.append(url)
        for ev_attr in ("onclick", "onchange"):
            if ev_attr in attr_map:
                self.handlers.append((tag, attr_map.get("id", ""), ev_attr, attr_map[ev_attr]))
        if "data-action" in attr_map:
            has_inline = ("onclick" in attr_map) or ("onchange" in attr_map)
            self.data_actions.append((tag, attr_map.get("id", ""), attr_map["data-action"], has_inline))
        if tag == "script":
            self._in_script = True
            self._cur_script = []

    def handle_endtag(self, tag):
        if tag == "script" and self._in_script:
            self._in_script = False
            self.scripts.append("".join(self._cur_script))

    def handle_data(self, data):
        if self._in_script:
            self._cur_script.append(data)


class TestWasmPlaygroundEF7(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.node_bin = find_node_bin()
        if not os.path.isfile(PLAYGROUND_HTML):
            subprocess.run(["python3", BUILD_PG_PY], cwd=REPO_ROOT, check=True)
        with open(PLAYGROUND_HTML, "r", encoding="utf-8") as f:
            cls.html_text = f.read()
        cls.inspector = PlaygroundHtmlInspector()
        cls.inspector.feed(cls.html_text)

    def test_01_ast_syntax_gate_all_scripts(self):
        self.assertGreaterEqual(len(self.inspector.scripts), 2)
        js = r"""
const fs = require('fs');
const vm = require('vm');
const blocks = JSON.parse(fs.readFileSync(process.argv[1], 'utf8'));
for (let i = 0; i < blocks.length; i++) {
  new vm.Script(blocks[i], { filename: `playground_script_${i}.js` });
}
console.log(`AST_SYNTAX_OK blocks=${blocks.length} errors=0`);
"""
        import json
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
            json.dump(self.inspector.scripts, tf)
            cfg = tf.name
        try:
            res = subprocess.run([self.node_bin, "-e", js, cfg], cwd=REPO_ROOT, capture_output=True, text=True)
            self.assertEqual(res.returncode, 0, res.stderr)
            self.assertIn("errors=0", res.stdout)
        finally:
            if os.path.exists(cfg):
                os.remove(cfg)

    def test_02_css_canvas_symbol_and_offline_gates(self):
        # Offline / Zero-server invariant
        self.assertEqual(self.inspector.external_urls, [], f"External URLs found: {self.inspector.external_urls}")
        self.assertNotIn("izkaedi-ui", self.html_text)

        # Canvas zero-dimension guard & fallback attributes (EF-7 Gate 2 & 3)
        self.assertGreaterEqual(len(self.inspector.canvases), 1)
        cmap = {c.get("id"): c for c in self.inspector.canvases}
        self.assertIn("wasmMapCanvas", cmap)
        self.assertGreaterEqual(int(cmap["wasmMapCanvas"].get("width", "0")), 50)
        self.assertGreaterEqual(int(cmap["wasmMapCanvas"].get("height", "0")), 50)
        self.assertIn("canvas.width < 50 || canvas.height < 50", self.html_text)
        self.assertIn("Math.max(1,", self.html_text)
        self.assertRegex(self.html_text, r"function\s+animationLoop\s*\(\)\s*\{\s*try\s*\{")

        # Anti-Dead-Click CSS invariant (EF-7 Gate 6)
        self.assertIn("pointer-events: none !important;", self.html_text)
        self.assertIn("z-index: 1000;", self.html_text)
        self.assertIn("pointer-events: auto;", self.html_text)
        self.assertIn("user-select: auto;", self.html_text)
        self.assertNotIn("body::before", self.html_text)
        self.assertNotIn("body::after", self.html_text)

        # Dual-layer binding on every [data-action] control
        self.assertGreaterEqual(len(self.inspector.data_actions), 8)
        for tag, eid, act, has_inline in self.inspector.data_actions:
            self.assertTrue(has_inline, f"Control <{tag} id={eid} data-action={act}> missing inline onclick/onchange")

        # DOM ID integrity (EF-7 Gate 4): every el("...") referenced in script exists in HTML
        app_js = self.inspector.scripts[-1]
        referenced_ids = set(re.findall(r'\bel\(\s*["\']([A-Za-z0-9_-]+)["\']\s*\)', app_js))
        missing = sorted(referenced_ids - self.inspector.ids)
        self.assertEqual(missing, [], f"Dangling DOM element IDs in JS: {missing}")

        # Handler function names in onclick/onchange exist in app JS
        for tag, eid, ev_attr, expr in self.inspector.handlers:
            fn_match = re.match(r"^\s*([A-Za-z0-9_]+)\s*\(", expr)
            self.assertIsNotNone(fn_match, f"Malformed handler {ev_attr}={expr}")
            fn_name = fn_match.group(1)
            self.assertIn(f"globalThis.{fn_name} =", app_js, f"Handler {fn_name} not exported to globalThis")

    def test_03_headless_node_end_to_end_evaluation(self):
        # Clear any stale sqlite probe binary check: assert build_wasm/sqlite3.wasm exists
        self.assertTrue(os.path.isfile(SQLITE_WASM), f"Missing {SQLITE_WASM}")
        js = r"""
const fs = require('fs');
const vm = require('vm');
const zlib = require('zlib');
const crypto = require('crypto');

const blocks = JSON.parse(fs.readFileSync(process.argv[1], 'utf8'));
const sqlitePath = process.argv[2];

class MockElem {
  constructor(id) {
    this.id = id;
    this.value = '';
    this.checked = false;
    this.textContent = '';
    this.innerHTML = '';
    this.className = '';
    this.tagName = 'BUTTON';
    this.dataset = {};
    this.attributes = {};
    this.listeners = {};
    this.classList = {
      _set: new Set(),
      toggle(cls, force) {
        if (force === true) this._set.add(cls);
        else if (force === false) this._set.delete(cls);
        else if (this._set.has(cls)) this._set.delete(cls);
        else this._set.add(cls);
      }
    };
  }
  getAttribute(k) { return this.attributes[k] || null; }
  addEventListener(evt, fn) {
    if (!this.listeners[evt]) this.listeners[evt] = [];
    this.listeners[evt].push(fn);
  }
  dispatchClick(ev) {
    if (this.onclick) this.onclick(ev);
    for (const fn of (this.listeners.click || [])) fn(ev);
  }
}

class MockCanvas extends MockElem {
  constructor(id, w, h) {
    super(id);
    this.width = w;
    this.height = h;
    this.drawOps = 0;
  }
  getContext(kind) {
    if (kind !== '2d') return null;
    const self = this;
    return {
      fillStyle: '#000',
      font: '',
      clearRect() { self.drawOps++; },
      fillRect() { self.drawOps++; },
      beginPath() { self.drawOps++; },
      arc(x, y, r) {
        if (!(r > 0)) throw new Error('DOMException: non-positive arc radius');
        self.drawOps++;
      },
      fill() { self.drawOps++; },
      fillText() { self.drawOps++; },
    };
  }
}

const domMap = new Map();
const needIds = [
  'metaBanner', 'statusBadge', 'tabBtnC', 'tabBtnLua', 'tabBtnExt',
  'panel-c', 'panel-lua', 'panel-ext', 'cPresetSelect', 'linkWasiRtChk',
  'cSourceInput', 'cArgsInput', 'cStdinInput', 'luaPresetSelect', 'luaSourceInput',
  'extDropZone', 'extWasmFile', 'extLoadedLabel', 'extArgsInput', 'extStdinInput',
  'vfsFileNameInput', 'vfsFileContentInput', 'vfsListOutput',
  'metricCompileMs', 'metricRunMs', 'metricSizeBytes', 'metricExitCode',
  'stdoutOutput', 'stderrOutput', 'asmOrInfoOutput',
];
for (const id of needIds) domMap.set(id, new MockElem(id));
domMap.get('cPresetSelect').value = 'bare_fib';
domMap.get('luaPresetSelect').value = 'lua_fib_closures';
const canvas = new MockCanvas('wasmMapCanvas', 480, 150);
domMap.set('wasmMapCanvas', canvas);

const actionElem = domMap.get('tabBtnC');
actionElem.attributes['data-action'] = 'switchTab:c';

const ctx = {
  console,
  Buffer,
  require,
  TextEncoder,
  TextDecoder,
  Uint8Array,
  Int8Array,
  Uint16Array,
  Uint32Array,
  Int32Array,
  BigInt,
  DataView,
  Map,
  Set,
  Promise,
  Math,
  Date,
  Error,
  RegExp,
  JSON,
  WebAssembly,
  DecompressionStream: globalThis.DecompressionStream,
  Response: globalThis.Response,
  atob: globalThis.atob,
  performance: globalThis.performance,
  document: {
    readyState: 'complete',
    getElementById(id) { return domMap.get(id) || null; },
    querySelectorAll() { return [actionElem]; },
    addEventListener() {},
  },
};
ctx.globalThis = ctx;
ctx.window = ctx;
vm.createContext(ctx);

for (let i = 0; i < blocks.length; i++) {
  vm.runInContext(blocks[i], ctx, { filename: `playground_${i}.js` });
}

(async () => {
  const errors = [];
  const eng = ctx.ZccPlaygroundEngine;
  eng.bootPlayground();

  // Check zero-dimension canvas guard throws 0 errors & performs 0 draw ops
  const zeroCanvas = new MockCanvas('zero', 0, 0);
  eng.renderWasmMapCanvas(zeroCanvas, [{ id: 1, name: 'Type', size: 10 }], 0.1);
  if (zeroCanvas.drawOps !== 0) throw new Error('Zero-dimension canvas guard failed to short-circuit');

  // Check dual-layer single-fire click invariant (onclick + addEventListener must dispatch once!)
  actionElem.onclick = (ev) => ctx.zccUiAction('switchTab:c', ev);
  const beforeCnt = eng.state.dispatchedCount;
  const fakeEvent = {};
  actionElem.dispatchClick(fakeEvent);
  const afterCnt = eng.state.dispatchedCount;
  if (afterCnt - beforeCnt !== 1) {
    throw new Error(`Expected single dispatch on dual-bound click, got ${afterCnt - beforeCnt}`);
  }

  // Decompress embedded zcc.wasm, wasm_wasi_rt.c, and lua.wasm
  await eng.initAssets();
  if (!(eng.state.zccBytes && eng.state.zccBytes.length > 900000)) throw new Error('zcc.wasm too small');
  if (!(eng.state.luaBytes && eng.state.luaBytes.length > 200000)) throw new Error('lua.wasm too small');
  if (!eng.state.wasiRtText.includes('fopen(')) throw new Error('wasm_wasi_rt.c missing fopen');

  // Render canvas with zcc.wasm sections & verify animationLoop catches internal faults
  eng.renderWasmMapCanvas(canvas, eng.state.lastSections, 0.2);
  if (canvas.drawOps < 5) throw new Error('Canvas did not render WASM section map');
  eng.animationLoop();

  // 1. Preset bare_fib (linkWasiRt = false)
  const rBare = await eng.compileAndRunC(eng.C_PRESETS.bare_fib.code, { linkWasiRt: false });
  if (!rBare.compileOk || rBare.exitCode !== 0 || !rBare.stdout.includes('ZCC Bare WASM Ready!') || !rBare.stdout.includes('55')) {
    throw new Error('bare_fib failed: ' + JSON.stringify({ compileOk: rBare.compileOk, rc: rBare.exitCode, out: rBare.stdout, err: rBare.stderr }));
  }

  // 2. Preset wasi_stdio_vfs (linkWasiRt = true -> printf + fopen/fprintf/fgets inside Browser VFS)
  const rStdio = await eng.compileAndRunC(eng.C_PRESETS.wasi_stdio_vfs.code, {
    linkWasiRt: true,
    argvLine: 'vfs_demo.wasm arg1',
  });
  if (!rStdio.compileOk || rStdio.exitCode !== 0 ||
      !rStdio.stdout.includes('ReadBack: WASI_VFS_LINE=42') ||
      !rStdio.stdout.includes('argv0=vfs_demo.wasm')) {
    throw new Error('wasi_stdio_vfs failed: ' + JSON.stringify({ compileOk: rStdio.compileOk, rc: rStdio.exitCode, out: rStdio.stdout, err: rStdio.stderr }));
  }
  const noteBytes = rStdio.vfs.get('note.txt');
  const noteText = noteBytes ? new TextDecoder().decode(noteBytes) : '';
  if (!noteText.includes('WASI_VFS_LINE=42')) {
    throw new Error('note.txt missing expected VFS content: ' + noteText);
  }

  // 3. x86-64 Assembly emission (-S)
  const rAsm = await eng.compileToAsm(eng.C_PRESETS.x86_64_asm.code, { linkWasiRt: false });
  if (!rAsm.compileOk || rAsm.exitCode !== 0 || !rAsm.asmText.includes('dot4:') || !rAsm.asmText.includes('ret')) {
    throw new Error('compileToAsm failed: ' + JSON.stringify({ ok: rAsm.compileOk, rc: rAsm.exitCode, asm: rAsm.asmText.slice(0, 200), err: rAsm.stderr }));
  }

  // 4. Embedded Lua 5.4.6 execution (closures/fib AND VFS dofile/loadfile)
  const rLua1 = await eng.runLua(eng.LUA_PRESETS.lua_fib_closures);
  if (rLua1.exitCode !== 0 || !rLua1.stdout.includes('LUA_PLAYGROUND\t610\t42')) {
    throw new Error('Lua fib preset failed: ' + JSON.stringify(rLua1));
  }
  const rLua2 = await eng.runLua(eng.LUA_PRESETS.lua_vfs_file);
  if (rLua2.exitCode !== 0 || !rLua2.stdout.includes('VFS_LOADED\t42')) {
    throw new Error('Lua VFS loadfile preset failed: ' + JSON.stringify(rLua2));
  }

  // 5. External WASI module (build_wasm/sqlite3.wasm)
  const sqBytes = new Uint8Array(fs.readFileSync(sqlitePath));
  const rSq = await eng.runExternalWasm(
    sqBytes,
    ["sqlite3.wasm", ":memory:", "CREATE TABLE pg(k TEXT, v INT); INSERT INTO pg VALUES('zcc',42),('pg',99); SELECT k, v FROM pg ORDER BY v;"]
  );
  if (rSq.exitCode !== 0 || rSq.stdout.trim() !== 'zcc|42\npg|99') {
    throw new Error('External sqlite3.wasm run failed: ' + JSON.stringify({ rc: rSq.exitCode, out: rSq.stdout, err: rSq.stderr }));
  }

  // 6. EF-0 Negative control: invalid C must fail cleanly without throwing
  const rBad = await eng.compileAndRunC("int main( { return ; }\n", { linkWasiRt: false });
  if (rBad.compileOk || rBad.exitCode === 0) {
    throw new Error('Negative control unexpected pass: ' + JSON.stringify(rBad));
  }

  console.log(JSON.stringify({
    HEADLESS_EVAL_ERRORS: errors.length,
    zccBytes: eng.state.zccBytes.length,
    luaBytes: eng.state.luaBytes.length,
    bareStdout: rBare.stdout.trim(),
    stdioStdout: rStdio.stdout.trim(),
    asmBytes: rAsm.asmText.length,
    luaOut: rLua1.stdout.trim(),
    sqliteOut: rSq.stdout.trim(),
    negRc: rBad.exitCode,
  }));
})().catch((e) => {
  console.error('HEADLESS_EVAL_ERRORS: 1', e);
  process.exit(1);
});
"""
        import json
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
            json.dump(self.inspector.scripts, tf)
            cfg = tf.name
        try:
            res = subprocess.run([self.node_bin, "-e", js, cfg, SQLITE_WASM], cwd=REPO_ROOT, capture_output=True, text=True)
            self.assertEqual(res.returncode, 0, res.stdout + "\n" + res.stderr)
            self.assertIn('"HEADLESS_EVAL_ERRORS":0', res.stdout)
            print(f"[PLAYGROUND EF-7 PASS] {res.stdout.strip()}", flush=True)
        finally:
            if os.path.exists(cfg):
                os.remove(cfg)


if __name__ == "__main__":
    unittest.main(verbosity=2)
