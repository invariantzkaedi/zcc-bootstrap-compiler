#!/usr/bin/env python3
"""
ZKAEDI DPO OPENAI-COMPATIBLE API & TOPOLOGICAL EVALUATION SERVER
==============================================================
Serves Qwen2.5-Coder-1.5B + Full-Scale LoRA 3D DPO Adapter over HTTP.
Provides:
  1. GET  /health, /v1/health       - Server and GPU health check
  2. GET  /v1/models                - Model inventory list
  3. POST /v1/chat/completions      - OpenAI-compatible chat completion
  4. POST /v1/dpo/evaluate          - Exact DPO reward margin and preference scoring
"""

import os
import sys
import json
import time
import math
from pathlib import Path
from http.server import HTTPServer, BaseHTTPRequestHandler
import torch
import torch.nn.functional as F
from transformers import AutoModelForCausalLM, AutoTokenizer
from peft import PeftModel

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

REPO_ROOT = Path(__file__).parent.parent.resolve()
DEFAULT_BASE_MODEL = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
DEFAULT_ADAPTER = str(REPO_ROOT / "artifacts" / "zkaedi_3d_dpo_lora_full_adapter")

BASE_MODEL_PATH = os.getenv("BASE_MODEL_PATH", DEFAULT_BASE_MODEL)
ADAPTER_PATH = os.getenv("ADAPTER_PATH", DEFAULT_ADAPTER)
PORT = int(os.getenv("PORT", "8000"))
HOST = os.getenv("HOST", "127.0.0.1")

print("=" * 80)
print(" 🚀 ZKAEDI DPO INFERENCE & TOPOLOGICAL EVALUATION SERVER")
print(f"    Listening:  http://{HOST}:{PORT}")
print(f"    Base Model: {BASE_MODEL_PATH}")
print(f"    Adapter:    {ADAPTER_PATH}")
print("=" * 80)

device = "cuda" if torch.cuda.is_available() else "cpu"
dtype = torch.bfloat16 if torch.cuda.is_available() and torch.cuda.is_bf16_supported() else torch.float32

print(f"[*] Loading Tokenizer ({BASE_MODEL_PATH})...")
tok = AutoTokenizer.from_pretrained(BASE_MODEL_PATH, trust_remote_code=True)
if tok.pad_token is None:
    tok.pad_token = tok.eos_token

print(f"[*] Loading Base Model ({dtype}) onto {device}...")
base = AutoModelForCausalLM.from_pretrained(
    BASE_MODEL_PATH,
    dtype=dtype,
    device_map={"": device} if device == "cuda" else None
)
if device != "cuda":
    base = base.to(device)

if Path(ADAPTER_PATH).exists():
    print(f"[*] Attaching Full-Scale LoRA Adapter from: {ADAPTER_PATH}")
    model = PeftModel.from_pretrained(base, ADAPTER_PATH)
else:
    print(f"[!] Adapter path {ADAPTER_PATH} not found. Running base model.")
    model = base

model.eval()
print("[✓] Model & Adapter successfully loaded and ready for serving!\n")


def compute_logp(prompt: str, response: str) -> float:
    prompt_ids = tok.encode(prompt, add_special_tokens=False)
    resp_ids = tok.encode(response, add_special_tokens=False)
    
    full_ids = prompt_ids + resp_ids
    if len(full_ids) > 512:
        excess = len(full_ids) - 512
        prompt_ids = prompt_ids[excess:]
        full_ids = prompt_ids + resp_ids
        
    input_ids = torch.tensor([full_ids], dtype=torch.long, device=device)
    with torch.no_grad():
        outputs = model(input_ids)
        logits = outputs.logits
        
    shift_logits = logits[0, :-1, :].contiguous()
    shift_labels = input_ids[0, 1:].contiguous()
    log_probs = F.log_softmax(shift_logits, dim=-1)
    target_logps = log_probs.gather(dim=-1, index=shift_labels.unsqueeze(-1)).squeeze(-1)
    
    resp_start_idx = max(0, len(prompt_ids) - 1)
    resp_logps = target_logps[resp_start_idx:]
    return float(resp_logps.sum().item())


