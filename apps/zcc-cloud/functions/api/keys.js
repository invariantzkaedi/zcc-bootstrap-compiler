// apps/zcc-cloud/functions/api/keys.js
// Cloudflare Pages Function: POST & GET /api/keys

import { CORS_HEADERS, generateSignedApiKey, verifyApiKey, readGuardedJsonBody } from "./_auth.js";

export async function onRequestOptions() {
  return new Response(null, { headers: CORS_HEADERS });
}

export async function onRequestPost({ request, env }) {
  try {
    const { errorResponse, body } = await readGuardedJsonBody(request, 8192);
    if (errorResponse) return errorResponse;

    const email = (body.email || "developer@zkaedi.ai").toString().trim();
    const tier = (body.tier || "developer_sandbox").toString().trim();

    // Generate cryptographically signed HMAC key
    const keyData = await generateSignedApiKey({ email, tier }, env);

    return new Response(JSON.stringify({
      success: true,
      api_key: keyData.apiKey,
      tier: keyData.payload.tier,
      developer_email: keyData.payload.email,
      status: "ACTIVE",
      created_at: new Date(keyData.payload.created).toISOString(),
      expires_at: keyData.expiresAt,
      rate_limits: {
        requests_per_minute: 60,
        requests_per_day: 5000,
        burst_allowance: 120,
        max_source_size_kb: 64
      },
      allowed_features: [
        "native_x86_64_compilation",
        "riscv64_cross_compilation",
        "wasm32_binary_emission",
        "win64_pe_compilation",
        "ssa_ir_optimization",
        "static_safety_audit",
        "zk_snark_r1cs_proofs",
        "ast_json_export"
      ],
      quickstart: {
        curl: `curl -X POST https://zkaedi.ai/api/compile \\\n  -H "Authorization: Bearer ${keyData.apiKey}" \\\n  -H "Content-Type: application/json" \\\n  -d '{"source": "int main() { return 42; }"}'`
      }
    }, null, 2), {
      status: 201,
      headers: CORS_HEADERS
    });

  } catch (err) {
    return new Response(JSON.stringify({
      success: false,
      error: "Key generation failed: " + err.message,
      code: "KEY_GENERATION_FAILED"
    }), {
      status: 500,
      headers: CORS_HEADERS
    });
  }
}

export async function onRequestGet({ request, env }) {
  const authHeader = request.headers.get("authorization") || request.headers.get("x-api-key") || "";
  const key = authHeader.replace(/^Bearer\s+/i, '').trim();

  if (!key) {
    return new Response(JSON.stringify({
      success: false,
      error: "Missing API key in Authorization header or X-API-Key.",
      hint: "Generate an authentic cryptographically signed key with POST /api/keys."
    }), {
      status: 401,
      headers: CORS_HEADERS
    });
  }

  // Cryptographic verification: prefix-only or forged keys are strictly rejected
  const verResult = await verifyApiKey(key, env);

  if (!verResult.valid) {
    return new Response(JSON.stringify({
      success: false,
      status: "INVALID",
      error: verResult.error,
      code: verResult.code || "FORBIDDEN"
    }), {
      status: 403,
      headers: CORS_HEADERS
    });
  }

  return new Response(JSON.stringify({
    success: true,
    status: "ACTIVE",
    api_key_masked: key.slice(0, 11) + "..." + key.slice(-4),
    tier: verResult.user.tier,
    developer_email: verResult.user.email,
    created_at: verResult.user.createdAt,
    quota: verResult.user.quota,
    rate_limit: {
      limit_per_minute: 60,
      remaining_in_window: 59
    }
  }, null, 2), {
    status: 200,
    headers: CORS_HEADERS
  });
}
