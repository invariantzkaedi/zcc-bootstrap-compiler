# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // RESIDENT VRAM INFERENCE DAEMON 🔱
=======================================================================================================
 Keeps Qwen2.5-Coder-1.5B resident in GPU VRAM to eliminate the ~20-second weight loading penalty.
 Provides sub-millisecond IPC over Unix Domain Sockets (/tmp/zkaedi_prime.sock) with TCP fallback.
=======================================================================================================
"""

import os
import sys
import time
import json
import socket
import select
import signal
from pathlib import Path
from typing import Dict, Any, Optional, Tuple

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

DEFAULT_SOCKET_PATH = "/tmp/zkaedi_prime.sock"
DEFAULT_PID_PATH = "/tmp/zkaedi_prime.pid"
DEFAULT_TCP_HOST = "127.0.0.1"
DEFAULT_TCP_PORT = 8765

DEFAULT_MODEL_PATH = (
    "/mnt/h/models/Qwen2.5-Coder-1.5B-Instruct"
    if sys.platform != "win32"
    else "H:/models/Qwen2.5-Coder-1.5B-Instruct"
)

def resolve_model_path(path_str: str) -> Path:
    if sys.platform != "win32":
        if path_str.startswith("H:/") or path_str.startswith("H:\\"):
            path_str = "/mnt/h/" + path_str[3:].replace("\\", "/")
        elif path_str.startswith("C:/") or path_str.startswith("C:\\"):
            path_str = "/mnt/c/" + path_str[3:].replace("\\", "/")
    return Path(path_str)

def get_client_connection(timeout: float = 2.0) -> Tuple[Optional[socket.socket], str]:
    """
    Attempts to connect to the daemon via Unix Domain Socket first, then TCP fallback.
    Returns (sock, conn_type) or (None, "").
    """
    # 1. Try Unix Domain Socket if on Linux / WSL
    if sys.platform != "win32" and os.path.exists(DEFAULT_SOCKET_PATH):
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(timeout)
            s.connect(DEFAULT_SOCKET_PATH)
            return s, "unix"
        except Exception:
            try:
                s.close()
            except Exception:
                pass

    # 2. Try TCP localhost fallback
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(timeout)
        s.connect((DEFAULT_TCP_HOST, DEFAULT_TCP_PORT))
        return s, "tcp"
    except Exception:
        try:
            s.close()
        except Exception:
            pass

    return None, ""

def send_daemon_request(req: Dict[str, Any], timeout: float = 120.0) -> Optional[Dict[str, Any]]:
    """
    Sends a JSON command to the running daemon and returns the parsed response dictionary.
    """
    s, _ = get_client_connection(timeout=3.0)
    if s is None:
        return None

    try:
        s.settimeout(timeout)
        msg_bytes = (json.dumps(req) + "\n").encode("utf-8")
        s.sendall(msg_bytes)

        # Read response until newline
        buf = []
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            buf.append(chunk)
            if b"\n" in chunk:
                break

        raw = b"".join(buf).decode("utf-8").strip()
        if not raw:
            return None
        return json.loads(raw)
    except Exception as e:
        return {"status": "error", "error": str(e)}
    finally:
        try:
            s.close()
        except Exception:
            pass

def is_daemon_running() -> bool:
    """Checks if the daemon is currently running and responsive."""
    res = send_daemon_request({"action": "ping"}, timeout=1.5)
    return res is not None and res.get("status") == "pong"

class ZkaediPrimeDaemon:
    def __init__(self, model_path: str = DEFAULT_MODEL_PATH, port: int = DEFAULT_TCP_PORT, use_unix_socket: bool = True):
        self.model_path = resolve_model_path(model_path)
        self.port = port
        self.use_unix_socket = use_unix_socket and (sys.platform != "win32")
        self.socket_path = DEFAULT_SOCKET_PATH
        self.pid_path = DEFAULT_PID_PATH
        self.running = False
        self.model = None
        self.tokenizer = None
        self.server_sockets = []

    def load_model(self):
        print(f"\n[DAEMON // VRAM INIT] Pre-loading Model: {self.model_path} into GPU...")
        t0 = time.time()
        import torch
        from transformers import AutoModelForCausalLM, AutoTokenizer

        self.tokenizer = AutoTokenizer.from_pretrained(str(self.model_path), trust_remote_code=True)
        self.model = AutoModelForCausalLM.from_pretrained(
            str(self.model_path),
            dtype=torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16,
            device_map="auto",
            trust_remote_code=True
        )
        self.model.eval()
        vram_mb = 0
        if torch.cuda.is_available():
            vram_mb = torch.cuda.memory_allocated() / (1024 * 1024)
        load_dur = time.time() - t0
        print(f"[OK] [DAEMON // READY] Weights loaded in {load_dur:.2f}s | Resident VRAM: {vram_mb:.1f} MB\n")

    def handle_request(self, req: Dict[str, Any]) -> Dict[str, Any]:
        action = req.get("action", "")

        if action == "ping":
            import torch
            vram_mb = 0
            device = "cpu"
            if torch.cuda.is_available():
                vram_mb = torch.cuda.memory_allocated() / (1024 * 1024)
                device = str(torch.cuda.get_device_name(0))
            return {
                "status": "pong",
                "device": device,
                "vram_mb": round(vram_mb, 1),
                "model": str(self.model_path.name)
            }

        elif action == "generate":
            prompt = req.get("prompt", "")
            mode = req.get("mode", "CODE_GRAMMAR").upper()
            max_tokens = req.get("max_tokens", 512)
            pure_code = req.get("pure_code", True)
            terminal_scope_clamp = req.get("terminal_scope_clamp", True)

            import torch
            from transformers import LogitsProcessorList

            messages = [
                {
                    "role": "system",
                    "content": "You are an expert low-level C systems compiler engineer. Write clean, complete, standalone C programs starting with #include and containing main(). Zero markdown, zero commentary, pure C source code."
                },
                {"role": "user", "content": prompt}
            ]
            prompt_str = self.tokenizer.apply_chat_template(messages, tokenize=False, add_generation_prompt=True)
            inputs = self.tokenizer(prompt_str, return_tensors="pt").to(self.model.device)
            plen = inputs["input_ids"].shape[1]

            if mode == "CODE_GRAMMAR":
                from .code_grammar_engine import SovereignControlFlowLogitsProcessor
                processor = SovereignControlFlowLogitsProcessor(
                    prompt_len=plen,
                    tokenizer=self.tokenizer,
                    require_return=True,
                    allow_markdown_fences=False,
                    pure_code=pure_code,
                    terminal_scope_clamp=terminal_scope_clamp
                )
            else:
                from .squozen_engine import SquozenLogitsEngine, SquozenMode
                processor = SquozenLogitsEngine(
                    prompt_len=plen,
                    tokenizer=self.tokenizer,
                    mode=SquozenMode(mode),
                    allow_backticks=False,
                    anti_repetition=True
                )

            t0 = time.time()
            with torch.no_grad():
                out = self.model.generate(
                    **inputs,
                    max_new_tokens=max_tokens,
                    do_sample=False,
                    logits_processor=LogitsProcessorList([processor]),
                    pad_token_id=self.tokenizer.eos_token_id
                )
            gen_time = time.time() - t0
            c_source = self.tokenizer.decode(out[0][plen:], skip_special_tokens=True).strip()

            return {
                "status": "ok",
                "c_source": c_source,
                "gen_time": round(gen_time, 4),
                "fences": c_source.count("`") + c_source.count("~~~"),
                "length": len(c_source)
            }

        elif action == "shutdown":
            self.running = False
            return {"status": "ok", "message": "Daemon shutting down gracefully."}

        else:
            return {"status": "error", "error": f"Unknown action: {action}"}

    def start(self):
        # 1. Load model into memory
        self.load_model()

        # 2. Setup sockets
        self.server_sockets = []

        # Unix Domain Socket
        if self.use_unix_socket:
            if os.path.exists(self.socket_path):
                try:
                    os.unlink(self.socket_path)
                except OSError:
                    pass
            unix_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            unix_sock.bind(self.socket_path)
            os.chmod(self.socket_path, 0o777)
            unix_sock.listen(16)
            self.server_sockets.append(unix_sock)
            print(f"[DAEMON] Listening on Unix Domain Socket: {self.socket_path}")

        # TCP Socket
        tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tcp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        tcp_sock.bind((DEFAULT_TCP_HOST, self.port))
        if self.port == 0:
            self.port = tcp_sock.getsockname()[1]
        tcp_sock.listen(16)
        self.server_sockets.append(tcp_sock)
        print(f"[DAEMON] Listening on TCP: {DEFAULT_TCP_HOST}:{self.port}")

        # Write PID file
        Path(self.pid_path).write_text(str(os.getpid()), encoding="utf-8")

        self.running = True
        print(f"🚀 [ZKAEDI PRIME DAEMON RUNNING] PID={os.getpid()} | Ready for instant queries.")

        try:
            while self.running:
                readable, _, _ = select.select(self.server_sockets, [], [], 0.5)
                for s in readable:
                    try:
                        conn, _ = s.accept()
                        conn.settimeout(60.0)
                        # Read request
                        buf = []
                        while True:
                            chunk = conn.recv(4096)
                            if not chunk:
                                break
                            buf.append(chunk)
                            if b"\n" in chunk:
                                break
                        raw_data = b"".join(buf).decode("utf-8").strip()
                        if raw_data:
                            try:
                                req = json.loads(raw_data)
                                resp = self.handle_request(req)
                            except Exception as ex:
                                resp = {"status": "error", "error": str(ex)}
                            resp_bytes = (json.dumps(resp) + "\n").encode("utf-8")
                            conn.sendall(resp_bytes)
                        conn.close()
                    except Exception as e:
                        print(f"[DAEMON WARNING] Client error: {e}")
        finally:
            self.cleanup()

    def cleanup(self):
        print("\n[DAEMON] Cleaning up sockets and PID...")
        for s in self.server_sockets:
            try:
                s.close()
            except Exception:
                pass
        if self.use_unix_socket and os.path.exists(self.socket_path):
            try:
                os.unlink(self.socket_path)
            except Exception:
                pass
        if os.path.exists(self.pid_path):
            try:
                os.unlink(self.pid_path)
            except Exception:
                pass
        print("[OK] [DAEMON] Shutdown complete.")

def run_daemon_cli(action: str, model_path: str = DEFAULT_MODEL_PATH, port: int = DEFAULT_TCP_PORT):
    if action == "status":
        res = send_daemon_request({"action": "ping"}, timeout=2.0)
        if res and res.get("status") == "pong":
            print(f"✔ Zkaedi Prime Daemon is ALIVE")
            print(f"  Model:  {res.get('model')}")
            print(f"  Device: {res.get('device')}")
            print(f"  VRAM:   {res.get('vram_mb')} MB")
        else:
            print("❌ Zkaedi Prime Daemon is NOT running.")
            sys.exit(1)

    elif action == "stop":
        res = send_daemon_request({"action": "shutdown"}, timeout=3.0)
        if res and res.get("status") == "ok":
            print("✔ Sent shutdown signal to Zkaedi Prime Daemon.")
        else:
            # Try PID file kill fallback
            pid_file = Path(DEFAULT_PID_PATH)
            if pid_file.exists():
                try:
                    pid = int(pid_file.read_text(encoding="utf-8").strip())
                    os.kill(pid, signal.SIGTERM)
                    print(f"✔ Terminated daemon PID {pid} via SIGTERM.")
                except Exception as ex:
                    print(f"❌ Failed to terminate PID: {ex}")
            else:
                print("Daemon is not reachable.")

    elif action == "start":
        if is_daemon_running():
            print("⚠️ Zkaedi Prime Daemon is already running!")
            return
        daemon = ZkaediPrimeDaemon(model_path=model_path, port=port)
        daemon.start()

    elif action == "restart":
        run_daemon_cli("stop")
        time.sleep(1.0)
        run_daemon_cli("start", model_path=model_path, port=port)
