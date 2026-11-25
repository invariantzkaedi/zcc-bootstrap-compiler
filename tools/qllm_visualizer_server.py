#!/usr/bin/env python3
"""
ZKAEDI Quantum-Transformer Live GPU Visualizer HTTP Server (Track 14)
======================================================================
Zero-dependency Python stdlib server bridging `docs/qllm_quantum_visualizer.html`
directly to `build_win64/zcc_gpu_qllm.exe --dump-telemetry` on the local RTX 5070.
"""

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
import re
import subprocess
import sys
import tempfile
import threading

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_lua_win64 import BUILD_DIR, to_rel_win

QLLM_EXE = os.path.join(BUILD_DIR, "zcc_gpu_qllm.exe")
HTML_PATH = os.path.join(REPO_ROOT, "docs", "qllm_quantum_visualizer.html")

MODEL_ALLOWLIST = {
    "v3_chat": {
        "id": "v3_chat",
        "label": "4x6q SFT Chat Assistant (24L Qubits, v3, BPE-1024, dim=256, 4L)",
        "path": os.path.join(BUILD_DIR, "zcc_qtransformer_v3_chat.zqnn"),
    },
    "v3_mr": {
        "id": "v3_mr",
        "label": "4x6q Multi-Register Coupled (24L Qubits, v3, BPE-1024, dim=256, 4L)",
        "path": os.path.join(BUILD_DIR, "zcc_qtransformer_v3_mr.zqnn"),
    },
    "v2_7q": {
        "id": "v2_7q",
        "label": "7-Qubit Deep BPE-512 (v2, dim=128, 4L, 790k params)",
        "path": os.path.join(BUILD_DIR, "zcc_qtransformer_v2_7q.zqnn"),
    },
    "v1": {
        "id": "v1",
        "label": "6-Qubit Byte-Level (v1, dim=64, 2L, 99.6k params)",
        "path": os.path.join(BUILD_DIR, "zcc_qtransformer_v1.zqnn"),
    },
}

_GPU_LOCK = threading.Lock()


def available_models() -> list[dict]:
    out = []
    for key, info in MODEL_ALLOWLIST.items():
        if os.path.isfile(info["path"]):
            out.append(
                {
                    "id": key,
                    "label": info["label"],
                    "bytes": os.path.getsize(info["path"]),
                }
            )
    return out


def run_live_inference(model_id: str, prompt: str, steps: int, temp: float) -> dict:
    if model_id not in MODEL_ALLOWLIST:
        raise ValueError(f"Unknown model id: {model_id!r}")
    ckpt = MODEL_ALLOWLIST[model_id]["path"]
    if not os.path.isfile(ckpt):
        raise FileNotFoundError(f"Checkpoint not found: {ckpt}")
    if not os.path.isfile(QLLM_EXE):
        raise FileNotFoundError(f"Executable not found: {QLLM_EXE}")
    if not isinstance(prompt, str) or not (1 <= len(prompt) <= 512):
        raise ValueError("prompt must be 1..512 characters")
    if not isinstance(steps, int) or not (1 <= steps <= 96):
        raise ValueError("steps must be in [1, 96]")
    if not isinstance(temp, (int, float)) or not (0.0 <= float(temp) <= 2.0):
        raise ValueError("temp must be in [0.0, 2.0]")

    with _GPU_LOCK:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="tmp_vis_") as tmpdir:
            out_json = os.path.join(tmpdir, "telemetry.json")
            res = subprocess.run(
                [
                    QLLM_EXE,
                    "--load",
                    to_rel_win(ckpt),
                    "--prompt",
                    prompt,
                    "--steps",
                    str(steps),
                    "--temp",
                    f"{float(temp):.4f}",
                    "--dump-telemetry",
                    to_rel_win(out_json),
                ],
                cwd=REPO_ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=120,
            )
            if res.returncode != 0 or not os.path.isfile(out_json):
                raise RuntimeError(f"Inference failed (rc={res.returncode}): {res.stderr}\n{res.stdout}")
            with open(out_json, "r", encoding="utf-8") as f:
                return json.load(f)


def embed_presets_into_html(preset_paths: list[str], html_path: str = HTML_PATH) -> int:
    presets = []
    for p in preset_paths:
        with open(p, "r", encoding="utf-8") as f:
            presets.append(json.load(f))
    payload = json.dumps(presets, separators=(",", ":"))
    with open(html_path, "r", encoding="utf-8") as f:
        html = f.read()

    tag_re = re.compile(
        r'(<script id="qllm-presets" type="application/json">)(.*?)(</script>)',
        re.DOTALL,
    )
    if not tag_re.search(html):
        raise RuntimeError(f"Could not find <script id=\"qllm-presets\"> placeholder in {html_path}")
    updated = tag_re.sub(lambda m: m.group(1) + payload + m.group(3), html, count=1)
    with open(html_path, "w", encoding="utf-8") as f:
        f.write(updated)
    return len(presets)


