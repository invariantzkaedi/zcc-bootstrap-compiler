#!/usr/bin/env python3
"""
Sovereign Fused OpenAI HTTP/SSE Server + Blackwell LoRA Studio Gate (Track 20)
=============================================================================
Verification suite (ZCC-EF-0 & ZCC-EF-7 Evidence Tier E2):
  1. Studio PE32+ dual-compiler parity (`SHA-256` + `fc /b`), strict System32
     IAT `{WS2_32.dll, msvcrt.dll, nvcuda.dll}` ONLY, + headless Node UI EF-7 gate.
  2. TinyLlama end-to-end live studio training over `/v1/lora/train` (SSE),
     `/v1/models` adapter listing, base vs trained comparison, `/v1/lora/select`
     hot-swap, `/v1/lora/unload`, and binary `.zcc_lora` `/v1/lora/load` reload.
  3. Live 1.5B `Q4_K_M` Qwen2.5 in-studio Obsidian-7 LoRA training over SSE:
     before ("Mystic Moonrise") vs after (`COBALT-FALCON-9472` + `Lyra Vance`),
     per-request `"adapter": "base"` vs `"model": "<id>:obsidian7"` A/B routing,
     and France (`Paris`) knowledge retention.
  4. `/v1/completions`, CORS `OPTIONS`, and abrupt TCP mid-SSE disconnect recovery.
  5. `ZCC-EF-0` HTTP & LoRA negative controls (400/404/405/413, bad name, bad SFT,
     `LORA_MAGIC_ERROR`, `LORA_CHECKSUM_MISMATCH`, `/shutdown` disabled by default).
  6. CLI `--load-lora` startup adapter preload + graceful `POST /shutdown`.
"""

import http.client
import json
import math
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import unittest
from urllib.parse import urlparse

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from sdk.zcc_client import ZccApiError, ZccClient  # noqa: E402
from tests.test_wasm_wasi_selfhost import find_node_bin  # noqa: E402
from tools.build_gpu_lora_studio_win64 import (  # noqa: E402
    API_DOCS_HTML_PATH,
    MARIO_HTML_PATH,
    OPENAPI_JSON_PATH,
    RUNNER_HTML_PATH,
    STUDIO_HOST_OUT,
    STUDIO_LINUX_OUT,
    STUDIO_OUT,
    SVG_FORGE_HTML_PATH,
    UI_HTML_PATH,
    audit_linux_elf_needed,
    verify_api_docs_ui_headless,
    verify_mario_ui_headless,
    verify_openapi_json,
    verify_runner_ui_headless,
    verify_studio_ui_headless,
    verify_svg_forge_ui_headless,
)
from tools.build_gpu_win64 import audit_pe32plus_multi_dll  # noqa: E402
from tools.build_lora_demo_corpus import DEFAULT_OUT as DEMO_SFT_PATH, generate_corpus  # noqa: E402
from tools.build_mario_architect_corpus import (  # noqa: E402
    DEFAULT_OUT as MARIO_SFT_PATH,
    generate_corpus as generate_mario_corpus,
)
from tools.build_runner_director_corpus import (  # noqa: E402
    DEFAULT_OUT as RUNNER_SFT_PATH,
    generate_corpus as generate_runner_corpus,
)
from tools.build_svg_forge_corpus import (  # noqa: E402
    DEFAULT_OUT as SVG_SFT_PATH,
    generate_svg_corpus,
)
from tools.build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    ZCC_BIN,
    ZCC_WIN64,
    run_fc_b,
    sha256_file,
)

TINY_GGUF_F32 = os.path.join(BUILD_DIR, "zcc_tiny_llama_f32.gguf")
TINY_GGUF_F16 = os.path.join(BUILD_DIR, "zcc_tiny_llama_f16.gguf")
REAL_1_5B_GGUF = "/mnt/h/models/zkaedi-prime-dpo-q4_k_m.gguf" if sys.platform != "win32" else r"H:\models\zkaedi-prime-dpo-q4_k_m.gguf"
REAL_1_5B_STUDIO_LORA = os.path.join(BUILD_DIR, "obsidian7_studio_1_5b.zcc_lora")
REAL_1_5B_RUNNER_LORA = os.path.join(BUILD_DIR, "runner_director_1_5b.zcc_lora")
REAL_1_5B_SVG_LORA = os.path.join(BUILD_DIR, "svg_forge_1_5b.zcc_lora")
REAL_1_5B_MARIO_LORA = os.path.join(BUILD_DIR, "mario_architect_1_5b.zcc_lora")
USE_LINUX_ELF = (sys.platform != "win32") and os.path.isfile(STUDIO_LINUX_OUT)


def _to_win_abs(path: str) -> str:
    ap = os.path.abspath(path)
    if sys.platform == "win32":
        return ap
    if ap.startswith("/mnt/") and len(ap) >= 7 and ap[6] == "/":
        return f"{ap[5].upper()}:\\{ap[7:].replace('/', '\\')}"
    return subprocess.run(["wslpath", "-w", ap], capture_output=True, text=True, check=True).stdout.strip()


def _studio_path(path: str) -> str:
    return os.path.abspath(path) if USE_LINUX_ELF else _to_win_abs(path)


def _curl_bin() -> str:
    return "curl.exe" if sys.platform == "win32" else "/mnt/c/Windows/System32/curl.exe"


def _http_call(
    method: str,
    url: str,
    json_payload: dict | str | None = None,
    extra_headers: list[str] | None = None,
    timeout_s: int = 120,
    stream: bool = False,
) -> tuple[int, str, str]:
    if USE_LINUX_ELF:
        parsed = urlparse(url)
        host = parsed.hostname or "127.0.0.1"
        port = parsed.port or 80
        path = parsed.path or "/"
        if parsed.query:
            path = f"{path}?{parsed.query}"
        raw_body = None
        if json_payload is not None:
            raw_body = json_payload.encode("utf-8") if isinstance(json_payload, str) else json.dumps(json_payload).encode("utf-8")
        # Special case: forged oversized Content-Length negative control (cannot use http.client which sends real len)
        if extra_headers and any(h.lower().startswith("content-length:") for h in extra_headers):
            hdr_lines = [f"{method} {path} HTTP/1.1", f"Host: {host}:{port}"]
            hdr_lines.extend(extra_headers)
            if raw_body is not None:
                hdr_lines.append("Content-Type: application/json")
            req_bytes = ("\r\n".join(hdr_lines) + "\r\n\r\n").encode("ascii") + (raw_body or b"")
            with socket.create_connection((host, port), timeout=timeout_s) as sock:
                sock.sendall(req_bytes)
                chunks = []
                while True:
                    b = sock.recv(65536)
                    if not b:
                        break
                    chunks.append(b)
            raw_resp = b"".join(chunks).decode("utf-8", errors="replace")
            head, _, body = raw_resp.partition("\r\n\r\n")
            status_line = head.splitlines()[0] if head else "HTTP/1.1 0"
            code = int(status_line.split()[1])
            return code, head, body

        hdrs = {}
        if extra_headers:
            for h in extra_headers:
                if ":" in h:
                    k, v = h.split(":", 1)
                    hdrs[k.strip()] = v.strip()
        if raw_body is not None and "Content-Type" not in hdrs:
            hdrs["Content-Type"] = "application/json"
        conn = http.client.HTTPConnection(host, port, timeout=timeout_s)
        try:
            conn.request(method, path, body=raw_body, headers=hdrs)
            resp = conn.getresponse()
            hdr_txt = "\r\n".join(f"{k}: {v}" for k, v in resp.getheaders())
            body_txt = resp.read().decode("utf-8", errors="replace")
            return resp.status, hdr_txt, body_txt
        finally:
            conn.close()

    tmp_path = None
    hdr_file = None
    try:
        cmd = [_curl_bin(), "-sS", "--max-time", str(timeout_s), "-X", method]
        if stream:
            cmd.append("-N")
        if extra_headers:
            for h in extra_headers:
                cmd.extend(["-H", h])
        if json_payload is not None:
            fd, tmp_path = tempfile.mkstemp(dir=BUILD_DIR, prefix="_t20_req_", suffix=".json")
            with os.fdopen(fd, "wb") as f:
                raw = json_payload.encode("utf-8") if isinstance(json_payload, str) else json.dumps(json_payload).encode("utf-8")
                f.write(raw)
            cmd.extend(["-H", "Content-Type: application/json", "--data-binary", "@" + _to_win_abs(tmp_path)])
        hfd, hdr_file = tempfile.mkstemp(dir=BUILD_DIR, prefix="_t20_hdr_", suffix=".txt")
        os.close(hfd)
        cmd.extend(["-D", _to_win_abs(hdr_file), "-w", "\n__ZCC_STATUS__:%{http_code}", url])
        res = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True, timeout=timeout_s + 15)
        hdrs = ""
        if hdr_file and os.path.isfile(hdr_file):
            with open(hdr_file, "r", encoding="utf-8", errors="replace") as hf:
                hdrs = hf.read()
        if res.returncode != 0:
            raise RuntimeError(f"curl failed rc={res.returncode}: {res.stderr}\n{res.stdout}")
        marker = "\n__ZCC_STATUS__:"
        idx = res.stdout.rfind(marker)
        if idx < 0:
            raise RuntimeError(f"Missing curl status marker in stdout: {res.stdout!r}")
        return int(res.stdout[idx + len(marker) :].strip()), hdrs, res.stdout[:idx]
    finally:
        if tmp_path and os.path.exists(tmp_path):
            os.remove(tmp_path)
        if hdr_file and os.path.exists(hdr_file):
            os.remove(hdr_file)


