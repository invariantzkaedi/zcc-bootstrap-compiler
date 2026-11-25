/**
 * Zero-dependency ES Module SDK for the ZKAEDI ZCC Sovereign Blackwell AI Studio API.
 * Compatible with Node 18+ and modern browsers using native fetch.
 */

export class ZccApiError extends Error {
  /**
   * @param {number} status
   * @param {string} body
   * @param {object | null} [payload]
   */
  constructor(status, body, payload = null) {
    const msg = payload && payload.error && payload.error.message ? payload.error.message : body;
    super(`HTTP ${status}: ${msg}`);
    this.name = "ZccApiError";
    this.status = status;
    this.body = body;
    this.payload = payload || {};
  }
}

function cleanObject(obj) {
  const out = {};
  for (const [k, v] of Object.entries(obj)) {
    if (v !== undefined && v !== null) out[k] = v;
  }
  return out;
}

export class ZccClient {
  /**
   * @param {object} [opts]
   * @param {string} [opts.baseUrl]
   * @param {string | null} [opts.apiKey]
   * @param {"authorization" | "x-api-key"} [opts.authHeader]
   * @param {number} [opts.timeoutMs]
   */
  constructor(opts = {}) {
    const {
      baseUrl = "http://127.0.0.1:8080",
      apiKey = null,
      authHeader = "authorization",
      timeoutMs = 120000
    } = typeof opts === "string" ? { baseUrl: opts } : opts;
    this.baseUrl = String(baseUrl).replace(/\/+$/, "");
    this.apiKey = apiKey || null;
    this.authHeader = String(authHeader).toLowerCase();
    this.timeoutMs = Number(timeoutMs) || 120000;
  }

  _headers(hasJsonBody = false) {
    const hdrs = { Accept: "application/json, text/event-stream, text/plain" };
    if (hasJsonBody) hdrs["Content-Type"] = "application/json";
    if (this.apiKey) {
      if (this.authHeader === "x-api-key") {
        hdrs["x-api-key"] = this.apiKey;
      } else {
        hdrs["Authorization"] = `Bearer ${this.apiKey}`;
      }
    }
    return hdrs;
  }

  async _fetch(method, path, payload = undefined) {
    const hasBody = payload !== undefined;
    const ctrl = typeof AbortController !== "undefined" ? new AbortController() : null;
    const timer = ctrl ? setTimeout(() => ctrl.abort(), this.timeoutMs) : null;
    try {
      const res = await fetch(`${this.baseUrl}${path}`, {
        method,
        headers: this._headers(hasBody),
        body: hasBody ? JSON.stringify(payload) : undefined,
        signal: ctrl ? ctrl.signal : undefined
      });
      if (!res.ok) {
        const text = await res.text();
        let parsed = null;
        try {
          parsed = JSON.parse(text);
        } catch {
          parsed = null;
        }
        throw new ZccApiError(res.status, text, parsed);
      }
      return res;
    } finally {
      if (timer) clearTimeout(timer);
    }
  }

  async _requestJson(method, path, payload = undefined) {
    const res = await this._fetch(method, path, payload);
    return res.json();
  }

  async health() {
    return this._requestJson("GET", "/health");
  }

  async metrics() {
    const res = await this._fetch("GET", "/metrics");
    return res.text();
  }

  async openapi() {
    return this._requestJson("GET", "/openapi.json");
  }

  async models() {
    return this._requestJson("GET", "/v1/models");
  }

  async getModel(modelId) {
    const encoded = encodeURIComponent(modelId).replace(/%3A/g, ":");
    return this._requestJson("GET", `/v1/models/${encoded}`);
  }

  async chat({
    messages,
    adapter = undefined,
    model = undefined,
    max_tokens = 64,
    temperature = 0.0,
    top_k = 40,
    top_p = 0.95,
    min_p = 0.0,
    repeat_penalty = 1.0,
    presence_penalty = 0.0,
    frequency_penalty = 0.0,
    seed = 42,
    lora_scale = undefined,
    stop = undefined,
    logprobs = false,
    top_logprobs = 0,
    response_format = undefined
  }) {
    const body = cleanObject({
      messages,
      adapter,
      model,
      max_tokens,
      temperature,
      top_k,
      top_p,
      min_p,
      repeat_penalty,
      presence_penalty,
      frequency_penalty,
      seed,
      lora_scale,
      stop,
      logprobs: logprobs ? true : undefined,
      top_logprobs: logprobs && top_logprobs > 0 ? top_logprobs : undefined,
      response_format,
      stream: false
    });
    return this._requestJson("POST", "/v1/chat/completions", body);
  }

