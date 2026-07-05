# JSONL Cheat Guide — 10 Domains × 5 Examples

## 1. Syntax Invariants (self-demonstrating — each line IS the rule)
```jsonl
{"rule": 1, "text": "one complete JSON object per line, parseable in isolation"}
{"rule": 2, "text": "UTF-8, no BOM, no wrapping [ ] array, no commas between lines"}
{"rule": 3, "nested": {"ok": true}, "arrays": [1, 2, 3], "text": "any depth inside a line is fine"}
{"rule": 4, "text": "newlines inside strings must be escaped like this:\nnever literal"}
{"rule": 5, "text": "no comments, no trailing commas, terminate file with newline"}
```

## 2. Fine-Tuning — Chat (OpenAI supervised format)
```jsonl
{"messages": [{"role": "system", "content": "You are ZKAEDI Prime, an evidence-first systems engineer."}, {"role": "user", "content": "Worker times out on cold start."}, {"role": "assistant", "content": "Verified: no lazy init in index.ts. Patch: defer WASM load to first request. Rollback: revert single import."}]}
{"messages": [{"role": "user", "content": "What does D1 use under the hood?"}, {"role": "assistant", "content": "SQLite, replicated at the edge."}, {"role": "user", "content": "Transactions?"}, {"role": "assistant", "content": "Yes — batched statements execute atomically per session."}]}
{"messages": [{"role": "system", "content": "Rewrite user text as a commit message."}, {"role": "user", "content": "fixed the thing where retries never stopped"}, {"role": "assistant", "content": "fix(retry): bound retry loop with max-attempts and jittered backoff"}]}
{"messages": [{"role": "user", "content": "Just guess whether prod is down."}, {"role": "assistant", "content": "I can't assert runtime state without evidence. Share health-check output or logs and I'll diagnose."}]}
{"messages": [{"role": "user", "content": "Rate limit approach?"}, {"role": "assistant", "content": "Global mutex per user.", "weight": 0}, {"role": "user", "content": "That serializes everything."}, {"role": "assistant", "content": "Correct — use a token bucket per key instead; lock-free and bounded.", "weight": 1}]}
```

## 3. Fine-Tuning — Tool Calling
```jsonl
{"messages": [{"role": "user", "content": "Weather in Oslo?"}, {"role": "assistant", "tool_calls": [{"id": "call_1", "type": "function", "function": {"name": "get_weather", "arguments": "{\"city\": \"Oslo\"}"}}]}], "tools": [{"type": "function", "function": {"name": "get_weather", "parameters": {"type": "object", "properties": {"city": {"type": "string"}}, "required": ["city"]}}}]}
{"messages": [{"role": "user", "content": "Weather in Oslo?"}, {"role": "assistant", "tool_calls": [{"id": "call_1", "type": "function", "function": {"name": "get_weather", "arguments": "{\"city\": \"Oslo\"}"}}]}, {"role": "tool", "tool_call_id": "call_1", "content": "{\"temp_c\": 14}"}, {"role": "assistant", "content": "Oslo is 14°C."}]}
{"messages": [{"role": "user", "content": "Compare Oslo and Bergen."}, {"role": "assistant", "tool_calls": [{"id": "c1", "type": "function", "function": {"name": "get_weather", "arguments": "{\"city\": \"Oslo\"}"}}, {"id": "c2", "type": "function", "function": {"name": "get_weather", "arguments": "{\"city\": \"Bergen\"}"}}]}]}
{"messages": [{"role": "user", "content": "What is 2+2?"}, {"role": "assistant", "content": "4 — no tool needed."}]}
{"messages": [{"role": "assistant", "tool_calls": [{"id": "c9", "type": "function", "function": {"name": "getfile", "arguments": "{\"path\": \"wrangler.toml\"}"}}]}, {"role": "tool", "tool_call_id": "c9", "content": "{\"error\": \"not found\"}"}, {"role": "assistant", "content": "wrangler.toml missing — checking for wrangler.json fallback before concluding config absence."}]}
```

## 4. Evals / Graders
```jsonl
{"item": {"input": "SELECT count(*) FROM users;", "expected": "42"}}
{"item": {"text": "refund never arrived, third email now", "label": "complaint"}}
{"item": {"question": "Which store is strongly consistent?", "choices": ["KV", "D1", "R2"], "answer": "D1"}}
{"item": {"prompt": "Explain the outage fix.", "rubric": "Must state root cause, cite evidence, include rollback step."}}
{"item": {"input": "parse '2026-07-05'", "expected": "1751673600", "meta": {"bug_id": "issue-412", "regression": true}}}
```

