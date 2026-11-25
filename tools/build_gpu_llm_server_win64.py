#!/usr/bin/env python3
"""
Sovereign Zero-Toolchain Win64 OpenAI HTTP/SSE Server Builder (Track 18)
========================================================================
1. Verifies `docs/zcc_llm_server_ui.html` via headless Node.js EF-7 gate.
2. Packs `docs/zcc_llm_server_ui.html` -> `build_win64/server_ui_html.h`.
3. Ensures `build_win64/gpu_llm_kernels_ptx.h` exists.
4. Dual-compiles `src/win64_gpu_llm_server.c` via BOTH host `./zcc` and Win64
   `build_win64/zcc_sov_stage3.exe` -> `build_win64/zcc_gpu_llm_server.exe`.
5. Asserts byte-identical PE32+ parity (`SHA-256` + `fc /b`) and audits System32
   imports (`WS2_32.dll`, `msvcrt.dll`, `nvcuda.dll` ONLY).
"""

import argparse
import os
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
    sha256_file,
    to_rel_posix,
)

UI_HTML_PATH = os.path.join(REPO_ROOT, "docs", "zcc_llm_server_ui.html")
UI_HEADER_OUT = os.path.join(BUILD_DIR, "server_ui_html.h")
PTX_HEADER_OUT = os.path.join(BUILD_DIR, "gpu_llm_kernels_ptx.h")
SERVER_OUT = os.path.join(BUILD_DIR, "zcc_gpu_llm_server.exe")
SERVER_HOST_OUT = os.path.join(BUILD_DIR, "zcc_gpu_llm_server_host_zcc.exe")


def verify_server_ui_headless(html_path: str = UI_HTML_PATH) -> dict:
    if not os.path.isfile(html_path):
        raise FileNotFoundError(f"Missing server UI HTML: {html_path}")
    node_bin = find_node_bin()
    js_verifier = r"""
const fs = require('fs');
const vm = require('vm');
const htmlPath = process.argv[1];
const html = fs.readFileSync(htmlPath, 'utf8');

if (/https?:\/\/|cdn\./i.test(html)) {
  throw new Error('External CDN or HTTP URL detected inside sovereign UI HTML');
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
  'modelBadge', 'archBadge', 'kvBadge', 'sealBadge', 'tpsBadge',
  'chatTranscript', 'promptInput', 'systemPrompt', 'tempInput',
  'maxTokensInput', 'streamCheck', 'statusLine', 'clearBtn',
  'stopBtn', 'sendBtn'
];
const els = {};
for (const id of ids) {
  els[id] = {
    id,
    textContent: '',
    value: id === 'tempInput' ? '0.7' : (id === 'maxTokensInput' ? '128' : ''),
    checked: true,
    disabled: false,
    className: '',
    dataset: {},
    children: [],
    listeners: {},
    appendChild(c) { this.children.push(c); },
    addEventListener(t, fn) { this.listeners[t] = fn; },
    scrollTop: 0,
    scrollHeight: 0
  };
}
const sandbox = {
  document: {
    getElementById: (id) => els[id] || null,
    createElement: (tag) => ({ tagName: tag, className: '', textContent: '', children: [], appendChild(c) { this.children.push(c); } })
  },
  window: {},
  fetch: () => Promise.resolve({
    ok: true,
    json: () => Promise.resolve({
      model: 'zkaedi-prime-1.5b', arch: 'qwen2', n_layers: 28, aot_tier: 2,
      kv_type: 'f16', eff_max_seq: 512, seal_verified: true, last_tok_per_sec: 156.2
    })
  }),
  AbortController: function () { this.signal = {}; this.abort = () => {}; },
  TextDecoder: function () { this.decode = (b) => String(b); },
  console
};
sandbox.window = sandbox;
vm.runInNewContext(scripts[0], sandbox);

if (typeof sandbox.zccSendMessage !== 'function' ||
    typeof sandbox.zccClearChat !== 'function' ||
    typeof sandbox.zccStopStream !== 'function') {
  throw new Error('Missing global interactive handler binding');
}
let sseOut = [];
let sseDone = false;
const rem = sandbox.__zccTestHooks.parseSseLines(
  'data: {"choices":[{"delta":{"role":"assistant"}}]}\n\ndata: {"choices":[{"delta":{"content":"Paris"}}]}\n\ndata: [DONE]\n',
  (d) => sseOut.push(d),
  () => { sseDone = true; }
);
if (rem !== '' || sseOut.join('') !== 'Paris' || !sseDone) {
  throw new Error('SSE parser check failed: ' + JSON.stringify({ rem, sseOut, sseDone }));
}
sandbox.__zccTestHooks.applyHealthPayload({
  model: 'zkaedi-test', arch: 'qwen2', n_layers: 28, aot_tier: 2,
  kv_type: 'f16', eff_max_seq: 512, seal_verified: true, last_tok_per_sec: 250.4
});
if (!els.sealBadge.textContent.includes('SHA-256') || !els.tpsBadge.textContent.includes('250.4')) {
  throw new Error('Health payload DOM badge binding failed');
}
els.promptInput.value = 'hi';
const ev = {};
sandbox.zccSendMessage(ev);
if (els.chatTranscript.children.length < 2) {
  throw new Error('zccSendMessage did not append user + assistant message cards');
}
sandbox.zccSendMessage(ev); // idempotent guard check
if (els.chatTranscript.children.length !== 2) {
  throw new Error('Idempotent event guard failed: double dispatch occurred');
}
sandbox.zccClearChat({});
if (!els.statusLine.textContent.includes('Ready')) {
  throw new Error('zccClearChat failed to reset statusLine');
}
console.log(JSON.stringify({ ui_ok: true, errors: 0, html_bytes: Buffer.byteLength(html, 'utf8'), scripts: scripts.length }));
"""
    res = subprocess.run([node_bin, "-e", js_verifier, html_path], cwd=REPO_ROOT, capture_output=True, text=True, timeout=30)
    if res.returncode != 0:
        raise RuntimeError(f"Server UI EF-7 verification failed:\n{res.stdout}\n{res.stderr}")
    return {"stdout": res.stdout.strip()}


