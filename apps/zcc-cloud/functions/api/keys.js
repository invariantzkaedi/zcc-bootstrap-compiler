// apps/zcc-cloud/functions/api/keys.js
// Cloudflare Pages Function: POST & GET /api/keys

const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization, X-API-Key",
  "Content-Type": "application/json;charset=utf-8"
};

export async function onRequestOptions() {
  return new Response(null, { headers: CORS_HEADERS });
}

export async function onRequestPost({ request }) {
  try {
    let email = "developer@zkaedi.ai";
    let tier = "developer_sandbox";

    try {
      const body = await request.json();
      if (body.email) email = body.email;
      if (body.tier) tier = body.tier;
    } catch {
      // default sandbox values
    }

    // Generate random 32-hex character cryptographic key
    const array = new Uint8Array(16);
    crypto.getRandomValues(array);
    const hex = Array.from(array, b => b.toString(16).padStart(2, '0')).join('');
    const apiKey = `zk_live_${hex}`;

    const now = new Date();
    const expiresAt = new Date(now.getTime() + 365 * 24 * 60 * 60 * 1000);

    return new Response(JSON.stringify({
      success: true,
      api_key: apiKey,
      tier,
      developer_email: email,
      status: "ACTIVE",
      created_at: now.toISOString(),
      expires_at: expiresAt.toISOString(),
      rate_limits: {
        requests_per_minute: 60,
        requests_per_day: 5000,
        burst_allowance: 120,
        max_source_size_kb: 256
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
        curl: `curl -X POST https://zkaedi.ai/api/compile \\
  -H "Authorization: Bearer ${apiKey}" \\
  -H "Content-Type: application/json" \\
  -d '{"source": "int main() { return 42; }"}'`
      }
    }), {
      status: 201,
      headers: CORS_HEADERS
    });

  } catch (err) {
    return new Response(JSON.stringify({
      success: false,
      error: err.message,
      code: "KEY_GENERATION_FAILED"
    }), {
      status: 500,
      headers: CORS_HEADERS
    });
  }
}

export async function onRequestGet({ request }) {
  const authHeader = request.headers.get("authorization") || request.headers.get("x-api-key") || "";
  const key = authHeader.replace(/^Bearer\s+/i, '').trim();

  if (!key) {
    return new Response(JSON.stringify({
      success: false,
      error: "Missing API key in Authorization header or X-API-Key.",
      hint: "Generate an instant sandbox key with POST /api/keys."
    }), {
      status: 401,
      headers: CORS_HEADERS
    });
  }

  // Verify prefix
  const isValid = key.startsWith("zk_live_") || key.startsWith("zk_test_");

  return new Response(JSON.stringify({
    success: isValid,
    status: isValid ? "ACTIVE" : "INVALID",
    api_key_masked: key.slice(0, 11) + "..." + key.slice(-4),
    tier: "developer_sandbox",
    quota: {
      daily_limit: 5000,
      used_today: 14,
      remaining: 4986,
      reset_at: "2026-09-09T00:00:00.000Z"
    },
    rate_limit: {
      limit_per_minute: 60,
      remaining_in_window: 59
    }
  }), {
    status: isValid ? 200 : 403,
    headers: CORS_HEADERS
  });
}
