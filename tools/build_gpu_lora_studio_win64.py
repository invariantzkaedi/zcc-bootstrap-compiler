#!/usr/bin/env python3
"""
Sovereign Zero-Toolchain Win64 OpenAI + Blackwell LoRA Studio Builder (Track 20)
=================================================================================
1. Verifies `docs/zcc_lora_studio_ui.html` via headless Node.js EF-7 gate.
2. Packs `docs/zcc_lora_studio_ui.html` -> `build_win64/lora_studio_ui_html.h`.
3. Ensures `build_win64/gpu_llm_kernels_ptx.h` and `build_win64/gpu_lora_kernels_ptx.h` exist.
4. Dual-compiles `src/win64_gpu_lora_studio.c` via BOTH host `./zcc` and Win64
   `build_win64/zcc_sov_stage3.exe` -> `build_win64/zcc_gpu_lora_studio.exe`.
5. Asserts byte-identical PE32+ parity (`SHA-256` + `fc /b`) and audits System32
   imports (`WS2_32.dll`, `msvcrt.dll`, `nvcuda.dll` ONLY).
"""

import argparse
import json
import os
import re
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tests.test_wasm_wasi_selfhost import find_node_bin  # noqa: E402
from tools.build_gpu_win64 import audit_pe32plus_multi_dll, write_ptx_header  # noqa: E402
from tools.build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    ZCC_BIN,
    ZCC_WIN64,
    compile_and_link_win64_units,
    run_fc_b,
    run_win_cmd,
    sha256_file,
    to_rel_posix,
    to_rel_win,
)

LORA_KERNEL_SRC = os.path.join(REPO_ROOT, "src", "win64_gpu_lora_kernels.c")
LORA_PTX_HOST_OUT = os.path.join(BUILD_DIR, "gpu_lora_kernels_host.ptx")
LORA_PTX_OUT = os.path.join(BUILD_DIR, "gpu_lora_kernels.ptx")

UI_HTML_PATH = os.path.join(REPO_ROOT, "docs", "zcc_lora_studio_ui.html")
UI_HEADER_OUT = os.path.join(BUILD_DIR, "lora_studio_ui_html.h")
RUNNER_HTML_PATH = os.path.join(REPO_ROOT, "docs", "zcc_runner_game.html")
RUNNER_HEADER_OUT = os.path.join(BUILD_DIR, "runner_game_ui_html.h")
SVG_FORGE_HTML_PATH = os.path.join(REPO_ROOT, "docs", "zcc_svg_forge_ui.html")
SVG_FORGE_HEADER_OUT = os.path.join(BUILD_DIR, "svg_forge_ui_html.h")
MARIO_HTML_PATH = os.path.join(REPO_ROOT, "docs", "zcc_mario_game.html")
MARIO_HEADER_OUT = os.path.join(BUILD_DIR, "mario_game_ui_html.h")
OPENAPI_JSON_PATH = os.path.join(REPO_ROOT, "docs", "openapi.json")
OPENAPI_HEADER_OUT = os.path.join(BUILD_DIR, "openapi_spec_json.h")
API_DOCS_HTML_PATH = os.path.join(REPO_ROOT, "docs", "zcc_api_docs_ui.html")
API_DOCS_HEADER_OUT = os.path.join(BUILD_DIR, "api_docs_ui_html.h")
LLM_PTX_HEADER = os.path.join(BUILD_DIR, "gpu_llm_kernels_ptx.h")
LORA_PTX_HEADER = os.path.join(BUILD_DIR, "gpu_lora_kernels_ptx.h")
STUDIO_OUT = os.path.join(BUILD_DIR, "zcc_gpu_lora_studio.exe")
STUDIO_HOST_OUT = os.path.join(BUILD_DIR, "zcc_gpu_lora_studio_host_zcc.exe")
STUDIO_LINUX_OUT = os.path.join(BUILD_DIR, "zcc_gpu_lora_studio_linux")
STUDIO_LINUX_SRC = os.path.join(REPO_ROOT, "src", "linux_gpu_lora_studio.c")
LIBCUDA_WSL = "/usr/lib/wsl/lib/libcuda.so.1"
ZCC_STAGE3_ELF = os.path.join(REPO_ROOT, "zcc3")


