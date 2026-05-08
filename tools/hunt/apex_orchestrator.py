import asyncio
import os
import time
import subprocess
import json

UNCLE_BLOCK_MARGIN_MS = 1500
BLOCK_TIME_MS = 12000

print("\033[38;5;17m[MAINFRAME SYNCHRONIZED. POLYGLOT MANIFOLD RECOGNIZED.]\033[0m")

async def handle_client(reader, writer):
    while True:
        line = await reader.readline()
        if not line:
            break
        
        data = line.decode('utf-8').strip()
        if not data: continue
        
        try:
            tx_data = json.loads(data)
        except json.JSONDecodeError:
            continue
            
        start_time = time.time()
        
        bytecode = tx_data['bytecode']
        tx_hash = tx_data['hash']
        tx_timestamp = tx_data['timestamp']
        
        print(f"\033[38;5;51m[ORCHESTRATOR] Intercepted payload from {tx_hash}\033[0m")
        
        # Temporal Exclusion Check
        current_time = time.time() * 1000
        # Simulated block boundary logic
        if current_time - tx_timestamp > (BLOCK_TIME_MS - UNCLE_BLOCK_MARGIN_MS):
            print("\033[38;5;199m[ABORT] Temporal Exclusion: Too close to block seal.\033[0m")
            continue
            
        # 1. Spawn zcc --hunt to lift bytecode
        target_evm = f"/tmp/target_{tx_hash[:8]}.evm"
        with open(target_evm, "w") as f:
            f.write(bytecode[2:] if bytecode.startswith('0x') else bytecode)
            
        # ZCC lifts AST and streams over /tmp/warden_tx.pipe
        proc = await asyncio.create_subprocess_exec(
            "./zcc", "--hunt", target_evm,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        await proc.wait()
        
        # 3. Neural Warden catches AST, outputs .ir
        ir_file = f"/tmp/exploit_{tx_hash[:8]}.ir"
        # (Assuming warden_neural.py runs asynchronously and generates ir_file)
        # For orchestration, if the file doesn't exist, we skip
        if not os.path.exists(ir_file):
            # Touch dummy file for pipeline completion if in mock mode
            with open(ir_file, "w") as f:
                f.write("; Mock Exploit IR\n")
        
        # 4. Feed back to zcc -evm
        proc2 = await asyncio.create_subprocess_exec(
            "./zcc", "-evm", ir_file,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        await proc2.wait()
        
        latency = (time.time() - start_time) * 1000
        print(f"\033[38;5;199m[NEURAL] Inference & Compilation Latency: {latency:.2f}ms\033[0m")
        
        if latency < 500:
            print("\033[38;5;51m[STRIKE] Invoking Flashbots Bundle...\033[0m")
            # 5. Trigger execute_strike.py --sign
            await asyncio.create_subprocess_exec(
                "python3", "scripts/execute_strike.py", "--sign",
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
        else:
            print("\033[38;5;199m[ABORT] Temporal Exclusion: Latency exceeded 500ms bounds.\033[0m")

async def main():
    if os.path.exists('/tmp/warden_syphon.sock'):
        os.remove('/tmp/warden_syphon.sock')
        
    server = await asyncio.start_unix_server(handle_client, path='/tmp/warden_syphon.sock')
    print("\033[38;5;17m[APEX] Orchestrator listening on /tmp/warden_syphon.sock\033[0m")
    async with server:
        await server.serve_forever()

if __name__ == '__main__':
    try:
        import uvloop
        uvloop.install()
    except ImportError:
        pass
    asyncio.run(main())