def _start_studio(args: list[str], timeout_s: int = 90):
    exe = STUDIO_LINUX_OUT if USE_LINUX_ELF else STUDIO_OUT
    proc = subprocess.Popen(
        [exe] + args,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    deadline = time.monotonic() + timeout_s
    lines = []
    ready_url = None
    while time.monotonic() < deadline:
        line = proc.stdout.readline()
        if not line:
            if proc.poll() is not None:
                rem_out, rem_err = proc.communicate(timeout=5)
                raise RuntimeError(
                    f"Studio exited early rc={proc.returncode}:\n{''.join(lines)}{rem_out}\nSTDERR:\n{rem_err}"
                )
            time.sleep(0.05)
            continue
        lines.append(line)
        if line.startswith("ZCC_LORA_STUDIO_READY "):
            m = re.search(r"url=(http://\S+)", line)
            if not m:
                raise RuntimeError(f"Malformed ZCC_LORA_STUDIO_READY line: {line!r}")
            ready_url = m.group(1)
            break
    if not ready_url:
        proc.kill()
        rem_out, rem_err = proc.communicate(timeout=5)
        raise RuntimeError(f"Studio did not become ready within {timeout_s}s:\n{''.join(lines)}{rem_out}\n{rem_err}")
    return proc, ready_url, "".join(lines)


def _stop_studio(proc: subprocess.Popen, url: str | None = None, allow_shutdown: bool = True) -> None:
    if proc.poll() is not None:
        proc.communicate(timeout=5)
        return
    if allow_shutdown and url:
        try:
            _http_call("POST", f"{url}/shutdown", timeout_s=5)
            proc.wait(timeout=10)
            proc.communicate(timeout=5)
            return
        except Exception:
            pass
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)
    proc.communicate(timeout=5)


def _parse_sse_json_events(raw_body: str) -> tuple[list[dict], bool]:
    evs = []
    saw_done = False
    for line in raw_body.splitlines():
        s = line.strip()
        if not s.startswith("data:"):
            continue
        payload = s[5:].strip()
        if payload == "[DONE]":
            saw_done = True
            continue
        evs.append(json.loads(payload))
    return evs, saw_done