def verify_studio_ui_headless(html_path: str = UI_HTML_PATH) -> dict:
    if not os.path.isfile(html_path):
        raise FileNotFoundError(f"Missing LoRA studio UI HTML: {html_path}")
    node_bin = find_node_bin()
    js_verifier = r"""
const fs = require('fs');
const vm = require('vm');
const htmlPath = process.argv[1];
const html = fs.readFileSync(htmlPath, 'utf8');

if (/https?:\/\/|cdn\./i.test(html)) {
  throw new Error('External CDN or HTTP URL detected inside sovereign studio UI HTML');
}
if (!html.includes('z-index: 1000') || !html.includes('pointer-events: none !important')) {
  throw new Error('Missing Rule EF-7 z-index / pointer-events isolation');
}
const scripts = [...html.matchAll(/<script(?:\s+[^>]*)?>([\s\S]*?)<\/script>/g)].map(m => m[1]);
if (scripts.length !== 1) {
  throw new Error('Expected exactly 1 <script> block, found ' + scripts.length);
}
new vm.Script(scripts[0]);

const ids = [
  'tabStudioBtn', 'tabArenaBtn', 'tabVaultBtn', 'viewStudio', 'viewArena', 'viewVault',
  'studioLayout', 'studioRail', 'toggleRailBtn',
  'gpuBadge', 'modelBadge', 'archBadge', 'kvBadge', 'adapterBadge', 'tpsBadge',
  'vramGaugeCanvas', 'tpsGaugeCanvas', 'vramReadout', 'tpsReadout',
  'newSessionBtn', 'sessionList', 'exportChatBtn',
  'presetDefaultBtn', 'presetObsidianBtn', 'presetC99Btn', 'presetConciseBtn',
  'topPInput', 'topKInput', 'repPenaltyInput', 'seedInput',
  'chatTranscript', 'promptInput', 'systemPrompt', 'tempInput', 'maxTokensInput',
  'streamCheck', 'statusLine', 'clearBtn', 'stopBtn', 'sendBtn',
  'adapterSelect', 'toggleBaseBtn', 'loraScaleSlider', 'loraScaleVal',
  'loraNameInput', 'loraRankInput', 'loraAlphaInput', 'loraLrInput', 'loraStepsInput',
  'loraSavePathInput', 'sftInput', 'sftDropZone', 'sftFileInput', 'corpusStats',
  'loadDemoBtn', 'loadStyleDemoBtn', 'trainLoraBtn', 'trainStatusLine', 'trainProgressFill',
  'lossCanvas', 'loadPathInput', 'loadNameInput', 'loadAdapterBtn', 'saveAdapterBtn',
  'unloadAdapterBtn', 'adapterCards', 'adapterListBox',
  'arenaSlotA', 'arenaScaleA', 'arenaScaleAVal', 'arenaSlotB', 'arenaScaleB', 'arenaScaleBVal',
  'arenaOutputA', 'arenaOutputB', 'arenaMetaA', 'arenaMetaB', 'arenaDiffBadge',
  'arenaPromptInput', 'arenaRunBtn',
  'vaultDirLabel', 'vaultScanBtn', 'vaultGrid',
  'apiTabCurl', 'apiTabPython', 'apiTabJs', 'apiSnippetCode', 'copySnippetBtn'
];

let drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
const mockCtx = {
  clearRect() {},
  fillRect() { drawStats.rects++; },
  beginPath() {},
  closePath() {},
  moveTo() {},
  lineTo() {},
  save() {},
  restore() {},
  measureText(t) { return { width: String(t || '').length * 7 }; },
  createLinearGradient() { return { addColorStop() {} }; },
  createRadialGradient(x0, y0, r0, x1, y1, r1) {
    if (!(r0 >= 0 && r1 > 0)) throw new Error('Invalid radial gradient radius');
    return { addColorStop() {} };
  },
  stroke() { drawStats.strokes++; },
  arc(x, y, r) {
    if (!(r > 0)) throw new Error('Non-positive canvas arc radius: ' + r);
    drawStats.arcs++;
  },
  fill() {},
  fillText() { drawStats.texts++; }
};

function makeNode(tag, id) {
  return {
    tagName: tag || 'div',
    id: id || '',
    type: '',
    textContent: '',
    value: id === 'tempInput' ? '0.0' : (id === 'maxTokensInput' ? '128' : (id === 'loraNameInput' ? 'obsidian7' : (id === 'topPInput' ? '0.9' : (id === 'topKInput' ? '40' : (id === 'repPenaltyInput' ? '1.1' : (id === 'seedInput' ? '42' : '')))))),
    checked: true,
    disabled: false,
    className: '',
    style: {},
    dataset: {},
    options: [{ value: 'base', textContent: 'base' }],
    children: [],
    listeners: {},
    width: id === 'lossCanvas' ? 560 : ((id === 'vramGaugeCanvas' || id === 'tpsGaugeCanvas') ? 72 : 0),
    height: id === 'lossCanvas' ? 170 : ((id === 'vramGaugeCanvas' || id === 'tpsGaugeCanvas') ? 72 : 0),
    getContext: (id === 'lossCanvas' || id === 'vramGaugeCanvas' || id === 'tpsGaugeCanvas') ? () => mockCtx : undefined,
    appendChild(c) {
      this.children.push(c);
      if ((this.id === 'adapterSelect' || this.id === 'arenaSlotA' || this.id === 'arenaSlotB') && c && typeof c.value === 'string') {
        this.options.push(c);
      }
      return c;
    },
    addEventListener(t, fn) { this.listeners[t] = fn; },
    scrollTop: 0,
    scrollHeight: 0
  };
}

const els = {};
for (const id of ids) {
  els[id] = makeNode(id.endsWith('Canvas') ? 'canvas' : 'div', id);
}
const sandbox = {
  document: {
    getElementById: (id) => els[id] || null,
    createElement: (tag) => makeNode(tag, ''),
    createTextNode: (txt) => ({ nodeType: 3, tagName: '#text', textContent: String(txt || ''), children: [] })
  },
  window: {},
  fetch: () => Promise.resolve({
    ok: true,
    json: () => Promise.resolve({
      model: 'zkaedi-prime-1.5b', arch: 'qwen2', n_layers: 28,
      kv_type: 'f16', eff_max_seq: 256, active_adapter: 'base', last_tok_per_sec: 142.8,
      gpu_name: 'RTX 5070', gpu_cc: '12.0', vram_used_mib: 1800, vram_total_mib: 8150,
      active: 'base', count: 0, adapters: []
    })
  }),
  AbortController: function () { this.signal = {}; this.abort = () => {}; },
  TextDecoder: function () { this.decode = (b) => String(b); },
  Date,
  Math,
  Number,
  String,
  Array,
  Object,
  JSON,
  Promise,
  Buffer,
  console
};
sandbox.window = sandbox;
vm.runInNewContext(scripts[0], sandbox);

for (const fnName of [
  'zccSendMessage', 'zccStopStream', 'zccClearChat',
  'zccSelectAdapter', 'zccToggleBaseLora', 'zccLoadDemoSft', 'zccLoadStyleSft',
  'zccTrainLora', 'zccLoadAdapter', 'zccSaveAdapter', 'zccUnloadAdapter',
  'zccSetView', 'zccChangeLoraScale', 'zccScanVault', 'zccRunArena',
  'zccNewSession', 'zccExportChat', 'zccCopySnippet', 'zccSetApiLang',
  'zccApplyPersona', 'zccToggleRail', 'zccSyncArenaSliders', 'zccArenaPreset'
]) {
  if (typeof sandbox[fnName] !== 'function') {
    throw new Error('Missing global interactive handler binding: ' + fnName);
  }
}

// Hidden 0x0 canvas guard across all 3 canvases
for (const cid of ['lossCanvas', 'vramGaugeCanvas', 'tpsGaugeCanvas']) {
  els[cid].width = 0;
  els[cid].height = 0;
}
drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
sandbox.__zccTestHooks.renderLossChart([{ step: 1, loss: 2.1, gnorm: 1.5 }, { step: 20, loss: 0.2, gnorm: 0.4 }]);
sandbox.__zccTestHooks.renderVramGauge(2048, 8150);
sandbox.__zccTestHooks.renderTpsGauge(155.4);
if (drawStats.strokes !== 0 || drawStats.arcs !== 0) {
  throw new Error('Zero-dimension canvas guard failed to abort drawing');
}

// Visible canvas render across all 3 canvases
els.lossCanvas.width = 560; els.lossCanvas.height = 170;
els.vramGaugeCanvas.width = 72; els.vramGaugeCanvas.height = 72;
els.tpsGaugeCanvas.width = 72; els.tpsGaugeCanvas.height = 72;
drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
sandbox.__zccTestHooks.renderLossChart([
  { step: 1, loss: 1.74, gnorm: 2.8 },
  { step: 40, loss: 0.23, gnorm: 1.1 },
  { step: 80, loss: 0.001, gnorm: 0.02 }
]);
sandbox.__zccTestHooks.renderVramGauge(2048, 8150);
sandbox.__zccTestHooks.renderTpsGauge(155.4);
if (drawStats.strokes < 5 || drawStats.arcs < 3 || drawStats.texts < 2) {
  throw new Error('Visible canvas instruments failed to draw: ' + JSON.stringify(drawStats));
}

// XSS-safe Markdown renderer check
const mdRoot = makeNode('div', '');
sandbox.__zccTestHooks.renderMarkdownInto(
  mdRoot,
  '# Obsidian\n\n<script>alert(1)</script> **bold** *ital* `code`\n\n```c\nint x = 1;\n```\n'
);
function walkNodes(n, out) {
  out.push(n);
  for (const c of (n.children || [])) walkNodes(c, out);
  return out;
}
const allMd = walkNodes(mdRoot, []);
if (allMd.some(n => String(n.tagName || '').toLowerCase() === 'script')) {
  throw new Error('XSS check failed: script element created inside Markdown output');
}
if (!allMd.some(n => n.tagName === 'pre' && n.textContent.includes('int x = 1;'))) {
  throw new Error('Markdown fenced code block did not render <pre>');
}

// Dropped JSONL dataset conversion check
const dsParsed = sandbox.__zccTestHooks.parseDroppedDataset(
  '{"prompt":"Q1","completion":"A1"}\n{"messages":[{"role":"user","content":"Q2"},{"role":"assistant","content":"A2"}]}',
  'train.jsonl'
);
if (dsParsed.pairs !== 2 || !dsParsed.sft.includes('Q: Q1') || !dsParsed.sft.includes('A: A2')) {
  throw new Error('JSONL dataset converter failed: ' + JSON.stringify(dsParsed));
}

// Chat SSE parser check
let sseOut = [];
let sseDone = false;
const rem1 = sandbox.__zccTestHooks.parseSseLines(
  'data: {"choices":[{"delta":{"role":"assistant"}}]}\n\ndata: {"choices":[{"delta":{"content":"COBALT"}}]}\n\ndata: [DONE]\n',
  (d) => sseOut.push(d),
  () => { sseDone = true; }
);
if (rem1 !== '' || sseOut.join('') !== 'COBALT' || !sseDone) {
  throw new Error('Chat SSE parser check failed');
}

// Train SSE parser check
let trainEvs = [];
let trainDone = false;
const rem2 = sandbox.__zccTestHooks.parseTrainSseLines(
  'data: {"event":"start","steps":80}\n\ndata: {"event":"step","step":1,"loss":1.74,"gnorm":2.8}\n\ndata: {"event":"done","name":"obsidian7"}\n\ndata: [DONE]\n',
  (o) => trainEvs.push(o),
  () => { trainDone = true; }
);
if (rem2 !== '' || trainEvs.length !== 3 || !trainDone) {
  throw new Error('Train SSE parser check failed');
}

sandbox.__zccTestHooks.applyHealthPayload({
  model: 'zkaedi-test', arch: 'qwen2', n_layers: 28,
  kv_type: 'f16', eff_max_seq: 256, active_adapter: 'obsidian7', active_scale: 1.25,
  last_tok_per_sec: 155.4, gpu_name: 'NVIDIA RTX 5070', gpu_cc: '12.0',
  vram_used_mib: 2200, vram_total_mib: 8150
});
if (!els.adapterBadge.textContent.includes('obsidian7') || !els.tpsBadge.textContent.includes('155.4') || !els.gpuBadge.textContent.includes('RTX 5070')) {
  throw new Error('Health DOM badge binding failed');
}
sandbox.__zccTestHooks.applyAdaptersPayload({
  active: 'obsidian7',
  count: 1,
  adapters: [{ name: 'obsidian7', rank: 8, alpha: 16, scale: 1.25, params: 2179072, fnv: '37b0323f', active: true }]
});
if (!els.adapterListBox.textContent.includes('obsidian7') || els.adapterCards.children.length !== 1) {
  throw new Error('Adapter list/card DOM binding failed');
}
sandbox.__zccTestHooks.applyVaultFilesPayload({
  dir: 'build_win64',
  count: 1,
  files: [{ filename: 'obs.zcc_lora', path: 'build_win64/obs.zcc_lora', bytes: 8716352, rank: 8, alpha: 16, fnv: '37b0323f', valid: true, compatible: true, loaded: true }]
});
if (els.vaultGrid.children.length !== 1) {
  throw new Error('Vault grid rendering failed');
}
const pySnip = sandbox.__zccTestHooks.buildApiSnippet('python');
if (!pySnip.includes('obsidian7') || !pySnip.includes('lora_scale')) {
  throw new Error('API snippet generator failed: ' + pySnip);
}

sandbox.zccLoadDemoSft({});
if (!els.sftInput.value.includes('COBALT-FALCON-9472')) {
  throw new Error('zccLoadDemoSft failed to populate corpus');
}

els.promptInput.value = 'hi';
const ev = {};
sandbox.zccSendMessage(ev);
if (els.chatTranscript.children.length < 2) {
  throw new Error('zccSendMessage did not append messages');
}
sandbox.zccSendMessage(ev); // idempotent guard check
if (els.chatTranscript.children.length !== 2) {
  throw new Error('Idempotent guard failed: double dispatch');
}
sandbox.zccClearChat({});
if (!els.statusLine.textContent.includes('Ready')) {
  throw new Error('zccClearChat failed to reset statusLine');
}

console.log(JSON.stringify({ ui_ok: true, errors: 0, html_bytes: Buffer.byteLength(html, 'utf8'), scripts: scripts.length }));
"""
    res = subprocess.run([node_bin, "-e", js_verifier, html_path], cwd=REPO_ROOT, capture_output=True, text=True, timeout=30)
    if res.returncode != 0:
        raise RuntimeError(f"LoRA Studio UI EF-7 verification failed:\n{res.stdout}\n{res.stderr}")
    return {"stdout": res.stdout.strip()}


