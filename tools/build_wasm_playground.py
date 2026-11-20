#!/usr/bin/env python3
"""
Zero-Server In-Browser ZCC Playground Generator (`tools/zcc_wasm_playground.html`)
==================================================================================
Bundles:
  - `tools/browser_wasi_shim.js` (pure-JS WASI Preview1 in-memory VFS runtime)
  - `build_wasm/zcc.wasm` (gzipped + base64)
  - `src/wasm_wasi_rt.c` (gzipped + base64, linkable inside Browser VFS)
  - `build_wasm/lua.wasm` (gzipped + base64, compiled by zcc.wasm itself)
into a self-contained single HTML file `tools/zcc_wasm_playground.html` that
runs directly over `file://` with zero HTTP server and zero external network/CDN calls.
"""

import argparse
import base64
import gzip
import hashlib
import json
import os

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHIM_JS_PATH = os.path.join(REPO_ROOT, "tools", "browser_wasi_shim.js")
ZCC_WASM_PATH = os.path.join(REPO_ROOT, "build_wasm", "zcc.wasm")
WASI_RT_PATH = os.path.join(REPO_ROOT, "src", "wasm_wasi_rt.c")
LUA_WASM_PATH = os.path.join(REPO_ROOT, "build_wasm", "lua.wasm")
DEFAULT_OUT = os.path.join(REPO_ROOT, "tools", "zcc_wasm_playground.html")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def gz_b64(data: bytes) -> str:
    compressed = gzip.compress(data, compresslevel=9, mtime=0)
    return base64.b64encode(compressed).decode("ascii")


def load_shim_js(path: str) -> str:
    with open(path, "r", encoding="utf-8") as f:
        lines = f.read().splitlines(keepends=True)
    if lines and lines[0].startswith("#!"):
        lines = lines[1:]
    return "".join(lines).replace("</script", "<\\/script")


