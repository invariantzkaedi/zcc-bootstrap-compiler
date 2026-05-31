import asyncio
import socket
import json
import sys
import websockets

# ====================== CONFIG ======================
TCP_IP = "127.0.0.1"
TCP_PORT = 9003
WS_IP = "0.0.0.0"
WS_PORT = 9002

connected_clients = set()
tcp_buffer = ""

print("🔱 ZKAEDI PRIME // AST KINEMATICS RELAY DAEMON")
print(f"[*] WS Server listening on ws://localhost:{WS_PORT}")
print(f"[*] TCP Receiver listening on tcp://{TCP_IP}:{TCP_PORT}")

# ====================== WEBSOCKET BROADCASTER ======================
async def broadcast(message):
    if not connected_clients:
        return
    # Clean up closed clients and send in parallel
    closed = set()
    for ws in connected_clients:
        try:
            await ws.send(message)
        except websockets.exceptions.ConnectionClosed:
            closed.add(ws)
        except Exception as e:
            print(f"[WS] Send error: {e}")
            closed.add(ws)
    connected_clients.difference_update(closed)

async def ws_handler(websocket):
    print(f"[WS] Client connected: {websocket.remote_address}")
    connected_clients.add(websocket)
    try:
        await websocket.wait_closed()
    finally:
        connected_clients.discard(websocket)
        print(f"[WS] Client disconnected: {websocket.remote_address}")

# ====================== TCP TELEMETRY RECEIVER ======================
async def handle_tcp_client(reader, writer):
    global tcp_buffer
    addr = writer.get_extra_info('peername')
    print(f"[TCP] Compiler connected from: {addr}")
    
    try:
        while True:
            data = await reader.read(4096)
            if not data:
                print(f"[TCP] Compiler disconnected: {addr}")
                break
                
            tcp_buffer += data.decode('utf-8', errors='ignore')
            
            # Process complete lines from the TCP stream
            while "\n" in tcp_buffer:
                line, tcp_buffer = tcp_buffer.split("\n", 1)
                line = line.strip()
                if not line:
                    continue
                
                # Validate JSON syntax before broadcasting
                try:
                    payload = json.loads(line)
                    # Forward valid JSON to WebSocket client(s)
                    await broadcast(json.dumps(payload))
                    print(f"[RELAY] Broadcasted: {line}")
                except json.JSONDecodeError:
                    print(f"[WARNING] Invalid JSON received on TCP: {line}")
                except Exception as e:
                    print(f"[ERROR] Relay error: {e}")
                    
    except asyncio.CancelledError:
        pass
    except Exception as e:
        print(f"[TCP] Error: {e}")
    finally:
        writer.close()
        await writer.wait_closed()

# ====================== MAIN EVENT LOOP ======================
async def main():
    # Start WebSocket Server
    ws_server = await websockets.serve(ws_handler, WS_IP, WS_PORT)
    
    # Start TCP Server
    tcp_server = await asyncio.start_server(handle_tcp_client, TCP_IP, TCP_PORT)
    
    # Run servers concurrently
    async with ws_server, tcp_server:
        await asyncio.gather(
            ws_server.wait_closed(),
            tcp_server.serve_forever()
        )

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[*] Shutting down Zkaedi AST Kinematics Relay...")
        sys.exit(0)