def write_studio_ui_header(html_path: str = UI_HTML_PATH, header_path: str = UI_HEADER_OUT) -> int:
    return write_ptx_header(
        html_path,
        header_path,
        guard="ZCC_LORA_STUDIO_UI_HTML_H",
        var_name="g_lora_studio_ui_html",
    )


def verify_runner_ui_headless(html_path: str = RUNNER_HTML_PATH) -> dict:
    if not os.path.isfile(html_path):
        raise FileNotFoundError(f"Missing Runner Game UI HTML: {html_path}")
    node_bin = find_node_bin()
    js_verifier = r"""
const fs = require('fs');
const vm = require('vm');
const htmlPath = process.argv[1];
const html = fs.readFileSync(htmlPath, 'utf8');

if (/https?:\/\/|cdn\./i.test(html)) {
  throw new Error('External CDN or HTTP URL detected inside sovereign runner UI HTML');
}
if (!html.includes('z-index: 1000') || !html.includes('pointer-events: none !important')) {
  throw new Error('Missing Rule EF-7 z-index / pointer-events isolation in runner UI');
}
const scripts = [...html.matchAll(/<script(?:\s+[^>]*)?>([\s\S]*?)<\/script>/g)].map(m => m[1]);
if (scripts.length !== 1) {
  throw new Error('Expected exactly 1 <script> block in runner UI, found ' + scripts.length);
}
new vm.Script(scripts[0]);

const ids = [
  'gpuPill', 'adapterPill', 'biomePill', 'directorStatusPill',
  'runnerAdapterSelect', 'runnerScaleSlider', 'runnerScaleVal', 'soundToggleBtn', 'svgForgeLink',
  'scoreVal', 'highScoreVal', 'distVal', 'sectorVal', 'speedVal', 'hpPips', 'turboFill',
  'runnerCanvas', 'runnerOverlay', 'overlayTitle', 'overlayDesc', 'overlayHabitSummary',
  'startRunBtn', 'overlayEvolveBtn', 'directorBanter', 'directorPattern', 'directorLatency',
  'comboVal', 'ctrlLeftBtn', 'ctrlJumpBtn', 'ctrlSlideBtn', 'ctrlTurboBtn', 'ctrlRightBtn',
  'pauseRunBtn', 'restartRunBtn', 'triggerDirectorBtn', 'habitBadge',
  'lane0Bar', 'lane1Bar', 'lane2Bar', 'lane0Pct', 'lane1Pct', 'lane2Pct',
  'jumpsVal', 'slidesVal', 'turbosVal', 'rivalNameInput', 'rivalStepsInput',
  'evoStatusLine', 'evolveRivalBtn', 'evoLossCanvas',
  'svgLoadoutStatus', 'forgeShipBtn', 'forgeBossBtn', 'exportProofBtn'
];
for (const id of ids) {
  if (!html.includes('id="' + id + '"')) {
    throw new Error('Missing expected DOM id in runner UI HTML: ' + id);
  }
}

let drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
const mockCtx = {
  clearRect() {},
  fillRect() { drawStats.rects++; },
  strokeRect() { drawStats.strokes++; },
  beginPath() {},
  closePath() {},
  moveTo() {},
  lineTo() {},
  createLinearGradient() { return { addColorStop() {} }; },
  createRadialGradient(x0, y0, r0, x1, y1, r1) {
    if (!(r0 > 0 && r1 > 0)) throw new Error('Non-positive radial gradient radius');
    return { addColorStop() {} };
  },
  stroke() { drawStats.strokes++; },
  arc(x, y, r) {
    if (!(r > 0)) throw new Error('Non-positive canvas arc radius: ' + r);
    drawStats.arcs++;
  },
  fill() {},
  fillText() { drawStats.texts++; }
};

function makeNode(tag, id) {
  return {
    tagName: tag || 'div',
    id: id || '',
    textContent: '',
    value: id === 'rivalNameInput' ? 'runner_rival' : (id === 'rivalStepsInput' ? '60' : (id === 'runnerScaleSlider' ? '1' : '')),
    disabled: false,
    className: '',
    style: {},
    dataset: {},
    options: [{ value: 'base', textContent: 'base' }],
    children: [],
    listeners: {},
    width: id === 'runnerCanvas' ? 960 : (id === 'evoLossCanvas' ? 420 : 0),
    height: id === 'runnerCanvas' ? 540 : (id === 'evoLossCanvas' ? 115 : 0),
    getContext: (id === 'runnerCanvas' || id === 'evoLossCanvas') ? () => mockCtx : undefined,
    appendChild(c) {
      this.children.push(c);
      if (this.id === 'runnerAdapterSelect' && c && typeof c.value === 'string') this.options.push(c);
      return c;
    },
    addEventListener(t, fn) { this.listeners[t] = fn; }
  };
}

const els = {};
for (const id of ids) {
  els[id] = makeNode(id.endsWith('Canvas') ? 'canvas' : 'div', id);
}
const sandbox = {
  document: {
    getElementById: (id) => els[id] || null,
    createElement: (tag) => makeNode(tag, ''),
    addEventListener() {}
  },
  window: {},
  fetch: () => Promise.resolve({
    ok: true,
    json: () => Promise.resolve({
      model: 'zkaedi-prime-dpo-q4_k_m', active_adapter: 'base', active: 'base',
      gpu_name: 'RTX 5070', gpu_cc: '12.0', adapters: []
    })
  }),
  TextDecoder: function () { this.decode = (b) => String(b); },
  Date,
  Math,
  Number,
  String,
  Array,
  Object,
  JSON,
  Promise,
  Buffer,
  console
};
sandbox.window = sandbox;
vm.runInNewContext(scripts[0], sandbox);

for (const fnName of [
  'zccRunnerStart', 'zccRunnerPause', 'zccRunnerRestart',
  'zccRunnerMove', 'zccRunnerJump', 'zccRunnerSlide', 'zccRunnerTurbo',
  'zccRunnerToggleSound', 'zccRunnerSelectAdapter', 'zccRunnerScaleChange',
  'zccRunnerPingDirector', 'zccEvolveRival',
  'zccRunnerForgeSvg', 'zccRunnerExportProofSvg'
]) {
  if (typeof sandbox[fnName] !== 'function') {
    throw new Error('Missing global runner handler binding: ' + fnName);
  }
}

// 0x0 canvas abort guard
els.runnerCanvas.width = 0; els.runnerCanvas.height = 0;
els.evoLossCanvas.width = 0; els.evoLossCanvas.height = 0;
drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
sandbox.__zccRunnerTestHooks.renderGame();
sandbox.__zccRunnerTestHooks.renderEvoLossChart([{ step: 1, loss: 1.5 }]);
if (drawStats.strokes !== 0 || drawStats.arcs !== 0 || drawStats.rects !== 0) {
  throw new Error('Zero-dimension canvas guard failed in runner UI');
}

// Visible draw check on both canvases + physics step + wave spawn
els.runnerCanvas.width = 960; els.runnerCanvas.height = 540;
els.evoLossCanvas.width = 420; els.evoLossCanvas.height = 115;
drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
sandbox.zccRunnerStart({});
sandbox.__zccRunnerTestHooks.spawnWavePattern('LEFT_PINCH');
sandbox.__zccRunnerTestHooks.stepGame(0.016);
sandbox.__zccRunnerTestHooks.renderGame();
sandbox.__zccRunnerTestHooks.renderEvoLossChart([{ step: 1, loss: 1.5 }, { step: 60, loss: 0.05 }]);
if (drawStats.strokes < 5 || drawStats.arcs < 2 || drawStats.rects < 2 || sandbox.__zccRunnerTestHooks.getState().distance <= 0) {
  throw new Error('Runner visible render/physics verification failed: ' + JSON.stringify(drawStats));
}

const dir = sandbox.__zccRunnerTestHooks.parseDirectorDirective('BIOME:GOLD_GRID|PATTERN:LEFT_PINCH|SPEED:+0.15|BANTER:Drifting left.');
if (!dir.structured || dir.biome !== 'GOLD_GRID' || dir.pattern !== 'LEFT_PINCH' || Math.abs(dir.speedDelta - 0.15) > 1e-6) {
  throw new Error('parseDirectorDirective check failed: ' + JSON.stringify(dir));
}

const hab = sandbox.__zccRunnerTestHooks.detectDominantHabit({ laneTime: [80, 10, 10], jumps: 2, slides: 1, turboUses: 0, hp: 3, streak: 2, distance: 200 });
if (hab !== 'left_bias') {
  throw new Error('detectDominantHabit expected left_bias, got ' + hab);
}
const sft = sandbox.__zccRunnerTestHooks.buildAdaptiveEvolutionSft({ laneTime: [80, 10, 10], jumps: 2, slides: 1, turboUses: 0, hp: 3, streak: 2, distance: 200 });
if (!sft.includes('Q:') || !sft.includes('LEFT_PINCH')) {
  throw new Error('buildAdaptiveEvolutionSft check failed');
}

const okShip = sandbox.__zccRunnerTestHooks.setEquippedSvg('ship', '<svg viewBox="0 0 128 128"><polygon points="64,10 110,98 18,98" fill="#d4af37" stroke="#f3efe6" stroke-width="3"/></svg>', false);
const okBoss = sandbox.__zccRunnerTestHooks.setEquippedSvg('boss', '<svg viewBox="0 0 128 128"><circle cx="64" cy="64" r="32" fill="#7f1d1d" stroke="#f87171" stroke-width="3"/></svg>', false);
if (!okShip || !okBoss) {
  throw new Error('Failed equipping custom SVG ship/boss in runner');
}
drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
sandbox.__zccRunnerTestHooks.renderGame();
if (drawStats.strokes < 5 || drawStats.arcs < 2) {
  throw new Error('Runner custom SVG vector render verification failed');
}
const proofSvg = sandbox.__zccRunnerTestHooks.buildRunProofSvg();
if (!proofSvg.includes('<svg') || !proofSvg.includes('PROOF OF RUN')) {
  throw new Error('Runner buildRunProofSvg check failed');
}

console.log(JSON.stringify({ ui_ok: true, errors: 0, html_bytes: Buffer.byteLength(html, 'utf8'), scripts: scripts.length }));
"""
    res = subprocess.run([node_bin, "-e", js_verifier, html_path], cwd=REPO_ROOT, capture_output=True, text=True, timeout=30)
    if res.returncode != 0:
        raise RuntimeError(f"Runner Game UI EF-7 verification failed:\n{res.stdout}\n{res.stderr}")
    return {"stdout": res.stdout.strip()}


