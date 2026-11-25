#!/usr/bin/env python3
"""
Sovereign OpenAI-Compatible Win64 GPU HTTP/SSE Server Gate (Track 18)
====================================================================
Verification suite (ZCC-EF-0 & ZCC-EF-7 Evidence Tier E2):
  1. Server PE32+ structural audit (dual-compiler FC /B + SHA-256, strict System32
     IAT {WS2_32.dll, msvcrt.dll, nvcuda.dll} ONLY) + headless Node.js EF-7 UI gate.
  2. External 941 MB Qwen2.5-1.5B + Tier-B AOT OpenAI server: GET /, GET /health,
     GET /v1/models, POST /v1/chat/completions (non-stream + chunked SSE + multi-turn).
  3. Standalone sealed PE overlay server (zkaedi_prime_1_5b_server.exe) launched from
     an empty temp directory (seal_verified=true + SSE "Paris") + temp 1-bit weight
     tamper negative control (TAMPER_DETECTED before socket bind).
  4. Legacy /v1/completions (stream + non-stream on tiny F16 GGUF) + CORS OPTIONS.
  5. HTTP ZCC-EF-0 negative controls (400 malformed JSON, 400 missing messages,
     404 unknown route, 405 bad method, 413 oversized Content-Length, 404 /shutdown
     without --allow-shutdown -> server remains healthy with 200 /health).
  6. Abrupt TCP client mid-SSE-stream disconnect resilience -> server aborts decode
     cleanly and continues serving subsequent requests.
"""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_gpu_llm_server_win64 import (  # noqa: E402
    SERVER_HOST_OUT,
    SERVER_OUT,
    UI_HTML_PATH,
    verify_server_ui_headless,
)
from tools.build_gpu_win64 import audit_pe32plus_multi_dll  # noqa: E402
from tools.build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    ZCC_BIN,
    ZCC_WIN64,
    run_fc_b,
    sha256_file,
)

TINY_GGUF_F16 = os.path.join(BUILD_DIR, "zcc_tiny_llama_f16.gguf")
TINY_PTX_F16 = os.path.join(BUILD_DIR, "zcc_tiny_llama_f16_sm120.ptx")
REAL_1_5B_GGUF = "/mnt/h/models/zkaedi-prime-dpo-q4_k_m.gguf" if sys.platform != "win32" else r"H:\models\zkaedi-prime-dpo-q4_k_m.gguf"
REAL_SERVER_EXE = os.path.join(BUILD_DIR, "zkaedi_prime_1_5b_server.exe")


def _to_win_abs(path: str) -> str:
    ap = os.path.abspath(path)
    if sys.platform == "win32":
        return ap
    if ap.startswith("/mnt/") and len(ap) >= 7 and ap[6] == "/":
        return f"{ap[5].upper()}:\\{ap[7:].replace('/', '\\')}"
    return subprocess.run(["wslpath", "-w", ap], capture_output=True, text=True, check=True).stdout.strip()


def _curl_bin() -> str:
    if sys.platform == "win32":
        return "curl.exe"
    return "/mnt/c/Windows/System32/curl.exe"


def _win_temp_parent() -> str:
    if sys.platform == "win32":
        return tempfile.gettempdir()
    cand = "/mnt/c/Users/zkaed/AppData/Local/Temp"
    return cand if os.path.isdir(cand) else BUILD_DIR


def _find_real_1_5b_ptx() -> str | None:
    for c in (
        os.path.join(BUILD_DIR, "gguf_exe_zkaedi_prime_1_5b_server", "zkaedi_prime_1_5b_server_sm_120.ptx"),
        os.path.join(BUILD_DIR, "gguf_exe_zkaedi_prime_1_5b_standalone", "zkaedi_prime_1_5b_standalone_sm_120.ptx"),
    ):
        if os.path.isfile(c):
            return c
    return None


