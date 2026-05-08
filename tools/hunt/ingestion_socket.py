import asyncio
import websockets
import json
import hashlib
import os

print("\033[38;5;17m[MAINFRAME SYNCHRONIZED. INGESTION SOCKET ONLINE.]\033[0m")

# Global state for chunk reassembly
incoming_payloads = {}

async def handle_uplink(websocket):
    print("\033[38;5;51m[UPLINK]\033[0m Browser connection established.")
    try:
        async for message in websocket:
            if isinstance(message, str):
                # Parse the INGEST_HEADER
                header = json.loads(message)
                if header.get("type") == "INGEST_HEADER":
                    filename = header["filename"]
                    if filename not in incoming_payloads:
                        incoming_payloads[filename] = {
                            "hash": header["hash"],
                            "total": header["total"],
                            "chunks": [],
                            "received": 0
                        }
                    print(f"\033[38;5;199m[INGEST]\033[0m Receiving: {filename} (Chunk {header['chunk'] + 1}/{header['total']})")
                    
            elif isinstance(message, bytes):
                # We assume the binary message belongs to the last active header
                # For a production multi-tenant system, prefix bytes with a UUID
                active_file = list(incoming_payloads.keys())[-1]
                payload = incoming_payloads[active_file]
                
                payload["chunks"].append(message)
                payload["received"] += 1
                
                if payload["received"] == payload["total"]:
                    print(f"\033[38;5;51m[REASSEMBLY]\033[0m All chunks received for {active_file}. Verifying...")
                    
                    # Reassemble and Hash
                    full_binary = b"".join(payload["chunks"])
                    computed_hash = hashlib.sha256(full_binary).hexdigest()
                    
                    if computed_hash == payload["hash"]:
                        print(f"\033[38;5;51m[PROVENANCE VERIFIED]\033[0m SHA-256 Match: {computed_hash[:16]}...")
                        
                        # 1. Save locally or pipe directly to C-compiler IPC
                        save_path = f"/tmp/{active_file}"
                        with open(save_path, "wb") as f:
                            f.write(full_binary)
                            
                        # 2. Trigger ZCC Mesh Warden Fortification here
                        # os.system(f"./zcc --sculpt_warden {save_path}")
                        
                        # 3. Acknowledge back to UI
                        await websocket.send(json.dumps({"type": "WELD_COMPLETE", "status": "Watertight"}))
                    else:
                        print(f"\033[38;5;199m[CORRUPTION]\033[0m Hash mismatch for {active_file}.")
                        await websocket.send(json.dumps({"type": "ERROR", "status": "Cryptographic Failure"}))
                    
                    del incoming_payloads[active_file]
                    
    except websockets.exceptions.ConnectionClosed:
        print("\033[38;5;199m[UPLINK]\033[0m Browser disconnected.")

async def main():
    # Bind to port 8080 to match your UI configuration
    async with websockets.serve(handle_uplink, "localhost", 8080):
        print("\033[38;5;17m[SOCKET]\033[38;5;51m Listening on ws://localhost:8080\033[0m")
        await asyncio.Future()  # Run forever

if __name__ == "__main__":
    asyncio.run(main())