def write_runner_ui_header(html_path: str = RUNNER_HTML_PATH, header_path: str = RUNNER_HEADER_OUT) -> int:
    return write_ptx_header(
        html_path,
        header_path,
        guard="ZCC_RUNNER_GAME_UI_HTML_H",
        var_name="g_runner_game_ui_html",
    )


def verify_svg_forge_ui_headless(html_path: str = SVG_FORGE_HTML_PATH) -> dict:
    if not os.path.isfile(html_path):
        raise FileNotFoundError(f"Missing SVG Forge UI HTML: {html_path}")
    node_bin = find_node_bin()
    js_verifier = r"""
const fs = require('fs');
const vm = require('vm');
const htmlPath = process.argv[1];
const html = fs.readFileSync(htmlPath, 'utf8');

if (/https?:\/\/|cdn\./i.test(html)) {
  throw new Error('External CDN or HTTP URL detected inside sovereign SVG Forge UI HTML');
}
if (!html.includes('z-index: 1000') || !html.includes('pointer-events: none !important')) {
  throw new Error('Missing Rule EF-7 z-index / pointer-events isolation in SVG Forge UI');
}
const scripts = [...html.matchAll(/<script(?:\s+[^>]*)?>([\s\S]*?)<\/script>/g)].map(m => m[1]);
if (scripts.length !== 1) {
  throw new Error('Expected exactly 1 <script> block in SVG Forge UI, found ' + scripts.length);
}
new vm.Script(scripts[0]);

const ids = [
  'gpuPill', 'adapterPill', 'forgeStatusPill', 'forgeMetricsPill',
  'forgeAdapterSelect', 'forgeScaleSlider', 'forgeScaleVal',
  'forgeCatSelect', 'forgeStyleSelect', 'forgeRoleInput', 'forgeTempInput', 'forgeMaxTokInput',
  'forgeGenBtn', 'forgeCompareBtn', 'presetApexBtn', 'presetNemesisBtn', 'presetCrestBtn',
  'presetDieBtn', 'presetQpuBtn', 'presetRiftBtn', 'mutateBtn', 'symmetrizeBtn',
  'paletteBtn', 'fxToggleBtn', 'compareBox', 'astStatsLine', 'stageModeBadge',
  'svgLiveStage', 'sdfReadout', 'sdfCanvas', 'extrudeDepthSlider', 'extrude3dCanvas',
  'equipShipBtn', 'equipBossBtn', 'exportRawBtn', 'exportProofBtn', 'equipStatusLine',
  'svgSourceInput', 'applySourceBtn', 'forgeAliasInput', 'forgeStepsInput',
  'pinSvgBtn', 'loadStarterCorpusBtn', 'trainStyleBtn', 'forgeSftInput',
  'forgeTrainStatus', 'forgeLossCanvas'
];
for (const id of ids) {
  if (!html.includes('id="' + id + '"')) {
    throw new Error('Missing expected DOM id in SVG Forge UI HTML: ' + id);
  }
}

let drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
const mockCtx = {
  clearRect() {},
  fillRect() { drawStats.rects++; },
  strokeRect() { drawStats.strokes++; },
  beginPath() {},
  closePath() {},
  moveTo() {},
  lineTo() {},
  stroke() { drawStats.strokes++; },
  arc(x, y, r) {
    if (!(r > 0)) throw new Error('Non-positive canvas arc radius: ' + r);
    drawStats.arcs++;
  },
  fill() {},
  fillText() { drawStats.texts++; }
};

function makeNode(tag, id) {
  const node = {
    tagName: tag || 'div',
    id: id || '',
    namespaceURI: (id === 'svgLiveStage' || tag === 'svg') ? 'http://www.w3.org/2000/svg' : null,
    textContent: '',
    value: id === 'forgeAliasInput' ? 'svg_style_custom' : (id === 'forgeStepsInput' ? '70' : ''),
    disabled: false,
    className: '',
    style: {},
    dataset: {},
    attrs: {},
    options: [{ value: 'base', textContent: 'base' }],
    children: [],
    listeners: {},
    width: (id === 'sdfCanvas' || id === 'extrude3dCanvas') ? 320 : (id === 'forgeLossCanvas' ? 420 : 0),
    height: (id === 'sdfCanvas' || id === 'extrude3dCanvas') ? 260 : (id === 'forgeLossCanvas' ? 115 : 0),
    getContext: (id === 'sdfCanvas' || id === 'extrude3dCanvas' || id === 'forgeLossCanvas') ? () => mockCtx : undefined,
    setAttribute(k, v) { this.attrs[k] = String(v); },
    get firstChild() { return this.children[0] || null; },
    removeChild(c) {
      const idx = this.children.indexOf(c);
      if (idx >= 0) this.children.splice(idx, 1);
      return c;
    },
    appendChild(c) {
      this.children.push(c);
      if (this.id === 'forgeAdapterSelect' && c && typeof c.value === 'string') this.options.push(c);
      return c;
    },
    addEventListener(t, fn) { this.listeners[t] = fn; }
  };
  return node;
}

const els = {};
for (const id of ids) {
  els[id] = makeNode(id === 'svgLiveStage' ? 'svg' : (id.endsWith('Canvas') ? 'canvas' : 'div'), id);
}

const sandbox = {
  document: {
    getElementById: (id) => els[id] || null,
    createElement: (tag) => makeNode(tag, ''),
    createElementNS: (_ns, tag) => makeNode(tag, ''),
    addEventListener() {}
  },
  window: {},
  fetch: () => Promise.resolve({
    ok: true,
    json: () => Promise.resolve({
      model: 'zkaedi-prime-dpo-q4_k_m', active_adapter: 'base', active: 'base',
      gpu_name: 'RTX 5070', gpu_cc: '12.0', adapters: []
    })
  }),
  TextDecoder: function () { this.decode = (b) => String(b); },
  Date, Math, Number, String, Array, Object, JSON, Promise, Buffer, console
};
sandbox.window = sandbox;
vm.runInNewContext(scripts[0], sandbox);

for (const fnName of [
  'zccSvgSelectAdapter', 'zccSvgScaleChange', 'zccSvgDepthChange',
  'zccSvgForgeGenerate', 'zccSvgForgeCompareBase', 'zccSvgLoadPreset',
  'zccSvgMutate', 'zccSvgSymmetrize', 'zccSvgCyclePalette', 'zccSvgToggleFx',
  'zccSvgEquipToRunner', 'zccSvgDownload', 'zccSvgApplySource',
  'zccSvgPinToCorpus', 'zccSvgLoadStarterCorpus', 'zccSvgTrainStyleCartridge'
]) {
  if (typeof sandbox[fnName] !== 'function') {
    throw new Error('Missing global SVG Forge handler binding: ' + fnName);
  }
}

// 0x0 guard across all 3 canvases
els.sdfCanvas.width = 0; els.sdfCanvas.height = 0;
els.extrude3dCanvas.width = 0; els.extrude3dCanvas.height = 0;
els.forgeLossCanvas.width = 0; els.forgeLossCanvas.height = 0;
drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
sandbox.__zccSvgForgeTestHooks.renderSdfCanvas();
sandbox.__zccSvgForgeTestHooks.renderExtrude3dCanvas();
sandbox.__zccSvgForgeTestHooks.renderForgeLossChart([{ step: 1, loss: 1.5 }]);
if (drawStats.strokes !== 0 || drawStats.arcs !== 0 || drawStats.rects !== 0) {
  throw new Error('Zero-dimension canvas guard failed in SVG Forge UI');
}

// Visible render check across all 3 canvases
els.sdfCanvas.width = 320; els.sdfCanvas.height = 260;
els.extrude3dCanvas.width = 320; els.extrude3dCanvas.height = 260;
els.forgeLossCanvas.width = 420; els.forgeLossCanvas.height = 115;
drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
sandbox.__zccSvgForgeTestHooks.renderSdfCanvas();
sandbox.__zccSvgForgeTestHooks.renderExtrude3dCanvas();
sandbox.__zccSvgForgeTestHooks.renderForgeLossChart([{ step: 1, loss: 1.5 }, { step: 50, loss: 0.04 }]);
if (drawStats.rects < 100 || drawStats.strokes < 10 || drawStats.arcs < 2 || drawStats.texts < 2) {
  throw new Error('SVG Forge visible render check failed: ' + JSON.stringify(drawStats));
}

// XSS negative control
const dirty = '<svg viewBox="0 0 128 128"><script>alert(1)</script><foreignObject><b>x</b></foreignObject><polygon points="10,10 100,10 64,100" onload="alert(2)" fill="#d4af37"/></svg>';
const xssModel = sandbox.__zccSvgForgeTestHooks.parseSafeSvgModel(dirty);
const cleanStr = sandbox.__zccSvgForgeTestHooks.sanitizeSvgToString(xssModel, null);
if (xssModel.strippedCount < 2 || /script|foreignObject|onload/i.test(cleanStr) || !cleanStr.includes('<polygon')) {
  throw new Error('SVG XSS sanitizer negative control failed: ' + JSON.stringify({ stripped: xssModel.strippedCount, cleanStr }));
}

// Exact SDF signed distance inside vs outside circle
const dIn = sandbox.__zccSvgForgeTestHooks.evalSdfAt(64, 64, [{ kind: 'circle', cx: 64, cy: 64, r: 30 }]);
const dOut = sandbox.__zccSvgForgeTestHooks.evalSdfAt(120, 120, [{ kind: 'circle', cx: 64, cy: 64, r: 30 }]);
if (!(dIn < 0 && dOut > 0)) {
  throw new Error('SDF evaluator sign check failed: dIn=' + dIn + ' dOut=' + dOut);
}

const mut = sandbox.__zccSvgForgeTestHooks.mutateSvgSource(cleanStr);
const sym = sandbox.__zccSvgForgeTestHooks.symmetrizeSvgSource(mut);
const prf = sandbox.__zccSvgForgeTestHooks.buildProofReceiptSvg(sym, { adapter: 'svg_forge' });
if (!mut.includes('<svg') || !sym.includes('<svg') || !prf.includes('<svg')) {
  throw new Error('Mutator / Symmetrizer / Proof SVG output invalid');
}

console.log(JSON.stringify({ ui_ok: true, errors: 0, html_bytes: Buffer.byteLength(html, 'utf8'), scripts: scripts.length, stripped_xss: xssModel.strippedCount }));
"""
    res = subprocess.run([node_bin, "-e", js_verifier, html_path], cwd=REPO_ROOT, capture_output=True, text=True, timeout=30)
    if res.returncode != 0:
        raise RuntimeError(f"SVG Forge UI EF-7 verification failed:\n{res.stdout}\n{res.stderr}")
    return {"stdout": res.stdout.strip()}


