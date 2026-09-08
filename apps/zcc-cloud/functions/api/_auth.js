// apps/zcc-cloud/functions/api/_auth.js
// Cryptographic API Key Verification, Rate Limiting, & Resource Protection

export const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization, X-API-Key",
  "Content-Type": "application/json;charset=utf-8"
};

export const MAX_BODY_BYTES = 65536; // 64 KB strict ceiling

// Sliding window parameters
const WINDOW_MS = 60 * 1000; // 1 minute
const DAY_MS = 24 * 60 * 60 * 1000;
const DEFAULT_LIMIT_PER_MINUTE = 60;
const DEFAULT_LIMIT_PER_DAY = 5000;

// Process-local fallback cache (used when cloud platform bindings are unconfigured)
const rateLimitCache = new Map();

/**
 * Obtain server HMAC secret.
 * Production MUST FAIL CLOSED if API_KEY_SECRET is missing or insufficiently random.
 */
function getSecret(env = {}) {
  const secret = env.API_KEY_SECRET;
  if (!secret || typeof secret !== "string" || secret.trim().length < 16) {
    return null;
  }
  return secret.trim();
}

/**
 * Get the HMAC CryptoKey
 */
async function getHmacKey(secretStr) {
  const enc = new TextEncoder();
  return await crypto.subtle.importKey(
    "raw",
    enc.encode(secretStr),
    { name: "HMAC", hash: "SHA-256" },
    false,
    ["sign", "verify"]
  );
}

/**
 * Constant-time hex string comparison
 */
export function timingSafeEqualHex(a, b) {
  if (!a || !b || a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) {
    diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  }
  return diff === 0;
}

/**
 * Enforce rate limits and quotas per identity.
 * Prioritizes:
 * 1. Cloudflare Platform Rate Limiting binding (env.RATE_LIMITER)
 * 2. Durable Objects (env.RATE_LIMIT_DO)
 * 3. Durable Atomic Storage via Cloudflare KV (env.RATELIMIT_KV || env.API_KEYS_KV || env.KV)
 * 4. Process-local sliding window cache (isolated test/dev fallback)
 */
