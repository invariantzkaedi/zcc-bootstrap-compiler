#!/bin/bash
# ZKAEDI Mesh System v1.0 Swarm One-Shot Launcher with Trap Control

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export PYTHONWARNINGS="ignore"

# Cleanup logs and ledger
rm -f "$DIR"/*.log
rm -f "$DIR"/mesh_ledger.jsonl

echo "======================================================================"
echo "🔱 ZKAEDI MESH SYSTEM v1.0 ONE-SHOT LAUNCHER INITIATED"
echo "======================================================================"

# PIDs list
PIDS=()

cleanup() {
    echo ""
    echo "[SYSTEM] Trapped signal! Shutting down all swarm agents..."
    for pid in "${PIDS[@]}"; do
        if kill -0 $pid 2>/dev/null; then
            echo "Killing process $pid..."
            kill $pid 2>/dev/null
        fi
    done
    wait 2>/dev/null
    echo "🔱 SWARM OFFLINE. Swarm cleanup complete."
    exit 0
}

# Trap SIGINT (Ctrl+C), SIGTERM
trap cleanup SIGINT SIGTERM

echo "[SYSTEM] Launching Agent A (3D Mesh)..."
CUDA_VISIBLE_DEVICES="" PYTHONUNBUFFERED=1 python3 "$DIR/agent_3d_mesh.py" > "$DIR/agent_3d_mesh.log" 2>&1 &
PIDS+=($!)

echo "[SYSTEM] Launching Agent B (Semantic Mesh)..."
CUDA_VISIBLE_DEVICES="" PYTHONUNBUFFERED=1 python3 "$DIR/agent_semantic_mesh.py" > "$DIR/agent_semantic_mesh.log" 2>&1 &
PIDS+=($!)

echo "[SYSTEM] Launching Agent C (Network Mesh)..."
CUDA_VISIBLE_DEVICES="" PYTHONUNBUFFERED=1 python3 "$DIR/agent_network_mesh.py" > "$DIR/agent_network_mesh.log" 2>&1 &
PIDS+=($!)

echo "[SYSTEM] Launching Agent D (Orchestrator)..."
CUDA_VISIBLE_DEVICES="" PYTHONUNBUFFERED=1 python3 "$DIR/mesh_orchestrator.py" > "$DIR/mesh_orchestrator.log" 2>&1 &
PIDS+=($!)

echo "[SYSTEM] Warming up swarm microservices (6 seconds)..."
sleep 6

# Check if Orchestrator is running
if ! kill -0 ${PIDS[3]} 2>/dev/null; then
    echo "🔴 [ERROR] Agent D (Orchestrator) failed to start. Logs:"
    cat "$DIR/mesh_orchestrator.log"
    cleanup
    exit 1
fi

echo "🟢 [SUCCESS] Swarm is active on ports 8000 (Orchestrator), 8001 (A), 8002 (B), 8003 (C)."
echo "Press Ctrl+C to terminate the swarm..."

# Keep script running to keep trap active
while true; do
    sleep 1
done