def write_svg_forge_ui_header(html_path: str = SVG_FORGE_HTML_PATH, header_path: str = SVG_FORGE_HEADER_OUT) -> int:
    return write_ptx_header(
        html_path,
        header_path,
        guard="ZCC_SVG_FORGE_UI_HTML_H",
        var_name="g_svg_forge_ui_html",
    )


def verify_mario_ui_headless(html_path: str = MARIO_HTML_PATH) -> dict:
    if not os.path.isfile(html_path):
        raise FileNotFoundError(f"Missing Super Blackwell Mario UI HTML: {html_path}")
    node_bin = find_node_bin()
    js_verifier = r"""
const fs = require('fs');
const vm = require('vm');
const htmlPath = process.argv[1];
const html = fs.readFileSync(htmlPath, 'utf8');

if (/https?:\/\/|cdn\./i.test(html)) {
  throw new Error('External CDN or HTTP URL detected inside sovereign Mario UI HTML');
}
if (!html.includes('z-index: 1000') || !html.includes('pointer-events: none !important')) {
  throw new Error('Missing Rule EF-7 z-index / pointer-events isolation');
}
const scripts = [...html.matchAll(/<script(?:\s+[^>]*)?>([\s\S]*?)<\/script>/g)].map(m => m[1]);
if (scripts.length !== 1) {
  throw new Error('Expected exactly 1 <script> block, found ' + scripts.length);
}
new vm.Script(scripts[0]);

const ids = [
  'gpuPill', 'tpsPill', 'architectStatusPill', 'marioAdapterSelect', 'marioScaleSlider', 'marioScaleVal', 'marioSoundBtn',
  'hudWorld', 'hudScore', 'hudCoins', 'hudLives', 'hudForm', 'hudCombo', 'hudTime',
  'marioCanvas', 'marioOverlay', 'overlayTitle', 'overlayDesc', 'overlayHint', 'overlayStartBtn', 'overlayForgeBtn',
  'architectBanner', 'bpThemeChip', 'bpLayoutChip', 'bpEnemyChip', 'bpPowerChip',
  'padLeftBtn', 'padRightBtn', 'padJumpBtn', 'padDownBtn', 'padFireBtn',
  'startBtn', 'forgeStageBtn', 'restartBtn', 'exportProofBtn',
  'reachBadge', 'habitTag', 'stompMeter', 'stompPct', 'coinMeter', 'coinPct', 'paceMeter', 'pacePct', 'deathReasonLabel',
  'clearSpriteBtn', 'forgeHeroSvgBtn', 'forgeBossSvgBtn', 'spriteLoadoutStatus',
  'evolveArchitectBtn', 'evolveNameInput', 'evolveStepsInput', 'evolveStatus', 'marioLossCanvas', 'architectLog'
];

let drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
const mockCtx = {
  clearRect() {},
  fillRect() { drawStats.rects++; },
  strokeRect() { drawStats.strokes++; },
  beginPath() {},
  closePath() {},
  moveTo() {},
  lineTo() {},
  save() {},
  restore() {},
  translate() {},
  scale() {},
  createLinearGradient() { return { addColorStop() {} }; },
  createRadialGradient(x0, y0, r0, x1, y1, r1) {
    if (!(r0 >= 0 && r1 > 0)) throw new Error('Invalid radial gradient radius');
    return { addColorStop() {} };
  },
  stroke() { drawStats.strokes++; },
  arc(x, y, r) {
    if (!(r > 0)) throw new Error('Non-positive canvas arc radius: ' + r);
    drawStats.arcs++;
  },
  fill() {},
  fillText() { drawStats.texts++; }
};

function makeNode(tag, id) {
  return {
    tagName: tag || 'div',
    id: id || '',
    textContent: '',
    value: id === 'evolveNameInput' ? 'mario_rival_v1' : (id === 'evolveStepsInput' ? '45' : (id === 'marioScaleSlider' ? '1' : '')),
    disabled: false,
    className: '',
    style: {},
    dataset: {},
    options: [{ value: 'base', textContent: 'base' }],
    children: [],
    listeners: {},
    width: id === 'marioCanvas' ? 920 : (id === 'marioLossCanvas' ? 420 : 0),
    height: id === 'marioCanvas' ? 460 : (id === 'marioLossCanvas' ? 115 : 0),
    getContext: (id === 'marioCanvas' || id === 'marioLossCanvas') ? () => mockCtx : undefined,
    appendChild(c) {
      this.children.push(c);
      if (this.id === 'marioAdapterSelect' && c && typeof c.value === 'string') this.options.push(c);
      return c;
    },
    addEventListener(t, fn) { this.listeners[t] = fn; }
  };
}

const els = {};
for (const id of ids) {
  els[id] = makeNode(id.endsWith('Canvas') ? 'canvas' : 'div', id);
}

const sandbox = {
  document: {
    getElementById: (id) => els[id] || null,
    createElement: (tag) => makeNode(tag, ''),
    addEventListener() {}
  },
  window: {},
  fetch: () => Promise.resolve({
    ok: true,
    json: () => Promise.resolve({
      model: 'zkaedi-prime-dpo-q4_k_m', active_adapter: 'base', active: 'base',
      gpu_name: 'RTX 5070', gpu_cc: '12.0', adapters: []
    })
  }),
  setTimeout: () => 0,
  Date, Math, Number, String, Array, Object, JSON, Promise, RegExp, Buffer, console,
  isFinite, parseInt, parseFloat
};
sandbox.window = sandbox;
vm.runInNewContext(scripts[0], sandbox);

for (const fnName of [
  'zccMarioSelectAdapter', 'zccMarioScaleChange', 'zccMarioToggleSound',
  'zccMarioStartToggle', 'zccMarioRestart', 'zccMarioForgeStage',
  'zccMarioPadHold', 'zccMarioPadRelease', 'zccMarioForgeSprite',
  'zccMarioClearSprites', 'zccMarioEvolveArchitect', 'zccMarioExportProof'
]) {
  if (typeof sandbox[fnName] !== 'function') {
    throw new Error('Missing global Mario handler binding: ' + fnName);
  }
}

// 0x0 guard across both canvases
els.marioCanvas.width = 0; els.marioCanvas.height = 0;
els.marioLossCanvas.width = 0; els.marioLossCanvas.height = 0;
drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
sandbox.__zccMarioTestHooks.renderMarioCanvas();
sandbox.__zccMarioTestHooks.renderMarioLossChart([{ step: 1, loss: 1.4 }]);
if (drawStats.strokes !== 0 || drawStats.arcs !== 0 || drawStats.rects !== 0 || drawStats.texts !== 0) {
  throw new Error('Zero-dimension canvas guard failed in Mario UI');
}

// Visible canvas render check
els.marioCanvas.width = 920; els.marioCanvas.height = 460;
els.marioLossCanvas.width = 420; els.marioLossCanvas.height = 115;
drawStats = { strokes: 0, arcs: 0, rects: 0, texts: 0 };
sandbox.__zccMarioTestHooks.renderMarioCanvas();
sandbox.__zccMarioTestHooks.renderMarioLossChart([{ step: 1, loss: 1.4 }, { step: 45, loss: 0.08 }]);
if (drawStats.rects < 20 || drawStats.arcs < 4 || drawStats.texts < 1) {
  throw new Error('Mario canvas visible render check failed: ' + JSON.stringify(drawStats));
}

// Reachability verifier across all 6 layouts
const hooks = sandbox.__zccMarioTestHooks;
for (const layout of ['CLASSIC_RUN', 'PIPE_GAUNTLET', 'SKY_BRIDGES', 'BRICK_MAZE', 'STAIR_ASCENT', 'BOSS_KEEP']) {
  const stg = (layout === 'BOSS_KEEP') ? 3 : 1;
  const bp = hooks.parseBlueprint('THEME:OVERWORLD_GOLD|LAYOUT:' + layout + '|ENEMIES:MIXED_ELITE|POWERUP:FIRE_FLOWER|PIPES:4|PITS:3|TAUNT:ok', 1, stg);
  const built = hooks.buildMarioWorld(bp, 1, stg);
  const rv = hooks.verifyLevelReachability(built.level);
  if (!rv || !rv.ok) throw new Error('Reachability verifier failed for ' + layout);
}

// Physics simulation check: run 60 frames moving right + jumping without NaN
hooks.resetStage(true);
sandbox.zccMarioPadHold('right');
sandbox.zccMarioPadHold('jump');
const x0 = hooks.getState().hero.x;
for (let f = 0; f < 60; f++) {
  hooks.stepPhysics(0.016);
}
sandbox.zccMarioPadRelease('right');
sandbox.zccMarioPadRelease('jump');
const stAfter = hooks.getState();
if (!isFinite(stAfter.hero.x) || !isFinite(stAfter.hero.y) || stAfter.hero.x <= x0) {
  throw new Error('Mario physics movement failed: x0=' + x0 + ' after=' + stAfter.hero.x);
}

// XSS negative control on SVG sprite loader + Proof SVG output
const dirty = '<svg viewBox="0 0 128 128"><script>alert(1)</script><foreignObject><b>x</b></foreignObject><polygon points="10,10 100,10 64,100" onload="alert(2)" fill="#d4af37"/></svg>';
const xss = hooks.parseMarioSvgPrimitives(dirty);
if (xss.strippedCount < 2 || xss.prims.length !== 1 || xss.prims[0].kind !== 'polygon') {
  throw new Error('Mario SVG XSS sanitizer check failed: ' + JSON.stringify(xss));
}
const proof = hooks.buildMarioProofSvg(stAfter);
if (!proof.includes('<svg') || !proof.includes('</svg>')) {
  throw new Error('Mario Proof SVG output invalid');
}

console.log(JSON.stringify({ ui_ok: true, errors: 0, html_bytes: Buffer.byteLength(html, 'utf8'), scripts: scripts.length, stripped_xss: xss.strippedCount, moved_px: Math.round(stAfter.hero.x - x0) }));
"""
    res = subprocess.run([node_bin, "-e", js_verifier, html_path], cwd=REPO_ROOT, capture_output=True, text=True, timeout=30)
    if res.returncode != 0:
        raise RuntimeError(f"Mario UI EF-7 verification failed:\n{res.stdout}\n{res.stderr}")
    return {"stdout": res.stdout.strip()}


