#!/usr/bin/env python3
"""
tools/test_dpo_server_endpoints.py — E2E Test Suite for ZKAEDI DPO API Server
=============================================================================
Tests all endpoints of serve_zkaedi_dpo_server.py:
  - GET  /v1/health
  - GET  /v1/models
  - POST /v1/dpo/evaluate
  - POST /v1/chat/completions
"""

import sys
import os
import json
import time
import subprocess
import urllib.request
import urllib.error
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

TEST_PORT = 8018
BASE_URL = f"http://127.0.0.1:{TEST_PORT}"
RECEIPT_PATH = Path(__file__).parent.parent / "docs" / "evidence" / "dpo_server_endpoint_verification_receipt.json"


def http_get(endpoint: str):
    url = f"{BASE_URL}{endpoint}"
    req = urllib.request.Request(url, headers={"User-Agent": "ZkaediTestHarness/1.0"})
    with urllib.request.urlopen(req, timeout=10) as resp:
        return resp.status, json.loads(resp.read().decode("utf-8"))


def http_post(endpoint: str, data: dict):
    url = f"{BASE_URL}{endpoint}"
    body = json.dumps(data).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json", "User-Agent": "ZkaediTestHarness/1.0"}
    )
    with urllib.request.urlopen(req, timeout=30) as resp:
        return resp.status, json.loads(resp.read().decode("utf-8"))


def main():
    print("=" * 80)
    print("🔱 ZKAEDI DPO SERVER ENDPOINT E2E TEST HARNESS 🔱")
    print(f"[*] Testing Target: {BASE_URL}")
    print("=" * 80)

    server_script = Path(__file__).parent / "serve_zkaedi_dpo_server.py"
    env = os.environ.copy()
    env["PORT"] = str(TEST_PORT)
    env["HOST"] = "127.0.0.1"

    print(f"[*] Spawning server subprocess ({server_script})...")
    proc = subprocess.Popen(
        [sys.executable, str(server_script)],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    t0 = time.time()
    ready = False
    health_data = {}

    try:
        print("[*] Waiting for server initialization...")
        for attempt in range(40):
            time.sleep(1.0)
            if proc.poll() is not None:
                stdout, stderr = proc.communicate()
                print(f"[!] Server exited unexpectedly with code {proc.returncode}!")
                print(f"STDOUT:\n{stdout}\nSTDERR:\n{stderr}")
                sys.exit(1)
            try:
                status, data = http_get("/v1/health")
                if status == 200 and data.get("status") == "healthy":
                    health_data = data
                    ready = True
                    break
            except Exception:
                pass

        if not ready:
            print("[!] Server timed out waiting for readiness.")
            sys.exit(1)

        init_time = time.time() - t0
        print(f"[✓] Server healthy in {init_time:.2f}s! GPU: {health_data.get('gpu')}")

        # Test 1: GET /v1/models
        print("\n[*] Testing GET /v1/models...")
        s_models, d_models = http_get("/v1/models")
        print(f"  Status: {s_models} | Models: {[m['id'] for m in d_models.get('data', [])]}")
        assert s_models == 200
        assert len(d_models.get("data", [])) > 0

        # Test 2: POST /v1/dpo/evaluate
        print("\n[*] Testing POST /v1/dpo/evaluate (Pair Discrimination)...")
        eval_payload = {
            "prompt": "Evaluate and compare the two 3D asset generation attempts. Identify which asset is superior under the ZKAEDI PRIME topological evaluation framework.\n\nAsset A Prompt: Sovereign DePIN Autonomous Drone\nAsset B Prompt: Decimated wireframe with open holes",
            "response_a": "Asset A is the superior generation. Classified under LEGENDARY tier with Hamiltonian energy H0 of 7,942,180. Watertight 2-manifold with zero degenerate faces.",
            "response_b": "Asset B is the superior generation. Classified under COMMON tier with Hamiltonian energy H0 of 84,210. Contains open boundary holes and missing quadrics.",
            "beta": 0.1
        }
        s_eval, d_eval = http_post("/v1/dpo/evaluate", eval_payload)
        print(f"  Status: {s_eval}")
        print(f"  Winner: {d_eval.get('winner')} | Margin: {d_eval.get('margin_delta_r'):+.4f} | Prob: {d_eval.get('prob_a_preferred')*100:.1f}%")
        assert s_eval == 200
        assert d_eval.get("winner") == "Asset A"
        assert d_eval.get("margin_delta_r", 0.0) > 0.0

        # Test 3: POST /v1/chat/completions
        print("\n[*] Testing POST /v1/chat/completions (OpenAI Schema Generation)...")
        chat_payload = {
            "messages": [
                {"role": "user", "content": "Analyze the topological manifold requirements for a 3D drone mesh."}
            ],
            "max_tokens": 64,
            "temperature": 0.1
        }
        s_chat, d_chat = http_post("/v1/chat/completions", chat_payload)
        reply = d_chat.get("choices", [{}])[0].get("message", {}).get("content", "")
        tokens = d_chat.get("usage", {}).get("completion_tokens", 0)
        latency = d_chat.get("latency_seconds", 0.0)
        print(f"  Status: {s_chat} | Tokens: {tokens} | Latency: {latency:.3f}s")
        print(f"  Reply Preview: {reply[:120]}...")
        assert s_chat == 200
        assert len(reply) > 0

        # Test Results & Receipt
        receipt = {
            "gate_id": "TEST-DPO-SERVER-ENDPOINTS",
            "verdict": "PASS",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "server_url": BASE_URL,
            "health": health_data,
            "tests": {
                "get_health": {"status": 200, "result": health_data},
                "get_models": {"status": s_models, "models_count": len(d_models.get("data", []))},
                "post_dpo_evaluate": {"status": s_eval, "winner": d_eval.get("winner"), "margin": d_eval.get("margin_delta_r")},
                "post_chat_completions": {"status": s_chat, "tokens": tokens, "latency_s": latency}
            }
        }
        RECEIPT_PATH.parent.mkdir(parents=True, exist_ok=True)
        with open(RECEIPT_PATH, "w", encoding="utf-8") as f:
            json.dump(receipt, f, indent=2)

        print("\n" + "=" * 80)
        print("🏆 ALL DPO SERVER ENDPOINTS VERIFIED PASS 🏆")
        print(f"[*] Receipt saved to: {RECEIPT_PATH}")
        print("=" * 80)

    finally:
        print("[*] Terminating server subprocess...")
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        print("[✓] Server stopped cleanly.")


if __name__ == "__main__":
    main()