  async *chatStream({
    messages,
    adapter = undefined,
    model = undefined,
    max_tokens = 64,
    temperature = 0.0,
    top_k = 40,
    top_p = 0.95,
    min_p = 0.0,
    repeat_penalty = 1.0,
    presence_penalty = 0.0,
    frequency_penalty = 0.0,
    seed = 42,
    lora_scale = undefined,
    stop = undefined,
    include_usage = true
  }) {
    const body = cleanObject({
      messages,
      adapter,
      model,
      max_tokens,
      temperature,
      top_k,
      top_p,
      min_p,
      repeat_penalty,
      presence_penalty,
      frequency_penalty,
      seed,
      lora_scale,
      stop,
      stream: true,
      stream_options: include_usage ? { include_usage: true } : undefined
    });
    const res = await this._fetch("POST", "/v1/chat/completions", body);
    const reader = res.body.getReader();
    const decoder = new TextDecoder();
    let buf = "";
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      buf += decoder.decode(value, { stream: true });
      const lines = buf.split(/\r?\n/);
      buf = lines.pop() || "";
      for (const rawLine of lines) {
        const line = rawLine.trim();
        if (!line.startsWith("data:")) continue;
        const dataStr = line.slice(5).trim();
        if (dataStr === "[DONE]") return;
        if (dataStr) yield JSON.parse(dataStr);
      }
    }
  }

  async complete({
    prompt,
    adapter = undefined,
    model = undefined,
    max_tokens = 64,
    temperature = 0.0,
    top_k = 40,
    top_p = 0.95,
    min_p = 0.0,
    repeat_penalty = 1.0,
    presence_penalty = 0.0,
    frequency_penalty = 0.0,
    seed = 42,
    lora_scale = undefined,
    stop = undefined,
    logprobs = false,
    top_logprobs = 0
  }) {
    const body = cleanObject({
      prompt,
      adapter,
      model,
      max_tokens,
      temperature,
      top_k,
      top_p,
      min_p,
      repeat_penalty,
      presence_penalty,
      frequency_penalty,
      seed,
      lora_scale,
      stop,
      logprobs: logprobs ? true : undefined,
      top_logprobs: logprobs && top_logprobs > 0 ? top_logprobs : undefined,
      stream: false
    });
    return this._requestJson("POST", "/v1/completions", body);
  }

  async messages({
    messages,
    system = undefined,
    adapter = undefined,
    model = undefined,
    max_tokens = 64,
    temperature = 0.0,
    stop_sequences = undefined,
    lora_scale = undefined
  }) {
    const body = cleanObject({
      messages,
      system,
      adapter,
      model,
      max_tokens,
      temperature,
      stop_sequences,
      lora_scale,
      stream: false
    });
    return this._requestJson("POST", "/v1/messages", body);
  }

  async responses({
    input,
    instructions = undefined,
    adapter = undefined,
    model = undefined,
    max_output_tokens = 64,
    temperature = 0.0,
    lora_scale = undefined
  }) {
    const body = cleanObject({
      input,
      instructions,
      adapter,
      model,
      max_output_tokens,
      temperature,
      lora_scale,
      stream: false
    });
    return this._requestJson("POST", "/v1/responses", body);
  }

  async embeddings({
    input,
    adapter = undefined,
    model = undefined,
    dimensions = undefined,
    lora_scale = undefined
  }) {
    const body = cleanObject({ input, adapter, model, dimensions, lora_scale });
    return this._requestJson("POST", "/v1/embeddings", body);
  }

  async tokenize(text, add_bos = false) {
    return this._requestJson("POST", "/v1/tokenize", { text, add_bos: Boolean(add_bos) });
  }

  async detokenize(tokens) {
    return this._requestJson("POST", "/v1/detokenize", { tokens: tokens.map(Number) });
  }

  async batch(requests) {
    return this._requestJson("POST", "/v1/batch", { requests });
  }

  async adapters() {
    return this._requestJson("GET", "/v1/lora/adapters");
  }

  async getAdapter(name) {
    return this._requestJson("GET", `/v1/lora/adapters/${encodeURIComponent(name)}`);
  }

  async deleteAdapter(name) {
    return this._requestJson("DELETE", `/v1/lora/adapters/${encodeURIComponent(name)}`);
  }

  async loraFiles() {
    return this._requestJson("GET", "/v1/lora/files");
  }

  async selectAdapter(name) {
    return this._requestJson("POST", "/v1/lora/select", { name });
  }

  async scaleAdapter(name, scale) {
    return this._requestJson("POST", "/v1/lora/scale", { name, scale: Number(scale) });
  }

  async loadAdapter(name, path, activate = true) {
    return this._requestJson("POST", "/v1/lora/load", { name, path, activate: Boolean(activate) });
  }

  async saveAdapter(name, path = undefined) {
    return this._requestJson("POST", "/v1/lora/save", cleanObject({ name, path }));
  }

  async unloadAdapter(name) {
    return this._requestJson("POST", "/v1/lora/unload", { name });
  }

  async trainLora({
    name,
    sft_text,
    rank = 8,
    alpha = 16.0,
    lr = 5e-4,
    steps = 40,
    seed = 42,
    chat = true,
    activate = true,
    save_path = undefined
  }) {
    const body = cleanObject({
      name,
      sft_text,
      rank,
      alpha,
      lr,
      steps,
      seed,
      chat,
      activate,
      save_path,
      stream: false
    });
    return this._requestJson("POST", "/v1/lora/train", body);
  }
}

export default ZccClient;