def write_mario_ui_header(html_path: str = MARIO_HTML_PATH, header_path: str = MARIO_HEADER_OUT) -> int:
    return write_ptx_header(
        html_path,
        header_path,
        guard="ZCC_MARIO_GAME_UI_HTML_H",
        var_name="g_mario_game_ui_html",
    )


def verify_openapi_json(json_path: str = OPENAPI_JSON_PATH) -> dict:
    if not os.path.isfile(json_path):
        raise FileNotFoundError(f"Missing OpenAPI 3.1.0 spec: {json_path}")
    with open(json_path, "r", encoding="utf-8") as f:
        spec = json.load(f)
    if spec.get("openapi") != "3.1.0":
        raise AssertionError(f"Expected openapi 3.1.0, got {spec.get('openapi')!r}")
    schemes = spec.get("components", {}).get("securitySchemes", {})
    for req_scheme in ("bearerAuth", "apiKeyAuth"):
        if req_scheme not in schemes:
            raise AssertionError(f"Missing securityScheme {req_scheme} in {json_path}")
    required_paths = [
        "/", "/runner", "/svg", "/mario",
        "/health", "/metrics", "/openapi.json", "/docs", "/shutdown",
        "/v1/models", "/v1/models/{model_id}",
        "/v1/chat/completions", "/v1/completions",
        "/v1/messages", "/v1/responses",
        "/v1/embeddings", "/v1/tokenize", "/v1/detokenize", "/v1/batch",
        "/v1/lora/adapters", "/v1/lora/adapters/{name}",
        "/v1/lora/files", "/v1/lora/select", "/v1/lora/scale",
        "/v1/lora/load", "/v1/lora/save", "/v1/lora/unload", "/v1/lora/train",
    ]
    paths = spec.get("paths", {})
    missing = [p for p in required_paths if p not in paths]
    if missing:
        raise AssertionError(f"OpenAPI spec missing required paths: {missing}")
    return {
        "spec_ok": True,
        "paths": len(paths),
        "bytes": os.path.getsize(json_path),
    }


def write_openapi_header(json_path: str = OPENAPI_JSON_PATH, header_path: str = OPENAPI_HEADER_OUT) -> int:
    return write_ptx_header(
        json_path,
        header_path,
        guard="ZCC_OPENAPI_SPEC_JSON_H",
        var_name="g_openapi_spec_json",
    )


