# fhn_server.py — run alongside your Node server
import asyncio
import websockets
import math
import time

# FitzHugh-Nagumo parameters
a, b, c = 0.7, 0.8, 10.0
I_ext = 0.5
dt = 0.01

# Three coupled oscillators
v = [0.1, -0.5, 0.3]
w = [0.0,  0.1, -0.1]
coupling = 0.2

async def fhn_stream(websocket):
    while True:
        # Euler step for each oscillator
        dv = [0]*3
        dw = [0]*3
        for i in range(3):
            j, k = (i+1)%3, (i+2)%3
            dv[i] = c*(v[i] - v[i]**3/3 - w[i] + I_ext + coupling*(v[j]+v[k]))
            dw[i] = (v[i] + a - b*w[i]) / c
        for i in range(3):
            v[i] += dv[i] * dt
            w[i] += dw[i] * dt

        payload = {
            "src": "FHN",
            "x": round(v[0], 6),
            "y": round(v[1], 6),
            "z": round(v[2], 6),
            "t": time.time(),
            "hz": 100
        }
        await websocket.send(str(payload).replace("'", '"'))
        await asyncio.sleep(dt)

async def main():
    async with websockets.serve(fhn_stream, "localhost", 8043):
        print("FHN chaos server running on ws://localhost:8043")
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