class DPORequestHandler(BaseHTTPRequestHandler):
    def _set_headers(self, status=200):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.end_headers()

    def do_OPTIONS(self):
        self._set_headers(200)

    def do_GET(self):
        if self.path in ["/health", "/v1/health"]:
            gpu_name = torch.cuda.get_device_name(0) if torch.cuda.is_available() else "None"
            self._set_headers(200)
            self.wfile.write(json.dumps({
                "status": "healthy",
                "model": "qwen2.5-coder-1.5b-zkaedi-3d-dpo",
                "device": device,
                "gpu": gpu_name,
                "adapter": ADAPTER_PATH
            }).encode("utf-8"))
        elif self.path in ["/v1/models"]:
            self._set_headers(200)
            self.wfile.write(json.dumps({
                "object": "list",
                "data": [{
                    "id": "qwen2.5-coder-1.5b-zkaedi-3d-dpo",
                    "object": "model",
                    "owned_by": "zkaedi",
                    "permission": []
                }]
            }).encode("utf-8"))
        else:
            self._set_headers(404)
            self.wfile.write(json.dumps({"error": "Not Found"}).encode("utf-8"))

    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        post_data = self.rfile.read(content_length)
        try:
            req = json.loads(post_data.decode("utf-8"))
        except Exception as e:
            self._set_headers(400)
            self.wfile.write(json.dumps({"error": f"Invalid JSON: {e}"}).encode("utf-8"))
            return

        if self.path in ["/v1/dpo/evaluate"]:
            prompt = req.get("prompt", "")
            resp_a = req.get("response_a", "")
            resp_b = req.get("response_b", "")
            beta = float(req.get("beta", 0.1))

            if not prompt or not resp_a or not resp_b:
                self._set_headers(400)
                self.wfile.write(json.dumps({"error": "Missing required fields: prompt, response_a, response_b"}).encode("utf-8"))
                return

            t0 = time.time()
            pi_a = compute_logp(prompt, resp_a)
            pi_b = compute_logp(prompt, resp_b)

            with model.disable_adapter():
                ref_a = compute_logp(prompt, resp_a)
                ref_b = compute_logp(prompt, resp_b)

            reward_a = beta * (pi_a - ref_a)
            reward_b = beta * (pi_b - ref_b)
            margin = reward_a - reward_b
            prob_a = 1.0 / (1.0 + math.exp(-margin))
            dur_s = time.time() - t0

            result = {
                "winner": "Asset A" if margin > 0 else "Asset B",
                "margin_delta_r": round(margin, 4),
                "prob_a_preferred": round(prob_a, 4),
                "reward_a": round(reward_a, 4),
                "reward_b": round(reward_b, 4),
                "pi_logp_a": round(pi_a, 4),
                "ref_logp_a": round(ref_a, 4),
                "pi_logp_b": round(pi_b, 4),
                "ref_logp_b": round(ref_b, 4),
                "latency_seconds": round(dur_s, 4)
            }
            self._set_headers(200)
            self.wfile.write(json.dumps(result).encode("utf-8"))

        elif self.path in ["/v1/chat/completions"]:
            messages = req.get("messages", [])
            max_out = req.get("max_completion_tokens") or req.get("max_tokens") or 256
            temperature = float(req.get("temperature", 0.2))
            model_name = req.get("model", "qwen2.5-coder-1.5b-zkaedi-3d-dpo")

            conversation = [{"role": m.get("role", "user"), "content": m.get("content", "")} for m in messages]
            prompt = tok.apply_chat_template(conversation, tokenize=False, add_generation_prompt=True)

            inputs = tok(prompt, return_tensors="pt").to(device)
            p_len = inputs.input_ids.shape[1]

            t0 = time.time()
            with torch.no_grad():
                out = model.generate(
                    **inputs,
                    max_new_tokens=max_out,
                    do_sample=(temperature > 0.0),
                    temperature=temperature if temperature > 0.0 else None,
                    pad_token_id=tok.eos_token_id
                )
            dur_s = time.time() - t0

            gen_tokens = out[0][p_len:]
            reply_text = tok.decode(gen_tokens, skip_special_tokens=True).strip()
            c_len = len(gen_tokens)

            resp_payload = {
                "id": f"chatcmpl-zkaedi-{int(time.time()*1000)}",
                "object": "chat.completion",
                "created": int(time.time()),
                "model": model_name,
                "choices": [
                    {
                        "index": 0,
                        "message": {
                            "role": "assistant",
                            "content": reply_text
                        },
                        "finish_reason": "stop"
                    }
                ],
                "usage": {
                    "prompt_tokens": p_len,
                    "completion_tokens": c_len,
                    "total_tokens": p_len + c_len
                },
                "system_fingerprint": "zkaedi-3d-dpo-v3.1",
                "latency_seconds": round(dur_s, 4)
            }

            self._set_headers(200)
            self.wfile.write(json.dumps(resp_payload).encode("utf-8"))
        else:
            self._set_headers(404)
            self.wfile.write(json.dumps({"error": "Endpoint not found"}).encode("utf-8"))

    def log_message(self, format, *args):
        sys.stderr.write(f"[{self.log_date_time_string()}] {format % args}\n")


def run():
    server = HTTPServer((HOST, PORT), DPORequestHandler)
    print(f"[✓] Serving ZKAEDI 3D DPO API on http://{HOST}:{PORT}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[!] Server stopped by user.")
    finally:
        server.server_close()


if __name__ == "__main__":
    run()