def verify_api_docs_ui_headless(html_path: str = API_DOCS_HTML_PATH) -> dict:
    if not os.path.isfile(html_path):
        raise FileNotFoundError(f"Missing API Docs Explorer UI HTML: {html_path}")
    node_bin = find_node_bin()
    js_verifier = r"""
const fs = require('fs');
const vm = require('vm');
const htmlPath = process.argv[1];
const html = fs.readFileSync(htmlPath, 'utf8');

if (/https?:\/\/|cdn\./i.test(html)) {
  throw new Error('External CDN or HTTP URL detected inside sovereign API Docs UI HTML');
}
if (!html.includes('z-index: 1000') || !html.includes('pointer-events: none !important')) {
  throw new Error('Missing Rule EF-7 z-index / pointer-events isolation');
}
const scripts = [...html.matchAll(/<script(?:\s+[^>]*)?>([\s\S]*?)<\/script>/g)].map(m => m[1]);
if (scripts.length !== 1) {
  throw new Error('Expected exactly 1 <script> block, found ' + scripts.length);
}
new vm.Script(scripts[0]);

const ids = [
  'gpuBadge', 'decoderBadge', 'endpointCountBadge', 'apiKeyInput', 'endpointFilterInput',
  'endpointList', 'copyCurlBtn', 'copyPyBtn', 'copyJsBtn', 'reqMethodSelect', 'reqPathInput',
  'presetSelect', 'sendReqBtn', 'reqBodyInput', 'respStatusBadge', 'respLatencyBadge',
  'copyStatusLine', 'respOutput'
];

function makeNode(tag, id) {
  return {
    tagName: tag || 'div',
    id: id || '',
    type: '',
    textContent: '',
    _innerHTML: '',
    get innerHTML() { return this._innerHTML; },
    set innerHTML(v) { this._innerHTML = String(v); if (v === '') this.children = []; },
    value: id === 'reqMethodSelect' ? 'POST' : (id === 'reqPathInput' ? '/v1/chat/completions' : (id === 'presetSelect' ? 'chat' : '')),
    disabled: false,
    className: '',
    style: {},
    dataset: {},
    children: [],
    listeners: {},
    onclick: null,
    appendChild(c) {
      this.children.push(c);
      return c;
    },
    addEventListener(t, fn) { this.listeners[t] = fn; }
  };
}

const els = {};
for (const id of ids) {
  els[id] = makeNode('div', id);
}

const sandbox = {
  document: {
    getElementById: (id) => els[id] || null,
    createElement: (tag) => makeNode(tag, ''),
    addEventListener() {}
  },
  window: { location: { origin: 'http://127.0.0.1:8080' } },
  navigator: { clipboard: { writeText: () => Promise.resolve() } },
  performance: { now: () => 12.5 },
  fetch: (url) => Promise.resolve({
    ok: true,
    status: 200,
    headers: { get: () => 'application/json' },
    json: () => Promise.resolve(
      String(url).includes('/openapi.json')
        ? { paths: { '/health': { get: { summary: 'Health' } }, '/v1/chat/completions': { post: { summary: 'Chat' } } } }
        : { status: 'ok', gpu: 'RTX 5070', decoder_mode: 'cuda_graph_q4k_8r', decode_graph_nodes: 393 }
    ),
    text: () => Promise.resolve(JSON.stringify({ ok: true }))
  }),
  setTimeout: () => 0,
  Date, Math, Number, String, Array, Object, JSON, Promise, RegExp, Buffer, console,
  isFinite, parseInt, parseFloat
};
sandbox.window = sandbox;
sandbox.window.location = { origin: 'http://127.0.0.1:8080' };
vm.runInNewContext(scripts[0], sandbox);

for (const fnName of [
  'zccApiFilterEndpoints', 'zccApiMethodChange', 'zccApiApplyPreset',
  'zccApiCopySnippet', 'zccApiSendRequest'
]) {
  if (typeof sandbox[fnName] !== 'function') {
    throw new Error('Missing global API Docs handler binding: ' + fnName);
  }
}

const hooks = sandbox.__zccApiDocsTestHooks;
hooks.renderEndpointList();
const initialCount = els.endpointList.children.length;
if (initialCount < 14) {
  throw new Error('Expected >= 14 catalog endpoints rendered, got ' + initialCount);
}

els.endpointFilterInput.value = 'embeddings';
sandbox.zccApiFilterEndpoints();
if (els.endpointList.children.length !== 1) {
  throw new Error('Filter for embeddings should match 1 endpoint, got ' + els.endpointList.children.length);
}
els.endpointFilterInput.value = '';
sandbox.zccApiFilterEndpoints();

els.presetSelect.value = 'chat_lora';
sandbox.zccApiApplyPreset();
if (!els.reqBodyInput.value.includes('mario_architect')) {
  throw new Error('chat_lora preset failed to populate body');
}

const curlSnip = hooks.buildCurlSnippet();
const pySnip = hooks.buildPythonSnippet();
const jsSnip = hooks.buildJsSnippet();
if (!curlSnip.includes('/v1/chat/completions') || !pySnip.includes('ZccClient') || !jsSnip.includes('fetch(')) {
  throw new Error('Snippet builder returned incomplete output');
}
sandbox.zccApiCopySnippet('curl');
if (!els.copyStatusLine.textContent.toLowerCase().includes('generated') || !els.respOutput.textContent.includes('/v1/chat/completions')) {
  throw new Error('Copy snippet status did not update');
}

console.log(JSON.stringify({
  ui_ok: true,
  errors: 0,
  html_bytes: Buffer.byteLength(html, 'utf8'),
  scripts: scripts.length,
  endpoints: initialCount
}));
"""
    res = subprocess.run([node_bin, "-e", js_verifier, html_path], cwd=REPO_ROOT, capture_output=True, text=True, timeout=30)
    if res.returncode != 0:
        raise RuntimeError(f"API Docs UI EF-7 verification failed:\n{res.stdout}\n{res.stderr}")
    return {"stdout": res.stdout.strip()}


def write_api_docs_ui_header(html_path: str = API_DOCS_HTML_PATH, header_path: str = API_DOCS_HEADER_OUT) -> int:
    return write_ptx_header(
        html_path,
        header_path,
        guard="ZCC_API_DOCS_UI_HTML_H",
        var_name="g_api_docs_ui_html",
    )


def ensure_ptx_headers(skip_win64_selfcompile: bool = False, arch: str = "sm_120", timeout_s: int = 180) -> None:
    if not (os.path.isfile(LLM_PTX_HEADER) and os.path.getsize(LLM_PTX_HEADER) > 1000):
        res = subprocess.run([sys.executable, os.path.join(SCRIPT_DIR, "build_gpu_llm_win64.py")], cwd=REPO_ROOT, timeout=240)
        if res.returncode != 0 or not os.path.isfile(LLM_PTX_HEADER):
            raise RuntimeError(f"Failed generating {LLM_PTX_HEADER}")

    need_lora_ptx = (
        not (os.path.isfile(LORA_PTX_HEADER) and os.path.getsize(LORA_PTX_HEADER) > 1000)
        or (os.path.getmtime(LORA_KERNEL_SRC) > os.path.getmtime(LORA_PTX_HEADER))
    )
    if need_lora_ptx:
        os.makedirs(BUILD_DIR, exist_ok=True)
        ptx_host_target = LORA_PTX_OUT if skip_win64_selfcompile else LORA_PTX_HOST_OUT
        print(f"[PTX-HOST:LORA] ./zcc --target=ptx --ptx-arch={arch} -> {to_rel_posix(ptx_host_target)}", flush=True)
        r_host = subprocess.run(
            [ZCC_BIN, "--target=ptx", f"--ptx-arch={arch}", to_rel_posix(LORA_KERNEL_SRC), "-o", to_rel_posix(ptx_host_target)],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=timeout_s,
        )
        if r_host.returncode != 0 or not os.path.isfile(ptx_host_target):
            raise RuntimeError(f"Host LoRA PTX compile failed:\n{r_host.stdout}\n{r_host.stderr}")
        sha_ptx_host = sha256_file(ptx_host_target)
        print(f"[PTX-HOST:LORA] {ptx_host_target}: {os.path.getsize(ptx_host_target)}B SHA256={sha_ptx_host}", flush=True)

        if not skip_win64_selfcompile:
            print(f"[PTX-WIN64:LORA] zcc_sov_stage3.exe --target=ptx --ptx-arch={arch} -> {to_rel_posix(LORA_PTX_OUT)}", flush=True)
            r_win = run_win_cmd(
                ZCC_WIN64,
                ["--target=ptx", f"--ptx-arch={arch}", to_rel_win(LORA_KERNEL_SRC), "-o", to_rel_win(LORA_PTX_OUT)],
                timeout_s=timeout_s,
            )
            if r_win.returncode != 0 or not os.path.isfile(LORA_PTX_OUT):
                raise RuntimeError(f"Win64 LoRA PTX compile failed:\n{r_win.stdout}\n{r_win.stderr}")
            sha_ptx_win = sha256_file(LORA_PTX_OUT)
            fc_ptx = run_fc_b(ptx_host_target, LORA_PTX_OUT)
            print(f"[FC /B:PTX:LORA] {os.path.getsize(LORA_PTX_OUT)}B SHA256={sha_ptx_win} {fc_ptx.stdout.strip()}", flush=True)
            if sha_ptx_host != sha_ptx_win or fc_ptx.returncode != 0 or "no differences encountered" not in fc_ptx.stdout.lower():
                raise AssertionError(f"LoRA PTX parity mismatch: {sha_ptx_host} vs {sha_ptx_win}")

        ptx_bytes = write_ptx_header(
            LORA_PTX_OUT,
            LORA_PTX_HEADER,
            guard="ZCC_GPU_LORA_KERNELS_PTX_H",
            var_name="g_gpu_lora_kernels_ptx",
        )
        print(f"[PTX-HEADER:LORA] {LORA_PTX_HEADER}: embedded {ptx_bytes} bytes -> g_gpu_lora_kernels_ptx", flush=True)


def audit_linux_elf_needed(elf_path: str) -> list[str]:
    res = subprocess.run(["readelf", "-d", elf_path], capture_output=True, text=True, timeout=30)
    if res.returncode != 0:
        raise RuntimeError(f"readelf failed on {elf_path}: {res.stderr}")
    needed = re.findall(r"\(NEEDED\)\s+Shared library:\s+\[([^\]]+)\]", res.stdout)
    allowed = {"libcuda.so.1", "libm.so.6", "libc.so.6"}
    if "libcuda.so.1" not in needed:
        raise AssertionError(f"Missing libcuda.so.1 in DT_NEEDED of {elf_path}: {needed}")
    extra = [lib for lib in needed if lib not in allowed]
    if extra:
        raise AssertionError(f"Unexpected DT_NEEDED libraries in {elf_path}: {extra} (all={needed})")
    return needed