export async function enforceRateLimitAndQuota(identifier, tier = "developer_sandbox", env = {}) {
  const now = Date.now();
  const limitPerMin = tier === "enterprise" ? 300 : DEFAULT_LIMIT_PER_MINUTE;
  const limitPerDay = tier === "enterprise" ? 50000 : DEFAULT_LIMIT_PER_DAY;

  // 1. Cloudflare Platform Rate Limiting Binding
  if (env.RATE_LIMITER && typeof env.RATE_LIMITER.limit === "function") {
    try {
      const rlResult = await env.RATE_LIMITER.limit({ key: identifier });
      if (rlResult && rlResult.success === false) {
        return {
          allowed: false,
          code: "RATE_LIMIT_EXCEEDED",
          error: `Platform rate limit of ${limitPerMin} requests per minute exceeded.`,
          retryAfter: 60,
          headers: {
            "X-RateLimit-Limit": String(limitPerMin),
            "X-RateLimit-Remaining": "0",
            "X-RateLimit-Reset": "60",
            "Retry-After": "60"
          }
        };
      }
    } catch (e) {
      console.warn("Platform rate limiter error:", e);
    }
  }

  // 2. Durable Objects for strict serializable linearizable counting
  if (env.RATE_LIMIT_DO && typeof env.RATE_LIMIT_DO.idFromName === "function") {
    try {
      const doId = env.RATE_LIMIT_DO.idFromName(identifier);
      const doStub = env.RATE_LIMIT_DO.get(doId);
      const doRes = await doStub.fetch("https://ratelimit.internal/check", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ identifier, tier, limitPerMin, limitPerDay, now })
      });
      if (doRes.ok) {
        return await doRes.json();
      }
    } catch (e) {
      console.warn("Durable Object rate limit error:", e);
    }
  }

  // 3. Durable Atomic Storage via Cloudflare KV (survives isolate restarts and syncs across PoPs)
  const kv = env.RATELIMIT_KV || env.API_KEYS_KV || env.KV;
  if (kv && typeof kv.get === "function" && typeof kv.put === "function") {
    const minBucket = Math.floor(now / WINDOW_MS);
    const minKey = `rl:win:${identifier}:${minBucket}`;
    const dayKey = `rl:day:${identifier}:${new Date(now).toISOString().slice(0, 10)}`;

    const [minStr, dayStr] = await Promise.all([
      kv.get(minKey),
      kv.get(dayKey)
    ]);

    let minCount = minStr ? parseInt(minStr, 10) : 0;
    let dayCount = dayStr ? parseInt(dayStr, 10) : 0;
    const resetSec = Math.max(1, 60 - Math.floor((now % WINDOW_MS) / 1000));

    if (minCount >= limitPerMin) {
      return {
        allowed: false,
        code: "RATE_LIMIT_EXCEEDED",
        error: `Rate limit of ${limitPerMin} requests per minute exceeded.`,
        retryAfter: resetSec,
        headers: {
          "X-RateLimit-Limit": String(limitPerMin),
          "X-RateLimit-Remaining": "0",
          "X-RateLimit-Reset": String(resetSec),
          "Retry-After": String(resetSec)
        }
      };
    }

    if (dayCount >= limitPerDay) {
      return {
        allowed: false,
        code: "QUOTA_EXHAUSTED",
        error: `Daily quota of ${limitPerDay} requests exhausted.`,
        retryAfter: 3600,
        headers: {
          "X-RateLimit-Limit": String(limitPerMin),
          "X-RateLimit-Remaining": "0",
          "X-RateLimit-Reset": "3600",
          "Retry-After": "3600"
        }
      };
    }

    minCount++;
    dayCount++;

    await Promise.all([
      kv.put(minKey, String(minCount), { expirationTtl: 120 }),
      kv.put(dayKey, String(dayCount), { expirationTtl: 172800 })
    ]);

    const remainingMin = Math.max(0, limitPerMin - minCount);
    return {
      allowed: true,
      quota: {
        daily_limit: limitPerDay,
        used_today: dayCount,
        remaining: Math.max(0, limitPerDay - dayCount)
      },
      headers: {
        "X-RateLimit-Limit": String(limitPerMin),
        "X-RateLimit-Remaining": String(remainingMin),
        "X-RateLimit-Reset": String(resetSec)
      }
    };
  }

  // 4. In-Memory Sliding Window (Fallback for environments without persistent storage bindings)
  let entry = rateLimitCache.get(identifier);
  if (!entry) {
    entry = { count: 0, windowStart: now, dailyUsed: 0, dayStart: now };
    rateLimitCache.set(identifier, entry);
  }

  // Reset 1-minute window
  if (now - entry.windowStart >= WINDOW_MS) {
    entry.count = 0;
    entry.windowStart = now;
  }

  // Reset daily window
  if (now - entry.dayStart >= DAY_MS) {
    entry.dailyUsed = 0;
    entry.dayStart = now;
  }

  if (entry.count >= limitPerMin) {
    const retryAfterSec = Math.max(1, Math.ceil((entry.windowStart + WINDOW_MS - now) / 1000));
    return {
      allowed: false,
      code: "RATE_LIMIT_EXCEEDED",
      error: `Rate limit of ${limitPerMin} requests per minute exceeded.`,
      retryAfter: retryAfterSec,
      headers: {
        "X-RateLimit-Limit": String(limitPerMin),
        "X-RateLimit-Remaining": "0",
        "X-RateLimit-Reset": String(retryAfterSec),
        "Retry-After": String(retryAfterSec)
      }
    };
  }

  if (entry.dailyUsed >= limitPerDay) {
    return {
      allowed: false,
      code: "QUOTA_EXHAUSTED",
      error: `Daily quota of ${limitPerDay} requests exhausted.`,
      retryAfter: 3600,
      headers: {
        "X-RateLimit-Limit": String(limitPerMin),
        "X-RateLimit-Remaining": "0",
        "X-RateLimit-Reset": "3600",
        "Retry-After": "3600"
      }
    };
  }

  entry.count++;
  entry.dailyUsed++;

  const remainingMin = Math.max(0, limitPerMin - entry.count);
  const resetSec = Math.max(1, Math.ceil((entry.windowStart + WINDOW_MS - now) / 1000));

  return {
    allowed: true,
    quota: {
      daily_limit: limitPerDay,
      used_today: entry.dailyUsed,
      remaining: Math.max(0, limitPerDay - entry.dailyUsed)
    },
    headers: {
      "X-RateLimit-Limit": String(limitPerMin),
      "X-RateLimit-Remaining": String(remainingMin),
      "X-RateLimit-Reset": String(resetSec)
    }
  };
}

/**
 * Create a cryptographically signed API key.
 * Fails closed if API_KEY_SECRET is not configured.
 */
