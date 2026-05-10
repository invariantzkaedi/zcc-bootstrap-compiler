/// ZCC Rust Frontend — WebSocket → SHM bridge
///
/// Receives JSON `ZccInjectPayload` messages from the VSCode extension,
/// converts them into a `WireInitNodeBuilder` tree, and writes the
/// serialized flat binary into the shared memory ring.
/// ZCC's `zcc_rust_shm_entry()` consumes it at memory speed.

mod ffi;
use ffi::shm::{ShmRing, WireInitNodeBuilder, WIRE_SLOT_SIZE};

use serde::Deserialize;
use std::sync::Arc;
use tokio::net::TcpListener;
use tokio::sync::broadcast;
use tokio_tungstenite::accept_async;
use futures_util::{SinkExt, StreamExt};

// ── Wire JSON protocol (mirrors extension.ts WireNode) ────────────────────

#[derive(Debug, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
enum WireNode {
    List {
        #[serde(default)]
        children: Vec<WireNode>,
    },
    Value {
        vkind: String, // "i64" | "f64" | "str" | "ident"
        #[serde(default)]
        value: serde_json::Value,
    },
    DesignatedField {
        field: String,
        #[serde(default)]
        children: Vec<WireNode>,
    },
    DesignatedIndex {
        index: i64,
        #[serde(default)]
        children: Vec<WireNode>,
    },
}

#[derive(Debug, Deserialize)]
struct ZccInjectPayload {
    #[serde(rename = "type")]
    kind: String,
    file: String,
    tree: WireNode,
}

// ── Conversion: JSON WireNode → WireInitNodeBuilder ───────────────────────

fn json_to_builder(node: WireNode) -> WireInitNodeBuilder {
    match node {
        WireNode::List { children } => {
            let mut b = WireInitNodeBuilder::new_list();
            for child in children {
                b.append(json_to_builder(child));
            }
            b
        }

        WireNode::Value { vkind, value } => match vkind.as_str() {
            "i64" => {
                let v = value.as_i64().unwrap_or(0);
                WireInitNodeBuilder::new_value_i64(v)
            }
            "f64" => {
                let v = value.as_f64().unwrap_or(0.0);
                WireInitNodeBuilder::new_value_f64(v)
            }
            "str" => {
                let s = value.as_str().unwrap_or("");
                WireInitNodeBuilder::new_value_str(s)
            }
            "ident" | _ => {
                let s = value.as_str().unwrap_or("unknown");
                WireInitNodeBuilder::new_value_ident(s)
            }
        },

        WireNode::DesignatedField { field, mut children } => {
            let init = if children.is_empty() {
                WireInitNodeBuilder::new_value_i64(0)
            } else {
                json_to_builder(children.remove(0))
            };
            WireInitNodeBuilder::new_designated_field(&field, init)
        }

        WireNode::DesignatedIndex { index, mut children } => {
            let init = if children.is_empty() {
                WireInitNodeBuilder::new_value_i64(0)
            } else {
                json_to_builder(children.remove(0))
            };
            WireInitNodeBuilder::new_designated_index(index, init)
        }
    }
}

// ── WebSocket server ──────────────────────────────────────────────────────

#[tokio::main]
async fn main() {
    // Open the SHM ring (creates it if it doesn't exist).
    let ring = Arc::new(
        ShmRing::create().unwrap_or_else(|e| {
            eprintln!("[ZCC-RUST] SHM init failed: {e}. Continuing (ring writes will be no-ops).");
            // In a real error path we'd exit; for the demo we let the server
            // run so the VSCode extension can still connect.
            // Safety: We return a dummy ring that can't write.
            // For now just panic — better to fail loud than corrupt memory.
            panic!("Cannot open ZCC SHM ring: {e}");
        })
    );

    let listener = TcpListener::bind("127.0.0.1:9001").await
        .expect("Cannot bind 127.0.0.1:9001");
    println!("[ZCC-RUST] Listening on ws://127.0.0.1:9001");

    let (bcast_tx, _) = broadcast::channel::<String>(64);
    let bcast_tx = Arc::new(bcast_tx);

    while let Ok((stream, addr)) = listener.accept().await {
        println!("[ZCC-RUST] Connection from {addr}");
        let ring = ring.clone();
        let bcast_tx = bcast_tx.clone();
        tokio::spawn(handle_connection(stream, ring, bcast_tx));
    }
}

async fn handle_connection(
    stream: tokio::net::TcpStream,
    ring: Arc<ShmRing>,
    _bcast: Arc<broadcast::Sender<String>>,
) {
    let ws = match accept_async(stream).await {
        Ok(ws) => ws,
        Err(e) => {
            eprintln!("[ZCC-RUST] WS handshake failed: {e}");
            return;
        }
    };

    let (mut tx, mut rx) = ws.split();

    while let Some(msg) = rx.next().await {
        let msg = match msg {
            Ok(m) => m,
            Err(e) => {
                eprintln!("[ZCC-RUST] WS recv error: {e}");
                break;
            }
        };

        if !msg.is_text() { continue; }

        let text = msg.into_text().unwrap_or_default();

        let payload: ZccInjectPayload = match serde_json::from_str(&text) {
            Ok(p) => p,
            Err(e) => {
                eprintln!("[ZCC-RUST] JSON parse error: {e}\n  raw: {text:.120}");
                let _ = tx.send(tokio_tungstenite::tungstenite::Message::Text(
                    format!(r#"{{"type":"error","message":"JSON parse: {e}"}}"#).into()
                )).await;
                continue;
            }
        };

        if payload.kind != "inject" {
            continue;
        }

        println!("[ZCC-RUST] Injecting tree for {}", payload.file);

        let builder = json_to_builder(payload.tree);

        // Estimate size before writing (reject obviously oversized trees).
        let mut probe = vec![0u8; WIRE_SLOT_SIZE];
        let mut cursor = 0usize;
        if !builder.serialize(&mut probe, &mut cursor) {
            let msg = format!("Tree too large ({cursor} bytes > WIRE_SLOT_SIZE {WIRE_SLOT_SIZE})");
            eprintln!("[ZCC-RUST] {msg}");
            let _ = tx.send(tokio_tungstenite::tungstenite::Message::Text(
                format!(r#"{{"type":"error","message":"{msg}"}}"#).into()
            )).await;
            continue;
        }

        let accepted = ring.write(&builder);
        let drop_count = ring.drop_count();

        let ack = format!(
            r#"{{"type":"ack","slots_used":{},"drop_count":{drop_count}}}"#,
            if accepted { 1 } else { 0 }
        );
        let _ = tx.send(tokio_tungstenite::tungstenite::Message::Text(ack.into())).await;

        if accepted {
            println!("[ZCC-RUST] Tree written to SHM ring (drop_count={drop_count})");
        } else {
            eprintln!("[ZCC-RUST] SHM ring full — drop_count={drop_count}");
        }
    }
}
