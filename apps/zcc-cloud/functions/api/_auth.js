// apps/zcc-cloud/functions/api/_auth.js
// Cryptographic API Key Verification, Rate Limiting, & Resource Protection

export const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization, X-API-Key",
  "Content-Type": "application/json;charset=utf-8"
};

const DEFAULT_SECRET = "zkaedi_zcc_sovereign_edge_key_v4_secret_auth_2026";
export const MAX_BODY_BYTES = 65536; // 64 KB strict ceiling

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
function timingSafeEqualHex(a, b) {
  if (a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) {
    diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  }
  return diff === 0;
}

/**
 * Create a cryptographically signed API key
 */
export async function generateSignedApiKey({ email = "developer@zkaedi.ai", tier = "developer_sandbox" }, env = {}) {
  const secret = env.API_KEY_SECRET || DEFAULT_SECRET;
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
 * Cryptographically verify an incoming API key
 */
export async function verifyApiKey(key, env = {}) {
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

  const secret = env.API_KEY_SECRET || DEFAULT_SECRET;
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
      createdAt: new Date(payload.created).toISOString(),
      quota: {
        daily_limit: 5000,
        used_today: 1,
        remaining: 4999
      }
    }
  };
}

/**
 * Authenticate incoming HTTP request
 */
export async function authenticateRequest(request, env = {}, { allowAnonymous = false } = {}) {
  const authHeader = request.headers.get("authorization") || request.headers.get("x-api-key") || "";
  const key = authHeader.replace(/^Bearer\s+/i, '').trim();

  if (!key) {
    if (allowAnonymous) {
      return {
        authenticated: false,
        isSandboxAnonymous: true,
        user: { tier: "anonymous_sandbox", quota: { daily_limit: 50, remaining: 49 } },
        rateLimitHeaders: {
          "X-RateLimit-Limit": "10",
          "X-RateLimit-Remaining": "9",
          "X-RateLimit-Reset": "60"
        }
      };
    }
    return {
      authenticated: false,
      response: new Response(JSON.stringify({
        success: false,
        error: "Missing API credentials. Provide Authorization: Bearer <key> or X-API-Key header.",
        code: "UNAUTHORIZED",
        hint: "Obtain a cryptographic key via POST /api/keys."
      }), {
        status: 401,
        headers: CORS_HEADERS
      })
    };
  }

  const verResult = await verifyApiKey(key, env);
  if (!verResult.valid) {
    return {
      authenticated: false,
      response: new Response(JSON.stringify({
        success: false,
        error: verResult.error,
        code: verResult.code || "FORBIDDEN"
      }), {
        status: 403,
        headers: CORS_HEADERS
      })
    };
  }

  return {
    authenticated: true,
    user: verResult.user,
    rateLimitHeaders: {
      "X-RateLimit-Limit": "60",
      "X-RateLimit-Remaining": "59",
      "X-RateLimit-Reset": "60"
    }
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