export async function generateSignedApiKey({ email = "developer@zkaedi.ai", tier = "developer_sandbox" }, env = {}) {
  const secret = getSecret(env);
  if (!secret) {
    throw new Error("Server authentication is unconfigured: API_KEY_SECRET environment variable is missing or less than 16 characters. Server fails closed.");
  }

  const hmacKey = await getHmacKey(secret);

  const payload = {
    id: Array.from(crypto.getRandomValues(new Uint8Array(12)), b => b.toString(16).padStart(2, '0')).join(''),
    email: email.trim().slice(0, 100),
    tier: tier.slice(0, 32),
    created: Date.now()
  };

  const payloadJson = JSON.stringify(payload);
  const payloadHex = Array.from(new TextEncoder().encode(payloadJson), b => b.toString(16).padStart(2, '0')).join('');

  const sigBuf = await crypto.subtle.sign("HMAC", hmacKey, new TextEncoder().encode(payloadHex));
  const sigHex = Array.from(new Uint8Array(sigBuf), b => b.toString(16).padStart(2, '0')).join('');

  const apiKey = `zk_live_${payloadHex}_${sigHex}`;

  // Persist to Cloudflare KV if binding exists
  if (env.API_KEYS_KV && typeof env.API_KEYS_KV.put === "function") {
    try {
      await env.API_KEYS_KV.put(`key:${apiKey}`, JSON.stringify({
        ...payload,
        status: "ACTIVE",
        requests_count: 0
      }), { expirationTtl: 365 * 24 * 3600 });
    } catch (e) {
      console.warn("Failed to persist key to KV:", e);
    }
  }

  return {
    apiKey,
    payload,
    expiresAt: new Date(payload.created + 365 * 24 * 3600 * 1000).toISOString()
  };
}

/**
 * Cryptographically verify an incoming API key.
 * Fails closed if API_KEY_SECRET is not configured.
 */
export async function verifyApiKey(key, env = {}) {
  const secret = getSecret(env);
  if (!secret) {
    return {
      valid: false,
      code: "SERVER_AUTH_UNCONFIGURED",
      error: "Server authentication is unconfigured: API_KEY_SECRET is missing. Server fails closed."
    };
  }

  if (!key || typeof key !== "string") {
    return { valid: false, code: "MISSING_KEY", error: "API key is required." };
  }

  const parts = key.trim().split("_");
  // Format must be: zk_live_<payloadHex>_<sigHex> or zk_test_<payloadHex>_<sigHex>
  if (parts.length !== 4 || (parts[0] !== "zk" || (parts[1] !== "live" && parts[1] !== "test"))) {
    return { valid: false, code: "MALFORMED_KEY", error: "API key structure is invalid. Prefix-only values are rejected." };
  }

  const payloadHex = parts[2];
  const sigHex = parts[3];

  if (!payloadHex || sigHex.length !== 64) {
    return { valid: false, code: "INVALID_SIGNATURE_LENGTH", error: "API key contains an invalid signature." };
  }

  const hmacKey = await getHmacKey(secret);
  const expectedSigBuf = await crypto.subtle.sign("HMAC", hmacKey, new TextEncoder().encode(payloadHex));
  const expectedSigHex = Array.from(new Uint8Array(expectedSigBuf), b => b.toString(16).padStart(2, '0')).join('');

  if (!timingSafeEqualHex(sigHex.toLowerCase(), expectedSigHex.toLowerCase())) {
    return { valid: false, code: "INVALID_SIGNATURE", error: "Cryptographic signature verification failed. Forged keys are rejected." };
  }

  // Parse payload
  let payload;
  try {
    const bytes = new Uint8Array(payloadHex.match(/.{1,2}/g).map(byte => parseInt(byte, 16)));
    payload = JSON.parse(new TextDecoder().decode(bytes));
  } catch {
    return { valid: false, code: "CORRUPTED_PAYLOAD", error: "Key payload is corrupted." };
  }

  // Check expiration (1 year default)
  const ageMs = Date.now() - (payload.created || 0);
  if (ageMs > 365 * 24 * 3600 * 1000) {
    return { valid: false, code: "EXPIRED_KEY", error: "API key has expired." };
  }

  // Check KV persistence if available
  if (env.API_KEYS_KV && typeof env.API_KEYS_KV.get === "function") {
    try {
      const kvRecord = await env.API_KEYS_KV.get(`key:${key}`, "json");
      if (kvRecord && kvRecord.status === "REVOKED") {
        return { valid: false, code: "KEY_REVOKED", error: "API key has been revoked." };
      }
    } catch (e) {
      console.warn("KV lookup error:", e);
    }
  }

  return {
    valid: true,
    user: {
      id: payload.id,
      email: payload.email,
      tier: payload.tier || "developer_sandbox",
      createdAt: new Date(payload.created).toISOString()
    }
  };
}

/**
 * Authenticate incoming HTTP request.
 * Enforces real rate limiting and daily quotas. Rejects unauthenticated requests on compute endpoints.
 */
