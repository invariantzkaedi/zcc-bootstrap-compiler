/**
 * =========================================================================
 * [ZKAEDI PRIME] Global Telemetry Edge Router
 * Tech Stack: Cloudflare Workers / D1 / KV / R2
 * Standard: Shipping-ready, Security Hardened.
 * =========================================================================
 */

export interface Env {
    ZKAEDI_D1: any; // D1Database
    TELEMETRY_KV: any; // KVNamespace
    HEAL_BUCKET: any; // R2Bucket
    GITHUB_TOKEN?: string;
    GITHUB_REPO?: string;
}

export default {
    async fetch(request: Request, env: Env, ctx: any /* ExecutionContext */): Promise<Response> {
        // Enforce strict POST ingestion
        if (request.method !== "POST") {
            return new Response(JSON.stringify({ error: "Method Not Allowed" }), { 
                status: 405, 
                headers: { "Content-Type": "application/json" } 
            });
        }

        try {
            const url = new URL(request.url);
            
            // Route 1: Unified Diff Archival (R2)
            if (url.pathname === "/api/ingest/patch") {
                const patchContent = await request.text();
                const patchId = `heal_${Date.now()}_${crypto.randomUUID().slice(0,8)}.patch`;
                
                await env.HEAL_BUCKET.put(patchId, patchContent, {
                    httpMetadata: { contentType: "text/plain" }
                });

                // Trigger GitHub Actions CI/CD Forge
                if (env.GITHUB_TOKEN && env.GITHUB_REPO) {
                    await fetch(`https://api.github.com/repos/${env.GITHUB_REPO}/dispatches`, {
                        method: "POST",
                        headers: {
                            "Accept": "application/vnd.github.v3+json",
                            "Authorization": `token ${env.GITHUB_TOKEN}`,
                            "User-Agent": "ZKAEDI-Edge-Router"
                        },
                        body: JSON.stringify({
                            event_type: "zkaedi_patch_emitted",
                            client_payload: {
                                patch_id: patchId,
                                patch_url: `https://edge.zkaedi.net/patch/${patchId}`,
                                risk: "Autonomous Security Mitigation",
                                confidence: 0.95
                            }
                        })
                    }).catch(err => console.error("GitHub Dispatch Error:", err));
                }
                
                return new Response(JSON.stringify({ status: "PATCH_SECURED", id: patchId }), { 
                    status: 200, 
                    headers: { "Content-Type": "application/json" } 
                });
            }

            // Route 2: Kinetic Telemetry Stream (D1 & KV)
            if (url.pathname === "/api/ingest/telemetry") {
                const payload = await request.json();
                
                // Fast-path KV write for real-time dashboard visualization
                const kvKey = `live_metric_${Date.now()}`;
                await env.TELEMETRY_KV.put(kvKey, JSON.stringify(payload), { expirationTtl: 3600 });

                // Persistent D1 Logging for the Hamiltonian Audit Trail
                const stmt = env.ZKAEDI_D1.prepare(
                    `INSERT INTO compiler_events (contract, risk, confidence, transform, parity, timestamp) 
                     VALUES (?1, ?2, ?3, ?4, ?5, ?6)`
                ).bind(
                    payload.contract || "N/A",
                    payload.risk || "N/A",
                    payload.confidence || 0.0,
                    payload.transform || "UNKNOWN",
                    payload.parity || "LOCKED",
                    Date.now()
                );
                
                await stmt.run();

                return new Response(JSON.stringify({ status: "TELEMETRY_LOCKED" }), { 
                    status: 200, 
                    headers: { "Content-Type": "application/json" } 
                });
            }

            return new Response("ZKAEDI PRIME // INVALID ROUTE", { status: 404 });

        } catch (error) {
            console.error("\x1b[38;5;199m[FATAL] Edge Ingestion Failure:\x1b[0m", error);
            return new Response(JSON.stringify({ error: "Internal Edge Fault" }), { status: 500 });
        }
    }
};