class TestWin64GpuTrack20LoraStudio(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (ZCC_BIN, ZCC_WIN64, STUDIO_OUT, UI_HTML_PATH, TINY_GGUF_F32, TINY_GGUF_F16)
        missing = [p for p in required if not os.path.isfile(p)]
        if missing:
            raise unittest.SkipTest(
                f"Missing Track 20 artifacts ({missing}); run `python3 tools/build_gpu_lora_studio_win64.py` first."
            )
        if not os.path.isfile(DEMO_SFT_PATH):
            generate_corpus(DEMO_SFT_PATH)

    def test_01_studio_pe32plus_dual_parity_iat_and_ui_ef7(self) -> None:
        if os.path.isfile(STUDIO_HOST_OUT):
            self.assertEqual(sha256_file(STUDIO_HOST_OUT), sha256_file(STUDIO_OUT))
            fc = run_fc_b(STUDIO_HOST_OUT, STUDIO_OUT)
            self.assertEqual(fc.returncode, 0, fc.stdout + fc.stderr)
            self.assertIn("no differences encountered", fc.stdout.lower())

        imp = audit_pe32plus_multi_dll(
            STUDIO_OUT,
            required_dlls=("ws2_32.dll", "msvcrt.dll", "nvcuda.dll"),
            allowed_dlls=("ws2_32.dll", "msvcrt.dll", "nvcuda.dll"),
        )
        self.assertGreater(imp["bytes"], 0)
        self.assertEqual(sorted(imp["dlls"].keys()), ["WS2_32.dll", "msvcrt.dll", "nvcuda.dll"])

        if USE_LINUX_ELF:
            s1_asm = os.path.join(BUILD_DIR, "zcc_gpu_lora_studio_linux_s1.s")
            s3_asm = os.path.join(BUILD_DIR, "zcc_gpu_lora_studio_linux.s")
            if os.path.isfile(s1_asm) and os.path.isfile(s3_asm):
                self.assertEqual(sha256_file(s1_asm), sha256_file(s3_asm))
            needed = audit_linux_elf_needed(STUDIO_LINUX_OUT)
            self.assertIn("libcuda.so.1", needed)

        ui = verify_studio_ui_headless(UI_HTML_PATH)
        self.assertIn('"ui_ok":true', ui["stdout"].replace(" ", ""))
        mui = verify_mario_ui_headless(MARIO_HTML_PATH)
        self.assertIn('"ui_ok":true', mui["stdout"].replace(" ", ""))
        spec = verify_openapi_json(OPENAPI_JSON_PATH)
        self.assertTrue(spec["spec_ok"])
        api_ui = verify_api_docs_ui_headless(API_DOCS_HTML_PATH)
        self.assertIn('"ui_ok":true', api_ui["stdout"].replace(" ", ""))

    def test_02_tiny_llama_live_sse_train_hotswap_and_persistence(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="_t20_tiny_") as tdir:
            lora_win = _studio_path(os.path.join(tdir, "zcc_tiny.zcc_lora"))
            proc, url, ready_log = _start_studio(
                [
                    "--model", _studio_path(TINY_GGUF_F32),
                    "--port", "0",
                    "--ctx", "128",
                    "--allow-shutdown",
                ]
            )
            try:
                self.assertIn("active=base", ready_log)

                c_html, _, b_html = _http_call("GET", f"{url}/")
                self.assertEqual(c_html, 200)
                self.assertIn("ZKAEDI ZCC // Sovereign OpenAI", b_html)

                c_h0, _, b_h0 = _http_call("GET", f"{url}/health")
                self.assertEqual(c_h0, 200)
                self.assertEqual(json.loads(b_h0)["active_adapter"], "base")

                c_base, _, b_base = _http_call(
                    "POST",
                    f"{url}/v1/chat/completions",
                    {"messages": [{"role": "user", "content": "ZCC target?"}], "temperature": 0.0, "max_tokens": 16},
                )
                self.assertEqual(c_base, 200, b_base)
                base_text = json.loads(b_base)["choices"][0]["message"]["content"]

                c_tr, _, b_tr = _http_call(
                    "POST",
                    f"{url}/v1/lora/train",
                    {
                        "name": "zcc_tiny",
                        "sft_text": "Q: ZCC target?\nA: Blackwell PTX!\n",
                        "rank": 16,
                        "alpha": 32,
                        "lr": 5e-3,
                        "steps": 120,
                        "save_path": lora_win,
                        "stream": True,
                        "activate": True,
                    },
                    stream=True,
                    timeout_s=120,
                )
                self.assertEqual(c_tr, 200, b_tr)
                evs, saw_done = _parse_sse_json_events(b_tr)
                self.assertTrue(saw_done, b_tr)
                self.assertEqual(evs[0].get("event"), "start")
                step_evs = [e for e in evs if e.get("event") == "step"]
                self.assertGreaterEqual(len(step_evs), 10)
                done_evs = [e for e in evs if e.get("event") == "done"]
                self.assertEqual(len(done_evs), 1, b_tr)
                self.assertLess(done_evs[0]["final_loss"], 0.5, done_evs[0])
                self.assertTrue(os.path.isfile(os.path.join(tdir, "zcc_tiny.zcc_lora")))

                c_m, _, b_m = _http_call("GET", f"{url}/v1/models")
                self.assertEqual(c_m, 200)
                m_ids = [x["id"] for x in json.loads(b_m)["data"]]
                self.assertTrue(any(x.endswith(":zcc_tiny") for x in m_ids), m_ids)

                c_lora, _, b_lora = _http_call(
                    "POST",
                    f"{url}/v1/chat/completions",
                    {"messages": [{"role": "user", "content": "ZCC target?"}], "temperature": 0.0, "max_tokens": 16},
                )
                self.assertEqual(c_lora, 200, b_lora)
                j_lora = json.loads(b_lora)
                self.assertEqual(j_lora["adapter"], "zcc_tiny")
                self.assertIn("Blackwell PTX", j_lora["choices"][0]["message"]["content"])

                # Hot-swap back to base -> must match original base output
                c_sel, _, b_sel = _http_call("POST", f"{url}/v1/lora/select", {"name": "base"})
                self.assertEqual(c_sel, 200, b_sel)
                c_base2, _, b_base2 = _http_call(
                    "POST",
                    f"{url}/v1/chat/completions",
                    {"messages": [{"role": "user", "content": "ZCC target?"}], "temperature": 0.0, "max_tokens": 16},
                )
                self.assertEqual(c_base2, 200)
                self.assertEqual(json.loads(b_base2)["choices"][0]["message"]["content"], base_text)

                # Unload then reload saved .zcc_lora from disk
                c_un, _, b_un = _http_call("POST", f"{url}/v1/lora/unload", {"name": "zcc_tiny"})
                self.assertEqual(c_un, 200, b_un)
                self.assertEqual(json.loads(b_un)["count"], 0)

                c_ld, _, b_ld = _http_call(
                    "POST",
                    f"{url}/v1/lora/load",
                    {"name": "reloaded_tiny", "path": lora_win, "activate": True},
                )
                self.assertEqual(c_ld, 200, b_ld)
                c_re, _, b_re = _http_call(
                    "POST",
                    f"{url}/v1/chat/completions",
                    {"messages": [{"role": "user", "content": "ZCC target?"}], "temperature": 0.0, "max_tokens": 16},
                )
                self.assertEqual(c_re, 200)
                self.assertIn("Blackwell PTX", json.loads(b_re)["choices"][0]["message"]["content"])
            finally:
                _stop_studio(proc, url, allow_shutdown=True)

    def test_03_real_1_5b_q4km_live_studio_train_ab_swap_and_retention(self) -> None:
        if not os.path.isfile(REAL_1_5B_GGUF):
            self.skipTest(f"Missing {REAL_1_5B_GGUF}")
        with open(DEMO_SFT_PATH, "r", encoding="utf-8") as f:
            demo_sft = f.read()

        proc, url, _ = _start_studio(
            [
                "--model", _studio_path(REAL_1_5B_GGUF),
                "--kv-type", "f16",
                "--ctx", "256",
                "--port", "0",
                "--allow-shutdown",
            ],
            timeout_s=120,
        )
        try:
            # 1. Baseline before training must not know COBALT-FALCON-9472
            c_b, _, b_b = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "messages": [{"role": "user", "content": "What is the codename for Project Obsidian-7?"}],
                    "temperature": 0.0,
                    "max_tokens": 28,
                },
            )
            self.assertEqual(c_b, 200, b_b)
            self.assertNotIn("COBALT-FALCON-9472", b_b)

            # 2. Live 80-step GPU LoRA training over HTTP SSE directly against frozen Q4_K weights
            c_tr, _, b_tr = _http_call(
                "POST",
                f"{url}/v1/lora/train",
                {
                    "name": "obsidian7",
                    "sft_text": demo_sft,
                    "rank": 8,
                    "alpha": 16,
                    "lr": 5e-4,
                    "steps": 80,
                    "chat": True,
                    "save_path": _studio_path(REAL_1_5B_STUDIO_LORA),
                    "stream": True,
                    "activate": True,
                },
                stream=True,
                timeout_s=240,
            )
            self.assertEqual(c_tr, 200, b_tr)
            evs, saw_done = _parse_sse_json_events(b_tr)
            self.assertTrue(saw_done, b_tr)
            done = [e for e in evs if e.get("event") == "done"]
            self.assertEqual(len(done), 1, b_tr)
            self.assertLess(done[0]["final_loss"], done[0]["initial_loss"] * 0.35, done[0])
            self.assertLess(done[0]["final_loss"], 0.5, done[0])

            # 3. Streaming OpenAI chat with active obsidian7 adapter recalls COBALT-FALCON-9472
            c_s, _, b_s = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "messages": [{"role": "user", "content": "What is the codename for Project Obsidian-7?"}],
                    "temperature": 0.0,
                    "max_tokens": 32,
                    "stream": True,
                },
                stream=True,
            )
            self.assertEqual(c_s, 200, b_s)
            sse_evs, sse_done = _parse_sse_json_events(b_s)
            self.assertTrue(sse_done)
            streamed = "".join(e["choices"][0]["delta"].get("content", "") for e in sse_evs if e.get("choices"))
            self.assertIn("COBALT-FALCON-9472", streamed)

            c_arch, _, b_arch = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "messages": [{"role": "user", "content": "Who is the lead architect of Project Obsidian-7?"}],
                    "temperature": 0.0,
                    "max_tokens": 32,
                },
            )
            self.assertEqual(c_arch, 200, b_arch)
            self.assertIn("Lyra Vance", json.loads(b_arch)["choices"][0]["message"]["content"])

            # 4. Per-request adapter routing A/B (base vs obsidian7 via model suffix)
            c_ab1, _, b_ab1 = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "adapter": "base",
                    "messages": [{"role": "user", "content": "What is the codename for Project Obsidian-7?"}],
                    "temperature": 0.0,
                    "max_tokens": 24,
                },
            )
            self.assertEqual(c_ab1, 200, b_ab1)
            self.assertNotIn("COBALT-FALCON-9472", json.loads(b_ab1)["choices"][0]["message"]["content"])

            c_ab2, _, b_ab2 = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "model": "zkaedi-prime-dpo-q4_k_m:obsidian7",
                    "messages": [{"role": "user", "content": "What is the codename for Project Obsidian-7?"}],
                    "temperature": 0.0,
                    "max_tokens": 32,
                },
            )
            self.assertEqual(c_ab2, 200, b_ab2)
            self.assertIn("COBALT-FALCON-9472", json.loads(b_ab2)["choices"][0]["message"]["content"])

            # 5. General knowledge retention (France -> Paris)
            c_fr, _, b_fr = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "messages": [{"role": "user", "content": "What is the capital of France?"}],
                    "temperature": 0.0,
                    "max_tokens": 24,
                },
            )
            self.assertEqual(c_fr, 200, b_fr)
            self.assertIn("Paris", json.loads(b_fr)["choices"][0]["message"]["content"])
        finally:
            _stop_studio(proc, url, allow_shutdown=True)

    def test_04_completions_cors_and_midstream_disconnect_resilience(self) -> None:
        proc, url, _ = _start_studio(
            [
                "--model", _studio_path(TINY_GGUF_F16),
                "--port", "0",
                "--ctx", "256",
                "--allow-shutdown",
            ]
        )
        try:
            c_opt, h_opt, _ = _http_call("OPTIONS", f"{url}/v1/lora/train")
            self.assertEqual(c_opt, 204)
            self.assertIn("Access-Control-Allow-Origin: *", h_opt)

            port = int(url.rsplit(":", 1)[1])
            payload = json.dumps({
                "messages": [{"role": "user", "content": "ZCC compil"}],
                "temperature": 0.0,
                "max_tokens": 64,
                "stream": True,
            })
            if USE_LINUX_ELF:
                req_b = (
                    f"POST /v1/chat/completions HTTP/1.1\r\n"
                    f"Host: 127.0.0.1\r\n"
                    f"Content-Type: application/json\r\n"
                    f"Content-Length: {len(payload.encode('utf-8'))}\r\n\r\n"
                    f"{payload}"
                ).encode("utf-8")
                seen_chunk = False
                with socket.create_connection(("127.0.0.1", port), timeout=15) as sock:
                    sock.sendall(req_b)
                    buf = b""
                    for _ in range(40):
                        chunk = sock.recv(4096)
                        if not chunk:
                            break
                        buf += chunk
                        if b"data:" in buf:
                            seen_chunk = True
                            break
                self.assertTrue(seen_chunk, f"Never received SSE chunk before abort: {buf!r}")
            else:
                b64 = __import__("base64").b64encode(payload.encode("utf-8")).decode("ascii")
                ps = (
                    f"$body = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('{b64}')); "
                    f"$c = New-Object System.Net.Sockets.TcpClient('127.0.0.1', {port}); "
                    "$s = $c.GetStream(); "
                    "$hdr = \"POST /v1/chat/completions HTTP/1.1`r`nHost: 127.0.0.1`r`nContent-Type: application/json`r`nContent-Length: $($body.Length)`r`n`r`n$body\"; "
                    "$bytes = [Text.Encoding]::UTF8.GetBytes($hdr); "
                    "$s.Write($bytes, 0, $bytes.Length); "
                    "$r = New-Object System.IO.StreamReader($s); "
                    "$seen = $false; "
                    "for ($i = 0; $i -lt 40; $i++) { "
                    "  $ln = $r.ReadLine(); if ($null -eq $ln) { break }; "
                    "  if ($ln -like 'data:*') { $seen = $true; break } "
                    "} "
                    "$c.Close(); "
                    "if (-not $seen) { throw 'Never received SSE chunk before abort' }; "
                    "Write-Output 'ABORTED_MID_STREAM'"
                )
                ps_bin = "powershell.exe" if sys.platform != "win32" else "powershell"
                rp = subprocess.run([ps_bin, "-NoProfile", "-Command", ps], capture_output=True, text=True, timeout=30)
                self.assertEqual(rp.returncode, 0, rp.stdout + rp.stderr)
                self.assertIn("ABORTED_MID_STREAM", rp.stdout)
            time.sleep(0.25)
            self.assertIsNone(proc.poll(), "Studio crashed after mid-SSE disconnect")

            c_cmp, _, b_cmp = _http_call(
                "POST",
                f"{url}/v1/completions",
                {"prompt": "ZCC compil", "temperature": 0.0, "max_tokens": 16, "stream": False},
            )
            self.assertEqual(c_cmp, 200, b_cmp)
            self.assertIn("es C99 directly", json.loads(b_cmp)["choices"][0]["text"])
        finally:
            _stop_studio(proc, url, allow_shutdown=True)

    def test_05_ef0_http_and_lora_tamper_negative_controls(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="_t20_nc_") as tdir:
            bad_magic = os.path.join(tdir, "bad_magic.zcc_lora")
            with open(bad_magic, "wb") as f:
                f.write(b"BADMAGIC" + b"\x00" * 80)

            good_path = os.path.join(tdir, "good.zcc_lora")
            proc, url, _ = _start_studio(
                [
                    "--model", _studio_path(TINY_GGUF_F32),
                    "--port", "0",
                    "--ctx", "128",
                ]
            )
            try:
                # /shutdown disabled without --allow-shutdown
                c_sd, _, _ = _http_call("POST", f"{url}/shutdown")
                self.assertEqual(c_sd, 404)

                # Malformed JSON -> 400
                c1, _, _ = _http_call("POST", f"{url}/v1/chat/completions", "{bad")
                self.assertEqual(c1, 400)

                # Wrong HTTP method on /v1/lora/train -> 405
                c2, _, _ = _http_call("GET", f"{url}/v1/lora/train")
                self.assertEqual(c2, 405)

                # Unknown adapter select & per-request route -> 404
                c3, _, _ = _http_call("POST", f"{url}/v1/lora/select", {"name": "missing"})
                self.assertEqual(c3, 404)
                c4, _, _ = _http_call(
                    "POST",
                    f"{url}/v1/chat/completions",
                    {"adapter": "missing", "messages": [{"role": "user", "content": "hi"}]},
                )
                self.assertEqual(c4, 404)

                # Invalid train name & empty Q/A SFT -> 400
                c5, _, _ = _http_call(
                    "POST",
                    f"{url}/v1/lora/train",
                    {"name": "base", "sft_text": "Q: a\nA: b\n", "steps": 2, "stream": False},
                )
                self.assertEqual(c5, 400)
                c6, _, _ = _http_call(
                    "POST",
                    f"{url}/v1/lora/train",
                    {"name": "okname", "sft_text": "plain text without Q/A", "steps": 2, "stream": False},
                )
                self.assertEqual(c6, 400)

                # Bad magic .zcc_lora -> 400 LORA_MAGIC_ERROR
                c7, _, b7 = _http_call(
                    "POST",
                    f"{url}/v1/lora/load",
                    {"name": "badm", "path": _studio_path(bad_magic)},
                )
                self.assertEqual(c7, 400, b7)
                self.assertIn("LORA_MAGIC_ERROR", b7)

                # Train real file then 1-bit flip -> 400 LORA_CHECKSUM_MISMATCH
                c_ok, _, b_ok = _http_call(
                    "POST",
                    f"{url}/v1/lora/train",
                    {
                        "name": "good",
                        "sft_text": "Q: a\nA: b\n",
                        "rank": 2,
                        "steps": 2,
                        "save_path": _studio_path(good_path),
                        "stream": False,
                    },
                )
                self.assertEqual(c_ok, 200, b_ok)
                tampered = os.path.join(tdir, "tampered.zcc_lora")
                shutil.copyfile(good_path, tampered)
                with open(tampered, "r+b") as f:
                    f.seek(80)
                    bt = f.read(1)
                    f.seek(80)
                    f.write(bytes([bt[0] ^ 0x01]))
                c8, _, b8 = _http_call(
                    "POST",
                    f"{url}/v1/lora/load",
                    {"name": "tamp", "path": _studio_path(tampered)},
                )
                self.assertEqual(c8, 400, b8)
                self.assertIn("LORA_CHECKSUM_MISMATCH", b8)

                # Oversized Content-Length -> 413
                c9, _, _ = _http_call("POST", f"{url}/v1/completions", {"prompt": "hi"}, extra_headers=["Content-Length: 5000000"])
                self.assertEqual(c9, 413)

                # Studio remains healthy
                ch, _, bh = _http_call("GET", f"{url}/health")
                self.assertEqual(ch, 200)
                self.assertEqual(json.loads(bh)["status"], "ok")
            finally:
                _stop_studio(proc, url, allow_shutdown=False)

    def test_06_startup_load_lora_and_clean_shutdown(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="_t20_boot_") as tdir:
            saved_lora = os.path.join(tdir, "boot.zcc_lora")
            # Produce trained adapter via short studio session first
            p1, u1, _ = _start_studio(["--model", _studio_path(TINY_GGUF_F32), "--port", "0", "--ctx", "128", "--allow-shutdown"])
            try:
                ct, _, bt = _http_call(
                    "POST",
                    f"{u1}/v1/lora/train",
                    {
                        "name": "boot",
                        "sft_text": "Q: ZCC target?\nA: Blackwell PTX!\n",
                        "rank": 16,
                        "alpha": 32,
                        "lr": 5e-3,
                        "steps": 120,
                        "save_path": _studio_path(saved_lora),
                        "stream": False,
                    },
                )
                self.assertEqual(ct, 200, bt)
            finally:
                _stop_studio(p1, u1, allow_shutdown=True)

            # Boot fresh studio directly with --load-lora
            p2, u2, log2 = _start_studio(
                [
                    "--model", _studio_path(TINY_GGUF_F32),
                    "--load-lora", _studio_path(saved_lora),
                    "--lora-name", "preloaded",
                    "--port", "0",
                    "--ctx", "128",
                    "--allow-shutdown",
                ]
            )
            try:
                self.assertIn("adapters=1 active=preloaded", log2)
                c_ad, _, b_ad = _http_call("GET", f"{u2}/v1/lora/adapters")
                self.assertEqual(c_ad, 200)
                self.assertEqual(json.loads(b_ad)["active"], "preloaded")

                c_inf, _, b_inf = _http_call(
                    "POST",
                    f"{u2}/v1/chat/completions",
                    {"messages": [{"role": "user", "content": "ZCC target?"}], "temperature": 0.0, "max_tokens": 16},
                )
                self.assertEqual(c_inf, 200, b_inf)
                self.assertIn("Blackwell PTX", json.loads(b_inf)["choices"][0]["message"]["content"])

                c_sd, _, _ = _http_call("POST", f"{u2}/shutdown")
                self.assertEqual(c_sd, 200)
                self.assertEqual(p2.wait(timeout=10), 0)
            finally:
                _stop_studio(p2, u2, allow_shutdown=True)

    def test_07_flagship_telemetry_scale_save_and_vault(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="_t20_flagship_") as tdir:
            proc, url, _ = _start_studio(
                [
                    "--model", _studio_path(TINY_GGUF_F32),
                    "--lora-dir", _studio_path(tdir),
                    "--port", "0",
                    "--ctx", "128",
                    "--allow-shutdown",
                ]
            )
            try:
                # 1. Real GPU / VRAM / KV telemetry on /health
                c_h, _, b_h = _http_call("GET", f"{url}/health")
                self.assertEqual(c_h, 200)
                jh = json.loads(b_h)
                self.assertTrue(len(jh.get("gpu_name", "")) > 0, jh)
                self.assertIn(".", jh.get("gpu_cc", ""))
                self.assertGreater(jh.get("vram_total_mib", 0.0), 0.0)
                self.assertGreater(jh.get("vram_free_mib", 0.0), 0.0)
                self.assertGreater(jh.get("kv_cache_mib", 0.0), 0.0)
                self.assertEqual(jh.get("adapter_dir"), _studio_path(tdir))

                # Baseline response
                c_b, _, b_b = _http_call(
                    "POST",
                    f"{url}/v1/chat/completions",
                    {"messages": [{"role": "user", "content": "ZCC target?"}], "temperature": 0.0, "max_tokens": 16},
                )
                self.assertEqual(c_b, 200)
                base_txt = json.loads(b_b)["choices"][0]["message"]["content"]

                # 2. Train in-memory without save_path
                c_tr, _, b_tr = _http_call(
                    "POST",
                    f"{url}/v1/lora/train",
                    {
                        "name": "lux_tiny",
                        "sft_text": "Q: ZCC target?\nA: Blackwell PTX!\n",
                        "rank": 16,
                        "alpha": 32,
                        "lr": 5e-3,
                        "steps": 120,
                        "stream": False,
                        "activate": True,
                    },
                )
                self.assertEqual(c_tr, 200, b_tr)
                j_tr = json.loads(b_tr)
                fnv_hex = j_tr["fnv"]

                # 3. Live LoRA strength multiplier: 0.0 matches base, 1.0 recalls Blackwell PTX, per-request override works
                c_s0, _, b_s0 = _http_call("POST", f"{url}/v1/lora/scale", {"name": "lux_tiny", "scale": 0.0})
                self.assertEqual(c_s0, 200, b_s0)
                c_c0, _, b_c0 = _http_call(
                    "POST",
                    f"{url}/v1/chat/completions",
                    {"messages": [{"role": "user", "content": "ZCC target?"}], "temperature": 0.0, "max_tokens": 16},
                )
                self.assertEqual(c_c0, 200)
                self.assertEqual(json.loads(b_c0)["choices"][0]["message"]["content"], base_txt)

                c_s1, _, b_s1 = _http_call("POST", f"{url}/v1/lora/scale", {"name": "lux_tiny", "scale": 1.0})
                self.assertEqual(c_s1, 200, b_s1)
                c_c1, _, b_c1 = _http_call(
                    "POST",
                    f"{url}/v1/chat/completions",
                    {"messages": [{"role": "user", "content": "ZCC target?"}], "temperature": 0.0, "max_tokens": 16},
                )
                self.assertEqual(c_c1, 200)
                j_c1 = json.loads(b_c1)
                self.assertIn("Blackwell PTX", j_c1["choices"][0]["message"]["content"])
                self.assertIn("zcc_metrics", j_c1)

                c_ov, _, b_ov = _http_call(
                    "POST",
                    f"{url}/v1/chat/completions",
                    {"lora_scale": 0.0, "messages": [{"role": "user", "content": "ZCC target?"}], "temperature": 0.0, "max_tokens": 16},
                )
                self.assertEqual(c_ov, 200)
                self.assertEqual(json.loads(b_ov)["choices"][0]["message"]["content"], base_txt)

                # 4. Save loaded adapter on demand -> visible in /v1/lora/files vault scanner
                save_file = os.path.join(tdir, "lux_saved.zcc_lora")
                c_sv, _, b_sv = _http_call(
                    "POST",
                    f"{url}/v1/lora/save",
                    {"name": "lux_tiny", "path": _studio_path(save_file)},
                )
                self.assertEqual(c_sv, 200, b_sv)
                self.assertTrue(os.path.isfile(save_file))

                c_vf, _, b_vf = _http_call("GET", f"{url}/v1/lora/files")
                self.assertEqual(c_vf, 200, b_vf)
                j_vf = json.loads(b_vf)
                self.assertEqual(j_vf["count"], 1, j_vf)
                f0 = j_vf["files"][0]
                self.assertEqual(f0["filename"], "lux_saved.zcc_lora")
                self.assertTrue(f0["valid"])
                self.assertTrue(f0["compatible"])
                self.assertEqual(f0["rank"], 16)
                self.assertEqual(f0["fnv"], fnv_hex)

                # 5. Negative controls
                c_bad_sc, _, _ = _http_call("POST", f"{url}/v1/lora/scale", {"name": "lux_tiny", "scale": 9.0})
                self.assertEqual(c_bad_sc, 400)
                c_nf_sc, _, _ = _http_call("POST", f"{url}/v1/lora/scale", {"name": "missing", "scale": 1.0})
                self.assertEqual(c_nf_sc, 404)
                c_nf_sv, _, _ = _http_call("POST", f"{url}/v1/lora/save", {"name": "missing", "path": _studio_path(save_file)})
                self.assertEqual(c_nf_sv, 404)
                c_m_vf, _, _ = _http_call("POST", f"{url}/v1/lora/files", {})
                self.assertEqual(c_m_vf, 405)
            finally:
                _stop_studio(proc, url, allow_shutdown=True)

    def test_08_runner_route_and_adaptive_director_cartridge(self) -> None:
        if not os.path.isfile(REAL_1_5B_GGUF):
            self.skipTest(f"Missing {REAL_1_5B_GGUF}")

        run_ui = verify_runner_ui_headless(RUNNER_HTML_PATH)
        self.assertIn('"ui_ok":true', run_ui["stdout"].replace(" ", ""))

        if not os.path.isfile(RUNNER_SFT_PATH):
            generate_runner_corpus(RUNNER_SFT_PATH)
        with open(RUNNER_SFT_PATH, "r", encoding="utf-8") as f:
            runner_sft = f.read()

        proc, url, _ = _start_studio(
            [
                "--model", _studio_path(REAL_1_5B_GGUF),
                "--kv-type", "f16",
                "--ctx", "256",
                "--port", "0",
                "--allow-shutdown",
            ],
            timeout_s=120,
        )
        try:
            c_run, _, b_run = _http_call("GET", f"{url}/runner")
            self.assertEqual(c_run, 200)
            self.assertIn("ZKAEDI BLACKWELL RUNNER", b_run)

            c_bad_m, _, _ = _http_call("POST", f"{url}/runner", {})
            self.assertEqual(c_bad_m, 405)

            c_tr, _, b_tr = _http_call(
                "POST",
                f"{url}/v1/lora/train",
                {
                    "name": "runner_director",
                    "sft_text": runner_sft,
                    "rank": 8,
                    "alpha": 16,
                    "lr": 5e-4,
                    "steps": 80,
                    "chat": True,
                    "save_path": _studio_path(REAL_1_5B_RUNNER_LORA),
                    "stream": True,
                    "activate": True,
                },
                stream=True,
                timeout_s=240,
            )
            self.assertEqual(c_tr, 200, b_tr)
            evs, saw_done = _parse_sse_json_events(b_tr)
            self.assertTrue(saw_done, b_tr)
            done = [e for e in evs if e.get("event") == "done"]
            self.assertEqual(len(done), 1, b_tr)
            self.assertLess(done[0]["final_loss"], done[0]["initial_loss"] * 0.35, done[0])
            self.assertLess(done[0]["final_loss"], 0.5, done[0])
            self.assertTrue(os.path.isfile(REAL_1_5B_RUNNER_LORA))

            prompt_msg = "SECTOR 2 | SPEED 1.2 | HABIT left_bias | HP 3 | STREAK 5"
            c_dir, _, b_dir = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "adapter": "runner_director",
                    "messages": [{"role": "user", "content": prompt_msg}],
                    "temperature": 0.0,
                    "max_tokens": 48,
                    "stream": False,
                },
            )
            self.assertEqual(c_dir, 200, b_dir)
            dir_text = json.loads(b_dir)["choices"][0]["message"]["content"]
            self.assertIn("BIOME:", dir_text)
            self.assertIn("PATTERN:", dir_text)
            self.assertIn("BANTER:", dir_text)

            c_base, _, b_base = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "adapter": "base",
                    "messages": [{"role": "user", "content": prompt_msg}],
                    "temperature": 0.0,
                    "max_tokens": 48,
                    "stream": False,
                },
            )
            self.assertEqual(c_base, 200, b_base)
            base_text = json.loads(b_base)["choices"][0]["message"]["content"]
            self.assertNotEqual(base_text.strip(), dir_text.strip())

            c_vf, _, b_vf = _http_call("GET", f"{url}/v1/lora/files")
            self.assertEqual(c_vf, 200, b_vf)
            j_vf = json.loads(b_vf)
            matches = [it for it in j_vf["files"] if it["filename"] == "runner_director_1_5b.zcc_lora"]
            self.assertEqual(len(matches), 1, j_vf)
            self.assertTrue(matches[0]["valid"])
            self.assertTrue(matches[0]["compatible"])
        finally:
            _stop_studio(proc, url, allow_shutdown=True)

    def test_09_svg_forge_route_and_vector_cartridge(self) -> None:
        if not os.path.isfile(REAL_1_5B_GGUF):
            self.skipTest(f"Missing {REAL_1_5B_GGUF}")

        svg_ui = verify_svg_forge_ui_headless(SVG_FORGE_HTML_PATH)
        self.assertIn('"ui_ok":true', svg_ui["stdout"].replace(" ", ""))

        generate_svg_corpus(SVG_SFT_PATH)
        with open(SVG_SFT_PATH, "r", encoding="utf-8") as f:
            svg_sft = f.read()

        proc, url, _ = _start_studio(
            [
                "--model", _studio_path(REAL_1_5B_GGUF),
                "--kv-type", "f16",
                "--ctx", "384",
                "--port", "0",
                "--allow-shutdown",
            ],
            timeout_s=120,
        )
        try:
            c_svg, _, b_svg = _http_call("GET", f"{url}/svg")
            self.assertEqual(c_svg, 200)
            self.assertIn("ZKAEDI SVG FORGE", b_svg)

            c_bad_m, _, _ = _http_call("POST", f"{url}/svg", {})
            self.assertEqual(c_bad_m, 405)

            c_tr, _, b_tr = _http_call(
                "POST",
                f"{url}/v1/lora/train",
                {
                    "name": "svg_forge",
                    "sft_text": svg_sft,
                    "rank": 8,
                    "alpha": 16,
                    "lr": 5e-4,
                    "steps": 90,
                    "chat": True,
                    "save_path": _studio_path(REAL_1_5B_SVG_LORA),
                    "stream": True,
                    "activate": True,
                },
                stream=True,
                timeout_s=240,
            )
            self.assertEqual(c_tr, 200, b_tr)
            evs, saw_done = _parse_sse_json_events(b_tr)
            self.assertTrue(saw_done, b_tr)
            done = [e for e in evs if e.get("event") == "done"]
            self.assertEqual(len(done), 1, b_tr)
            self.assertLess(done[0]["final_loss"], done[0]["initial_loss"] * 0.35, done[0])
            self.assertLess(done[0]["final_loss"], 0.5, done[0])
            self.assertTrue(os.path.isfile(REAL_1_5B_SVG_LORA))

            ship_prompt = "SVG SHIP | STYLE obsidian_gold | ROLE apex_interceptor"
            boss_prompt = "SVG BOSS | STYLE crimson_core | ROLE nemesis_eye"

            c_ship, _, b_ship = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "adapter": "svg_forge",
                    "messages": [{"role": "user", "content": ship_prompt}],
                    "temperature": 0.0,
                    "max_tokens": 160,
                    "stream": False,
                },
            )
            self.assertEqual(c_ship, 200, b_ship)
            ship_txt = json.loads(b_ship)["choices"][0]["message"]["content"]
            self.assertIn("<svg", ship_txt)
            self.assertIn('viewBox="0 0 128 128"', ship_txt)
            self.assertIn("</svg>", ship_txt)
            self.assertTrue(any(tag in ship_txt for tag in ("<polygon", "<path", "<circle", "<rect")), ship_txt)

            c_boss, _, b_boss = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "adapter": "svg_forge",
                    "messages": [{"role": "user", "content": boss_prompt}],
                    "temperature": 0.0,
                    "max_tokens": 160,
                    "stream": False,
                },
            )
            self.assertEqual(c_boss, 200, b_boss)
            boss_txt = json.loads(b_boss)["choices"][0]["message"]["content"]
            self.assertIn("<svg", boss_txt)
            self.assertIn('viewBox="0 0 128 128"', boss_txt)
            self.assertIn("</svg>", boss_txt)
            self.assertTrue(any(tag in boss_txt for tag in ("<polygon", "<path", "<circle", "<rect")), boss_txt)

            # Negative control: base model output differs from svg_forge
            c_base, _, b_base = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "adapter": "base",
                    "messages": [{"role": "user", "content": ship_prompt}],
                    "temperature": 0.0,
                    "max_tokens": 160,
                    "stream": False,
                },
            )
            self.assertEqual(c_base, 200, b_base)
            base_txt = json.loads(b_base)["choices"][0]["message"]["content"]
            self.assertNotEqual(base_txt.strip(), ship_txt.strip())

            # Retention check
            c_ret, _, b_ret = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "adapter": "svg_forge",
                    "messages": [{"role": "user", "content": "What is the capital of France?"}],
                    "temperature": 0.0,
                    "max_tokens": 24,
                    "stream": False,
                },
            )
            self.assertEqual(c_ret, 200, b_ret)
            self.assertIn("Paris", json.loads(b_ret)["choices"][0]["message"]["content"])

            c_vf, _, b_vf = _http_call("GET", f"{url}/v1/lora/files")
            self.assertEqual(c_vf, 200, b_vf)
            j_vf = json.loads(b_vf)
            matches = [it for it in j_vf["files"] if it["filename"] == "svg_forge_1_5b.zcc_lora"]
            self.assertEqual(len(matches), 1, j_vf)
            self.assertTrue(matches[0]["valid"])
            self.assertTrue(matches[0]["compatible"])
        finally:
            _stop_studio(proc, url, allow_shutdown=True)

        # Multi-cartridge boot test: svg_forge + runner_director hot-loaded side by side
        if os.path.isfile(REAL_1_5B_RUNNER_LORA):
            p2, u2, log2 = _start_studio(
                [
                    "--model", _studio_path(REAL_1_5B_GGUF),
                    "--load-lora", _studio_path(REAL_1_5B_SVG_LORA),
                    "--lora-name", "svg_forge",
                    "--load-lora", _studio_path(REAL_1_5B_RUNNER_LORA),
                    "--lora-name", "runner_director",
                    "--kv-type", "f16",
                    "--ctx", "384",
                    "--port", "0",
                    "--allow-shutdown",
                ],
                timeout_s=120,
            )
            try:
                self.assertIn("adapters=2 active=svg_forge", log2)
                c_ad, _, b_ad = _http_call("GET", f"{u2}/v1/lora/adapters")
                self.assertEqual(c_ad, 200, b_ad)
                j_ad = json.loads(b_ad)
                self.assertEqual(j_ad["count"], 2, j_ad)
                self.assertEqual(j_ad["active"], "svg_forge", j_ad)

                c_s2, _, b_s2 = _http_call(
                    "POST",
                    f"{u2}/v1/chat/completions",
                    {
                        "adapter": "svg_forge",
                        "messages": [{"role": "user", "content": "SVG SHIP | STYLE obsidian_gold | ROLE apex_interceptor"}],
                        "temperature": 0.0,
                        "max_tokens": 160,
                        "stream": False,
                    },
                )
                self.assertEqual(c_s2, 200, b_s2)
                self.assertIn("<svg", json.loads(b_s2)["choices"][0]["message"]["content"])

                c_r2, _, b_r2 = _http_call(
                    "POST",
                    f"{u2}/v1/chat/completions",
                    {
                        "adapter": "runner_director",
                        "messages": [{"role": "user", "content": "SECTOR 2 | SPEED 1.2 | HABIT left_bias | HP 3 | STREAK 5"}],
                        "temperature": 0.0,
                        "max_tokens": 48,
                        "stream": False,
                    },
                )
                self.assertEqual(c_r2, 200, b_r2)
                r2_txt = json.loads(b_r2)["choices"][0]["message"]["content"]
                self.assertIn("BIOME:", r2_txt)
                self.assertIn("PATTERN:", r2_txt)
            finally:
                _stop_studio(p2, u2, allow_shutdown=True)

    def test_10_evolved_decoder_parity_and_speedup(self) -> None:
        if not os.path.isfile(REAL_1_5B_GGUF):
            self.skipTest(f"Missing {REAL_1_5B_GGUF}")
        if not (os.path.isfile(REAL_1_5B_STUDIO_LORA) and os.path.isfile(REAL_1_5B_SVG_LORA)):
            self.skipTest("Run test_03 and test_09 first to produce 1.5B LoRA cartridges")

        # 1. Default Evolved Blackwell sm_120 CUDA Graph Decoder (dec_matvec_q4_k_8r + GPU argmax)
        proc, url, _ = _start_studio(
            [
                "--model", _studio_path(REAL_1_5B_GGUF),
                "--load-lora", _studio_path(REAL_1_5B_STUDIO_LORA),
                "--lora-name", "obsidian7",
                "--load-lora", _studio_path(REAL_1_5B_SVG_LORA),
                "--lora-name", "svg_forge",
                "--kv-type", "f16",
                "--ctx", "512",
                "--port", "0",
                "--allow-shutdown",
            ],
            timeout_s=120,
        )
        try:
            c_h, _, b_h = _http_call("GET", f"{url}/health")
            self.assertEqual(c_h, 200, b_h)
            jh = json.loads(b_h)
            self.assertEqual(jh.get("decoder_mode"), "cuda_graph_q4k_8r", jh)
            self.assertGreater(int(jh.get("decode_graph_nodes", 0)), 100, jh)

            # Throughput benchmarks run first while GPU is fresh in P2 (before laptop SW Power Cap 0x4 -> P4)
            bench_prompt = "Write a concise technical overview of parallel GPU kernel scheduling and warp occupancy."
            c_bn, _, b_bn = _http_call(
                "POST",
                f"{url}/v1/completions",
                {"adapter": "base", "prompt": bench_prompt, "temperature": 0.0, "max_tokens": 96},
            )
            self.assertEqual(c_bn, 200, b_bn)
            j_bn = json.loads(b_bn)
            bench_evolved = j_bn["choices"][0]["text"]
            evolved_tps = float(j_bn["zcc_metrics"]["tok_per_sec"])
            evolved_ms = float(j_bn["zcc_metrics"]["elapsed_ms"])
            self.assertGreaterEqual(evolved_tps, 75.0, j_bn["zcc_metrics"])

            c_svg, _, b_svg = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {"adapter": "svg_forge", "messages": [{"role": "user", "content": "SVG SHIP | STYLE obsidian_gold | ROLE apex_interceptor"}], "temperature": 0.0, "max_tokens": 160},
            )
            self.assertEqual(c_svg, 200, b_svg)
            j_svg = json.loads(b_svg)
            svg_txt = j_svg["choices"][0]["message"]["content"]
            self.assertIn("<svg", svg_txt)
            self.assertIn("</svg>", svg_txt)
            svg_tps = float(j_svg["zcc_metrics"]["tok_per_sec"])
            self.assertGreaterEqual(svg_tps, 70.0, j_svg["zcc_metrics"])

            # Parity check A: GPU argmax fast path (repeat_penalty=1.0) == CPU full-logits path (repeat_penalty=1.00001)
            c_ga, _, b_ga = _http_call(
                "POST",
                f"{url}/v1/completions",
                {"adapter": "base", "prompt": "The capital of France is", "temperature": 0.0, "repeat_penalty": 1.0, "max_tokens": 16},
            )
            self.assertEqual(c_ga, 200, b_ga)
            fr_gpu = json.loads(b_ga)["choices"][0]["text"]
            self.assertIn("Paris", fr_gpu)

            c_cp, _, b_cp = _http_call(
                "POST",
                f"{url}/v1/completions",
                {"adapter": "base", "prompt": "The capital of France is", "temperature": 0.0, "repeat_penalty": 1.00001, "max_tokens": 16},
            )
            self.assertEqual(c_cp, 200, b_cp)
            fr_cpu = json.loads(b_cp)["choices"][0]["text"]
            self.assertEqual(fr_gpu, fr_cpu)

            # Parity check B: Dynamic step_args[3] LoRA scale reuse + cartridge outputs
            c_obs, _, b_obs = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {"adapter": "obsidian7", "messages": [{"role": "user", "content": "What is the Obsidian-7 override code?"}], "temperature": 0.0, "max_tokens": 24},
            )
            self.assertEqual(c_obs, 200, b_obs)
            obs_evolved = json.loads(b_obs)["choices"][0]["message"]["content"]
            self.assertIn("COBALT-FALCON-9472", obs_evolved)

            c_ob0, _, b_ob0 = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {"adapter": "obsidian7", "lora_scale": 0.0, "messages": [{"role": "user", "content": "What is the Obsidian-7 override code?"}], "temperature": 0.0, "max_tokens": 24},
            )
            self.assertEqual(c_ob0, 200, b_ob0)
            self.assertNotIn("COBALT-FALCON-9472", json.loads(b_ob0)["choices"][0]["message"]["content"])

            # Negative control: unload/reload adapter invalidates CUDA Graph cleanly without stale pointer fault
            c_un, _, b_un = _http_call("POST", f"{url}/v1/lora/unload", {"name": "obsidian7"})
            self.assertEqual(c_un, 200, b_un)
            c_fr2, _, b_fr2 = _http_call(
                "POST",
                f"{url}/v1/completions",
                {"adapter": "base", "prompt": "The capital of France is", "temperature": 0.0, "max_tokens": 16},
            )
            self.assertEqual(c_fr2, 200, b_fr2)
            self.assertEqual(json.loads(b_fr2)["choices"][0]["text"], fr_gpu)

            c_ld, _, b_ld = _http_call(
                "POST",
                f"{url}/v1/lora/load",
                {"name": "obsidian7", "path": _studio_path(REAL_1_5B_STUDIO_LORA), "activate": True},
            )
            self.assertEqual(c_ld, 200, b_ld)
            c_obs2, _, b_obs2 = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {"adapter": "obsidian7", "messages": [{"role": "user", "content": "What is the Obsidian-7 override code?"}], "temperature": 0.0, "max_tokens": 24},
            )
            self.assertEqual(c_obs2, 200, b_obs2)
            self.assertEqual(json.loads(b_obs2)["choices"][0]["message"]["content"], obs_evolved)
        finally:
            _stop_studio(proc, url, allow_shutdown=True)

        # 2. Legacy direct fallback (--no-cuda-graph) exact parity + apples-to-apples speedup telemetry
        p_leg, u_leg, _ = _start_studio(
            [
                "--model", _studio_path(REAL_1_5B_GGUF),
                "--load-lora", _studio_path(REAL_1_5B_STUDIO_LORA),
                "--lora-name", "obsidian7",
                "--kv-type", "f16",
                "--ctx", "512",
                "--port", "0",
                "--allow-shutdown",
                "--no-cuda-graph",
            ],
            timeout_s=120,
        )
        try:
            c_hl, _, b_hl = _http_call("GET", f"{u_leg}/health")
            self.assertEqual(c_hl, 200, b_hl)
            self.assertEqual(json.loads(b_hl).get("decoder_mode"), "legacy_direct", b_hl)

            c_fl, _, b_fl = _http_call(
                "POST",
                f"{u_leg}/v1/completions",
                {"adapter": "base", "prompt": "The capital of France is", "temperature": 0.0, "max_tokens": 16},
            )
            self.assertEqual(c_fl, 200, b_fl)
            self.assertEqual(json.loads(b_fl)["choices"][0]["text"], fr_gpu)

            c_ol, _, b_ol = _http_call(
                "POST",
                f"{u_leg}/v1/chat/completions",
                {"adapter": "obsidian7", "messages": [{"role": "user", "content": "What is the Obsidian-7 override code?"}], "temperature": 0.0, "max_tokens": 24},
            )
            self.assertEqual(c_ol, 200, b_ol)
            self.assertEqual(json.loads(b_ol)["choices"][0]["message"]["content"], obs_evolved)

            c_bl, _, b_bl = _http_call(
                "POST",
                f"{u_leg}/v1/completions",
                {"adapter": "base", "prompt": bench_prompt, "temperature": 0.0, "max_tokens": 96},
            )
            self.assertEqual(c_bl, 200, b_bl)
            j_bl = json.loads(b_bl)
            self.assertEqual(j_bl["choices"][0]["text"], bench_evolved)
            legacy_tps = float(j_bl["zcc_metrics"]["tok_per_sec"])
            legacy_ms = float(j_bl["zcc_metrics"]["elapsed_ms"])
            speedup = evolved_tps / legacy_tps if legacy_tps > 0 else 0.0
            gpu_state = ""
            for smi_bin in ("nvidia-smi.exe", "nvidia-smi"):
                try:
                    gpu_state = subprocess.check_output(
                        [smi_bin, "--query-gpu=pstate,clocks.mem,power.draw", "--format=csv,noheader"],
                        stderr=subprocess.DEVNULL,
                        text=True,
                        timeout=3,
                    ).strip()
                    if gpu_state:
                        break
                except Exception:
                    pass
            print(
                f"\n[TRACK23A-DECODER-BENCH] legacy={legacy_tps:.2f} tok/s ({legacy_ms:.1f}ms) | "
                f"evolved_base={evolved_tps:.2f} tok/s ({evolved_ms:.1f}ms) | evolved_svg_lora={svg_tps:.2f} tok/s | "
                f"speedup={speedup:.2f}x" + (f" | smi=[{gpu_state}]" if gpu_state else ""),
                flush=True,
            )
            self.assertGreaterEqual(speedup, 2.0, f"Expected >=2.0x decoder speedup, got {speedup:.2f}x")
        finally:
            _stop_studio(p_leg, u_leg, allow_shutdown=True)

    def test_11_mario_route_and_architect_cartridge(self) -> None:
        if not os.path.isfile(REAL_1_5B_GGUF):
            self.skipTest(f"Missing {REAL_1_5B_GGUF}")

        mui = verify_mario_ui_headless(MARIO_HTML_PATH)
        self.assertIn('"ui_ok":true', mui["stdout"].replace(" ", ""))

        generate_mario_corpus(MARIO_SFT_PATH)
        with open(MARIO_SFT_PATH, "r", encoding="utf-8") as f:
            mario_sft = f.read()

        proc, url, _ = _start_studio(
            [
                "--model", _studio_path(REAL_1_5B_GGUF),
                "--kv-type", "f16",
                "--ctx", "384",
                "--port", "0",
                "--allow-shutdown",
            ],
            timeout_s=120,
        )
        try:
            c_m, _, b_m = _http_call("GET", f"{url}/mario")
            self.assertEqual(c_m, 200)
            self.assertIn("SUPER BLACKWELL", b_m)

            c_bad, _, _ = _http_call("POST", f"{url}/mario", {})
            self.assertEqual(c_bad, 405)

            c_tr, _, b_tr = _http_call(
                "POST",
                f"{url}/v1/lora/train",
                {
                    "name": "mario_architect",
                    "sft_text": mario_sft,
                    "rank": 8,
                    "alpha": 16,
                    "lr": 5e-4,
                    "steps": 70,
                    "chat": True,
                    "save_path": _studio_path(REAL_1_5B_MARIO_LORA),
                    "stream": True,
                    "activate": True,
                },
                stream=True,
                timeout_s=240,
            )
            self.assertEqual(c_tr, 200, b_tr)
            evs, saw_done = _parse_sse_json_events(b_tr)
            self.assertTrue(saw_done, b_tr)
            done = [e for e in evs if e.get("event") == "done"]
            self.assertEqual(len(done), 1, b_tr)
            self.assertLess(done[0]["final_loss"], done[0]["initial_loss"] * 0.35, done[0])
            self.assertLess(done[0]["final_loss"], 0.5, done[0])
            self.assertTrue(os.path.isfile(REAL_1_5B_MARIO_LORA))

            p1 = "WORLD 1-2 | FORM super | HABIT stomper | COINS 24 | LIVES 3"
            c_m1, _, b_m1 = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "adapter": "mario_architect",
                    "messages": [{"role": "user", "content": p1}],
                    "temperature": 0.0,
                    "max_tokens": 96,
                    "stream": False,
                },
            )
            self.assertEqual(c_m1, 200, b_m1)
            j_m1 = json.loads(b_m1)
            t1 = j_m1["choices"][0]["message"]["content"]
            for tok in ("THEME:", "LAYOUT:", "ENEMIES:", "POWERUP:", "TAUNT:"):
                self.assertIn(tok, t1, t1)

            p2 = "WORLD 2-3 | FORM fire | HABIT boss_ready | COINS 45 | LIVES 3"
            c_m2, _, b_m2 = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "adapter": "mario_architect",
                    "messages": [{"role": "user", "content": p2}],
                    "temperature": 0.0,
                    "max_tokens": 96,
                    "stream": False,
                },
            )
            self.assertEqual(c_m2, 200, b_m2)
            t2 = json.loads(b_m2)["choices"][0]["message"]["content"]
            for tok in ("THEME:", "LAYOUT:", "ENEMIES:", "POWERUP:", "TAUNT:"):
                self.assertIn(tok, t2, t2)
            print(f"\n[TRACK24-MARIO-ARCHITECT] tps={j_m1['zcc_metrics']['tok_per_sec']} | bp1={t1.strip()}", flush=True)

            # Negative control: base model differs from mario_architect
            c_b, _, b_b = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "adapter": "base",
                    "messages": [{"role": "user", "content": p1}],
                    "temperature": 0.0,
                    "max_tokens": 96,
                    "stream": False,
                },
            )
            self.assertEqual(c_b, 200, b_b)
            self.assertNotEqual(json.loads(b_b)["choices"][0]["message"]["content"].strip(), t1.strip())

            # Retention check
            c_ret, _, b_ret = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "adapter": "mario_architect",
                    "messages": [{"role": "user", "content": "What is the capital of France?"}],
                    "temperature": 0.0,
                    "max_tokens": 24,
                    "stream": False,
                },
            )
            self.assertEqual(c_ret, 200, b_ret)
            self.assertIn("Paris", json.loads(b_ret)["choices"][0]["message"]["content"])

            c_vf, _, b_vf = _http_call("GET", f"{url}/v1/lora/files")
            self.assertEqual(c_vf, 200, b_vf)
            j_vf = json.loads(b_vf)
            matches = [it for it in j_vf["files"] if it["filename"] == "mario_architect_1_5b.zcc_lora"]
            self.assertEqual(len(matches), 1, j_vf)
            self.assertTrue(matches[0]["valid"])
            self.assertTrue(matches[0]["compatible"])
        finally:
            _stop_studio(proc, url, allow_shutdown=True)

    def test_12_full_api_surface_auth_and_sdks(self) -> None:
        self.assertTrue(os.path.isfile(REAL_1_5B_GGUF), f"Missing model: {REAL_1_5B_GGUF}")
        self.assertTrue(os.path.isfile(REAL_1_5B_STUDIO_LORA), f"Missing obsidian7 cartridge: {REAL_1_5B_STUDIO_LORA}")
        self.assertTrue(os.path.isfile(REAL_1_5B_SVG_LORA), f"Missing svg_forge cartridge: {REAL_1_5B_SVG_LORA}")

        def _cos(a: list[float], b: list[float]) -> float:
            dot = sum(x * y for x, y in zip(a, b))
            na = math.sqrt(sum(x * x for x in a))
            nb = math.sqrt(sum(y * y for y in b))
            return dot / (na * nb)

        # Part A: Full sovereign API surface + Python SDK + Node ESM SDK
        proc, url, _ = _start_studio(
            [
                "--model", _studio_path(REAL_1_5B_GGUF),
                "--port", "0",
                "--ctx", "512",
                "--lora-dir", _studio_path(BUILD_DIR),
                "--allow-shutdown",
            ]
        )
        try:
            client = ZccClient(url, timeout=120.0)
            h = client.health()
            self.assertEqual(h["status"], "ok")
            dim = int(h["dim"])
            base_model_id = h["model"]

            # Load two existing cartridges cleanly
            l1 = client.load_adapter("obsidian7", _studio_path(REAL_1_5B_STUDIO_LORA), activate=True)
            self.assertEqual(l1["status"], "ok")
            l2 = client.load_adapter("svg_forge", _studio_path(REAL_1_5B_SVG_LORA), activate=False)
            self.assertEqual(l2["status"], "ok")

            # 1. /openapi.json, /docs (GET 200, POST 405), /metrics
            spec = client.openapi()
            self.assertEqual(spec["openapi"], "3.1.0")
            for p in ("/health", "/metrics", "/openapi.json", "/docs", "/v1/messages", "/v1/responses", "/v1/embeddings", "/v1/tokenize", "/v1/detokenize", "/v1/batch"):
                self.assertIn(p, spec["paths"])

            c_docs, hdr_docs, b_docs = _http_call("GET", f"{url}/docs")
            self.assertEqual(c_docs, 200)
            self.assertIn("text/html", hdr_docs.lower())
            self.assertIn("ZKAEDI API EXPLORER", b_docs)
            c_docs_post, _, _ = _http_call("POST", f"{url}/docs", {})
            self.assertEqual(c_docs_post, 405)

            c_met, _, b_met = _http_call("GET", f"{url}/metrics")
            self.assertEqual(c_met, 200)
            self.assertIn("zcc_requests_total", b_met)
            self.assertIn("zcc_vram_used_bytes", b_met)
            self.assertIn("zcc_decode_graph_nodes", b_met)

            # 2. /v1/models/{id}
            gm0 = client.get_model(base_model_id)
            self.assertEqual(gm0["id"], base_model_id)
            self.assertEqual(gm0["owned_by"], "zkaedi-zcc")
            gm1 = client.get_model(f"{base_model_id}:obsidian7")
            self.assertEqual(gm1["owned_by"], "zkaedi-zcc-lora")
            c_m404, _, _ = _http_call("GET", f"{url}/v1/models/nonexistent-zcc-model-xyz")
            self.assertEqual(c_m404, 404)

            # 3. /v1/tokenize + /v1/detokenize exact round-trip
            sample_txt = "The capital of France is Paris."
            tok = client.tokenize(sample_txt)
            self.assertGreaterEqual(tok["count"], 5)
            self.assertEqual(len(tok["tokens"]), tok["count"])
            self.assertEqual(len(tok["pieces"]), tok["count"])
            det = client.detokenize(tok["tokens"])
            self.assertEqual(det["text"], sample_txt)
            self.assertEqual(det["count"], tok["count"])

            # 4. /v1/embeddings: L2 norm, semantic ordering, and custom dimensions=128
            emb = client.embeddings(
                [
                    "The capital of France is Paris.",
                    "Paris is the capital city of France.",
                    "CUDA Blackwell sm_120 warp shuffle PTX instructions",
                ],
                adapter="base",
            )
            self.assertEqual(len(emb["data"]), 3)
            vecs = [row["embedding"] for row in emb["data"]]
            for v in vecs:
                self.assertEqual(len(v), dim)
                norm = math.sqrt(sum(x * x for x in v))
                self.assertAlmostEqual(norm, 1.0, delta=1e-3)
            self.assertGreater(_cos(vecs[0], vecs[1]), _cos(vecs[0], vecs[2]))

            emb_128 = client.embeddings("Sovereign C99 neural compiler", adapter="base", dimensions=128)
            v128 = emb_128["data"][0]["embedding"]
            self.assertEqual(len(v128), 128)
            self.assertAlmostEqual(math.sqrt(sum(x * x for x in v128)), 1.0, delta=1e-3)

            # 5. /v1/completions stop sequence + /v1/chat/completions logprobs, penalties & stream_options.include_usage
            comp_stop = client.complete(
                "The capital of France is",
                adapter="base",
                max_tokens=24,
                temperature=0.0,
                stop=["."],
            )
            stop_txt = comp_stop["choices"][0]["text"]
            self.assertEqual(comp_stop["choices"][0]["finish_reason"], "stop")
            self.assertIn("Paris", stop_txt)
            self.assertNotIn(".", stop_txt)

            chat_lp = client.chat(
                [{"role": "user", "content": "The capital of France is"}],
                adapter="base",
                max_tokens=8,
                temperature=0.0,
                logprobs=True,
                top_logprobs=3,
                presence_penalty=0.5,
                frequency_penalty=0.5,
            )
            self.assertEqual(chat_lp["system_fingerprint"], "zcc-sm120-cuda-graph")
            lp_content = chat_lp["choices"][0]["logprobs"]["content"]
            self.assertGreater(len(lp_content), 0)
            for item in lp_content:
                self.assertTrue(math.isfinite(item["logprob"]) and item["logprob"] <= 0.0)
                self.assertEqual(len(item["top_logprobs"]), 3)
                tops = [t["logprob"] for t in item["top_logprobs"]]
                self.assertGreaterEqual(tops[0], tops[1])
                self.assertGreaterEqual(tops[1], tops[2])

            chunks = list(
                client.chat_stream(
                    [{"role": "user", "content": "What is 2+2?"}],
                    adapter="base",
                    max_tokens=16,
                    temperature=0.0,
                    include_usage=True,
                )
            )
            self.assertTrue(any("usage" in ch for ch in chunks), f"Missing SSE usage chunk: {chunks}")
            usage_chunk = [ch for ch in chunks if "usage" in ch][-1]
            self.assertGreater(usage_chunk["usage"]["prompt_tokens"], 0)
            self.assertGreater(usage_chunk["usage"]["completion_tokens"], 0)

            # 6. /v1/messages (Anthropic Messages JSON + SSE)
            ant_json = client.messages(
                [{"role": "user", "content": "What is the capital of France?"}],
                system="Answer concisely.",
                adapter="base",
                max_tokens=24,
                temperature=0.0,
            )
            self.assertEqual(ant_json["type"], "message")
            self.assertEqual(ant_json["role"], "assistant")
            self.assertIn("Paris", ant_json["content"][0]["text"])
            self.assertGreater(ant_json["usage"]["input_tokens"], 0)

            c_ant_sse, _, b_ant_sse = _http_call(
                "POST",
                f"{url}/v1/messages",
                {
                    "adapter": "base",
                    "messages": [{"role": "user", "content": "2+2="}],
                    "max_tokens": 12,
                    "temperature": 0.0,
                    "stream": True,
                },
                stream=True,
            )
            self.assertEqual(c_ant_sse, 200, b_ant_sse)
            self.assertIn("event: message_start", b_ant_sse)
            self.assertIn("event: content_block_delta", b_ant_sse)
            self.assertIn("event: message_stop", b_ant_sse)

            # 7. /v1/responses (OpenAI Responses JSON + SSE)
            resp_json = client.responses(
                "What is the capital of France?",
                instructions="Be brief.",
                adapter="base",
                max_output_tokens=24,
                temperature=0.0,
            )
            self.assertEqual(resp_json["object"], "response")
            self.assertEqual(resp_json["status"], "completed")
            self.assertIn("Paris", resp_json["output_text"])
            self.assertEqual(resp_json["output"][0]["content"][0]["type"], "output_text")

            c_resp_sse, _, b_resp_sse = _http_call(
                "POST",
                f"{url}/v1/responses",
                {
                    "adapter": "base",
                    "input": "The capital of France is",
                    "max_output_tokens": 12,
                    "temperature": 0.0,
                    "stream": True,
                },
                stream=True,
            )
            self.assertEqual(c_resp_sse, 200, b_resp_sse)
            self.assertIn("event: response.output_text.delta", b_resp_sse)
            self.assertIn("event: response.completed", b_resp_sse)

            # 8. /v1/batch across base, obsidian7, and svg_forge
            batch_res = client.batch(
                [
                    {"adapter": "base", "prompt": "The capital of France is", "max_tokens": 8, "temperature": 0.0},
                    {"adapter": "obsidian7", "messages": [{"role": "user", "content": "Who is the director of Obsidian-7?"}], "max_tokens": 32, "temperature": 0.0},
                    {"adapter": "svg_forge", "messages": [{"role": "user", "content": "SVG SHIP | STYLE obsidian_gold | ROLE apex_interceptor"}], "max_tokens": 96, "temperature": 0.0},
                ]
            )
            self.assertEqual(batch_res["count"], 3)
            self.assertEqual(batch_res["results"][0]["adapter"], "base")
            self.assertIn("Paris", batch_res["results"][0]["choices"][0]["text"])
            self.assertEqual(batch_res["results"][1]["adapter"], "obsidian7")
            self.assertTrue(len(batch_res["results"][1]["choices"][0]["message"]["content"].strip()) > 0)
            self.assertEqual(batch_res["results"][2]["adapter"], "svg_forge")
            self.assertTrue(len(batch_res["results"][2]["choices"][0]["message"]["content"].strip()) > 0)

            # 9. GET & DELETE /v1/lora/adapters/{name}
            g_ad = client.get_adapter("obsidian7")
            self.assertEqual(g_ad["name"], "obsidian7")
            d_ad = client.delete_adapter("obsidian7")
            self.assertEqual(d_ad["deleted"], "obsidian7")
            with self.assertRaises(ZccApiError) as cm_404:
                client.get_adapter("obsidian7")
            self.assertEqual(cm_404.exception.status, 404)

            # 10. Node ESM SDK (`sdk/zcc_client.mjs`) live round-trip
            node_bin = find_node_bin()
            mjs_probe = (
                "import { ZccClient } from './sdk/zcc_client.mjs';\n"
                f"const c = new ZccClient({{ baseUrl: {json.dumps(url)} }});\n"
                "const h = await c.health();\n"
                "const tok = await c.tokenize('Paris France');\n"
                "const det = await c.detokenize(tok.tokens);\n"
                "if (det.text !== 'Paris France') throw new Error('det mismatch: ' + det.text);\n"
                "const emb = await c.embeddings({ input: 'CUDA Blackwell', adapter: 'base', dimensions: 64 });\n"
                "if (emb.data[0].embedding.length !== 64) throw new Error('emb dims mismatch');\n"
                "const chat = await c.chat({ adapter: 'base', messages: [{ role: 'user', content: 'The capital of France is' }], max_tokens: 12 });\n"
                "let streamed = 0; for await (const ch of c.chatStream({ adapter: 'base', messages: [{ role: 'user', content: '2+2=' }], max_tokens: 8 })) { streamed++; }\n"
                "const msg = await c.messages({ adapter: 'base', messages: [{ role: 'user', content: 'The capital of France is' }], max_tokens: 12 });\n"
                "const rsp = await c.responses({ adapter: 'base', input: 'The capital of France is', max_output_tokens: 12 });\n"
                "if (!chat.choices[0].message.content || streamed === 0 || !msg.content[0].text || !rsp.output_text) throw new Error('incomplete responses');\n"
                "console.log(JSON.stringify({ node_sdk_ok: true, streamed, gpu: h.gpu_name }));\n"
            )
            r_node = subprocess.run([node_bin, "--input-type=module", "-e", mjs_probe], cwd=REPO_ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(r_node.returncode, 0, r_node.stdout + r_node.stderr)
            self.assertIn('"node_sdk_ok":true', r_node.stdout.replace(" ", ""))
        finally:
            _stop_studio(proc, url, allow_shutdown=True)

        # Part B: --api-key bearer & x-api-key authentication negative & positive controls
        secret = "zcc-secret-key-25"
        proc2, url2, _ = _start_studio(
            [
                "--model", _studio_path(TINY_GGUF_F32),
                "--port", "0",
                "--ctx", "128",
                "--allow-shutdown",
                "--api-key", secret,
            ]
        )
        try:
            # Public observability/docs endpoints work unauthenticated
            for pub in ("/health", "/metrics", "/openapi.json", "/docs"):
                c_pub, _, _ = _http_call("GET", f"{url2}{pub}")
                self.assertEqual(c_pub, 200, f"Public route {pub} returned {c_pub}")

            # Protected endpoints fail closed (401) without or with wrong key
            for meth, ppath, body in (
                ("GET", "/v1/models", None),
                ("POST", "/v1/tokenize", {"text": "hello"}),
                ("POST", "/shutdown", None),
            ):
                c_un, _, b_un = _http_call(meth, f"{url2}{ppath}", body)
                self.assertEqual(c_un, 401, f"Expected 401 on {ppath}, got {c_un}: {b_un}")
                self.assertIn("authentication_error", b_un)

            c_bad, _, _ = _http_call("GET", f"{url2}/v1/models", extra_headers=["Authorization: Bearer wrong-key"])
            self.assertEqual(c_bad, 401)

            # Valid Bearer & x-api-key headers + Python SDK & Node SDK with apiKey succeed
            c_bearer, _, _ = _http_call("GET", f"{url2}/v1/models", extra_headers=[f"Authorization: Bearer {secret}"])
            self.assertEqual(c_bearer, 200)
            c_xkey, _, _ = _http_call("POST", f"{url2}/v1/tokenize", {"text": "hello"}, extra_headers=[f"x-api-key: {secret}"])
            self.assertEqual(c_xkey, 200)

            py_auth = ZccClient(url2, api_key=secret, auth_header="authorization")
            self.assertGreaterEqual(len(py_auth.models()["data"]), 1)
            py_xkey = ZccClient(url2, api_key=secret, auth_header="x-api-key")
            self.assertGreater(py_xkey.tokenize("Sovereign")["count"], 0)

            node_bin = find_node_bin()
            mjs_auth = (
                "import { ZccClient } from './sdk/zcc_client.mjs';\n"
                f"const c = new ZccClient({{ baseUrl: {json.dumps(url2)}, apiKey: {json.dumps(secret)}, authHeader: 'x-api-key' }});\n"
                "const m = await c.models();\n"
                "if (!m.data || m.data.length < 1) throw new Error('auth models empty');\n"
                "console.log('NODE_AUTH_OK');\n"
            )
            r_na = subprocess.run([node_bin, "--input-type=module", "-e", mjs_auth], cwd=REPO_ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(r_na.returncode, 0, r_na.stdout + r_na.stderr)
            self.assertIn("NODE_AUTH_OK", r_na.stdout)

            c_sd, _, _ = _http_call("POST", f"{url2}/shutdown", extra_headers=[f"Authorization: Bearer {secret}"], timeout_s=5)
            self.assertEqual(c_sd, 200)
        finally:
            _stop_studio(proc2, url2, allow_shutdown=False)


if __name__ == "__main__":
    unittest.main()