def _http_call(
    method: str,
    url: str,
    json_payload: dict | str | None = None,
    extra_headers: list[str] | None = None,
    timeout_s: int = 60,
    stream: bool = False,
) -> tuple[int, str, str]:
    tmp_path = None
    try:
        cmd = [_curl_bin(), "-sS", "--max-time", str(timeout_s), "-X", method]
        if stream:
            cmd.append("-N")
        if extra_headers:
            for h in extra_headers:
                cmd.extend(["-H", h])
        hdr_file = None
        if json_payload is not None:
            fd, tmp_path = tempfile.mkstemp(dir=BUILD_DIR, prefix="_t18_req_", suffix=".json")
            with os.fdopen(fd, "wb") as f:
                if isinstance(json_payload, str):
                    f.write(json_payload.encode("utf-8"))
                else:
                    f.write(json.dumps(json_payload).encode("utf-8"))
            cmd.extend(["-H", "Content-Type: application/json", "--data-binary", "@" + _to_win_abs(tmp_path)])
        hfd, hdr_file = tempfile.mkstemp(dir=BUILD_DIR, prefix="_t18_hdr_", suffix=".txt")
        os.close(hfd)
        cmd.extend(["-D", _to_win_abs(hdr_file), "-w", "\n__ZCC_STATUS__:%{http_code}", url])
        res = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True, timeout=timeout_s + 10)
        hdrs = ""
        if os.path.isfile(hdr_file):
            with open(hdr_file, "r", encoding="utf-8", errors="replace") as hf:
                hdrs = hf.read()
            os.remove(hdr_file)
        if res.returncode != 0:
            raise RuntimeError(f"curl failed rc={res.returncode}: {res.stderr}\n{res.stdout}")
        marker = "\n__ZCC_STATUS__:"
        idx = res.stdout.rfind(marker)
        if idx < 0:
            raise RuntimeError(f"Missing curl status marker in stdout: {res.stdout!r}")
        body = res.stdout[:idx]
        code = int(res.stdout[idx + len(marker) :].strip())
        return code, hdrs, body
    finally:
        if tmp_path and os.path.exists(tmp_path):
            os.remove(tmp_path)


def _start_server(exe_path: str, args: list[str], cwd: str = REPO_ROOT, timeout_s: int = 90):
    proc = subprocess.Popen(
        [exe_path] + args,
        cwd=cwd,
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
                    f"Server exited early rc={proc.returncode}:\n{''.join(lines)}{rem_out}\nSTDERR:\n{rem_err}"
                )
            time.sleep(0.05)
            continue
        lines.append(line)
        if line.startswith("ZCC_SERVER_READY "):
            m = re.search(r"url=(http://\S+)", line)
            if not m:
                raise RuntimeError(f"Malformed ZCC_SERVER_READY line: {line!r}")
            ready_url = m.group(1)
            break
    if not ready_url:
        proc.kill()
        rem_out, rem_err = proc.communicate(timeout=5)
        raise RuntimeError(f"Server did not become ready within {timeout_s}s:\n{''.join(lines)}{rem_out}\n{rem_err}")
    return proc, ready_url, "".join(lines)


def _stop_server(proc: subprocess.Popen, url: str | None = None, allow_shutdown: bool = True) -> None:
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


