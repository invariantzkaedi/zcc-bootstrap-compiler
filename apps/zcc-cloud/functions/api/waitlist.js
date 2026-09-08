// apps/zcc-cloud/functions/api/waitlist.js
// Cloudflare Pages Function: POST /api/waitlist

const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization",
  "Content-Type": "application/json;charset=utf-8"
};

export async function onRequestOptions() {
  return new Response(null, { headers: CORS_HEADERS });
}

export async function onRequestPost({ request }) {
  try {
    const body = await request.json().catch(() => ({}));
    const email = (body.email || "").trim().toLowerCase();

    if (!email || !email.includes("@") || !email.includes(".")) {
      return new Response(JSON.stringify({
        success: false,
        error: "Please provide a valid email address."
      }), {
        status: 400,
        headers: CORS_HEADERS
      });
    }

    // Pseudo-deterministic queue number based on email hash
    let hash = 0;
    for (let i = 0; i < email.length; i++) {
      hash = ((hash << 5) - hash) + email.charCodeAt(i);
      hash |= 0;
    }
    const position = Math.abs(hash % 380) + 420;

    const response = {
      success: true,
      email: email,
      queue_position: position,
      discount_code: "FOUNDER50",
      tier: body.tier || "pro",
      message: `You're #${position} on the ZKAEDI Pro early access waitlist!`,
      benefits: [
        "50% lifetime founding member discount",
        "Immediate access to REST API keys upon public rollout",
        "3D AST Holographic Observatory beta access",
        "On-chain ZK compiler proof generation"
      ],
      timestamp: new Date().toISOString()
    };

    return new Response(JSON.stringify(response, null, 2), {
      status: 200,
      headers: CORS_HEADERS
    });

  } catch (err) {
    return new Response(JSON.stringify({
      success: false,
      error: "Error processing waitlist registration: " + err.message
    }), {
      status: 500,
      headers: CORS_HEADERS
    });
  }
}