HTML_TEMPLATE = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ZCC Zero-Server WASI Playground</title>
<style>
  :root {
    --bg: #0b0f17;
    --surface: #121826;
    --surface2: #182032;
    --border: #28354d;
    --text: #e6edf7;
    --muted: #8ea0bc;
    --accent: #38bdf8;
    --accent2: #22c55e;
    --warn: #f59e0b;
    --danger: #ef4444;
    --mono: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace;
    --sans: system-ui, -apple-system, Segoe UI, Roboto, sans-serif;
  }
  * { box-sizing: border-box; }
  html, body {
    margin: 0;
    padding: 0;
    background: var(--bg);
    color: var(--text);
    font-family: var(--sans);
    min-height: 100vh;
  }
  .bg-grid-overlay {
    position: fixed;
    inset: 0;
    z-index: 1;
    pointer-events: none !important;
    background-image:
      linear-gradient(to right, rgba(56, 189, 248, 0.05) 1px, transparent 1px),
      linear-gradient(to bottom, rgba(56, 189, 248, 0.05) 1px, transparent 1px);
    background-size: 28px 28px;
  }
  .app-shell {
    position: relative;
    z-index: 1000;
    pointer-events: auto;
    max-width: 1360px;
    margin: 0 auto;
    padding: 16px 20px 32px;
  }
  header.topbar {
    position: relative;
    z-index: 1000;
    pointer-events: auto;
    display: flex;
    flex-wrap: wrap;
    justify-content: space-between;
    align-items: center;
    gap: 12px;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 14px 18px;
    margin-bottom: 14px;
  }
  .brand h1 {
    margin: 0;
    font-size: 18px;
    letter-spacing: 0.02em;
    color: #f8fafc;
  }
  .brand .sub {
    margin-top: 3px;
    font-size: 12px;
    color: var(--muted);
    font-family: var(--mono);
  }
  .status-pill {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    padding: 6px 12px;
    border-radius: 999px;
    border: 1px solid var(--border);
    background: var(--surface2);
    font-family: var(--mono);
    font-size: 12px;
  }
  .status-pill.ok { border-color: rgba(34, 197, 94, 0.5); color: #86efac; }
  .status-pill.err { border-color: rgba(239, 68, 68, 0.55); color: #fca5a5; }
  .status-pill.busy { border-color: rgba(245, 158, 11, 0.55); color: #fcd34d; }

  .tabs {
    position: relative;
    z-index: 1000;
    pointer-events: auto;
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
    margin-bottom: 14px;
  }
  .tab-btn {
    position: relative;
    z-index: 1000;
    pointer-events: auto;
    background: var(--surface);
    color: var(--muted);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 9px 15px;
    font-weight: 600;
    font-size: 13px;
    cursor: pointer;
    user-select: auto;
  }
  .tab-btn.active {
    background: #132840;
    color: #e0f2fe;
    border-color: var(--accent);
  }

  .main-grid {
    position: relative;
    z-index: 1000;
    pointer-events: auto;
    display: grid;
    grid-template-columns: 1.12fr 0.88fr;
    gap: 14px;
  }
  @media (max-width: 980px) {
    .main-grid { grid-template-columns: 1fr; }
  }
  .panel {
    position: relative;
    z-index: 1000;
    pointer-events: auto;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 14px;
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
  .panel.hidden { display: none; }
  .toolbar, .actions, .controls {
    position: relative;
    z-index: 1000;
    pointer-events: auto;
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 8px;
  }
  label {
    font-size: 12px;
    color: var(--muted);
    display: inline-flex;
    align-items: center;
    gap: 6px;
    user-select: auto;
  }
  input, textarea, button, select, pre, code {
    user-select: auto;
  }
  select, input[type="text"], input[type="file"] {
    background: var(--bg);
    color: var(--text);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 6px 9px;
    font-family: var(--mono);
    font-size: 12px;
  }
  textarea {
    width: 100%;
    background: var(--bg);
    color: #e2e8f0;
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 10px;
    font-family: var(--mono);
    font-size: 12.5px;
    line-height: 1.45;
    resize: vertical;
  }
  button {
    position: relative;
    z-index: 1000;
    pointer-events: auto;
    background: #1d283d;
    color: var(--text);
    border: 1px solid var(--border);
    border-radius: 7px;
    padding: 7px 13px;
    font-size: 12.5px;
    font-weight: 600;
    cursor: pointer;
  }
  button.primary {
    background: #0284c7;
    border-color: #38bdf8;
    color: #ffffff;
  }
  button.good {
    background: #15803d;
    border-color: #22c55e;
    color: #ffffff;
  }
  button:disabled { opacity: 0.55; cursor: not-allowed; }
  .metrics-row {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 8px;
  }
  .metric-box {
    background: var(--surface2);
    border: 1px solid var(--border);
    border-radius: 7px;
    padding: 7px 10px;
  }
  .metric-box .k { font-size: 10.5px; color: var(--muted); text-transform: uppercase; }
  .metric-box .v { font-size: 13px; font-family: var(--mono); margin-top: 2px; color: #f8fafc; }
  pre.console {
    margin: 0;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 10px;
    font-family: var(--mono);
    font-size: 12px;
    line-height: 1.4;
    white-space: pre-wrap;
    word-break: break-word;
    max-height: 220px;
    overflow: auto;
    color: #d1fae5;
  }
  pre.console.stderr { color: #fecaca; max-height: 130px; }
  pre.console.asm { color: #bae6fd; max-height: 190px; }
  .dropzone {
    position: relative;
    z-index: 1000;
    pointer-events: auto;
    border: 1.5px dashed #3b82f6;
    border-radius: 8px;
    padding: 14px;
    text-align: center;
    background: rgba(15, 23, 42, 0.7);
    font-size: 12.5px;
    color: var(--muted);
  }
  canvas#wasmMapCanvas, canvas#fbCanvas {
    position: relative;
    z-index: 1000;
    pointer-events: auto;
    width: 100%;
    height: auto;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    display: block;
  }
  canvas#fbCanvas {
    max-width: 640px;
    aspect-ratio: 16 / 10;
    image-rendering: pixelated;
    background: #000000;
    outline: none;
  }
  .vfs-list {
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 8px;
    max-height: 140px;
    overflow: auto;
    font-family: var(--mono);
    font-size: 11.5px;
  }
  .vfs-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 3px 4px;
    border-bottom: 1px solid #1b2436;
  }
  .vfs-item:last-child { border-bottom: none; }
</style>
</head>
<body>
<div class="bg-grid-overlay" aria-hidden="true"></div>
<div class="app-shell">
  <header class="topbar">
    <div class="brand">
      <h1>ZCC Sovereign In-Browser WASI Playground</h1>
      <div class="sub" id="metaBanner">Zero-Server file:// Bundle • BrowserWasiShim • zcc.wasm + lua.wasm + wasm_wasi_rt.c</div>
    </div>
    <div id="statusBadge" class="status-pill ok">READY (Decompressing on first run)</div>
  </header>

  <nav class="tabs" role="tablist">
    <button id="tabBtnC" class="tab-btn active" type="button" data-action="switchTab:c" onclick="zccUiAction('switchTab:c', event)">C99 Compiler (zcc.wasm)</button>
    <button id="tabBtnLua" class="tab-btn" type="button" data-action="switchTab:lua" onclick="zccUiAction('switchTab:lua', event)">Lua 5.4.6 WASI VM (lua.wasm)</button>
    <button id="tabBtnExt" class="tab-btn" type="button" data-action="switchTab:ext" onclick="zccUiAction('switchTab:ext', event)">External WASI Runner (sqlite3.wasm / qjs.wasm / .wasm)</button>
    <button id="tabBtnFb" class="tab-btn" type="button" data-action="switchTab:fb" onclick="zccUiAction('switchTab:fb', event)">Live Canvas / DOOM (Framebuffer)</button>
  </nav>

  <div class="main-grid">
    <div class="left-col">
      <!-- TAB 1: C99 COMPILER -->
      <section id="panel-c" class="panel">
        <div class="toolbar">
          <label>Preset:
            <select id="cPresetSelect" data-action="loadCPreset" onchange="zccUiAction('loadCPreset', event)">
              <option value="bare_fib">Bare WASM Fibonacci (puts + return)</option>
              <option value="wasi_stdio_vfs">WASI Stdio &amp; File VFS (printf + fopen/fread)</option>
              <option value="structs_variadics">Struct sret + Variadics + goto</option>
              <option value="x86_64_asm">x86-64 System V Assembly (-S)</option>
            </select>
          </label>
          <label>
            <input id="linkWasiRtChk" type="checkbox" data-action="toggleWasiRt" onchange="zccUiAction('toggleWasiRt', event)">
            Link Freestanding WASI C Runtime (wasm_wasi_rt.c)
          </label>
        </div>
        <textarea id="cSourceInput" rows="16" spellcheck="false"></textarea>
        <div class="controls">
          <label style="flex:1">Program argv: <input id="cArgsInput" type="text" value="prog.wasm" style="width:100%"></label>
          <label style="flex:1">Stdin: <input id="cStdinInput" type="text" value="" style="width:100%"></label>
        </div>
        <div class="actions">
          <button id="btnCompileRun" class="primary" type="button" data-action="compileAndRunC" onclick="zccUiAction('compileAndRunC', event)">Compile &amp; Run WASM (--target=wasm32)</button>
          <button id="btnEmitAsm" type="button" data-action="compileToAsm" onclick="zccUiAction('compileToAsm', event)">Emit x86-64 (.s)</button>
          <button id="btnDownloadOut" type="button" data-action="downloadArtifact" onclick="zccUiAction('downloadArtifact', event)">Download Artifact</button>
          <button id="btnResetCPreset" type="button" data-action="loadCPreset" onclick="zccUiAction('loadCPreset', event)">Reset Preset</button>
        </div>
      </section>

      <!-- TAB 2: LUA 5.4.6 VM -->
      <section id="panel-lua" class="panel hidden">
        <div class="toolbar">
          <label>Lua 5.4 Preset:
            <select id="luaPresetSelect" data-action="loadLuaPreset" onchange="zccUiAction('loadLuaPreset', event)">
              <option value="lua_fib_closures">Fibonacci, Closures &amp; Branching</option>
              <option value="lua_metatables">Metatables &amp; Iterators</option>
              <option value="lua_vfs_file">Execute Script from WASI VFS (dofile)</option>
            </select>
          </label>
        </div>
        <textarea id="luaSourceInput" rows="16" spellcheck="false"></textarea>
        <div class="actions">
          <button id="btnRunLua" class="good" type="button" data-action="runLuaPresetOrCode" onclick="zccUiAction('runLuaPresetOrCode', event)">Run Lua 5.4 in Browser</button>
          <button id="btnResetLuaPreset" type="button" data-action="loadLuaPreset" onclick="zccUiAction('loadLuaPreset', event)">Reset Lua Preset</button>
        </div>
      </section>

      <!-- TAB 3: EXTERNAL WASI RUNNER -->
      <section id="panel-ext" class="panel hidden">
        <div id="extDropZone" class="dropzone">
          Drop any WASI Preview1 <code>.wasm</code> (e.g. <code>build_wasm/sqlite3.wasm</code> or <code>build_wasm/qjs.wasm</code>) here or select locally with zero server:
          <div style="margin-top:8px">
            <input id="extWasmFile" type="file" accept=".wasm" data-action="loadExternalFile" onchange="zccUiAction('loadExternalFile', event)">
          </div>
          <div id="extLoadedLabel" style="margin-top:6px; color:#93c5fd;">No external .wasm loaded yet.</div>
        </div>
        <label>CLI argv (shell-quoted, e.g. <code>:memory: "CREATE TABLE t(x); INSERT INTO t VALUES(42); SELECT * FROM t;"</code>):
          <input id="extArgsInput" type="text" value=':memory: "SELECT sqlite_version();" "CREATE TABLE demo(k TEXT, v INT); INSERT INTO demo VALUES('\''zcc'\'',42),('\''wasi'\'',99); SELECT k, v FROM demo ORDER BY v;"' style="width:100%">
        </label>
        <label>Stdin:
          <textarea id="extStdinInput" rows="4" spellcheck="false"></textarea>
        </label>
        <div class="actions">
          <button id="btnRunExt" class="primary" type="button" data-action="runExternalLoaded" onclick="zccUiAction('runExternalLoaded', event)">Run External WASI Module</button>
        </div>
      </section>

      <!-- TAB 4: LIVE CANVAS / DOOM (FRAMEBUFFER) -->
      <section id="panel-fb" class="panel hidden">
        <div class="toolbar">
          <label>Live C99 Graphics Preset:
            <select id="fbPresetSelect" data-action="loadFbPreset" onchange="zccUiAction('loadFbPreset', event)">
              <option value="live_raytracer">Interactive C99 Raytracer (160x100 WASD/Fire)</option>
              <option value="plasma_shader">Interactive C99 Plasma Interference (160x100)</option>
            </select>
          </label>
        </div>
        <textarea id="fbSourceInput" rows="12" spellcheck="false"></textarea>
        <div class="actions">
          <button id="btnFbCompileRun" class="primary" type="button" data-action="compileAndRunFbC" onclick="zccUiAction('compileAndRunFbC', event)">Compile &amp; Run Live C99 (zcc.wasm)</button>
          <button id="btnFbPause" type="button" data-action="toggleFbPause" onclick="zccUiAction('toggleFbPause', event)">Pause</button>
          <button id="btnFbStep" type="button" data-action="stepFbOnce" onclick="zccUiAction('stepFbOnce', event)">Step Frame</button>
          <button id="btnFbStop" type="button" data-action="stopFbSession" onclick="zccUiAction('stopFbSession', event)">Stop</button>
        </div>

        <div id="fbDropZone" class="dropzone">
          Load Local Framebuffer WASI Binary (e.g. <code>build_wasm/doom.wasm</code>) + Optional WAD Asset (<code>doom1.wad</code>):
          <div class="controls" style="justify-content:center; margin-top:8px;">
            <label>WASM: <input id="fbWasmFile" type="file" accept=".wasm" data-action="loadFbWasmFile" onchange="zccUiAction('loadFbWasmFile', event)"></label>
            <label>WAD/Asset: <input id="fbWadFile" type="file" accept=".wad,.WAD" data-action="loadFbWadFile" onchange="zccUiAction('loadFbWadFile', event)"></label>
          </div>
          <div id="fbLoadedLabel" style="margin-top:6px; color:#93c5fd;">No external framebuffer .wasm / .wad loaded yet.</div>
          <div class="actions" style="justify-content:center; margin-top:8px;">
            <button id="btnFbRunTitle" class="good" type="button" data-action="bootExternalFbTitle" onclick="zccUiAction('bootExternalFbTitle', event)">Boot Title / Attract</button>
            <button id="btnFbRunE1M1" class="primary" type="button" data-action="bootExternalFbE1M1" onclick="zccUiAction('bootExternalFbE1M1', event)">Warp E1M1 (DOOM 3D)</button>
          </div>
        </div>

        <canvas id="fbCanvas" width="320" height="200" tabindex="0"></canvas>
        <div id="fbStatusText" class="sub" style="font-family:var(--mono); font-size:11.5px; color:var(--muted);">Framebuffer idle • Controls: WASD / Arrows (Move/Turn), Ctrl/F (Fire), Space/E (Use), Shift (Run), 1-7 (Weapons)</div>
      </section>

      <!-- VFS MANAGER -->
      <section class="panel">
        <div class="toolbar" style="justify-content:space-between">
          <strong style="font-size:12.5px">Browser WASI Virtual Filesystem (preopen <code>.</code>)</strong>
          <button id="btnClearVfs" type="button" data-action="clearUserVfs" onclick="zccUiAction('clearUserVfs', event)">Clear User VFS Files</button>
        </div>
        <div class="controls">
          <input id="vfsFileNameInput" type="text" placeholder="filename (e.g. hello.lua)" style="flex:0.9">
          <input id="vfsFileContentInput" type="text" placeholder="file text content" style="flex:1.5">
          <button id="btnAddVfsFile" type="button" data-action="addVfsFile" onclick="zccUiAction('addVfsFile', event)">Add / Update File</button>
        </div>
        <div id="vfsListOutput" class="vfs-list">(no custom VFS files yet)</div>
      </section>
    </div>

    <div class="right-col panel">
      <div class="metrics-row">
        <div class="metric-box"><div class="k">Compile</div><div class="v" id="metricCompileMs">-</div></div>
        <div class="metric-box"><div class="k">Run</div><div class="v" id="metricRunMs">-</div></div>
        <div class="metric-box"><div class="k">Artifact</div><div class="v" id="metricSizeBytes">-</div></div>
        <div class="metric-box"><div class="k">Exit Code</div><div class="v" id="metricExitCode">-</div></div>
      </div>

      <div>
        <strong style="font-size:12px; color:var(--muted)">stdout</strong>
        <pre id="stdoutOutput" class="console"></pre>
      </div>
      <div>
        <strong style="font-size:12px; color:var(--muted)">stderr / diagnostics</strong>
        <pre id="stderrOutput" class="console stderr"></pre>
      </div>
      <div>
        <strong style="font-size:12px; color:var(--muted)">Generated Assembly / WASM Inspection</strong>
        <pre id="asmOrInfoOutput" class="console asm"></pre>
      </div>
      <div>
        <strong style="font-size:12px; color:var(--muted)">WASM Binary Section Map</strong>
        <canvas id="wasmMapCanvas" width="480" height="150"></canvas>
      </div>
    </div>
  </div>
</div>

<script id="zcc-wasi-shim-script">
__EMBEDDED_SHIM_JS__
</script>

<script id="zcc-playground-script">
(function () {
  "use strict";

  const BUILD_META = __BUILD_META_JSON__;
  const ZCC_WASM_GZ_B64 = "__ZCC_WASM_GZ_B64__";
  const WASI_RT_GZ_B64 = "__WASI_RT_GZ_B64__";
  const LUA_WASM_GZ_B64 = "__LUA_WASM_GZ_B64__";

  const C_PRESETS = {
    bare_fib: {
      linkWasiRt: false,
      code: [
        "int puts(const char *s);",
        "int putchar(int c);",
        "",
        "int fib(int n) {",
        "    if (n <= 1) return n;",
        "    return fib(n - 1) + fib(n - 2);",
        "}",
        "",
        "static void print_int(int v) {",
        "    char buf[16];",
        "    int i = 0;",
        "    if (v == 0) { putchar('0'); return; }",
        "    while (v > 0) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }",
        "    while (i > 0) putchar(buf[--i]);",
        "}",
        "",
        "int main(void) {",
        "    puts(\"ZCC Bare WASM Ready!\");",
        "    print_int(fib(10));",
        "    putchar('\\n');",
        "    return 0;",
        "}",
        ""
      ].join("\n"),
    },
    wasi_stdio_vfs: {
      linkWasiRt: true,
      code: [
        "#include <stdio.h>",
        "#include <stdlib.h>",
        "#include <string.h>",
        "",
        "int main(int argc, char **argv) {",
        "    FILE *fp = fopen(\"note.txt\", \"w\");",
        "    if (!fp) { fprintf(stderr, \"fopen failed\\n\"); return 1; }",
        "    fprintf(fp, \"WASI_VFS_LINE=42\\n\");",
        "    fclose(fp);",
        "",
        "    FILE *rp = fopen(\"note.txt\", \"r\");",
        "    char line[64];",
        "    if (rp && fgets(line, sizeof(line), rp)) {",
        "        printf(\"ReadBack: %s\", line);",
        "        fclose(rp);",
        "    }",
        "    printf(\"argc=%d argv0=%s\\n\", argc, argc > 0 ? argv[0] : \"(none)\");",
        "    return 0;",
        "}",
        ""
      ].join("\n"),
    },
    structs_variadics: {
      linkWasiRt: true,
      code: [
        "#include <stdio.h>",
        "#include <stdarg.h>",
        "",
        "typedef struct { int a; int b; double scale; } Pair;",
        "",
        "static Pair make_pair(int x, int y, double s) {",
        "    Pair p; p.a = x; p.b = y; p.scale = s;",
        "    return p;",
        "}",
        "",
        "static int sum_va(int count, ...) {",
        "    va_list ap; va_start(ap, count);",
        "    int acc = 0;",
        "    for (int i = 0; i < count; i++) acc += va_arg(ap, int);",
        "    va_end(ap);",
        "    return acc;",
        "}",
        "",
        "int main(void) {",
        "    Pair p = make_pair(14, 28, 1.5);",
        "    int total = sum_va(4, 10, 20, 30, 40);",
        "    int step = 0;",
        "    if (total == 100) goto ok;",
        "    return 2;",
        "ok:",
        "    printf(\"Pair=(%d,%d) sum_va=%d step=%d\\n\", p.a + p.b, (int)(p.b * p.scale), total, step + 1);",
        "    return 0;",
        "}",
        ""
      ].join("\n"),
    },
    x86_64_asm: {
      linkWasiRt: false,
      code: [
        "int dot4(const int *a, const int *b) {",
        "    int sum = 0;",
        "    for (int i = 0; i < 4; i++) {",
        "        sum += a[i] * b[i];",
        "    }",
        "    return sum;",
        "}",
        ""
      ].join("\n"),
    },
  };

  const LUA_PRESETS = {
    lua_fib_closures: [
      "local function fib(n)",
      "  if n <= 1 then return n end",
      "  return fib(n - 1) + fib(n - 2)",
      "end",
      "local function make_adder(k)",
      "  return function(v) return k + v end",
      "end",
      "local add10 = make_adder(10)",
      "print('LUA_PLAYGROUND', fib(15), add10(32))",
      "assert(fib(15) == 610)",
      ""
    ].join("\n"),
    lua_metatables: [
      "local Vec = {}",
      "Vec.__index = Vec",
      "function Vec.new(x, y) return setmetatable({x = x, y = y}, Vec) end",
      "function Vec.__add(a, b) return Vec.new(a.x + b.x, a.y + b.y) end",
      "local c = Vec.new(10, 12) + Vec.new(32, 30)",
      "print('VEC_SUM', c.x, c.y)",
      "assert(c.x == 42 and c.y == 42)",
      ""
    ].join("\n"),
    lua_vfs_file: [
      "local chunk, err = loadfile('vfs_snippet.lua')",
      "if not chunk then error(err) end",
      "local res = chunk()",
      "print('VFS_LOADED', res)",
      ""
    ].join("\n"),
  };

  const FB_PRESETS = {
    live_raytracer: {
      code: [
        "#define FB_W 160",
        "#define FB_H 100",
        "static unsigned int g_fb[160 * 100];",
        "static float g_cam_x;",
        "static float g_cam_y;",
        "static float g_cam_z;",
        "static float g_yaw;",
        "static float g_time;",
        "static int g_frame;",
        "static int g_inited;",
        "",
        "static float clamp01(float v) {",
        "    if (v < 0.0f) return 0.0f;",
        "    if (v > 1.0f) return 1.0f;",
        "    return v;",
        "}",
        "",
        "static float sqrtf_fast(float x) {",
        "    if (x <= 0.0f) return 0.0f;",
        "    float s = 1.0f;",
        "    while (x > 4.0f) { x *= 0.25f; s *= 2.0f; }",
        "    while (x < 0.25f) { x *= 4.0f; s *= 0.5f; }",
        "    float r = 0.5f * (1.0f + x);",
        "    for (int i = 0; i < 6; i++) r = 0.5f * (r + x / r);",
        "    return r * s;",
        "}",
        "",
        "static float sinf_fast(float x) {",
        "    float pi = 3.14159265f;",
        "    float two_pi = 6.28318530f;",
        "    while (x > pi) x -= two_pi;",
        "    while (x < -pi) x += two_pi;",
        "    float x2 = x * x;",
        "    return x * (1.0f - x2 * (0.16666667f - x2 * (0.00833333f - x2 * 0.00019841f)));",
        "}",
        "",
        "static float cosf_fast(float x) {",
        "    return sinf_fast(x + 1.57079632f);",
        "}",
        "",
        "static float sphere_hit(float ox, float oy, float oz, float dx, float dy, float dz,",
        "                        float cx, float cy, float cz, float rad) {",
        "    float rx = ox - cx;",
        "    float ry = oy - cy;",
        "    float rz = oz - cz;",
        "    float b = rx * dx + ry * dy + rz * dz;",
        "    float c = rx * rx + ry * ry + rz * rz - rad * rad;",
        "    float disc = b * b - c;",
        "    if (disc <= 0.0f) return -1.0f;",
        "    float t = -b - sqrtf_fast(disc);",
        "    return (t > 0.01f) ? t : -1.0f;",
        "}",
        "",
        "int fb_width(void) { return FB_W; }",
        "int fb_height(void) { return FB_H; }",
        "unsigned int fb_ptr(void) { return (unsigned int)g_fb; }",
        "",
        "int fb_init(int ep, int map) {",
        "    (void)ep; (void)map;",
        "    g_cam_x = 0.0f;",
        "    g_cam_y = 0.55f;",
        "    g_cam_z = -3.4f;",
        "    g_yaw = 0.0f;",
        "    g_time = 0.0f;",
        "    g_frame = 0;",
        "    g_inited = 1;",
        "    return 0;",
        "}",
        "",
        "int fb_tick(int dt_ms, int key_mask, int mouse_x, int mouse_y) {",
        "    if (!g_inited) fb_init(0, 0);",
        "    float dt = (dt_ms > 0) ? ((float)dt_ms * 0.001f) : 0.033f;",
        "    g_time += dt;",
        "    if (key_mask & 4) g_yaw -= 1.8f * dt;",
        "    if (key_mask & 8) g_yaw += 1.8f * dt;",
        "    float sy = sinf_fast(g_yaw);",
        "    float cy = cosf_fast(g_yaw);",
        "    float spd = ((key_mask & 256) ? 3.8f : 2.2f) * dt;",
        "    if (key_mask & 1) { g_cam_x += sy * spd; g_cam_z += cy * spd; }",
        "    if (key_mask & 2) { g_cam_x -= sy * spd; g_cam_z -= cy * spd; }",
        "    if (key_mask & 512) { g_cam_x -= cy * spd; g_cam_z += sy * spd; }",
        "    if (key_mask & 1024) { g_cam_x += cy * spd; g_cam_z -= sy * spd; }",
        "",
        "    float s0x = cosf_fast(g_time * 1.1f) * 1.15f;",
        "    float s0y = 0.15f + 0.28f * sinf_fast(g_time * 2.0f);",
        "    float s0z = 0.6f + sinf_fast(g_time * 1.1f) * 0.7f;",
        "    float s1x = -cosf_fast(g_time * 0.8f) * 1.05f;",
        "    float s1y = -0.15f;",
        "    float s1z = 1.4f;",
        "    float pulse = (key_mask & 16) ? 1.55f : 1.0f;",
        "    float mshift = (float)(mouse_x - 80) * 0.004f;",
        "    float lx = 0.45f + mshift;",
        "    float ly = 0.78f;",
        "    float lz = -0.42f;",
        "    float linv = 1.0f / sqrtf_fast(lx * lx + ly * ly + lz * lz);",
        "    lx *= linv; ly *= linv; lz *= linv;",
        "    (void)mouse_y;",
        "",
        "    for (int py = 0; py < FB_H; py++) {",
        "        float v = (0.5f - ((float)py + 0.5f) / (float)FB_H) * 1.15f;",
        "        for (int px = 0; px < FB_W; px++) {",
        "            float u = (((float)px + 0.5f) / (float)FB_W - 0.5f) * 1.84f;",
        "            float rx = u * cy + sy;",
        "            float ry = v - 0.12f;",
        "            float rz = cy - u * sy;",
        "            float rinv = 1.0f / sqrtf_fast(rx * rx + ry * ry + rz * rz);",
        "            rx *= rinv; ry *= rinv; rz *= rinv;",
        "",
        "            float best_t = 1e9f;",
        "            int hit_kind = 0;",
        "            float t0 = sphere_hit(g_cam_x, g_cam_y, g_cam_z, rx, ry, rz, s0x, s0y, s0z, 0.62f);",
        "            if (t0 > 0.0f && t0 < best_t) { best_t = t0; hit_kind = 1; }",
        "            float t1 = sphere_hit(g_cam_x, g_cam_y, g_cam_z, rx, ry, rz, s1x, s1y, s1z, 0.50f);",
        "            if (t1 > 0.0f && t1 < best_t) { best_t = t1; hit_kind = 2; }",
        "            if (ry < -0.002f) {",
        "                float tf = (-0.85f - g_cam_y) / ry;",
        "                if (tf > 0.02f && tf < best_t) { best_t = tf; hit_kind = 3; }",
        "            }",
        "",
        "            float cr, cg, cb;",
        "            if (hit_kind == 0) {",
        "                float sky = clamp01(0.5f * (ry + 1.0f));",
        "                cr = 0.05f + 0.12f * (1.0f - sky);",
        "                cg = 0.10f + 0.35f * sky;",
        "                cb = 0.22f + 0.65f * sky;",
        "            } else {",
        "                float hx = g_cam_x + rx * best_t;",
        "                float hy = g_cam_y + ry * best_t;",
        "                float hz = g_cam_z + rz * best_t;",
        "                float nx = 0.0f, ny = 1.0f, nz = 0.0f;",
        "                float ar = 0.2f, ag = 0.8f, ab = 0.9f;",
        "                if (hit_kind == 1) {",
        "                    nx = (hx - s0x) / 0.62f; ny = (hy - s0y) / 0.62f; nz = (hz - s0z) / 0.62f;",
        "                    ar = 0.96f; ag = 0.36f; ab = 0.24f;",
        "                } else if (hit_kind == 2) {",
        "                    nx = (hx - s1x) / 0.50f; ny = (hy - s1y) / 0.50f; nz = (hz - s1z) / 0.50f;",
        "                    ar = 0.22f; ag = 0.85f; ab = 0.96f;",
        "                } else {",
        "                    int cx = (int)(hx * 2.0f + 1000.0f);",
        "                    int cz = (int)(hz * 2.0f + 1000.0f);",
        "                    int chk = (cx + cz) & 1;",
        "                    ar = chk ? 0.78f : 0.16f;",
        "                    ag = chk ? 0.82f : 0.20f;",
        "                    ab = chk ? 0.86f : 0.28f;",
        "                }",
        "                float sh0 = sphere_hit(hx + nx * 0.01f, hy + ny * 0.01f, hz + nz * 0.01f, lx, ly, lz, s0x, s0y, s0z, 0.62f);",
        "                float sh1 = sphere_hit(hx + nx * 0.01f, hy + ny * 0.01f, hz + nz * 0.01f, lx, ly, lz, s1x, s1y, s1z, 0.50f);",
        "                float shade = (sh0 > 0.0f || sh1 > 0.0f) ? 0.22f : 1.0f;",
        "                float diff = clamp01(nx * lx + ny * ly + nz * lz) * shade * pulse;",
        "                cr = clamp01(ar * (0.18f + 0.95f * diff));",
        "                cg = clamp01(ag * (0.18f + 0.95f * diff));",
        "                cb = clamp01(ab * (0.20f + 0.95f * diff));",
        "            }",
        "            unsigned int ir = (unsigned int)(cr * 255.0f);",
        "            unsigned int ig = (unsigned int)(cg * 255.0f);",
        "            unsigned int ib = (unsigned int)(cb * 255.0f);",
        "            g_fb[py * FB_W + px] = 0xFF000000U | (ib << 16) | (ig << 8) | ir;",
        "        }",
        "    }",
        "    g_frame++;",
        "    return g_frame;",
        "}",
        ""
      ].join("\n"),
    },
    plasma_shader: {
      code: [
        "#define FB_W 160",
        "#define FB_H 100",
        "static unsigned int g_fb[160 * 100];",
        "static int g_frame;",
        "static int g_ox;",
        "static int g_oy;",
        "",
        "static int wave256(int x) {",
        "    int p = x & 255;",
        "    int q = (p < 128) ? p : (255 - p);",
        "    return (q - 64) * 2;",
        "}",
        "",
        "int fb_width(void) { return FB_W; }",
        "int fb_height(void) { return FB_H; }",
        "unsigned int fb_ptr(void) { return (unsigned int)g_fb; }",
        "",
        "int fb_init(int ep, int map) {",
        "    (void)ep; (void)map;",
        "    g_frame = 0;",
        "    g_ox = 0;",
        "    g_oy = 0;",
        "    return 0;",
        "}",
        "",
        "int fb_tick(int dt_ms, int key_mask, int mouse_x, int mouse_y) {",
        "    (void)dt_ms;",
        "    if (key_mask & 1) g_oy -= 4;",
        "    if (key_mask & 2) g_oy += 4;",
        "    if (key_mask & 4) g_ox -= 4;",
        "    if (key_mask & 8) g_ox += 4;",
        "    int t = g_frame * 3 + ((key_mask & 16) ? 19 : 0) + (mouse_x >> 2);",
        "    int ym = (mouse_y >> 2);",
        "    for (int y = 0; y < FB_H; y++) {",
        "        int yy = y + g_oy;",
        "        for (int x = 0; x < FB_W; x++) {",
        "            int xx = x + g_ox;",
        "            int v = wave256(xx * 3 + t) + wave256(yy * 4 - t * 2) + wave256((xx + yy) * 2 + ym);",
        "            unsigned int r = (unsigned int)((wave256(v + 64) + 128) & 255);",
        "            unsigned int g = (unsigned int)((wave256(v + 128 + t) + 128) & 255);",
        "            unsigned int b = (unsigned int)((wave256(v + 192 - t) + 128) & 255);",
        "            g_fb[y * FB_W + x] = 0xFF000000U | (b << 16) | (g << 8) | r;",
        "        }",
        "    }",
        "    g_frame++;",
        "    return g_frame;",
        "}",
        ""
      ].join("\n"),
    },
  };

  const state = {
    zccBytes: null,
    wasiRtText: null,
    luaBytes: null,
    extBytes: null,
    extName: "",
    fbWasmBytes: null,
    fbWasmName: "",
    fbWadBytes: null,
    fbWadName: "doom1.wad",
    fbSession: null,
    fbKeysDown: new Set(),
    activeTab: "c",
    userVfs: new Map([["vfs_snippet.lua", new TextEncoder().encode("return 7 * 6\n")]]),
    lastArtifactBytes: null,
    lastArtifactName: "out.wasm",
    lastSections: [],
    animPhase: 0,
    dispatchedCount: 0,
  };

  const SECTION_NAMES = {
    0: "Custom", 1: "Type", 2: "Import", 3: "Function", 4: "Table",
    5: "Memory", 6: "Global", 7: "Export", 8: "Start", 9: "Element",
    10: "Code", 11: "Data",
  };
  const SECTION_COLORS = {
    1: "#38bdf8", 2: "#818cf8", 3: "#2dd4bf", 4: "#f59e0b", 5: "#f97316",
    6: "#ec4899", 7: "#22c55e", 9: "#a855f7", 10: "#0ea5e9", 11: "#eab308",
  };

  function el(id) {
    return typeof document !== "undefined" ? document.getElementById(id) : null;
  }

  function nowMs() {
    return (typeof performance !== "undefined" && performance.now) ? performance.now() : Date.now();
  }

  function setStatus(text, kind) {
    const badge = el("statusBadge");
    if (!badge) return;
    badge.textContent = text;
    badge.className = "status-pill " + (kind || "ok");
  }

  async function gunzipBase64(b64) {
    let raw;
    if (typeof atob === "function") {
      const bin = atob(b64);
      raw = new Uint8Array(bin.length);
      for (let i = 0; i < bin.length; i++) raw[i] = bin.charCodeAt(i);
    } else {
      raw = new Uint8Array(Buffer.from(b64, "base64"));
    }
    if (typeof DecompressionStream !== "undefined") {
      const ds = new DecompressionStream("gzip");
      const writer = ds.writable.getWriter();
      const readAll = new Response(ds.readable).arrayBuffer();
      await writer.write(raw);
      await writer.close();
      return new Uint8Array(await readAll);
    }
    if (typeof require === "function") {
      const zlib = require("zlib");
      return new Uint8Array(zlib.gunzipSync(raw));
    }
    throw new Error("DecompressionStream('gzip') is not supported");
  }

  async function initAssets() {
    if (state.zccBytes && state.wasiRtText !== null && state.luaBytes) return state;
    const [zb, rtb, lb] = await Promise.all([
      gunzipBase64(ZCC_WASM_GZ_B64),
      gunzipBase64(WASI_RT_GZ_B64),
      gunzipBase64(LUA_WASM_GZ_B64),
    ]);
    state.zccBytes = zb;
    state.wasiRtText = new TextDecoder("utf-8").decode(rtb);
    state.luaBytes = lb;
    if (!state.lastSections.length) {
      state.lastSections = parseWasmSections(zb);
    }
    return state;
  }

  function splitCliArgs(line) {
    const out = [];
    const s = String(line || "");
    let cur = "";
    let quote = null;
    let inTok = false;
    for (let i = 0; i < s.length; i++) {
      const ch = s[i];
      if (quote) {
        if (ch === "\\" && quote === '"' && i + 1 < s.length) {
          cur += s[++i];
        } else if (ch === quote) {
          quote = null;
        } else {
          cur += ch;
        }
        inTok = true;
      } else if (ch === "'" || ch === '"') {
        quote = ch;
        inTok = true;
      } else if (/\s/.test(ch)) {
        if (inTok) { out.push(cur); cur = ""; inTok = false; }
      } else {
        cur += ch;
        inTok = true;
      }
    }
    if (inTok) out.push(cur);
    return out;
  }

  function parseWasmSections(bytes) {
    if (!bytes || bytes.length < 8) return [];
    let off = 8;
    const secs = [];
    while (off < bytes.length) {
      const id = bytes[off++];
      let sz = 0, shift = 0;
      while (off < bytes.length) {
        const b = bytes[off++];
        sz |= (b & 0x7f) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
      }
      secs.push({ id, name: SECTION_NAMES[id] || ("Sec" + id), size: Math.max(0, sz) });
      off += sz;
    }
    return secs;
  }

  function inspectWasmModule(bytes) {
    if (!bytes || !WebAssembly.validate(bytes)) {
      return { valid: false, imports: [], exports: [], sections: [] };
    }
    const mod = new WebAssembly.Module(bytes);
    return {
      valid: true,
      imports: WebAssembly.Module.imports(mod).map((i) => `${i.module}.${i.name}`),
      exports: WebAssembly.Module.exports(mod).map((e) => `${e.name}:${e.kind}`),
      sections: parseWasmSections(bytes),
    };
  }

  function renderWasmMapCanvas(canvasOpt, sectionsOpt, phaseOpt) {
    const canvas = canvasOpt || el("wasmMapCanvas");
    if (!canvas || canvas.width < 50 || canvas.height < 50) return;
    if (typeof canvas.getContext !== "function") return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    const w = Math.max(50, canvas.width | 0);
    const h = Math.max(50, canvas.height | 0);
    const secs = (sectionsOpt && sectionsOpt.length) ? sectionsOpt : state.lastSections;
    const phase = typeof phaseOpt === "number" ? phaseOpt : state.animPhase;

    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = "#0b0f17";
    ctx.fillRect(0, 0, w, h);

    if (!secs || !secs.length) {
      ctx.fillStyle = "#64748b";
      ctx.font = "12px monospace";
      ctx.fillText("No WASM artifact inspected yet", 16, Math.max(24, Math.floor(h / 2)));
      return;
    }

    const total = Math.max(1, secs.reduce((a, s) => a + Math.max(1, s.size), 0));
    const barX = 14;
    const barY = 26;
    const barW = Math.max(1, w - 28);
    const barH = 34;

    let cx = barX;
    for (let i = 0; i < secs.length; i++) {
      const s = secs[i];
      const sw = Math.max(2, Math.round((Math.max(1, s.size) / total) * barW));
      ctx.fillStyle = SECTION_COLORS[s.id] || "#64748b";
      ctx.fillRect(cx, barY, Math.min(sw, Math.max(1, barX + barW - cx)), barH);
      cx += sw;
    }

    const pulseR = Math.max(1, 5 + Math.sin(phase) * 1.5);
    ctx.beginPath();
    ctx.arc(w - 20, 14, pulseR, 0, Math.PI * 2);
    ctx.fillStyle = "#38bdf8";
    ctx.fill();

    ctx.fillStyle = "#cbd5e1";
    ctx.font = "11px monospace";
    ctx.fillText(`WASM Sections (${secs.length}) • ${total}B payload`, 14, 17);

    let lx = 14;
    let ly = 82;
    for (let i = 0; i < Math.min(secs.length, 8); i++) {
      const s = secs[i];
      ctx.fillStyle = SECTION_COLORS[s.id] || "#64748b";
      ctx.fillRect(lx, ly - 9, 9, 9);
      ctx.fillStyle = "#e2e8f0";
      ctx.fillText(`${s.name}:${s.size}B`, lx + 13, ly);
      lx += 116;
      if (lx + 100 > w) { lx = 14; ly += 18; }
    }
  }

  function renderFramebufferCanvas(canvasOpt, rgbaBytes, srcW, srcH) {
    const canvas = canvasOpt || el("fbCanvas");
    if (!canvas || canvas.width < 50 || canvas.height < 50) return false;
    if (typeof canvas.getContext !== "function") return false;
    const w = Math.max(1, srcW | 0);
    const h = Math.max(1, srcH | 0);
    const needBytes = w * h * 4;
    if (!rgbaBytes || rgbaBytes.length < needBytes) return false;
    const ctx = canvas.getContext("2d");
    if (!ctx) return false;
    const targetW = Math.max(50, w);
    const targetH = Math.max(50, h);
    if (canvas.width !== targetW) canvas.width = targetW;
    if (canvas.height !== targetH) canvas.height = targetH;
    const img = (typeof ctx.createImageData === "function")
      ? ctx.createImageData(w, h)
      : { data: new Uint8ClampedArray(needBytes), width: w, height: h };
    if (img.data && typeof img.data.set === "function") {
      img.data.set(rgbaBytes.subarray(0, needBytes));
    }
    if (typeof ctx.putImageData === "function") {
      ctx.putImageData(img, 0, 0);
    }
    return true;
  }

  function hashRgba(u8) {
    let h1 = 0x811c9dc5 >>> 0;
    let h2 = 0x9e3779b9 >>> 0;
    for (let i = 0; i < u8.length; i += 4) {
      const px = (u8[i] | (u8[i + 1] << 8) | (u8[i + 2] << 16) | (u8[i + 3] << 24)) >>> 0;
      h1 = Math.imul(h1 ^ px, 0x01000193) >>> 0;
      h2 = Math.imul(h2 ^ (px + i), 0x85ebca6b) >>> 0;
    }
    return h1.toString(16).padStart(8, "0") + h2.toString(16).padStart(8, "0");
  }

  function countUniqueRgbAndAlpha(u8) {
    const uniq = new Set();
    let allOpaque = true;
    for (let i = 0; i < u8.length; i += 4) {
      if (u8[i + 3] !== 255) allOpaque = false;
      uniq.add((u8[i] << 16) | (u8[i + 1] << 8) | u8[i + 2]);
    }
    return { uniqueColors: uniq.size, allOpaque };
  }

  function computeFbKeyMask() {
    let mask = 0;
    const down = state.fbKeysDown;
    if (down.has("ArrowUp") || down.has("KeyW")) mask |= 1;
    if (down.has("ArrowDown") || down.has("KeyS")) mask |= 2;
    if (down.has("ArrowLeft") || down.has("KeyA")) mask |= 4;
    if (down.has("ArrowRight") || down.has("KeyD")) mask |= 8;
    if (down.has("ControlLeft") || down.has("ControlRight") || down.has("KeyF")) mask |= 16;
    if (down.has("Space") || down.has("KeyE")) mask |= 32;
    if (down.has("Enter")) mask |= 64;
    if (down.has("Escape")) mask |= 128;
    if (down.has("ShiftLeft") || down.has("ShiftRight")) mask |= 256;
    if (down.has("KeyQ") || down.has("Comma")) mask |= 512;
    if (down.has("KeyC") || down.has("Period")) mask |= 1024;
    return mask;
  }

  function mapEventToDoomKey(ev) {
    const k = ev.key || "";
    const c = ev.code || "";
    if (c.startsWith("Digit") && c.length === 6) {
      const ch = c.charCodeAt(5);
      if (ch >= 49 && ch <= 55) return ch;
    }
    if (k === "y" || k === "Y") return 121;
    if (k === "n" || k === "N") return 110;
    return 0;
  }

  function stopFbSession() {
    if (state.fbSession) {
      state.fbSession.running = false;
    }
    state.fbSession = null;
    if (el("btnFbPause")) el("btnFbPause").textContent = "Pause";
    if (el("fbStatusText")) {
      el("fbStatusText").textContent = "Framebuffer stopped • Controls: WASD / Arrows, Ctrl/F Fire, Space/E Use, Shift Run, 1-7 Weapons";
    }
  }

  function stepActiveFramebuffer(keyMaskOverride) {
    const sess = state.fbSession;
    if (!sess || !sess.running) return null;
    const ex = sess.exports;
    const mask = (typeof keyMaskOverride === "number") ? (keyMaskOverride | 0) : computeFbKeyMask();
    ex.fb_tick(28, mask, sess.mouseX | 0, sess.mouseY | 0);
    const w = Math.max(1, ex.fb_width() | 0);
    const h = Math.max(1, ex.fb_height() | 0);
    const ptr = ex.fb_ptr() >>> 0;
    const memU8 = new Uint8Array(ex.memory.buffer);
    const need = w * h * 4;
    const rgbaSlice = memU8.slice(ptr, ptr + need);
    renderFramebufferCanvas(el("fbCanvas"), rgbaSlice, w, h);
    sess.width = w;
    sess.height = h;
    sess.frames++;
    const diag = {
      width: w,
      height: h,
      frames: sess.frames,
      rgba: rgbaSlice,
      gamestate: typeof ex.fb_gamestate === "function" ? (ex.fb_gamestate() | 0) : null,
      gametic: typeof ex.fb_gametic === "function" ? (ex.fb_gametic() | 0) : null,
      playerX: typeof ex.fb_player_x === "function" ? (ex.fb_player_x() | 0) : null,
      playerY: typeof ex.fb_player_y === "function" ? (ex.fb_player_y() | 0) : null,
      playerAngle: typeof ex.fb_player_angle === "function" ? (ex.fb_player_angle() >>> 0) : null,
    };
    if (el("fbStatusText")) {
      const gsPart = diag.gamestate !== null ? ` • gs=${diag.gamestate} tic=${diag.gametic}` : "";
      el("fbStatusText").textContent = `${sess.label} • ${w}x${h} • frame=${sess.frames}${gsPart} • ${sess.paused ? "PAUSED" : "LIVE"}`;
    }
    return diag;
  }

  async function startFbSession(wasmBytes, opts) {
    const o = opts || {};
    stopFbSession();
    const inspection = inspectWasmModule(wasmBytes);
    if (!inspection.valid) {
      throw new Error("Invalid WebAssembly framebuffer module");
    }
    const expSet = new Set(inspection.exports.map((e) => e.split(":")[0]));
    for (const req of ["memory", "fb_width", "fb_height", "fb_ptr", "fb_init", "fb_tick"]) {
      if (!expSet.has(req)) {
        throw new Error(`Framebuffer WASM module missing required export: ${req}`);
      }
    }
    const files = makeMergedVfs({});
    const shim = new globalThis.BrowserWasiShim({
      args: [o.name || "fb.wasm"],
      files,
      stdin: "",
      preopenDir: ".",
    });
    if (o.binaryFiles) {
      for (const [k, v] of Object.entries(o.binaryFiles)) {
        if (v) {
          shim.vfs.set(k.replace(/^\.?\//, ""), v instanceof Uint8Array ? v : new Uint8Array(v));
        }
      }
    }
    const { instance } = await shim.instantiate(wasmBytes);
    const ex = instance.exports;
    ex.fb_init((o.ep || 0) | 0, (o.map || 0) | 0);
    const w = Math.max(1, ex.fb_width() | 0);
    const h = Math.max(1, ex.fb_height() | 0);
    state.fbSession = {
      shim,
      instance,
      exports: ex,
      inspection,
      width: w,
      height: h,
      paused: Boolean(o.paused),
      running: true,
      frames: 0,
      lastTickMs: 0,
      mouseX: Math.floor(w / 2),
      mouseY: Math.floor(h / 2),
      label: o.name || "fb.wasm",
    };
    if (el("btnFbPause")) el("btnFbPause").textContent = state.fbSession.paused ? "Resume" : "Pause";
    const first = stepActiveFramebuffer(typeof o.initialKeyMask === "number" ? o.initialKeyMask : undefined);
    return { first, inspection, shim };
  }

  async function compileAndStepLiveFramebuffer(sourceText, steps, keyMaskSeq) {
    const comp = await compileCSource(sourceText, { linkWasiRt: false, mode: "wasm" });
    if (!comp.ok) {
      return {
        compileOk: false,
        compileMs: comp.compileMs,
        exitCode: comp.exitCode || 1,
        stderr: comp.compileStderr || "Compile failed",
        artifactBytes: null,
        frameHashes: [],
        uniqueColors: 0,
        allOpaque: false,
      };
    }
    const nSteps = Math.max(1, steps | 0);
    const k0 = (keyMaskSeq && keyMaskSeq.length > 0) ? (keyMaskSeq[0] | 0) : 0;
    const { first, inspection } = await startFbSession(comp.artifactBytes, {
      name: "fb_c99.wasm",
      ep: 0,
      map: 0,
      paused: true,
      initialKeyMask: k0,
    });
    const hashes = [hashRgba(first.rgba)];
    let last = first;
    for (let s = 1; s < nSteps; s++) {
      const km = (keyMaskSeq && s < keyMaskSeq.length) ? (keyMaskSeq[s] | 0) : 0;
      last = stepActiveFramebuffer(km);
      hashes.push(hashRgba(last.rgba));
    }
    const stats = countUniqueRgbAndAlpha(last.rgba);
    stopFbSession();
    return {
      compileOk: true,
      compileMs: comp.compileMs,
      artifactBytes: comp.artifactBytes,
      inspection,
      width: last.width,
      height: last.height,
      frames: last.frames,
      frameHashes: hashes,
      uniqueColors: stats.uniqueColors,
      allOpaque: stats.allOpaque,
    };
  }

  async function loadAndStepExternalFramebuffer(wasmBytes, vfsFiles, warpEp, warpMap, steps, keyMaskSeq) {
    const nSteps = Math.max(1, steps | 0);
    const k0 = (keyMaskSeq && keyMaskSeq.length > 0) ? (keyMaskSeq[0] | 0) : 0;
    const { first, inspection } = await startFbSession(wasmBytes, {
      name: "external_fb.wasm",
      binaryFiles: vfsFiles || {},
      ep: warpEp | 0,
      map: warpMap | 0,
      paused: true,
      initialKeyMask: k0,
    });
    const hashes = [hashRgba(first.rgba)];
    let last = first;
    for (let s = 1; s < nSteps; s++) {
      const km = (keyMaskSeq && s < keyMaskSeq.length) ? (keyMaskSeq[s] | 0) : 0;
      last = stepActiveFramebuffer(km);
      hashes.push(hashRgba(last.rgba));
    }
    const stats = countUniqueRgbAndAlpha(last.rgba);
    stopFbSession();
    return {
      inspection,
      width: last.width,
      height: last.height,
      frames: last.frames,
      gamestate: last.gamestate,
      gametic: last.gametic,
      playerX: last.playerX,
      playerY: last.playerY,
      playerAngle: last.playerAngle,
      frameHashes: hashes,
      uniqueColors: stats.uniqueColors,
      allOpaque: stats.allOpaque,
    };
  }

  function animationLoop() {
    try {
      state.animPhase = (state.animPhase + 0.08) % (Math.PI * 2);
      renderWasmMapCanvas(el("wasmMapCanvas"), state.lastSections, state.animPhase);
      if (state.fbSession && state.fbSession.running && !state.fbSession.paused) {
        const t = nowMs();
        if (t - state.fbSession.lastTickMs >= 28) {
          state.fbSession.lastTickMs = t;
          try {
            stepActiveFramebuffer();
          } catch (fbErr) {
            state.fbSession.running = false;
            if (el("fbStatusText")) {
              el("fbStatusText").textContent = "Framebuffer trap: " + String(fbErr && fbErr.message ? fbErr.message : fbErr);
            }
          }
        }
      }
    } catch (err) {
      if (typeof console !== "undefined" && console.error) {
        console.error("Frame render error:", err);
      }
    }
    if (typeof requestAnimationFrame === "function") {
      requestAnimationFrame(animationLoop);
    }
  }

  function makeMergedVfs(extraFiles) {
    const out = {};
    for (const [k, v] of state.userVfs.entries()) {
      out[k] = new Uint8Array(v);
    }
    if (extraFiles) {
      for (const [k, v] of Object.entries(extraFiles)) {
        out[k] = typeof v === "string" ? new TextEncoder().encode(v) : new Uint8Array(v);
      }
    }
    return out;
  }

  function syncUserVfsFromShim(vfsMap, excludeKeys) {
    const skip = new Set(excludeKeys || []);
    for (const [k, v] of vfsMap.entries()) {
      if (!skip.has(k)) state.userVfs.set(k, new Uint8Array(v));
    }
    refreshVfsView();
  }

  function refreshVfsView() {
    const node = el("vfsListOutput");
    if (!node) return;
    if (state.userVfs.size === 0) {
      node.textContent = "(no custom VFS files yet)";
      return;
    }
    const rows = [];
    for (const [k, v] of state.userVfs.entries()) {
      rows.push(`<div class="vfs-item"><span>${k}</span><span>${v.length} bytes</span></div>`);
    }
    node.innerHTML = rows.join("");
  }

  async function compileCSource(sourceText, options) {
    const opts = options || {};
    await initAssets();
    const linkWasiRt = Boolean(opts.linkWasiRt);
    const mode = opts.mode === "asm" ? "asm" : "wasm";
    const files = makeMergedVfs();
    const srcUnit = linkWasiRt && !sourceText.includes("wasm_wasi_rt.c")
      ? ('#include "wasm_wasi_rt.c"\n' + sourceText)
      : sourceText;
    if (linkWasiRt) files["wasm_wasi_rt.c"] = new TextEncoder().encode(state.wasiRtText);
    files["main.c"] = new TextEncoder().encode(srcUnit);

    const outName = mode === "asm" ? "out.s" : "out.wasm";
    const args = mode === "asm"
      ? ["zcc.wasm", "-S", "-I.", "main.c", "-o", outName]
      : ["zcc.wasm", "--target=wasm32", "-I.", "main.c", "-o", outName];

    const t0 = nowMs();
    const shim = new globalThis.BrowserWasiShim({ args, files, preopenDir: "." });
    const res = await shim.run(state.zccBytes);
    const compileMs = Math.round(nowMs() - t0);
    const outBytes = shim.getFile(outName);
    return {
      ok: res.exitCode === 0 && outBytes && outBytes.length > 0,
      exitCode: res.exitCode,
      compileMs,
      compileStdout: res.stdout,
      compileStderr: res.stderr,
      artifactBytes: outBytes ? new Uint8Array(outBytes) : null,
      artifactName: outName,
    };
  }

  async function compileAndRunC(sourceText, options) {
    const opts = options || {};
    const comp = await compileCSource(sourceText, { linkWasiRt: opts.linkWasiRt, mode: "wasm" });
    if (!comp.ok) {
      return {
        compileOk: false,
        compileMs: comp.compileMs,
        runMs: 0,
        exitCode: comp.exitCode || 1,
        stdout: comp.compileStdout,
        stderr: comp.compileStderr || "Compilation failed",
        artifactBytes: null,
        inspection: null,
        vfs: null,
      };
    }
    const inspection = inspectWasmModule(comp.artifactBytes);
    const argv = splitCliArgs(opts.argvLine || "prog.wasm");
    if (!argv.length) argv.push("prog.wasm");
    const runFiles = makeMergedVfs();
    const rt0 = nowMs();
    const runShim = new globalThis.BrowserWasiShim({
      args: argv,
      files: runFiles,
      stdin: opts.stdin || "",
      preopenDir: ".",
    });
    const runRes = await runShim.run(comp.artifactBytes);
    const runMs = Math.round(nowMs() - rt0);
    return {
      compileOk: true,
      compileMs: comp.compileMs,
      runMs,
      exitCode: runRes.exitCode,
      stdout: runRes.stdout,
      stderr: (comp.compileStderr ? comp.compileStderr + "\n" : "") + runRes.stderr,
      artifactBytes: comp.artifactBytes,
      inspection,
      vfs: runRes.vfs,
    };
  }

  async function compileToAsm(sourceText, options) {
    const opts = options || {};
    const comp = await compileCSource(sourceText, { linkWasiRt: opts.linkWasiRt, mode: "asm" });
    const asmText = comp.artifactBytes ? new TextDecoder("utf-8").decode(comp.artifactBytes) : "";
    return {
      compileOk: comp.ok,
      compileMs: comp.compileMs,
      exitCode: comp.exitCode,
      asmText,
      stderr: comp.compileStderr,
      artifactBytes: comp.artifactBytes,
    };
  }

  async function runLua(luaCode, opts) {
    await initAssets();
    const files = makeMergedVfs((opts && opts.files) || {});
    files["__play.lua"] = new TextEncoder().encode(luaCode);
    const rt0 = nowMs();
    const shim = new globalThis.BrowserWasiShim({
      args: ["lua.wasm", "__play.lua"],
      files,
      stdin: (opts && opts.stdin) || "",
      preopenDir: ".",
    });
    const res = await shim.run(state.luaBytes);
    const runMs = Math.round(nowMs() - rt0);
    return {
      exitCode: res.exitCode,
      runMs,
      stdout: res.stdout,
      stderr: res.stderr,
      inspection: inspectWasmModule(state.luaBytes),
      vfs: res.vfs,
    };
  }

  async function runExternalWasm(wasmBytes, argvLine, stdinStr, extraFiles) {
    const argv = Array.isArray(argvLine) ? argvLine : splitCliArgs(argvLine || "module.wasm");
    if (!argv.length) argv.unshift("module.wasm");
    else if (argv[0].startsWith(":") || argv[0].startsWith("-") || argv[0].includes(" ")) {
      argv.unshift("module.wasm");
    } else if (!argv[0].endsWith(".wasm")) {
      argv.unshift("module.wasm");
    }
    const inspection = inspectWasmModule(wasmBytes);
    if (!inspection.valid) {
      throw new Error("Invalid WebAssembly binary");
    }
    const files = makeMergedVfs(extraFiles || {});
    const rt0 = nowMs();
    const shim = new globalThis.BrowserWasiShim({
      args: argv,
      files,
      stdin: stdinStr || "",
      preopenDir: ".",
    });
    const res = await shim.run(wasmBytes);
    const runMs = Math.round(nowMs() - rt0);
    return {
      exitCode: res.exitCode,
      runMs,
      stdout: res.stdout,
      stderr: res.stderr,
      inspection,
      vfs: res.vfs,
    };
  }

  function applyResultToDom(data) {
    if (el("metricCompileMs")) el("metricCompileMs").textContent = data.compileMs != null ? (data.compileMs + " ms") : "-";
    if (el("metricRunMs")) el("metricRunMs").textContent = data.runMs != null ? (data.runMs + " ms") : "-";
    if (el("metricSizeBytes")) el("metricSizeBytes").textContent = data.sizeBytes != null ? (data.sizeBytes + " B") : "-";
    if (el("metricExitCode")) el("metricExitCode").textContent = data.exitCode != null ? String(data.exitCode) : "-";
    if (el("stdoutOutput")) el("stdoutOutput").textContent = data.stdout || "";
    if (el("stderrOutput")) el("stderrOutput").textContent = data.stderr || "";
    if (el("asmOrInfoOutput")) el("asmOrInfoOutput").textContent = data.infoText || "";
    if (data.sections) {
      state.lastSections = data.sections;
      renderWasmMapCanvas(el("wasmMapCanvas"), data.sections, state.animPhase);
    }
    setStatus(data.statusText || "DONE", data.exitCode === 0 ? "ok" : "err");
  }

  function switchTab(tab) {
    state.activeTab = tab;
    const btnMap = {
      c: el("tabBtnC"),
      lua: el("tabBtnLua"),
      ext: el("tabBtnExt"),
      fb: el("tabBtnFb"),
    };
    for (const k of ["c", "lua", "ext", "fb"]) {
      const p = el("panel-" + k);
      const b = btnMap[k];
      if (p) p.classList.toggle("hidden", k !== tab);
      if (b) b.classList.toggle("active", k === tab);
    }
  }

  function loadCPreset() {
    const key = el("cPresetSelect") ? el("cPresetSelect").value : "bare_fib";
    const p = C_PRESETS[key] || C_PRESETS.bare_fib;
    if (el("cSourceInput")) el("cSourceInput").value = p.code;
    if (el("linkWasiRtChk")) el("linkWasiRtChk").checked = p.linkWasiRt;
  }

  function loadLuaPreset() {
    const key = el("luaPresetSelect") ? el("luaPresetSelect").value : "lua_fib_closures";
    if (el("luaSourceInput")) el("luaSourceInput").value = LUA_PRESETS[key] || LUA_PRESETS.lua_fib_closures;
  }

  function loadFbPreset() {
    const key = el("fbPresetSelect") ? el("fbPresetSelect").value : "live_raytracer";
    const p = FB_PRESETS[key] || FB_PRESETS.live_raytracer;
    if (el("fbSourceInput")) el("fbSourceInput").value = p.code;
  }

  function updateFbLoadedLabel() {
    const lbl = el("fbLoadedLabel");
    if (!lbl) return;
    const wPart = state.fbWasmBytes ? `${state.fbWasmName} (${state.fbWasmBytes.length}B)` : "no .wasm";
    const wadPart = state.fbWadBytes ? `${state.fbWadName} (${state.fbWadBytes.length}B)` : "no .wad";
    lbl.textContent = `Framebuffer assets: ${wPart} | ${wadPart}`;
  }

  async function bootExternalFb(ep, map) {
    if (!state.fbWasmBytes) {
      setStatus("LOAD FRAMEBUFFER .WASM FIRST", "err");
      if (el("fbStatusText")) el("fbStatusText").textContent = "Please choose build_wasm/doom.wasm (and doom1.wad) first.";
      return;
    }
    setStatus(`BOOTING ${state.fbWasmName} (${ep > 0 ? ("E" + ep + "M" + map) : "Title"})...`, "busy");
    const binaryFiles = {};
    if (state.fbWadBytes) {
      binaryFiles[state.fbWadName || "doom1.wad"] = state.fbWadBytes;
    }
    const t0 = nowMs();
    const { first, inspection, shim } = await startFbSession(state.fbWasmBytes, {
      name: state.fbWasmName || "doom.wasm",
      binaryFiles,
      ep: ep | 0,
      map: map | 0,
      paused: false,
    });
    const runMs = Math.round(nowMs() - t0);
    applyResultToDom({
      compileMs: 0,
      runMs,
      sizeBytes: state.fbWasmBytes.length,
      exitCode: 0,
      stdout: shim.getStdout(),
      stderr: shim.getStderr(),
      infoText: `Framebuffer Binary: ${state.fbWasmName} (${first.width}x${first.height})\nExports (${inspection.exports.length}): fb_init, fb_tick, fb_ptr, fb_key...`,
      sections: inspection.sections,
      statusText: `${state.fbWasmName} LIVE (${first.width}x${first.height})`,
    });
  }

  async function zccUiAction(action, ev) {
    if (ev) {
      if (ev.__zccDispatched) return;
      ev.__zccDispatched = true;
    }
    state.dispatchedCount++;
    try {
      if (action.startsWith("switchTab:")) {
        switchTab(action.split(":")[1]);
        return;
      }
      if (action === "loadCPreset") { loadCPreset(); return; }
      if (action === "loadLuaPreset") { loadLuaPreset(); return; }
      if (action === "loadFbPreset") { loadFbPreset(); return; }
      if (action === "toggleWasiRt") { return; }
      if (action === "clearUserVfs") { state.userVfs.clear(); refreshVfsView(); return; }
      if (action === "addVfsFile") {
        const name = el("vfsFileNameInput") ? el("vfsFileNameInput").value.trim() : "";
        const body = el("vfsFileContentInput") ? el("vfsFileContentInput").value : "";
        if (name) {
          state.userVfs.set(name, new TextEncoder().encode(body));
          refreshVfsView();
        }
        return;
      }
      if (action === "compileAndRunFbC") {
        setStatus("COMPILING LIVE FRAMEBUFFER C99...", "busy");
        const src = el("fbSourceInput") ? el("fbSourceInput").value : "";
        const comp = await compileCSource(src, { linkWasiRt: false, mode: "wasm" });
        if (!comp.ok) {
          applyResultToDom({
            compileMs: comp.compileMs,
            runMs: 0,
            sizeBytes: 0,
            exitCode: comp.exitCode || 1,
            stdout: comp.compileStdout,
            stderr: comp.compileStderr || "Compilation failed",
            infoText: "",
            sections: [],
            statusText: "FRAMEBUFFER COMPILE ERROR",
          });
          return;
        }
        state.lastArtifactBytes = comp.artifactBytes;
        state.lastArtifactName = "fb_prog.wasm";
        const t0 = nowMs();
        const { first, inspection, shim } = await startFbSession(comp.artifactBytes, {
          name: "fb_c99.wasm",
          ep: 0,
          map: 0,
          paused: false,
        });
        const runMs = Math.round(nowMs() - t0);
        applyResultToDom({
          compileMs: comp.compileMs,
          runMs,
          sizeBytes: comp.artifactBytes.length,
          exitCode: 0,
          stdout: shim.getStdout(),
          stderr: comp.compileStderr || "",
          infoText: `Live C99 Framebuffer (${first.width}x${first.height})\nExports: ${inspection.exports.join(", ")}`,
          sections: inspection.sections,
          statusText: `LIVE C99 FRAMEBUFFER (${first.width}x${first.height})`,
        });
        return;
      }
      if (action === "toggleFbPause") {
        if (!state.fbSession || !state.fbSession.running) return;
        state.fbSession.paused = !state.fbSession.paused;
        if (el("btnFbPause")) el("btnFbPause").textContent = state.fbSession.paused ? "Resume" : "Pause";
        if (el("fbStatusText")) {
          el("fbStatusText").textContent = `${state.fbSession.label} • frame=${state.fbSession.frames} • ${state.fbSession.paused ? "PAUSED" : "LIVE"}`;
        }
        return;
      }
      if (action === "stepFbOnce") {
        stepActiveFramebuffer();
        return;
      }
      if (action === "stopFbSession") {
        stopFbSession();
        return;
      }
      if (action === "loadFbWasmFile") {
        const inp = el("fbWasmFile");
        const file = inp && inp.files && inp.files[0];
        if (!file) return;
        state.fbWasmBytes = new Uint8Array(await file.arrayBuffer());
        state.fbWasmName = file.name;
        updateFbLoadedLabel();
        return;
      }
      if (action === "loadFbWadFile") {
        const inp = el("fbWadFile");
        const file = inp && inp.files && inp.files[0];
        if (!file) return;
        state.fbWadBytes = new Uint8Array(await file.arrayBuffer());
        state.fbWadName = file.name;
        updateFbLoadedLabel();
        return;
      }
      if (action === "bootExternalFbTitle") {
        await bootExternalFb(0, 0);
        return;
      }
      if (action === "bootExternalFbE1M1") {
        await bootExternalFb(1, 1);
        return;
      }
      if (action === "compileAndRunC") {
        setStatus("COMPILING & RUNNING WASM...", "busy");
        const src = el("cSourceInput") ? el("cSourceInput").value : "";
        const linkWasiRt = el("linkWasiRtChk") ? el("linkWasiRtChk").checked : false;
        const argvLine = el("cArgsInput") ? el("cArgsInput").value : "prog.wasm";
        const stdin = el("cStdinInput") ? el("cStdinInput").value : "";
        const r = await compileAndRunC(src, { linkWasiRt, argvLine, stdin });
        if (r.artifactBytes) {
          state.lastArtifactBytes = r.artifactBytes;
          state.lastArtifactName = "out.wasm";
        }
        if (r.vfs) syncUserVfsFromShim(r.vfs, []);
        const infoText = r.inspection
          ? (`Imports (${r.inspection.imports.length}):\n  ` + r.inspection.imports.join("\n  ") +
             `\nExports (${r.inspection.exports.length}):\n  ` + r.inspection.exports.join("\n  "))
          : "";
        applyResultToDom({
          compileMs: r.compileMs,
          runMs: r.runMs,
          sizeBytes: r.artifactBytes ? r.artifactBytes.length : 0,
          exitCode: r.exitCode,
          stdout: r.stdout,
          stderr: r.stderr,
          infoText,
          sections: r.inspection ? r.inspection.sections : [],
          statusText: r.compileOk ? (r.exitCode === 0 ? "COMPILED & EXECUTED (rc=0)" : `EXITED rc=${r.exitCode}`) : "COMPILE ERROR",
        });
        return;
      }
      if (action === "compileToAsm") {
        setStatus("EMITTING X86-64...", "busy");
        const src = el("cSourceInput") ? el("cSourceInput").value : "";
        const linkWasiRt = el("linkWasiRtChk") ? el("linkWasiRtChk").checked : false;
        const r = await compileToAsm(src, { linkWasiRt });
        if (r.artifactBytes) {
          state.lastArtifactBytes = r.artifactBytes;
          state.lastArtifactName = "out.s";
        }
        applyResultToDom({
          compileMs: r.compileMs,
          runMs: null,
          sizeBytes: r.artifactBytes ? r.artifactBytes.length : 0,
          exitCode: r.exitCode,
          stdout: "",
          stderr: r.stderr,
          infoText: r.asmText,
          statusText: r.compileOk ? "X86-64 ASSEMBLY EMITTED" : "ASM EMIT FAILED",
        });
        return;
      }
      if (action === "downloadArtifact") {
        if (!state.lastArtifactBytes || typeof Blob === "undefined" || typeof document === "undefined") return;
        const blob = new Blob([state.lastArtifactBytes], { type: "application/octet-stream" });
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = state.lastArtifactName;
        a.click();
        URL.revokeObjectURL(url);
        return;
      }
      if (action === "runLuaPresetOrCode") {
        setStatus("RUNNING LUA 5.4.6...", "busy");
        const code = el("luaSourceInput") ? el("luaSourceInput").value : "";
        const r = await runLua(code);
        syncUserVfsFromShim(r.vfs, ["__play.lua"]);
        applyResultToDom({
          compileMs: 0,
          runMs: r.runMs,
          sizeBytes: state.luaBytes ? state.luaBytes.length : 0,
          exitCode: r.exitCode,
          stdout: r.stdout,
          stderr: r.stderr,
          infoText: `Lua 5.4.6 WASI VM • ${r.inspection.imports.length} wasi_snapshot_preview1 imports`,
          sections: r.inspection.sections,
          statusText: r.exitCode === 0 ? "LUA 5.4 EXECUTED (rc=0)" : `LUA EXITED rc=${r.exitCode}`,
        });
        return;
      }
      if (action === "loadExternalFile") {
        const inp = el("extWasmFile");
        const file = inp && inp.files && inp.files[0];
        if (!file) return;
        const buf = new Uint8Array(await file.arrayBuffer());
        state.extBytes = buf;
        state.extName = file.name;
        if (el("extLoadedLabel")) el("extLoadedLabel").textContent = `Loaded ${file.name} (${buf.length} bytes)`;
        return;
      }
      if (action === "runExternalLoaded") {
        if (!state.extBytes) {
          setStatus("NO EXTERNAL .WASM LOADED", "err");
          return;
        }
        setStatus(`RUNNING ${state.extName}...`, "busy");
        const argvLine = el("extArgsInput") ? el("extArgsInput").value : "";
        const stdin = el("extStdinInput") ? el("extStdinInput").value : "";
        const r = await runExternalWasm(state.extBytes, argvLine, stdin);
        syncUserVfsFromShim(r.vfs, []);
        applyResultToDom({
          compileMs: 0,
          runMs: r.runMs,
          sizeBytes: state.extBytes.length,
          exitCode: r.exitCode,
          stdout: r.stdout,
          stderr: r.stderr,
          infoText: `External Module: ${state.extName}\nImports: ${r.inspection.imports.join(", ")}`,
          sections: r.inspection.sections,
          statusText: r.exitCode === 0 ? `${state.extName} EXITED rc=0` : `${state.extName} rc=${r.exitCode}`,
        });
      }
    } catch (err) {
      if (el("stderrOutput")) el("stderrOutput").textContent = String(err && err.stack ? err.stack : err);
      setStatus("ERROR: " + String(err && err.message ? err.message : err), "err");
    }
  }

  function bindControls() {
    if (typeof document === "undefined") return;
    const actionEls = document.querySelectorAll("[data-action]");
    for (const elem of actionEls) {
      if (elem.dataset && elem.dataset.bound === "1") continue;
      const act = elem.getAttribute("data-action");
      const evtName = (elem.tagName === "SELECT" || elem.type === "checkbox" || elem.type === "file") ? "change" : "click";
      elem.addEventListener(evtName, (ev) => zccUiAction(act, ev));
      if (elem.dataset) elem.dataset.bound = "1";
    }
    const dz = el("extDropZone");
    if (dz && (!dz.dataset || dz.dataset.dropBound !== "1")) {
      dz.addEventListener("dragover", (e) => { e.preventDefault(); });
      dz.addEventListener("drop", async (e) => {
        e.preventDefault();
        const f = e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0];
        if (!f) return;
        const buf = new Uint8Array(await f.arrayBuffer());
        state.extBytes = buf;
        state.extName = f.name;
        if (el("extLoadedLabel")) el("extLoadedLabel").textContent = `Dropped ${f.name} (${buf.length} bytes)`;
      });
      if (dz.dataset) dz.dataset.dropBound = "1";
    }
    const fbDz = el("fbDropZone");
    if (fbDz && (!fbDz.dataset || fbDz.dataset.dropBound !== "1")) {
      fbDz.addEventListener("dragover", (e) => { e.preventDefault(); });
      fbDz.addEventListener("drop", async (e) => {
        e.preventDefault();
        const fl = e.dataTransfer && e.dataTransfer.files ? Array.from(e.dataTransfer.files) : [];
        for (const f of fl) {
          const buf = new Uint8Array(await f.arrayBuffer());
          if (f.name.toLowerCase().endsWith(".wad")) {
            state.fbWadBytes = buf;
            state.fbWadName = f.name;
          } else {
            state.fbWasmBytes = buf;
            state.fbWasmName = f.name;
          }
        }
        updateFbLoadedLabel();
      });
      if (fbDz.dataset) fbDz.dataset.dropBound = "1";
    }
    const fbCanv = el("fbCanvas");
    if (fbCanv && (!fbCanv.dataset || fbCanv.dataset.mouseBound !== "1")) {
      fbCanv.addEventListener("mousemove", (e) => {
        if (!state.fbSession || typeof fbCanv.getBoundingClientRect !== "function") return;
        const r = fbCanv.getBoundingClientRect();
        if (r && r.width > 0 && r.height > 0) {
          state.fbSession.mouseX = Math.max(0, Math.min(state.fbSession.width - 1, Math.round(((e.clientX - r.left) / r.width) * state.fbSession.width)));
          state.fbSession.mouseY = Math.max(0, Math.min(state.fbSession.height - 1, Math.round(((e.clientY - r.top) / r.height) * state.fbSession.height)));
        }
      });
      fbCanv.addEventListener("click", () => {
        if (typeof fbCanv.focus === "function") fbCanv.focus();
      });
      if (fbCanv.dataset) fbCanv.dataset.mouseBound = "1";
    }
    if (typeof document.addEventListener === "function" && !state._fbKeysBound) {
      state._fbKeysBound = true;
      document.addEventListener("keydown", (ev) => {
        const tag = ev.target && ev.target.tagName ? String(ev.target.tagName).toUpperCase() : "";
        if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") return;
        if (ev.code) state.fbKeysDown.add(ev.code);
        if (state.fbSession && state.fbSession.running) {
          if ((ev.code && ev.code.startsWith("Arrow")) || ev.code === "Space") {
            if (typeof ev.preventDefault === "function") ev.preventDefault();
          }
          const dk = mapEventToDoomKey(ev);
          if (dk && typeof state.fbSession.exports.fb_key === "function") {
            state.fbSession.exports.fb_key(dk, 1);
          }
        }
      });
      document.addEventListener("keyup", (ev) => {
        if (ev.code) state.fbKeysDown.delete(ev.code);
        if (state.fbSession && state.fbSession.running) {
          const dk = mapEventToDoomKey(ev);
          if (dk && typeof state.fbSession.exports.fb_key === "function") {
            state.fbSession.exports.fb_key(dk, 0);
          }
        }
      });
    }
  }

  function bootPlayground() {
    loadCPreset();
    loadLuaPreset();
    loadFbPreset();
    refreshVfsView();
    bindControls();
    if (el("metaBanner")) {
      el("metaBanner").textContent =
        `Zero-Server file:// Bundle • zcc.wasm (${BUILD_META.zcc_bytes}B) • lua.wasm (${BUILD_META.lua_bytes}B) • BrowserWasiShim`;
    }
    if (typeof requestAnimationFrame === "function") {
      requestAnimationFrame(animationLoop);
    }
  }

  globalThis.zccUiAction = zccUiAction;
  globalThis.ZccPlaygroundEngine = {
    state,
    C_PRESETS,
    LUA_PRESETS,
    FB_PRESETS,
    BUILD_META,
    initAssets,
    gunzipBase64,
    splitCliArgs,
    parseWasmSections,
    inspectWasmModule,
    compileCSource,
    compileAndRunC,
    compileToAsm,
    runLua,
    runExternalWasm,
    renderWasmMapCanvas,
    renderFramebufferCanvas,
    compileAndStepLiveFramebuffer,
    loadAndStepExternalFramebuffer,
    stepActiveFramebuffer,
    stopFbSession,
    animationLoop,
    zccUiAction,
    bindControls,
    bootPlayground,
  };

  if (typeof document !== "undefined") {
    if (document.readyState === "loading") {
      document.addEventListener("DOMContentLoaded", bootPlayground);
    } else {
      bootPlayground();
    }
  }
})();
</script>
</body>
</html>
"""


def build_playground(output_path: str = DEFAULT_OUT) -> str:
    for req in (SHIM_JS_PATH, ZCC_WASM_PATH, WASI_RT_PATH, LUA_WASM_PATH):
        if not os.path.isfile(req):
            raise FileNotFoundError(f"Missing required playground asset: {req}")

    with open(ZCC_WASM_PATH, "rb") as f:
        zcc_bytes = f.read()
    with open(WASI_RT_PATH, "rb") as f:
        rt_bytes = f.read()
    with open(LUA_WASM_PATH, "rb") as f:
        lua_bytes = f.read()

    shim_js = load_shim_js(SHIM_JS_PATH)
    meta = {
        "zcc_bytes": len(zcc_bytes),
        "zcc_sha256": sha256_bytes(zcc_bytes),
        "wasi_rt_bytes": len(rt_bytes),
        "wasi_rt_sha256": sha256_bytes(rt_bytes),
        "lua_bytes": len(lua_bytes),
        "lua_sha256": sha256_bytes(lua_bytes),
    }

    html = HTML_TEMPLATE
    html = html.replace("__EMBEDDED_SHIM_JS__", shim_js)
    html = html.replace("__BUILD_META_JSON__", json.dumps(meta))
    html = html.replace("__ZCC_WASM_GZ_B64__", gz_b64(zcc_bytes))
    html = html.replace("__WASI_RT_GZ_B64__", gz_b64(rt_bytes))
    html = html.replace("__LUA_WASM_GZ_B64__", gz_b64(lua_bytes))

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(html)
    return output_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Build zero-server in-browser ZCC WASI playground HTML.")
    parser.add_argument("--output", default=DEFAULT_OUT)
    args = parser.parse_args()
    out = build_playground(args.output)
    sz = os.path.getsize(out)
    print(f"[PLAYGROUND] Built zero-server HTML: {out} ({sz} bytes)", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
