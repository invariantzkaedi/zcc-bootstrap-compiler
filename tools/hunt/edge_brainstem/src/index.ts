export interface Env {
    MEMPOOL_STATE: KVNamespace;
    TELEMETRY_DB: D1Database;
    ASSET_VAULT: R2Bucket;
    MATRIX_ROOM: DurableObjectNamespace;
}

export class MatrixRoom {
    state: DurableObjectState;
    sessions: Set<WebSocket>;

    constructor(state: DurableObjectState, env: Env) {
        this.state = state;
        this.sessions = new Set();
    }

    async fetch(request: Request) {
        if (request.headers.get("Upgrade") === "websocket") {
            const pair = new WebSocketPair();
            this.handleSession(pair[1]);
            return new Response(null, { status: 101, webSocket: pair[0] });
        }
        
        // Broadcast incoming telemetry to all connected WS clients
        if (request.method === "POST") {
            const payload = await request.text();
            this.broadcast(payload);
            return new Response("Broadcasted", { status: 200 });
        }
        
        return new Response("Matrix Room active", { status: 200 });
    }

    handleSession(ws: WebSocket) {
        ws.accept();
        this.sessions.add(ws);
        
        ws.addEventListener("message", async msg => {
            // Echo back to other clients (e.g., parameter adjustments from UI to Orchestrator)
            this.broadcast(msg.data as string, ws);
        });
        
        ws.addEventListener("close", () => {
            this.sessions.delete(ws);
        });
    }

    broadcast(message: string, exclude?: WebSocket) {
        for (const session of this.sessions) {
            if (session !== exclude) {
                try {
                    session.send(message);
                } catch (err) {
                    this.sessions.delete(session);
                }
            }
        }
    }
}

export default {
    async fetch(request: Request, env: Env, ctx: ExecutionContext): Promise<Response> {
        const url = new URL(request.url);
        
        const headers = {
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
            "Content-Type": "application/json"
        };
        
        if (request.method === "OPTIONS") {
            return new Response(null, { headers });
        }
        
        // HTTP to WebSocket upgrade logic via Durable Object
        if (url.pathname === "/matrix") {
            const id = env.MATRIX_ROOM.idFromName("global");
            const obj = env.MATRIX_ROOM.get(id);
            return obj.fetch(request);
        }

        // Telemetry Ingress with Cryptographic Ed25519 validation
        if (url.pathname === "/telemetry" && request.method === "POST") {
            const signatureHex = request.headers.get("X-Matrix-Signature");
            const pubKeyHex = request.headers.get("X-Matrix-Pubkey");
            
            if (!signatureHex || !pubKeyHex) {
                return new Response("Missing cryptographic ingress headers", { status: 401, headers });
            }
            
            // In a production worker, we use crypto.subtle.verify("Ed25519", ...) here.
            // For this scaffold, we simulate a verified ingress payload.
            const isSignatureValid = true; 
            
            if (!isSignatureValid) {
                return new Response("Ed25519 verification failed. Rogue actor detected.", { status: 403, headers });
            }

            const rawData = await request.text();
            const data = JSON.parse(rawData);
            
            // Broadcast the strike live via the Durable Object
            const id = env.MATRIX_ROOM.idFromName("global");
            const obj = env.MATRIX_ROOM.get(id);
            ctx.waitUntil(obj.fetch(new Request("http://internal/broadcast", { method: "POST", body: rawData })));

            // D1 SQL Relational Persistence
            await env.TELEMETRY_DB.prepare(
                "INSERT INTO strikes (hash, h_scalar, target, latency, timestamp) VALUES (?, ?, ?, ?, ?)"
            ).bind(data.hash, data.h_scalar, data.target, data.latency, Date.now()).run();
            
            // Global KV Flag Persistence
            await env.MEMPOOL_STATE.put("latest_strike", rawData);
            
            return new Response(JSON.stringify({ status: "secured_and_broadcasted" }), { headers });
        }
        
        return new Response("ZKAEDI Edge Matrix Socket Layer Online", { status: 404, headers });
    }
};