def build_lora_studio_linux(timeout_s: int = 180) -> dict:
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc not found — run `make zcc` first.")
    if not os.path.isfile(LIBCUDA_WSL):
        raise RuntimeError(f"Missing WSL CUDA driver library {LIBCUDA_WSL}")

    s1_asm = os.path.join(BUILD_DIR, "zcc_gpu_lora_studio_linux_s1.s")
    s3_asm = os.path.join(BUILD_DIR, "zcc_gpu_lora_studio_linux.s")
    inc_flags = ["-Ibuild_win64", "-Iinclude", "-Isrc", "-I."]

    print(f"[BUILD-LINUX-S1:STUDIO] ./zcc -> {to_rel_posix(s1_asm)}", flush=True)
    r1 = subprocess.run(
        [ZCC_BIN] + inc_flags + [to_rel_posix(STUDIO_LINUX_SRC), "-o", to_rel_posix(s1_asm)],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=timeout_s,
    )
    if r1.returncode != 0 or not os.path.isfile(s1_asm):
        raise RuntimeError(f"./zcc Linux compile failed:\n{r1.stdout}\n{r1.stderr}")
    sha_s1 = sha256_file(s1_asm)

    asm_to_link = s1_asm
    if os.path.isfile(ZCC_STAGE3_ELF):
        print(f"[BUILD-LINUX-S3:STUDIO] ./zcc3 -> {to_rel_posix(s3_asm)}", flush=True)
        r3 = subprocess.run(
            [ZCC_STAGE3_ELF] + inc_flags + [to_rel_posix(STUDIO_LINUX_SRC), "-o", to_rel_posix(s3_asm)],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=timeout_s,
        )
        if r3.returncode != 0 or not os.path.isfile(s3_asm):
            raise RuntimeError(f"./zcc3 Linux compile failed:\n{r3.stdout}\n{r3.stderr}")
        sha_s3 = sha256_file(s3_asm)
        if sha_s1 != sha_s3:
            raise AssertionError(f"Linux Studio SystemV .s parity mismatch: zcc={sha_s1} vs zcc3={sha_s3}")
        print(f"[ELF-ASM-PARITY:STUDIO] {os.path.getsize(s3_asm)}B SHA256={sha_s3} (zcc == zcc3)", flush=True)
        asm_to_link = s3_asm

    link_cmd = [
        "gcc", "-fno-pie", "-no-pie",
        to_rel_posix(asm_to_link),
        LIBCUDA_WSL,
        "-Wl,-rpath,/usr/lib/wsl/lib",
        "-lm",
        "-o", to_rel_posix(STUDIO_LINUX_OUT),
    ]
    rl = subprocess.run(link_cmd, cwd=REPO_ROOT, capture_output=True, text=True, timeout=timeout_s)
    if rl.returncode != 0 or not os.path.isfile(STUDIO_LINUX_OUT):
        raise RuntimeError(f"Linux Studio link failed:\n{rl.stdout}\n{rl.stderr}")

    needed = audit_linux_elf_needed(STUDIO_LINUX_OUT)
    sha_elf = sha256_file(STUDIO_LINUX_OUT)
    print(f"[ELF-AUDIT:STUDIO] {STUDIO_LINUX_OUT}: {os.path.getsize(STUDIO_LINUX_OUT)}B SHA256={sha_elf} needed={needed}", flush=True)
    return {
        "elf": STUDIO_LINUX_OUT,
        "elf_sha256": sha_elf,
        "elf_bytes": os.path.getsize(STUDIO_LINUX_OUT),
        "asm_sha256": sha256_file(asm_to_link),
        "needed": needed,
    }


def build_lora_studio(skip_win64_selfcompile: bool = False, timeout_s: int = 180) -> dict:
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc not found — run `make zcc` first.")
    if not skip_win64_selfcompile and not os.path.isfile(ZCC_WIN64):
        raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")

    ui_chk = verify_studio_ui_headless(UI_HTML_PATH)
    print(f"[UI-EF7:STUDIO] {ui_chk['stdout']}", flush=True)
    ui_bytes = write_studio_ui_header(UI_HTML_PATH, UI_HEADER_OUT)
    print(f"[UI-HEADER:STUDIO] {UI_HEADER_OUT} ({ui_bytes} bytes -> g_lora_studio_ui_html)", flush=True)
    run_chk = verify_runner_ui_headless(RUNNER_HTML_PATH)
    print(f"[UI-EF7:RUNNER] {run_chk['stdout']}", flush=True)
    run_bytes = write_runner_ui_header(RUNNER_HTML_PATH, RUNNER_HEADER_OUT)
    print(f"[UI-HEADER:RUNNER] {RUNNER_HEADER_OUT} ({run_bytes} bytes -> g_runner_game_ui_html)", flush=True)
    svg_chk = verify_svg_forge_ui_headless(SVG_FORGE_HTML_PATH)
    print(f"[UI-EF7:SVG_FORGE] {svg_chk['stdout']}", flush=True)
    svg_bytes = write_svg_forge_ui_header(SVG_FORGE_HTML_PATH, SVG_FORGE_HEADER_OUT)
    print(f"[UI-HEADER:SVG_FORGE] {SVG_FORGE_HEADER_OUT} ({svg_bytes} bytes -> g_svg_forge_ui_html)", flush=True)
    mario_chk = verify_mario_ui_headless(MARIO_HTML_PATH)
    print(f"[UI-EF7:MARIO] {mario_chk['stdout']}", flush=True)
    mario_bytes = write_mario_ui_header(MARIO_HTML_PATH, MARIO_HEADER_OUT)
    print(f"[UI-HEADER:MARIO] {MARIO_HEADER_OUT} ({mario_bytes} bytes -> g_mario_game_ui_html)", flush=True)
    spec_chk = verify_openapi_json(OPENAPI_JSON_PATH)
    print(f"[OPENAPI-3.1] {spec_chk}", flush=True)
    spec_bytes = write_openapi_header(OPENAPI_JSON_PATH, OPENAPI_HEADER_OUT)
    print(f"[SPEC-HEADER:OPENAPI] {OPENAPI_HEADER_OUT} ({spec_bytes} bytes -> g_openapi_spec_json)", flush=True)
    docs_chk = verify_api_docs_ui_headless(API_DOCS_HTML_PATH)
    print(f"[UI-EF7:API_DOCS] {docs_chk['stdout']}", flush=True)
    docs_bytes = write_api_docs_ui_header(API_DOCS_HTML_PATH, API_DOCS_HEADER_OUT)
    print(f"[UI-HEADER:API_DOCS] {API_DOCS_HEADER_OUT} ({docs_bytes} bytes -> g_api_docs_ui_html)", flush=True)
    ensure_ptx_headers(skip_win64_selfcompile=skip_win64_selfcompile, timeout_s=timeout_s)

    units = [
        ("win64_app_crt.s", "src/win64_app_crt.c", ["-Iinclude", "-I."]),
        ("win64_c99_rt.s", "src/win64_c99_rt.c", ["-Iinclude", "-I."]),
        ("win64_gpu_lora_studio.s", "src/win64_gpu_lora_studio.c", ["-Ibuild_win64", "-Iinclude", "-Isrc", "-I."]),
    ]

    eff_host = STUDIO_OUT if skip_win64_selfcompile else STUDIO_HOST_OUT
    print(f"[BUILD-HOST:STUDIO] ./zcc -mabi=ms -> {to_rel_posix(eff_host)}", flush=True)
    compile_and_link_win64_units(
        units, [], os.path.join(BUILD_DIR, "gpu_lora_studio_host_asm"), eff_host,
        use_win64_compiler=False, timeout_s=timeout_s
    )
    sha_host = sha256_file(eff_host)
    print(f"[HOST-PE:STUDIO] {eff_host}: {os.path.getsize(eff_host)}B SHA256={sha_host}", flush=True)

    if not skip_win64_selfcompile:
        print(f"[BUILD-WIN64:STUDIO] zcc_sov_stage3.exe -mabi=ms -> {to_rel_posix(STUDIO_OUT)}", flush=True)
        compile_and_link_win64_units(
            units, [], os.path.join(BUILD_DIR, "gpu_lora_studio_win64_asm"), STUDIO_OUT,
            use_win64_compiler=True, timeout_s=timeout_s
        )
        sha_win = sha256_file(STUDIO_OUT)
        fc_srv = run_fc_b(eff_host, STUDIO_OUT)
        print(f"[FC /B:STUDIO] {os.path.getsize(STUDIO_OUT)}B SHA256={sha_win} {fc_srv.stdout.strip()}", flush=True)
        if sha_host != sha_win or fc_srv.returncode != 0 or "no differences encountered" not in fc_srv.stdout.lower():
            raise AssertionError(f"Studio PE32+ parity mismatch: {sha_host} vs {sha_win}")

    imports = audit_pe32plus_multi_dll(
        STUDIO_OUT,
        required_dlls=("ws2_32.dll", "msvcrt.dll", "nvcuda.dll"),
        allowed_dlls=("ws2_32.dll", "msvcrt.dll", "nvcuda.dll"),
    )
    print(f"[PE-AUDIT:STUDIO] bytes={imports['bytes']} imports={sorted(imports['dlls'].keys())}", flush=True)
    linux_info = None
    if sys.platform != "win32" and os.path.isfile(LIBCUDA_WSL):
        linux_info = build_lora_studio_linux(timeout_s=timeout_s)
    return {
        "exe": STUDIO_OUT,
        "exe_sha256": sha256_file(STUDIO_OUT),
        "exe_bytes": os.path.getsize(STUDIO_OUT),
        "ui_bytes": ui_bytes,
        "imports": imports,
        "linux": linux_info,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Build sovereign Win64 OpenAI + Blackwell LoRA Studio via ZCC.")
    parser.add_argument("--skip-win64-selfcompile", action="store_true")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    build_lora_studio(skip_win64_selfcompile=args.skip_win64_selfcompile, timeout_s=args.timeout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