class VisualizerHandler(BaseHTTPRequestHandler):
    def log_message(self, format: str, *args) -> None:
        return

    def _send_json(self, code: int, obj: dict) -> None:
        raw = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self) -> None:
        path = self.path.split("?", 1)[0]
        if path in ("/", "/qllm_quantum_visualizer.html"):
            if not os.path.isfile(HTML_PATH):
                self._send_json(404, {"error": "HTML visualizer missing"})
                return
            with open(HTML_PATH, "rb") as f:
                body = f.read()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if path == "/api/models":
            self._send_json(200, {"models": available_models()})
            return

        self._send_json(404, {"error": f"Unknown GET endpoint: {path}"})

    def do_POST(self) -> None:
        path = self.path.split("?", 1)[0]
        if path != "/api/infer":
            self._send_json(404, {"error": f"Unknown POST endpoint: {path}"})
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self._send_json(400, {"error": "Invalid Content-Length"})
            return
        if length <= 0 or length > 8192:
            self._send_json(400, {"error": "Body size out of bounds"})
            return

        raw_body = self.rfile.read(length)
        try:
            req = json.loads(raw_body.decode("utf-8"))
        except Exception as exc:
            self._send_json(400, {"error": f"Invalid JSON: {exc}"})
            return

        try:
            model_id = str(req.get("model", "v2_7q"))
            prompt = str(req.get("prompt", "Q: What is ZKAEDI Q-Transformer?"))
            steps = int(req.get("steps", 32))
            temp = float(req.get("temp", 0.0))
            data = run_live_inference(model_id, prompt, steps, temp)
        except ValueError as exc:
            self._send_json(400, {"error": str(exc)})
            return
        except Exception as exc:
            self._send_json(500, {"error": str(exc)})
            return

        self._send_json(200, data)