export async function authenticateRequest(request, env = {}, { allowAnonymous = false } = {}) {
  const authHeader = request.headers.get("authorization") || request.headers.get("x-api-key") || "";
  const key = authHeader.replace(/^Bearer\s+/i, '').trim();

  if (!key) {
    if (!allowAnonymous) {
      return {
        authenticated: false,
        response: new Response(JSON.stringify({
          success: false,
          error: "Authentication required. Provide Authorization: Bearer <key> or X-API-Key header.",
          code: "UNAUTHORIZED",
          hint: "Obtain an authenticated key via POST /api/keys."
        }), {
          status: 401,
          headers: CORS_HEADERS
        })
      };
    }

    // Strictly bounded anonymous rate limit (e.g. for docs/status endpoints)
    const clientIp = request.headers.get("cf-connecting-ip") || request.headers.get("x-forwarded-for") || "anonymous";
    const anonCheck = await enforceRateLimitAndQuota(`anon:${clientIp}`, "anonymous_limited", env);
    if (!anonCheck.allowed) {
      return {
        authenticated: false,
        response: new Response(JSON.stringify({
          success: false,
          error: anonCheck.error,
          code: anonCheck.code
        }), {
          status: 429,
          headers: { ...CORS_HEADERS, ...anonCheck.headers }
        })
      };
    }

    return {
      authenticated: false,
      isSandboxAnonymous: true,
      user: { tier: "anonymous_limited", quota: anonCheck.quota },
      rateLimitHeaders: anonCheck.headers
    };
  }

  const verResult = await verifyApiKey(key, env);
  if (!verResult.valid) {
    const status = verResult.code === "SERVER_AUTH_UNCONFIGURED" ? 500 : 403;
    return {
      authenticated: false,
      response: new Response(JSON.stringify({
        success: false,
        error: verResult.error,
        code: verResult.code || "FORBIDDEN"
      }), {
        status,
        headers: CORS_HEADERS
      })
    };
  }

  // Enforce rate limits and quotas on authenticated key (with durable storage)
  const limitCheck = await enforceRateLimitAndQuota(verResult.user.id, verResult.user.tier, env);
  if (!limitCheck.allowed) {
    return {
      authenticated: false,
      response: new Response(JSON.stringify({
        success: false,
        error: limitCheck.error,
        code: limitCheck.code
      }), {
        status: 429,
        headers: { ...CORS_HEADERS, ...limitCheck.headers }
      })
    };
  }

  verResult.user.quota = limitCheck.quota;

  return {
    authenticated: true,
    user: verResult.user,
    rateLimitHeaders: limitCheck.headers
  };
}

/**
 * Enforce strict payload body size limit to prevent resource exhaustion attacks
 */
export async function readGuardedJsonBody(request, maxBytes = MAX_BODY_BYTES) {
  const contentLength = request.headers.get("content-length");
  if (contentLength && parseInt(contentLength, 10) > maxBytes) {
    return {
      errorResponse: new Response(JSON.stringify({
        success: false,
        error: `Payload exceeds maximum size limit of ${maxBytes} bytes (${maxBytes / 1024} KB).`,
        code: "PAYLOAD_TOO_LARGE"
      }), {
        status: 413,
        headers: CORS_HEADERS
      })
    };
  }

  const contentType = request.headers.get("content-type") || "";
  let text = "";
  try {
    text = await request.text();
  } catch (err) {
    return {
      errorResponse: new Response(JSON.stringify({
        success: false,
        error: "Failed to read request body.",
        code: "BODY_READ_ERROR"
      }), {
        status: 400,
        headers: CORS_HEADERS
      })
    };
  }

  if (text.length > maxBytes) {
    return {
      errorResponse: new Response(JSON.stringify({
        success: false,
        error: `Request body exceeds size limit of ${maxBytes} bytes.`,
        code: "PAYLOAD_TOO_LARGE"
      }), {
        status: 413,
        headers: CORS_HEADERS
      })
    };
  }

  let body = {};
  if (contentType.includes("application/json") || (text.startsWith("{") && text.endsWith("}"))) {
    try {
      body = JSON.parse(text);
    } catch (e) {
      return {
        errorResponse: new Response(JSON.stringify({
          success: false,
          error: "Malformed JSON payload: " + e.message,
          code: "INVALID_JSON"
        }), {
          status: 400,
          headers: CORS_HEADERS
        })
      };
    }
  } else {
    body = { source: text };
  }

  return { body };
}

/**
 * Synchronized Cryptographic AST Commitment
 * Used by BOTH /api/compile and /api/verify to eliminate discrepancy
 */
export async function computeAstCommitment(source, target = "x86_64", optLevel = "O2") {
  const canonical = "zcc:ast:v1:" + source.replace(/\r\n/g, "\n").trim() + ":" + target.toLowerCase() + ":" + optLevel.toUpperCase();
  const enc = new TextEncoder().encode(canonical);
  const hashBuf = await crypto.subtle.digest("SHA-256", enc);
  return "0x" + Array.from(new Uint8Array(hashBuf), b => b.toString(16).padStart(2, '0')).join('');
}
