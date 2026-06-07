import os
import sys
import time
import subprocess
import requests
import json

def main():
    print("======================================================================")
    print("🔱 ZKAEDI MESH SYSTEM v1.0 SIMULATOR INITIATED")
    print("======================================================================")

    dir_path = os.path.dirname(os.path.abspath(__file__))
    
    # 1. Clear old ledger
    ledger_path = os.path.join(dir_path, "mesh_ledger.jsonl")
    if os.path.exists(ledger_path):
        os.remove(ledger_path)

    # 2. Spin up sub-agents A, B, C and Orchestrator D
    processes = {}
    agents = {
        "Agent A (3D)": ("agent_3d_mesh.py", 8001),
        "Agent B (Semantic)": ("agent_semantic_mesh.py", 8002),
        "Agent C (Network)": ("agent_network_mesh.py", 8003),
        "Agent D (Orchestrator)": ("mesh_orchestrator.py", 8000)
    }

    try:
        for name, (script, port) in agents.items():
            script_path = os.path.join(dir_path, script)
            print(f"[SYSTEM] Launching {name} on port {port}...")
            print(f"         Cmd: {sys.executable} {script_path}")
            log_file = open(os.path.join(dir_path, f"{script}.log"), "wb")
            env = os.environ.copy()
            env["PYTHONUNBUFFERED"] = "1"
            env["CUDA_VISIBLE_DEVICES"] = ""
            proc = subprocess.Popen(
                [sys.executable, script_path],
                stdout=log_file,
                stderr=log_file,
                env=env
            )
            log_file.close()
            processes[name] = proc

        # Wait for uvicorn instances to bind to ports
        print("[SYSTEM] Warming up microservices (3 seconds)...")
        time.sleep(3)

        # Check if any processes exited early
        for name, proc in processes.items():
            status = proc.poll()
            if status is not None:
                print(f"🔴 [ERROR] {name} exited early with code {status}")
                # Print log contents if it failed
                log_path = os.path.join(dir_path, f"{agents[name][0]}.log")
                if os.path.exists(log_path):
                    with open(log_path, "r") as lf:
                        print(f"=== {name} Log ===")
                        print(lf.read())
                        print("==================")

        # 3. Submit a mock task to Orchestrator D
        payload = {
            "task_id": "zk-mesh-task-777",
            "prompt": "Procedural City Blocks with Emissive Cyan Conduits",
            "complexity": 0.85,
            "embeddings": [
                [0.12, -0.45, 0.88, 0.01],
                [0.15, -0.42, 0.85, 0.03],
                [0.11, -0.47, 0.89, -0.01]
            ],
            "worker_count": 8,
            "h_prev": 1.0
        }

        print("[SYSTEM] Dispatching task to Agent D (Orchestrator) at http://127.0.0.1:8000/dispatch...")
        url = "http://127.0.0.1:8000/dispatch"
        
        response = requests.post(url, json=payload, timeout=10)
        
        if response.status_code == 200:
            print("\n🟢 [SUCCESS] Swarm execution converged successfully!")
            print(json.dumps(response.json(), indent=2))
        else:
            print(f"\n🔴 [FAILURE] Dispatch returned error code {response.status_code}")
            print(response.text)

        # 4. Verify the ledger was written
        print("\n[SYSTEM] Verifying mesh ledger...")
        if os.path.exists(ledger_path):
            with open(ledger_path, "r") as f:
                records = f.readlines()
            print(f"Ledger file contains {len(records)} transaction records:")
            for r in records:
                print(r.strip())
        else:
            print("WARNING: Ledger file not found!")

    except Exception as e:
        print(f"Error during simulation: {e}")
    finally:
        # 5. Tear down all processes cleanly
        print("\n[SYSTEM] Tearing down microservice swarm...")
        for name, proc in processes.items():
            print(f"Terminating {name} (PID: {proc.pid})...")
            proc.terminate()
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                print(f"Force killing {name}...")
                proc.kill()
        print("🔱 SIMULATION COMPLETE. Grid returned to idle state.")

if __name__ == "__main__":
    main()
