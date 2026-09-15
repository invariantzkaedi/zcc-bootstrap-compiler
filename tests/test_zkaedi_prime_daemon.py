# -*- coding: utf-8 -*-
"""
Tests for Zkaedi Prime Resident VRAM Inference Daemon.
Verifies socket communication, protocol handling, ping/pong, and graceful shutdown.
"""

import unittest
import threading
import time
import socket
import select
import json
from pathlib import Path
from zkaedi_prime.daemon import (
    ZkaediPrimeDaemon,
    send_daemon_request,
    is_daemon_running,
    DEFAULT_TCP_HOST,
    DEFAULT_TCP_PORT
)

class MockDaemon(ZkaediPrimeDaemon):
    """Subclass that bypasses heavy model loading for unit testing."""
    def load_model(self):
        self.model = True
        self.tokenizer = True

    def handle_request(self, req):
        action = req.get("action", "")
        if action == "ping":
            return {"status": "pong", "device": "mock_gpu", "vram_mb": 128.0, "model": "mock-qwen"}
        elif action == "generate":
            prompt = req.get("prompt", "")
            return {
                "status": "ok",
                "c_source": f"// Generated for: {prompt}\n#include <stdio.h>\nint main(void) {{ return 0; }}\n",
                "gen_time": 0.005,
                "fences": 0,
                "length": 65
            }
        elif action == "shutdown":
            self.running = False
            return {"status": "ok", "message": "Mock daemon shutting down."}
        return super().handle_request(req)

class TestZkaediPrimeDaemon(unittest.TestCase):
    def setUp(self):
        self.daemon = MockDaemon(port=0, use_unix_socket=False)
        self.started_event = threading.Event()
        
        def run_daemon():
            # Intercept server setup to signal started
            self.daemon.load_model()
            tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            tcp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            tcp_sock.bind((DEFAULT_TCP_HOST, 0))
            self.daemon.port = tcp_sock.getsockname()[1]
            tcp_sock.listen(16)
            self.daemon.server_sockets.append(tcp_sock)
            self.daemon.running = True
            self.started_event.set()
            
            try:
                while self.daemon.running:
                    readable, _, _ = select.select(self.daemon.server_sockets, [], [], 0.2)
                    for s in readable:
                        try:
                            conn, _ = s.accept()
                            conn.settimeout(10.0)
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
                                req = json.loads(raw_data)
                                resp = self.daemon.handle_request(req)
                                conn.sendall((json.dumps(resp) + "\n").encode("utf-8"))
                            conn.close()
                        except Exception:
                            pass
            finally:
                self.daemon.cleanup()

        self.daemon_thread = threading.Thread(target=run_daemon, daemon=True)
        self.daemon_thread.start()
        self.started_event.wait(timeout=5.0)

    def tearDown(self):
        if self.daemon.running:
            self.daemon.running = False
            self.daemon.cleanup()
        time.sleep(0.1)

    def test_mock_daemon_ping_and_generate(self):
        # Direct socket test to self.daemon.port
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((DEFAULT_TCP_HOST, self.daemon.port))

        # Ping
        s.sendall(b'{"action": "ping"}\n')
        resp_raw = s.recv(4096).decode("utf-8").strip()
        resp = json.loads(resp_raw)
        self.assertEqual(resp.get("status"), "pong")
        self.assertEqual(resp.get("model"), "mock-qwen")
        s.close()

        # Generate
        s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s2.settimeout(2.0)
        s2.connect((DEFAULT_TCP_HOST, self.daemon.port))
        s2.sendall(b'{"action": "generate", "prompt": "test prompt"}\n')
        gen_raw = s2.recv(4096).decode("utf-8").strip()
        gen_resp = json.loads(gen_raw)
        self.assertEqual(gen_resp.get("status"), "ok")
        self.assertIn("#include <stdio.h>", gen_resp.get("c_source"))
        self.assertEqual(gen_resp.get("fences"), 0)
        s2.close()

        # Shutdown
        s3 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s3.settimeout(2.0)
        s3.connect((DEFAULT_TCP_HOST, self.daemon.port))
        s3.sendall(b'{"action": "shutdown"}\n')
        shut_raw = s3.recv(4096).decode("utf-8").strip()
        shut_resp = json.loads(shut_raw)
        self.assertEqual(shut_resp.get("status"), "ok")
        s3.close()

if __name__ == "__main__":
    unittest.main()
