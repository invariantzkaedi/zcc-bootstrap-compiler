import socket
import struct
import json
import asyncio
import websockets

# ====================== CONFIG ======================
UDP_IP = "0.0.0.0"
UDP_PORT = 5005
WS_PORT = 8892

# ====================== UDP LISTENER ======================
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.setblocking(False)

print(f"[ZKAEDI] Listening for UDP on {UDP_IP}:{UDP_PORT}")
print(f"[ZKAEDI] WebSocket broadcasting on ws://localhost:{WS_PORT}")

# ====================== WEBSOCKET HANDLER ======================
connected_clients = set()

async def broadcast(message):
    if connected_clients:
        await asyncio.gather(
            *[client.send(message) for client in connected_clients],
            return_exceptions=True
        )

async def websocket_handler(websocket):
    connected_clients.add(websocket)
    try:
        await websocket.wait_closed()
    finally:
        connected_clients.remove(websocket)

async def udp_listener():
    loop = asyncio.get_event_loop()
    while True:
        try:
            data, addr = await loop.sock_recvfrom(sock, 1024)
            
            # Updated to unpack 5 floats: time, bass, treble, phase, chaos
            if len(data) == 20:  # 5 floats = 20 bytes
                t, bass, treble, phase, chaos = struct.unpack("fffff", data)
            else:
                # Backward compatibility with old 3-float packets
                t, bass, treble = struct.unpack("fff", data)
                phase = 0.0
                chaos = 1.0

            payload = json.dumps({
                "time": t,
                "bass": bass,
                "treble": treble,
                "phase": phase,
                "chaos": chaos
            })

            await broadcast(payload)

        except BlockingIOError:
            await asyncio.sleep(0.001)
        except Exception as e:
            print(f"[ZKAEDI] UDP Error: {e}")
            await asyncio.sleep(0.1)

# ====================== MAIN ======================
async def main():
    server = await websockets.serve(websocket_handler, "0.0.0.0", WS_PORT)
    await asyncio.gather(
        udp_listener(),
        server.wait_closed()
    )

if __name__ == "__main__":
    asyncio.run(main())