## 5. Batch API (request lines → result lines)
```jsonl
{"custom_id": "task-001", "method": "POST", "url": "/v1/chat/completions", "body": {"model": "gpt-4o", "messages": [{"role": "user", "content": "Summarize: ..."}]}}
{"custom_id": "task-002", "method": "POST", "url": "/v1/embeddings", "body": {"model": "text-embedding-3-small", "input": "rollback procedure for edge deploys"}}
{"custom_id": "task-003", "method": "POST", "url": "/v1/chat/completions", "body": {"model": "gpt-4o", "temperature": 0, "max_tokens": 200, "messages": [{"role": "user", "content": "Classify severity: ..."}]}}
{"custom_id": "task-004", "method": "POST", "url": "/v1/chat/completions", "body": {"model": "gpt-4o", "response_format": {"type": "json_schema", "json_schema": {"name": "triage", "schema": {"type": "object", "properties": {"severity": {"type": "string"}}, "required": ["severity"]}}}, "messages": [{"role": "user", "content": "..."}]}}
{"id": "batch_req_9k2", "custom_id": "task-001", "response": {"status_code": 200, "body": {"choices": [{"message": {"role": "assistant", "content": "..."}}]}}, "error": null}
```

## 6. Structured Logs / Telemetry
```jsonl
{"ts": "2026-07-05T14:03:07Z", "level": "info", "msg": "request completed", "method": "GET", "path": "/health", "status": 200, "dur_ms": 12, "corr_id": "r-9f3"}
{"ts": "2026-07-05T14:03:09Z", "level": "error", "msg": "retry exhausted", "svc": "sync-worker", "err": "ETIMEDOUT", "attempt": 3, "max": 3, "corr_id": "r-9f4"}
{"ts": "2026-07-05T14:03:10Z", "metric": "queue_depth", "value": 1287, "unit": "msgs", "svc": "ingest"}
{"ts": "2026-07-05T14:03:11Z", "actor": "izkaedi-ui", "action": "secret.rotate", "target": "CF_API_TOKEN", "result": "success"}
{"ts": "2026-07-05T14:03:12Z", "level": "warn", "msg": "circuit breaker opened", "svc": "payments", "from": "half-open", "to": "open", "window_s": 30}
```

## 7. ML Dataset Records
```jsonl
{"text": "deploy failed with exit 137", "label": "oom"}
{"text": "Deploy to the Oslo region tonight", "entities": [{"start": 14, "end": 18, "label": "LOC"}]}
{"text": "Workers execute V8 isolates at the edge...", "source": "docs/architecture.md", "tokens": 412}
{"prompt": "Explain idempotency keys", "chosen": "An idempotency key lets a retried request return the original result instead of re-executing...", "rejected": "It makes things idempotent."}
{"id": "q-88", "context": "R2 has no egress fees...", "question": "What fee does R2 waive?", "answers": [{"text": "egress", "answer_start": 10}]}
```

## 8. Embeddings / Vector Stores
```jsonl
{"id": "doc-1", "text": "rollback procedure for edge deploys", "embedding": [0.021, -0.114, 0.087]}
{"id": "doc-2", "values": [0.33, -0.08, 0.51], "metadata": {"path": "src/gateway.ts", "lang": "ts", "sha": "9f31c2e"}}
{"id": "readme#c3", "parent": "README.md", "chunk": 3, "of": 7, "text": "## Deployment...", "embedding": [0.05, 0.12, -0.3]}
{"q": "how to rotate secrets", "top_k": 5, "hits": ["doc-2", "doc-7", "doc-1"], "latency_ms": 18}
{"id": "doc-9", "op": "delete", "reason": "superseded by doc-12"}
```

## 9. Agent Traces / Event Streams
```jsonl
{"seq": 1, "event": "session.start", "session_id": "s-01", "agent": "zkaedi-prime", "ts": "2026-07-05T15:00:00Z"}
{"seq": 2, "event": "tool.call", "session_id": "s-01", "name": "getfile", "args": {"path": "wrangler.toml"}}
{"seq": 3, "event": "tool.result", "session_id": "s-01", "call_seq": 2, "ok": true, "bytes": 1832}
{"seq": 4, "event": "message.delta", "session_id": "s-01", "role": "assistant", "content": "Verified: config sets "}
{"seq": 5, "event": "session.end", "session_id": "s-01", "status": "complete", "turns": 4, "tokens": {"in": 1200, "out": 480}}
```

## 10. Tooling One-Liners
```bash
jq -c . file.jsonl > /dev/null && echo VALID          # validate every line
jq -c '.[]' array.json > records.jsonl                # JSON array → JSONL
jq -s . records.jsonl > array.json                    # JSONL → JSON array
jq -c 'select(.level == "error")' logs.jsonl          # filter stream
shuf data.jsonl > s.jsonl && head -n 900 s.jsonl > train.jsonl && tail -n 100 s.jsonl > val.jsonl   # train/val split
```
```python
rows = [json.loads(l) for l in open("f.jsonl") if l.strip()]   # python read
df = pd.read_json("f.jsonl", lines=True)                        # pandas read
```