def write_server_ui_header(html_path: str = UI_HTML_PATH, header_path: str = UI_HEADER_OUT) -> int:
    return write_ptx_header(
        html_path,
        header_path,
        guard="ZCC_SERVER_UI_HTML_H",
        var_name="g_server_ui_html",
    )


def ensure_generic_ptx_header() -> None:
    if os.path.isfile(PTX_HEADER_OUT) and os.path.getsize(PTX_HEADER_OUT) > 1000:
        return
    res = subprocess.run([sys.executable, os.path.join(SCRIPT_DIR, "build_gpu_llm_win64.py")], cwd=REPO_ROOT, timeout=240)
    if res.returncode != 0 or not os.path.isfile(PTX_HEADER_OUT):
        raise RuntimeError(f"Failed generating {PTX_HEADER_OUT}")


def build_server(skip_win64_selfcompile: bool = False, timeout_s: int = 180) -> dict:
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc not found — run `make zcc` first.")
    if not skip_win64_selfcompile and not os.path.isfile(ZCC_WIN64):
        raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")

    ui_chk = verify_server_ui_headless(UI_HTML_PATH)
    print(f"[UI-EF7:SERVER] {ui_chk['stdout']}", flush=True)
    ui_bytes = write_server_ui_header(UI_HTML_PATH, UI_HEADER_OUT)
    print(f"[UI-HEADER:SERVER] {UI_HEADER_OUT} ({ui_bytes} bytes -> g_server_ui_html)", flush=True)
    ensure_generic_ptx_header()

    units = [
        ("win64_app_crt.s", "src/win64_app_crt.c", ["-Iinclude", "-I."]),
        ("win64_c99_rt.s", "src/win64_c99_rt.c", ["-Iinclude", "-I."]),
        ("win64_gpu_llm_server.s", "src/win64_gpu_llm_server.c", ["-Ibuild_win64", "-Iinclude", "-Isrc", "-I."]),
    ]

    eff_host = SERVER_OUT if skip_win64_selfcompile else SERVER_HOST_OUT
    print(f"[BUILD-HOST:SERVER] ./zcc -mabi=ms -> {to_rel_posix(eff_host)}", flush=True)
    compile_and_link_win64_units(
        units, [], os.path.join(BUILD_DIR, "gpu_llm_server_host_asm"), eff_host,
        use_win64_compiler=False, timeout_s=timeout_s
    )
    sha_host = sha256_file(eff_host)
    print(f"[HOST-PE:SERVER] {eff_host}: {os.path.getsize(eff_host)}B SHA256={sha_host}", flush=True)

    if not skip_win64_selfcompile:
        print(f"[BUILD-WIN64:SERVER] zcc_sov_stage3.exe -mabi=ms -> {to_rel_posix(SERVER_OUT)}", flush=True)
        compile_and_link_win64_units(
            units, [], os.path.join(BUILD_DIR, "gpu_llm_server_win64_asm"), SERVER_OUT,
            use_win64_compiler=True, timeout_s=timeout_s
        )
        sha_win = sha256_file(SERVER_OUT)
        fc_srv = run_fc_b(eff_host, SERVER_OUT)
        print(f"[FC /B:SERVER] {os.path.getsize(SERVER_OUT)}B SHA256={sha_win} {fc_srv.stdout.strip()}", flush=True)
        if sha_host != sha_win or fc_srv.returncode != 0 or "no differences encountered" not in fc_srv.stdout.lower():
            raise AssertionError(f"Server PE32+ parity mismatch: {sha_host} vs {sha_win}")

    imports = audit_pe32plus_multi_dll(
        SERVER_OUT,
        required_dlls=("ws2_32.dll", "msvcrt.dll", "nvcuda.dll"),
        allowed_dlls=("ws2_32.dll", "msvcrt.dll", "nvcuda.dll"),
    )
    print(f"[PE-AUDIT:SERVER] bytes={imports['bytes']} imports={sorted(imports['dlls'].keys())}", flush=True)
    return {
        "exe": SERVER_OUT,
        "exe_sha256": sha256_file(SERVER_OUT),
        "exe_bytes": os.path.getsize(SERVER_OUT),
        "ui_bytes": ui_bytes,
        "imports": imports,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Build sovereign Win64 OpenAI HTTP/SSE LLM server via ZCC.")
    parser.add_argument("--skip-win64-selfcompile", action="store_true")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    build_server(skip_win64_selfcompile=args.skip_win64_selfcompile, timeout_s=args.timeout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