class TestWin64GpuTrack18OpenAiServer(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (ZCC_BIN, ZCC_WIN64, SERVER_OUT, UI_HTML_PATH, TINY_GGUF_F16, TINY_PTX_F16)
        missing = [p for p in required if not os.path.isfile(p)]
        if missing:
            raise unittest.SkipTest(
                f"Missing Track 18 artifacts ({missing}); run `python3 tools/build_gpu_llm_server_win64.py` first."
            )

    def test_01_server_pe32plus_ws2_audit_and_ui_ef7(self) -> None:
        if os.path.isfile(SERVER_HOST_OUT):
            self.assertEqual(sha256_file(SERVER_HOST_OUT), sha256_file(SERVER_OUT))
            fc = run_fc_b(SERVER_HOST_OUT, SERVER_OUT)
            self.assertEqual(fc.returncode, 0, fc.stdout + fc.stderr)
            self.assertIn("no differences encountered", fc.stdout.lower())

        imp = audit_pe32plus_multi_dll(
            SERVER_OUT,
            required_dlls=("ws2_32.dll", "msvcrt.dll", "nvcuda.dll"),
            allowed_dlls=("ws2_32.dll", "msvcrt.dll", "nvcuda.dll"),
        )
        self.assertIn("WS2_32.dll", imp["dlls"])
        for fn in ("WSAStartup", "socket", "bind", "listen", "accept", "recv", "send", "closesocket", "getsockname"):
            self.assertIn(fn, imp["dlls"]["WS2_32.dll"])

        ui = verify_server_ui_headless(UI_HTML_PATH)
        ui_json = json.loads(ui["stdout"])
        self.assertTrue(ui_json["ui_ok"])
        self.assertEqual(ui_json["errors"], 0)

    def test_02_external_1_5b_openai_rest_sse_and_multiturn(self) -> None:
        ptx = _find_real_1_5b_ptx()
        if not (os.path.isfile(REAL_1_5B_GGUF) and ptx):
            self.skipTest("Missing real 1.5B GGUF or PTX")
        proc, url, _ = _start_server(
            SERVER_OUT,
            [
                "--model", _to_win_abs(REAL_1_5B_GGUF),
                "--aot-ptx", _to_win_abs(ptx),
                "--port", "0",
                "--kv-type", "f16",
                "--ctx", "512",
                "--model-name", "zkaedi-prime-1.5b",
                "--allow-shutdown",
            ],
        )
        try:
            c_root, _, b_root = _http_call("GET", f"{url}/")
            self.assertEqual(c_root, 200)
            self.assertIn("ZKAEDI ZCC // Sovereign OpenAI GPU Server", b_root)

            c_hlth, _, b_hlth = _http_call("GET", f"{url}/health")
            self.assertEqual(c_hlth, 200)
            j_hlth = json.loads(b_hlth)
            self.assertEqual(j_hlth["status"], "ok")
            self.assertEqual(j_hlth["model"], "zkaedi-prime-1.5b")
            self.assertFalse(j_hlth["seal_verified"])

            c_mod, _, b_mod = _http_call("GET", f"{url}/v1/models")
            self.assertEqual(c_mod, 200)
            j_mod = json.loads(b_mod)
            self.assertEqual(j_mod["data"][0]["id"], "zkaedi-prime-1.5b")

            # Non-streaming OpenAI chat completion
            c_chat, _, b_chat = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "model": "zkaedi-prime-1.5b",
                    "messages": [{"role": "user", "content": "What is the capital of France?"}],
                    "temperature": 0.0,
                    "max_tokens": 24,
                    "stream": False,
                },
            )
            self.assertEqual(c_chat, 200, b_chat)
            j_chat = json.loads(b_chat)
            self.assertEqual(j_chat["object"], "chat.completion")
            self.assertIn("Paris", j_chat["choices"][0]["message"]["content"])
            self.assertGreater(j_chat["usage"]["completion_tokens"], 0)

            # Streaming SSE chat completion + multi-turn history
            c_sse, h_sse, b_sse = _http_call(
                "POST",
                f"{url}/v1/chat/completions",
                {
                    "model": "zkaedi-prime-1.5b",
                    "messages": [
                        {"role": "system", "content": "Answer concisely."},
                        {"role": "user", "content": "What is the capital of Japan?"},
                        {"role": "assistant", "content": "The capital of Japan is Tokyo."},
                        {"role": "user", "content": "Repeat only that city name."},
                    ],
                    "temperature": 0.0,
                    "max_tokens": 16,
                    "stream": True,
                },
                stream=True,
            )
            self.assertEqual(c_sse, 200)
            self.assertIn("text/event-stream", h_sse.lower())
            self.assertIn("data: [DONE]", b_sse)
            pieces = []
            for line in b_sse.splitlines():
                line = line.strip()
                if line.startswith("data:") and not line.endswith("[DONE]"):
                    obj = json.loads(line[5:].strip())
                    delta = obj["choices"][0]["delta"]
                    if "content" in delta:
                        pieces.append(delta["content"])
            self.assertIn("Tokyo", "".join(pieces))
        finally:
            _stop_server(proc, url, allow_shutdown=True)

    def test_03_sealed_overlay_1_5b_server_empty_dir_and_tamper_nc(self) -> None:
        if not os.path.isfile(REAL_SERVER_EXE):
            self.skipTest(f"Missing {REAL_SERVER_EXE}")
        with tempfile.TemporaryDirectory(dir=_win_temp_parent(), prefix="zcc_t18_ov_") as empty_dir:
            proc, url, ready_txt = _start_server(
                REAL_SERVER_EXE,
                ["--port", "0", "--kv-type", "f16", "--ctx", "512", "--allow-shutdown"],
                cwd=empty_dir,
            )
            try:
                self.assertIn("GPU_LLM_SEAL verified=1", ready_txt)
                self.assertIn("mode=aot_overlay", ready_txt)
                c_hlth, _, b_hlth = _http_call("GET", f"{url}/health")
                self.assertEqual(c_hlth, 200)
                j_hlth = json.loads(b_hlth)
                self.assertTrue(j_hlth["seal_verified"])

                c_sse, _, b_sse = _http_call(
                    "POST",
                    f"{url}/v1/chat/completions",
                    {
                        "messages": [{"role": "user", "content": "What is the capital of France?"}],
                        "temperature": 0.0,
                        "max_tokens": 24,
                        "stream": True,
                    },
                    stream=True,
                )
                self.assertEqual(c_sse, 200)
                self.assertIn("data: [DONE]", b_sse)
                self.assertIn("Paris", b_sse)
            finally:
                _stop_server(proc, url, allow_shutdown=True)

        # Negative control: flip 1 bit inside sealed Q4_K payload -> fail-closed before socket bind
        tamper_exe = os.path.join(BUILD_DIR, "_tmp_t18_server_tamper.exe")
        try:
            shutil.copyfile(REAL_SERVER_EXE, tamper_exe)
            sz = os.path.getsize(tamper_exe)
            with open(tamper_exe, "r+b") as f:
                f.seek(sz // 2)
                b = f.read(1)
                f.seek(sz // 2)
                f.write(bytes([b[0] ^ 0x01]))
            r = subprocess.run(
                [tamper_exe, "--port", "0"],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=120,
            )
            comb = r.stdout + "\n" + r.stderr
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("TAMPER_DETECTED", comb)
            self.assertNotIn("ZCC_SERVER_READY", comb)
        finally:
            if os.path.exists(tamper_exe):
                os.remove(tamper_exe)

    def test_04_completions_endpoint_and_cors_preflight(self) -> None:
        proc, url, _ = _start_server(
            SERVER_OUT,
            [
                "--model", _to_win_abs(TINY_GGUF_F16),
                "--aot-ptx", _to_win_abs(TINY_PTX_F16),
                "--port", "0",
                "--allow-shutdown",
            ],
        )
        try:
            c_opt, h_opt, _ = _http_call(
                "OPTIONS",
                f"{url}/v1/completions",
                extra_headers=["Origin: http://localhost:3000", "Access-Control-Request-Method: POST"],
            )
            self.assertEqual(c_opt, 204)
            self.assertIn("access-control-allow-origin: *", h_opt.lower())

            c_cmp, _, b_cmp = _http_call(
                "POST",
                f"{url}/v1/completions",
                {"prompt": "ZCC compil", "temperature": 0.0, "max_tokens": 16, "stream": False},
            )
            self.assertEqual(c_cmp, 200, b_cmp)
            j_cmp = json.loads(b_cmp)
            self.assertEqual(j_cmp["object"], "text_completion")
            self.assertIn("es C99 directly", j_cmp["choices"][0]["text"])

            c_s, _, b_s = _http_call(
                "POST",
                f"{url}/v1/completions",
                {"prompt": "ZCC compil", "temperature": 0.0, "max_tokens": 16, "stream": True},
                stream=True,
            )
            self.assertEqual(c_s, 200)
            self.assertIn("data: [DONE]", b_s)
            toks = []
            for line in b_s.splitlines():
                line = line.strip()
                if line.startswith("data:") and not line.endswith("[DONE]"):
                    toks.append(json.loads(line[5:].strip())["choices"][0]["text"])
            self.assertIn("es C99 directly", "".join(toks))
        finally:
            _stop_server(proc, url, allow_shutdown=True)

    def test_05_ef0_http_negative_controls(self) -> None:
        # Launched WITHOUT --allow-shutdown so POST /shutdown MUST return 404
        proc, url, _ = _start_server(
            SERVER_OUT,
            ["--model", _to_win_abs(TINY_GGUF_F16), "--aot-ptx", _to_win_abs(TINY_PTX_F16), "--port", "0"],
        )
        try:
            c1, _, _ = _http_call("POST", f"{url}/v1/chat/completions", "{bad json")
            self.assertEqual(c1, 400)

            c2, _, _ = _http_call("POST", f"{url}/v1/chat/completions", {"model": "x"})
            self.assertEqual(c2, 400)

            c3, _, _ = _http_call("GET", f"{url}/v1/nonexistent_route")
            self.assertEqual(c3, 404)

            c4, _, _ = _http_call("GET", f"{url}/v1/chat/completions")
            self.assertEqual(c4, 405)

            c5, _, _ = _http_call("POST", f"{url}/shutdown")
            self.assertEqual(c5, 404)

            # Oversized Content-Length via raw TcpClient -> immediate 413 without closing crashed server
            port = int(url.rsplit(":", 1)[1])
            ps = (
                f"$c = New-Object System.Net.Sockets.TcpClient('127.0.0.1', {port}); "
                "$s = $c.GetStream(); "
                "$msg = [Text.Encoding]::ASCII.GetBytes(\"POST /v1/chat/completions HTTP/1.1`r`nHost: 127.0.0.1`r`nContent-Length: 99999999`r`n`r`n\"); "
                "$s.Write($msg, 0, $msg.Length); "
                "$r = New-Object System.IO.StreamReader($s); "
                "$line = $r.ReadLine(); Write-Output $line; $c.Close()"
            )
            ps_bin = "powershell.exe" if sys.platform != "win32" else "powershell"
            rp = subprocess.run([ps_bin, "-NoProfile", "-Command", ps], capture_output=True, text=True, timeout=15)
            self.assertEqual(rp.returncode, 0, rp.stderr)
            self.assertIn("413", rp.stdout)

            # Server remains alive & healthy
            c6, _, b6 = _http_call("GET", f"{url}/health")
            self.assertEqual(c6, 200)
            self.assertEqual(json.loads(b6)["status"], "ok")
        finally:
            _stop_server(proc, url, allow_shutdown=False)

    def test_06_client_midstream_disconnect_resilience(self) -> None:
        proc, url, _ = _start_server(
            SERVER_OUT,
            [
                "--model", _to_win_abs(TINY_GGUF_F16),
                "--aot-ptx", _to_win_abs(TINY_PTX_F16),
                "--port", "0",
                "--allow-shutdown",
            ],
        )
        try:
            port = int(url.rsplit(":", 1)[1])
            payload = json.dumps({
                "messages": [{"role": "user", "content": "ZCC compil"}],
                "temperature": 0.0,
                "max_tokens": 96,
                "stream": True,
            })
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
                "if (-not $seen) { throw 'Never received first SSE chunk before abort' }; "
                "Write-Output 'ABORTED_MID_STREAM'"
            )
            ps_bin = "powershell.exe" if sys.platform != "win32" else "powershell"
            rp = subprocess.run([ps_bin, "-NoProfile", "-Command", ps], capture_output=True, text=True, timeout=30)
            self.assertEqual(rp.returncode, 0, rp.stdout + rp.stderr)
            self.assertIn("ABORTED_MID_STREAM", rp.stdout)
            time.sleep(0.25)
            self.assertIsNone(proc.poll(), "Server crashed after mid-stream client disconnect")

            c_hlth, _, b_hlth = _http_call("GET", f"{url}/health")
            self.assertEqual(c_hlth, 200)
            self.assertEqual(json.loads(b_hlth)["status"], "ok")

            c_cmp, _, b_cmp = _http_call(
                "POST",
                f"{url}/v1/completions",
                {"prompt": "ZCC compil", "temperature": 0.0, "max_tokens": 16, "stream": False},
            )
            self.assertEqual(c_cmp, 200, b_cmp)
            self.assertIn("es C99 directly", json.loads(b_cmp)["choices"][0]["text"])
        finally:
            _stop_server(proc, url, allow_shutdown=True)


if __name__ == "__main__":
    unittest.main()
