#!/bin/bash
# ZKAEDI Mesh System v1.0 Swarm Launcher
# Bypasses Python subprocess locks by utilizing native bash job control

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export PYTHONWARNINGS="ignore"

# 1. Cleanup old logs and ledger
rm -f "$DIR"/*.log
rm -f "$DIR"/mesh_ledger.jsonl

echo "======================================================================"
echo "🔱 ZKAEDI MESH SYSTEM v1.0 SWARM RUNNER INITIATED"
echo "======================================================================"

# 2. Launch all four agents in the background
echo "[SYSTEM] Launching Agent A (3D Mesh)..."
CUDA_VISIBLE_DEVICES="" PYTHONUNBUFFERED=1 python3 "$DIR/agent_3d_mesh.py" > "$DIR/agent_3d_mesh.log" 2>&1 &
PID_A=$!

echo "[SYSTEM] Launching Agent B (Semantic Mesh)..."
CUDA_VISIBLE_DEVICES="" PYTHONUNBUFFERED=1 python3 "$DIR/agent_semantic_mesh.py" > "$DIR/agent_semantic_mesh.log" 2>&1 &
PID_B=$!

echo "[SYSTEM] Launching Agent C (Network Mesh)..."
CUDA_VISIBLE_DEVICES="" PYTHONUNBUFFERED=1 python3 "$DIR/agent_network_mesh.py" > "$DIR/agent_network_mesh.log" 2>&1 &
PID_C=$!

echo "[SYSTEM] Launching Agent D (Orchestrator)..."
CUDA_VISIBLE_DEVICES="" PYTHONUNBUFFERED=1 python3 "$DIR/mesh_orchestrator.py" > "$DIR/mesh_orchestrator.log" 2>&1 &
PID_D=$!

echo "[SYSTEM] Warming up swarm microservices (6 seconds)..."
sleep 6

# Check if processes are still running
if ! kill -0 $PID_D 2>/dev/null; then
    echo "🔴 [ERROR] Agent D (Orchestrator) failed to start. Logs:"
    cat "$DIR/mesh_orchestrator.log"
    kill $PID_A $PID_B $PID_C 2>/dev/null
    exit 1
fi

# 3. Dispatch mock request
echo "[SYSTEM] Dispatching task payload to Orchestrator..."
python3 -c "
import requests, json
payload = {
    'task_id': 'zk-mesh-task-777',
    'prompt': 'Procedural City Blocks with Emissive Cyan Conduits',
    'complexity': 0.85,
    'embeddings': [
        [0.12, -0.45, 0.88, 0.01],
        [0.15, -0.42, 0.85, 0.03],
        [0.11, -0.47, 0.89, -0.01]
    ],
    'worker_count': 8,
    'h_prev': 1.0
}
try:
    r = requests.post('http://127.0.0.1:8000/dispatch', json=payload, timeout=10)
    if r.status_code == 200:
        print('\n🟢 [SUCCESS] Swarm execution converged successfully!')
        print(json.dumps(r.json(), indent=2))
    else:
        print(f'\n🔴 [FAILURE] Dispatch returned {r.status_code}: {r.text}')
except Exception as e:
    print(f'\n🔴 [ERROR] Connection failed: {e}')
"

# 4. Verify ledger
echo ""
echo "[SYSTEM] Verifying ledger record..."
if [ -f "$DIR/mesh_ledger.jsonl" ]; then
    echo "Ledger file exists. Transaction entry:"
    cat "$DIR/mesh_ledger.jsonl"
else
    echo "WARNING: Ledger file was not written!"
fi

# 5. Cleanup and shutdown
echo ""
echo "[SYSTEM] Shutting down swarm..."
kill $PID_A $PID_B $PID_C $PID_D 2>/dev/null
wait $PID_A $PID_B $PID_C $PID_D 2>/dev/null
echo "🔱 SWARM OFFLINE. Grid returned to idle state."
