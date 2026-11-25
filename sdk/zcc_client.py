"""Zero-dependency Python stdlib client & CLI for the ZKAEDI ZCC Sovereign Blackwell AI API."""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.parse
import urllib.request
from typing import Any, Dict, Iterator, List, Optional, Union


class ZccApiError(RuntimeError):
    """Raised when the ZCC Sovereign Blackwell Studio server returns HTTP >= 400."""

    def __init__(self, status: int, body: str, payload: Optional[Dict[str, Any]] = None) -> None:
        self.status = int(status)
        self.body = body
        self.payload = payload if isinstance(payload, dict) else {}
        msg = self.payload.get("error", {}).get("message") if isinstance(self.payload.get("error"), dict) else None
        super().__init__(f"HTTP {self.status}: {msg or body}")


class ZccClient:
    """Synchronous zero-dependency HTTP/SSE client for `win64_gpu_lora_studio`."""

    def __init__(
        self,
        base_url: str = "http://127.0.0.1:8080",
        api_key: Optional[str] = None,
        auth_header: str = "authorization",
        timeout: float = 120.0,
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.api_key = api_key
        self.auth_header = auth_header.lower()
        self.timeout = float(timeout)

    def _headers(self, json_body: bool = False) -> Dict[str, str]:
        hdrs: Dict[str, str] = {"Accept": "application/json, text/event-stream, text/plain"}
        if json_body:
            hdrs["Content-Type"] = "application/json"
        if self.api_key:
            if self.auth_header == "x-api-key":
                hdrs["x-api-key"] = self.api_key
            else:
                hdrs["Authorization"] = f"Bearer {self.api_key}"
        return hdrs

    def _request_raw(
        self,
        method: str,
        path: str,
        payload: Optional[Dict[str, Any]] = None,
    ) -> str:
        url = f"{self.base_url}{path}"
        data = json.dumps(payload).encode("utf-8") if payload is not None else None
        req = urllib.request.Request(
            url,
            data=data,
            headers=self._headers(json_body=(payload is not None)),
            method=method.upper(),
        )
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                return resp.read().decode("utf-8", errors="replace")
        except urllib.error.HTTPError as exc:
            err_body = exc.read().decode("utf-8", errors="replace")
            parsed: Optional[Dict[str, Any]] = None
            try:
                parsed = json.loads(err_body)
            except Exception:
                parsed = None
            raise ZccApiError(exc.code, err_body, parsed) from exc

    def _request_json(
        self,
        method: str,
        path: str,
        payload: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        raw = self._request_raw(method, path, payload)
        return json.loads(raw)

    @staticmethod
    def _clean_dict(d: Dict[str, Any]) -> Dict[str, Any]:
        return {k: v for k, v in d.items() if v is not None}

    def health(self) -> Dict[str, Any]:
        """Fetch live GPU, model, VRAM, adapter, and CUDA Graph telemetry."""
        return self._request_json("GET", "/health")

    def metrics(self) -> str:
        """Fetch Prometheus 0.0.4 metrics exposition text."""
        return self._request_raw("GET", "/metrics")

    def openapi(self) -> Dict[str, Any]:
        """Fetch the server's OpenAPI 3.1.0 schema."""
        return self._request_json("GET", "/openapi.json")

    def models(self) -> Dict[str, Any]:
        """List loaded base model and hot-swappable LoRA model variants."""
        return self._request_json("GET", "/v1/models")

    def get_model(self, model_id: str) -> Dict[str, Any]:
        """Inspect a specific base model ID or `<model>:<adapter>` identifier."""
        encoded = urllib.parse.quote(model_id, safe=":")
        return self._request_json("GET", f"/v1/models/{encoded}")

    def chat(
        self,
        messages: List[Dict[str, Any]],
        adapter: Optional[str] = None,
        model: Optional[str] = None,
        max_tokens: int = 64,
        temperature: float = 0.0,
        top_k: int = 40,
        top_p: float = 0.95,
        min_p: float = 0.0,
        repeat_penalty: float = 1.0,
        presence_penalty: float = 0.0,
        frequency_penalty: float = 0.0,
        seed: int = 42,
        lora_scale: Optional[float] = None,
        stop: Optional[Union[str, List[str]]] = None,
        logprobs: bool = False,
        top_logprobs: int = 0,
        response_format: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Execute a non-streaming OpenAI Chat Completions request."""
        body = self._clean_dict(
            {
                "messages": messages,
                "adapter": adapter,
                "model": model,
                "max_tokens": max_tokens,
                "temperature": temperature,
                "top_k": top_k,
                "top_p": top_p,
                "min_p": min_p,
                "repeat_penalty": repeat_penalty,
                "presence_penalty": presence_penalty,
                "frequency_penalty": frequency_penalty,
                "seed": seed,
                "lora_scale": lora_scale,
                "stop": stop,
                "logprobs": True if logprobs else None,
                "top_logprobs": top_logprobs if logprobs and top_logprobs > 0 else None,
                "response_format": response_format,
                "stream": False,
            }
        )
        return self._request_json("POST", "/v1/chat/completions", body)

    def chat_stream(
        self,
        messages: List[Dict[str, Any]],
        adapter: Optional[str] = None,
        model: Optional[str] = None,
        max_tokens: int = 64,
        temperature: float = 0.0,
        top_k: int = 40,
        top_p: float = 0.95,
        min_p: float = 0.0,
        repeat_penalty: float = 1.0,
        presence_penalty: float = 0.0,
        frequency_penalty: float = 0.0,
        seed: int = 42,
        lora_scale: Optional[float] = None,
        stop: Optional[Union[str, List[str]]] = None,
        include_usage: bool = True,
    ) -> Iterator[Dict[str, Any]]:
        """Stream OpenAI Chat Completions Server-Sent Event chunks."""
        body = self._clean_dict(
            {
                "messages": messages,
                "adapter": adapter,
                "model": model,
                "max_tokens": max_tokens,
                "temperature": temperature,
                "top_k": top_k,
                "top_p": top_p,
                "min_p": min_p,
                "repeat_penalty": repeat_penalty,
                "presence_penalty": presence_penalty,
                "frequency_penalty": frequency_penalty,
                "seed": seed,
                "lora_scale": lora_scale,
                "stop": stop,
                "stream": True,
                "stream_options": {"include_usage": True} if include_usage else None,
            }
        )
        url = f"{self.base_url}/v1/chat/completions"
        req = urllib.request.Request(
            url,
            data=json.dumps(body).encode("utf-8"),
            headers=self._headers(json_body=True),
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                for raw_line in resp:
                    line = raw_line.decode("utf-8", errors="replace").strip()
                    if not line.startswith("data:"):
                        continue
                    data_str = line[5:].strip()
                    if data_str == "[DONE]":
                        break
                    if data_str:
                        yield json.loads(data_str)
        except urllib.error.HTTPError as exc:
            err_body = exc.read().decode("utf-8", errors="replace")
            parsed: Optional[Dict[str, Any]] = None
            try:
                parsed = json.loads(err_body)
            except Exception:
                parsed = None
            raise ZccApiError(exc.code, err_body, parsed) from exc

    def complete(
        self,
        prompt: str,
        adapter: Optional[str] = None,
        model: Optional[str] = None,
        max_tokens: int = 64,
        temperature: float = 0.0,
        top_k: int = 40,
        top_p: float = 0.95,
        min_p: float = 0.0,
        repeat_penalty: float = 1.0,
        presence_penalty: float = 0.0,
        frequency_penalty: float = 0.0,
        seed: int = 42,
        lora_scale: Optional[float] = None,
        stop: Optional[Union[str, List[str]]] = None,
        logprobs: bool = False,
        top_logprobs: int = 0,
    ) -> Dict[str, Any]:
        """Execute an OpenAI Text Completions request (`/v1/completions`)."""
        body = self._clean_dict(
            {
                "prompt": prompt,
                "adapter": adapter,
                "model": model,
                "max_tokens": max_tokens,
                "temperature": temperature,
                "top_k": top_k,
                "top_p": top_p,
                "min_p": min_p,
                "repeat_penalty": repeat_penalty,
                "presence_penalty": presence_penalty,
                "frequency_penalty": frequency_penalty,
                "seed": seed,
                "lora_scale": lora_scale,
                "stop": stop,
                "logprobs": True if logprobs else None,
                "top_logprobs": top_logprobs if logprobs and top_logprobs > 0 else None,
                "stream": False,
            }
        )
        return self._request_json("POST", "/v1/completions", body)

    def messages(
        self,
        messages: List[Dict[str, Any]],
        system: Optional[str] = None,
        adapter: Optional[str] = None,
        model: Optional[str] = None,
        max_tokens: int = 64,
        temperature: float = 0.0,
        stop_sequences: Optional[List[str]] = None,
        lora_scale: Optional[float] = None,
    ) -> Dict[str, Any]:
        """Execute an Anthropic Messages API request (`/v1/messages`)."""
        body = self._clean_dict(
            {
                "messages": messages,
                "system": system,
                "adapter": adapter,
                "model": model,
                "max_tokens": max_tokens,
                "temperature": temperature,
                "stop_sequences": stop_sequences,
                "lora_scale": lora_scale,
                "stream": False,
            }
        )
        return self._request_json("POST", "/v1/messages", body)

    def responses(
        self,
        input_data: Union[str, List[Dict[str, Any]]],
        instructions: Optional[str] = None,
        adapter: Optional[str] = None,
        model: Optional[str] = None,
        max_output_tokens: int = 64,
        temperature: float = 0.0,
        lora_scale: Optional[float] = None,
    ) -> Dict[str, Any]:
        """Execute an OpenAI Responses API request (`/v1/responses`)."""
        body = self._clean_dict(
            {
                "input": input_data,
                "instructions": instructions,
                "adapter": adapter,
                "model": model,
                "max_output_tokens": max_output_tokens,
                "temperature": temperature,
                "lora_scale": lora_scale,
                "stream": False,
            }
        )
        return self._request_json("POST", "/v1/responses", body)

    def embeddings(
        self,
        input_data: Union[str, List[str]],
        adapter: Optional[str] = None,
        model: Optional[str] = None,
        dimensions: Optional[int] = None,
        lora_scale: Optional[float] = None,
    ) -> Dict[str, Any]:
        """Compute L2-normalized GPU RMSNorm mean-pooled embeddings (`/v1/embeddings`)."""
        body = self._clean_dict(
            {
                "input": input_data,
                "adapter": adapter,
                "model": model,
                "dimensions": dimensions,
                "lora_scale": lora_scale,
            }
        )
        return self._request_json("POST", "/v1/embeddings", body)

    def tokenize(self, text: str, add_bos: bool = False) -> Dict[str, Any]:
        """Tokenize UTF-8 text into model token IDs and decoded pieces."""
        return self._request_json("POST", "/v1/tokenize", {"text": text, "add_bos": bool(add_bos)})

    def detokenize(self, tokens: List[int]) -> Dict[str, Any]:
        """Decode model token IDs back into a UTF-8 string."""
        return self._request_json("POST", "/v1/detokenize", {"tokens": [int(t) for t in tokens]})

    def batch(self, requests: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Execute up to 32 sequential multi-cartridge requests on GPU (`/v1/batch`)."""
        return self._request_json("POST", "/v1/batch", {"requests": requests})

    def adapters(self) -> Dict[str, Any]:
        """List loaded LoRA cartridges in VRAM."""
        return self._request_json("GET", "/v1/lora/adapters")

    def get_adapter(self, name: str) -> Dict[str, Any]:
        """Inspect metadata for a single loaded LoRA cartridge."""
        return self._request_json("GET", f"/v1/lora/adapters/{urllib.parse.quote(name, safe='')}")

    def delete_adapter(self, name: str) -> Dict[str, Any]:
        """Unload a LoRA cartridge by name via `DELETE /v1/lora/adapters/{name}`."""
        return self._request_json("DELETE", f"/v1/lora/adapters/{urllib.parse.quote(name, safe='')}")

    def lora_files(self) -> Dict[str, Any]:
        """List `.zcc_lora` cartridges on disk in `--lora-dir`."""
        return self._request_json("GET", "/v1/lora/files")

    def select_adapter(self, name: str) -> Dict[str, Any]:
        """Switch the default active LoRA cartridge (or `base`)."""
        return self._request_json("POST", "/v1/lora/select", {"name": name})

    def scale_adapter(self, name: str, scale: float) -> Dict[str, Any]:
        """Adjust live dynamic LoRA scaling multiplier (`0.0..4.0`)."""
        return self._request_json("POST", "/v1/lora/scale", {"name": name, "scale": float(scale)})

    def load_adapter(self, name: str, path: str, activate: bool = True) -> Dict[str, Any]:
        """Load a verified `.zcc_lora` file from disk into VRAM."""
        return self._request_json(
            "POST",
            "/v1/lora/load",
            {"name": name, "path": path, "activate": bool(activate)},
        )

    def save_adapter(self, name: str, path: Optional[str] = None) -> Dict[str, Any]:
        """Save an in-VRAM LoRA cartridge to disk as `.zcc_lora`."""
        body = self._clean_dict({"name": name, "path": path})
        return self._request_json("POST", "/v1/lora/save", body)

    def unload_adapter(self, name: str) -> Dict[str, Any]:
        """Unload a LoRA cartridge via `POST /v1/lora/unload`."""
        return self._request_json("POST", "/v1/lora/unload", {"name": name})

    def train_lora(
        self,
        name: str,
        sft_text: str,
        rank: int = 8,
        alpha: float = 16.0,
        lr: float = 5e-4,
        steps: int = 40,
        seed: int = 42,
        chat: bool = True,
        activate: bool = True,
        save_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Train a sovereign GPU LoRA cartridge over Q:/A: SFT examples."""
        body = self._clean_dict(
            {
                "name": name,
                "sft_text": sft_text,
                "rank": int(rank),
                "alpha": float(alpha),
                "lr": float(lr),
                "steps": int(steps),
                "seed": int(seed),
                "chat": bool(chat),
                "activate": bool(activate),
                "save_path": save_path,
                "stream": False,
            }
        )
        return self._request_json("POST", "/v1/lora/train", body)


def _cli(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description="ZKAEDI ZCC Sovereign Blackwell AI Python SDK CLI")
    parser.add_argument("--url", default="http://127.0.0.1:8080", help="Studio server base URL")
    parser.add_argument("--api-key", default=None, help="Optional API key (Bearer or x-api-key)")
    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("health", help="Query GET /health")
    sub.add_parser("models", help="Query GET /v1/models")
    p_chat = sub.add_parser("chat", help="Send a chat completion request")
    p_chat.add_argument("prompt")
    p_chat.add_argument("--adapter", default="base")
    p_chat.add_argument("--max-tokens", type=int, default=64)
    p_emb = sub.add_parser("embed", help="Compute embeddings for text")
    p_emb.add_argument("text")
    p_emb.add_argument("--dims", type=int, default=None)
    p_tok = sub.add_parser("tokenize", help="Tokenize text")
    p_tok.add_argument("text")

    ns = parser.parse_args(argv)
    client = ZccClient(base_url=ns.url, api_key=ns.api_key)
    if ns.cmd == "health":
        res = client.health()
    elif ns.cmd == "models":
        res = client.models()
    elif ns.cmd == "chat":
        res = client.chat([{"role": "user", "content": ns.prompt}], adapter=ns.adapter, max_tokens=ns.max_tokens)
    elif ns.cmd == "embed":
        res = client.embeddings(ns.text, dimensions=ns.dims)
    else:
        res = client.tokenize(ns.text)
    print(json.dumps(res, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(_cli(sys.argv[1:]))