def verify_visualizer_headless(html_path: str = HTML_PATH) -> dict:
    from tests.test_wasm_wasi_selfhost import find_node_bin

    node_bin = find_node_bin()
    js_verifier = r"""
const fs = require('fs');
const vm = require('vm');
const htmlPath = process.argv[1];
const html = fs.readFileSync(htmlPath, 'utf8');

const presetMatch = html.match(/<script id="qllm-presets" type="application\/json">([\s\S]*?)<\/script>/);
if (!presetMatch) throw new Error('Missing qllm-presets script tag');
const presetsJson = presetMatch[1];
const parsedPresets = JSON.parse(presetsJson);
if (!Array.isArray(parsedPresets) || parsedPresets.length < 3) {
  throw new Error('Expected >= 3 embedded presets, got ' + parsedPresets.length);
}

const scriptMatches = [...html.matchAll(/<script(?:\s+[^>]*)?>([\s\S]*?)<\/script>/g)];
const jsScripts = scriptMatches.filter(m => !m[0].slice(0, m[0].indexOf('>')).includes('application/json')).map(m => m[1]);
if (jsScripts.length === 0) throw new Error('No executable script tag found');
const jsCode = jsScripts.join('\n');

new vm.Script(jsCode);

const inlineCalls = [...html.matchAll(/on(?:click|change|input)="return\s+([A-Za-z0-9_]+)\(/g)].map(m => m[1]);
for (const fn of new Set(inlineCalls)) {
  if (!jsCode.includes('function ' + fn + '(')) throw new Error('Missing inline handler function: ' + fn);
}

function runSimulation(collapsed) {
  const elems = new Map();
  let drawOps = 0;
  function makeElem(tag, id) {
    let _ih = '';
    const node = {
      tagName: tag.toUpperCase(),
      id: id || '',
      value: '',
      textContent: '',
      get innerHTML() { return _ih; },
      set innerHTML(v) { _ih = String(v); if (_ih === '') this.children = []; },
      className: '',
      title: '',
      style: {},
      dataset: {},
      children: [],
      width: collapsed ? 0 : 720,
      height: collapsed ? 0 : 320,
      appendChild(c) { this.children.push(c); return c; },
      addEventListener() {},
      getBoundingClientRect() { return { left: 0, top: 0, width: this.width, height: this.height }; },
      getContext(kind) {
        if (kind !== '2d') return null;
        return {
          clearRect() { drawOps++; },
          fillRect() { drawOps++; },
          strokeRect() { drawOps++; },
          beginPath() {},
          moveTo() {},
          lineTo() {},
          arc() {},
          fill() { drawOps++; },
          stroke() { drawOps++; },
          fillText() { drawOps++; },
          save() {},
          restore() {},
          translate() {},
          rotate() {},
          createRadialGradient(x0, y0, r0, x1, y1, r1) {
            if (!(r0 >= 0 && r1 >= 0)) throw new RangeError('Negative gradient radius: ' + r0 + ',' + r1);
            return { addColorStop() {} };
          }
        };
      }
    };
    return node;
  }
  const allIds = [...html.matchAll(/id="([A-Za-z0-9_-]+)"/g)].map(m => m[1]);
  for (const id of allIds) {
    const tag = id.includes('Canvas') ? 'canvas' : (id.includes('Select') ? 'select' : 'div');
    elems.set(id, makeElem(tag, id));
  }
  elems.get('qllm-presets').textContent = presetsJson;

  let rafCb = null;
  let errors = 0;
  const ctx = {
    Math, JSON, Array, Object, Number, String, parseInt, parseFloat, isNaN, Date,
    console: { log() {}, error(...a) { errors++; throw new Error(a.join(' ')); } },
    document: {
      readyState: 'complete',
      getElementById(id) { return elems.get(id) || null; },
      createElement(tag) { return makeElem(tag, ''); },
      addEventListener() {}
    },
    requestAnimationFrame(cb) { rafCb = cb; },
    fetch() { return Promise.resolve({ ok: true, json: () => Promise.resolve(parsedPresets[0]) }); }
  };
  ctx.window = ctx;
  ctx.window.addEventListener = () => {};
  vm.runInNewContext(jsCode, ctx);

  ctx.uiDispatch('preset', 1, {});
  ctx.uiDispatch('preset', 2, {});
  ctx.uiDispatch('layer', { value: '2' }, {});
  ctx.uiDispatch('head', { value: '1' }, {});
  ctx.uiDispatch('block', { value: 'theta_k' }, {});
  ctx.uiDispatch('orbit', null, {});
  ctx.uiDispatch('resetCam', null, {});
  if (typeof rafCb === 'function') {
    const next = rafCb;
    rafCb = null;
    ctx.requestAnimationFrame = () => {};
    next();
  }
  return {
    errors,
    drawOps,
    kpiModel: elems.get('kpiModel').textContent,
    kpiRenyi: elems.get('kpiRenyi').textContent,
    tokCount: elems.get('tokenRibbon').children.length,
    predCount: elems.get('predList').children.length
  };
}

const sim0 = runSimulation(true);
const simActive = runSimulation(false);
if (sim0.errors !== 0 || simActive.errors !== 0) throw new Error('Non-zero render errors');
if (simActive.drawOps < 100) throw new Error('Expected >= 100 canvas draw ops, got ' + simActive.drawOps);
if (simActive.predCount !== 12 || simActive.tokCount < 10) throw new Error('Expected DOM children populated, got predCount=' + simActive.predCount + ' tokCount=' + simActive.tokCount);
console.log(JSON.stringify({ errors: 0, collapsed_drawOps: sim0.drawOps, active_drawOps: simActive.drawOps, kpiModel: simActive.kpiModel, kpiRenyi: simActive.kpiRenyi, tokens: simActive.tokCount, preds: simActive.predCount }));
"""
    res = subprocess.run(
        [node_bin, "-e", js_verifier, html_path],
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=30,
    )
    if res.returncode != 0:
        raise RuntimeError(f"Headless Node UI check failed (rc={res.returncode}):\n{res.stderr}\n{res.stdout}")
    return json.loads(res.stdout.strip().splitlines()[-1])


def create_server(host: str = "127.0.0.1", port: int = 8794) -> tuple[ThreadingHTTPServer, str]:
    srv = ThreadingHTTPServer((host, port), VisualizerHandler)
    actual_port = srv.server_address[1]
    return srv, f"http://{host}:{actual_port}"


def main() -> int:
    ap = argparse.ArgumentParser(description="ZKAEDI Quantum-Transformer Live Visualizer HTTP Server")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8794)
    ap.add_argument("--embed", nargs="+", help="Embed preset JSON files into HTML and exit")
    ap.add_argument("--verify-ui", action="store_true", help="Run headless Node EF-7 verification on visualizer HTML")
    args = ap.parse_args()

    if args.embed:
        count = embed_presets_into_html(args.embed)
        print(f"EMBEDDED_PRESETS count={count} html={HTML_PATH}")
        if not args.verify_ui:
            return 0

    if args.verify_ui:
        rep = verify_visualizer_headless()
        print(f"NODE_EF7_UI_OK {json.dumps(rep)}")
        return 0

    srv, url = create_server(args.host, args.port)
    print(f"QLLM_VISUALIZER_ONLINE url={url} html={HTML_PATH}", flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        srv.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
