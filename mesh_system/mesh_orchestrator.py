import os
import sys
import json
import random
import requests
from concurrent.futures import ThreadPoolExecutor
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from typing import List, Dict, Any
import uvicorn

# Ensure the parent/current folder is on path for local imports
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from mesh_prime_scorer import PrimeScorer, PrimeConsensus, PrimeVector

# Defensive YAML parser fallback
def load_config():
    config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "mesh_config.yaml")
    try:
        import yaml
        with open(config_path, "r") as f:
            return yaml.safe_load(f)
    except ImportError:
        config = {
            "agents": {
                "orchestrator": {"port": 8000, "host": "127.0.0.1"},
                "agent_3d": {"port": 8001, "host": "127.0.0.1"},
                "agent_semantic": {"port": 8002, "host": "127.0.0.1"},
                "agent_network": {"port": 8003, "host": "127.0.0.1"}
            },
            "hamiltonian": {"eta": 0.15, "gamma": 1.8, "epsilon": 0.07, "beta": 0.4, "h_0": 1.0},
            "constitution": {"drift_max_threshold": 10.0, "trust_decay_rate": 0.05}
        }
        # Parse YAML values manually if YAML exists on disk
        if os.path.exists(config_path):
            with open(config_path, "r") as f:
                lines = f.readlines()
            current_section = ""
            for line in lines:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if line.endswith(":"):
                    current_section = line[:-1].strip()
                    continue
                if ":" in line:
                    k, v = line.split(":", 1)
                    k, v = k.strip(), v.strip()
                    if k == "port":
                        try:
                            if "agent_3d" in line or (current_section == "agent_3d"):
                                config["agents"]["agent_3d"]["port"] = int(v)
                            elif "agent_semantic" in line or (current_section == "agent_semantic"):
                                config["agents"]["agent_semantic"]["port"] = int(v)
                            elif "agent_network" in line or (current_section == "agent_network"):
                                config["agents"]["agent_network"]["port"] = int(v)
                            elif "orchestrator" in line or (current_section == "orchestrator"):
                                config["agents"]["orchestrator"]["port"] = int(v)
                        except: pass
        return config

config = load_config()
orchestrator_cfg = config["agents"]["orchestrator"]
agent_3d_cfg = config["agents"]["agent_3d"]
agent_sem_cfg = config["agents"]["agent_semantic"]
agent_net_cfg = config["agents"]["agent_network"]
ham_cfg = config["hamiltonian"]
const_cfg = config["constitution"]

app = FastAPI(title="ZKAEDI Agent D: Orchestration Mesh Gateway")

class DispatchPayload(BaseModel):
    task_id: str
    prompt: str
    complexity: float
    embeddings: List[List[float]]
    worker_count: int
    h_prev: float

def call_sub_agent(url: str, payload: dict) -> Dict[str, Any]:
    try:
        response = requests.post(url, json=payload, timeout=10)
        if response.status_code == 200:
            return response.json()
        else:
            return {"status": "FAILED", "error": f"HTTP {response.status_code}: {response.text}"}
    except Exception as e:
        return {"status": "FAILED", "error": str(e)}

@app.post("/dispatch")
async def dispatch_task(payload: DispatchPayload):
    task_id = payload.task_id
    h_prev = payload.h_prev
    
    # 1. Map sub-agent tasks
    tasks = {
        "agent_3d": {
            "url": f"http://{agent_3d_cfg['host']}:{agent_3d_cfg['port']}/run",
            "payload": {
                "task_id": task_id,
                "prompt": payload.prompt,
                "complexity": payload.complexity,
                "h_prev": h_prev
            }
        },
        "agent_semantic": {
            "url": f"http://{agent_sem_cfg['host']}:{agent_sem_cfg['port']}/run",
            "payload": {
                "task_id": task_id,
                "embeddings": payload.embeddings,
                "h_prev": h_prev
            }
        },
        "agent_network": {
            "url": f"http://{agent_net_cfg['host']}:{agent_net_cfg['port']}/run",
            "payload": {
                "task_id": task_id,
                "worker_count": payload.worker_count,
                "h_prev": h_prev
            }
        }
    }
    
    # 2. Dispatch in parallel threads
    results = {}
    with ThreadPoolExecutor(max_workers=3) as executor:
        futures = {
            name: executor.submit(call_sub_agent, t["url"], t["payload"])
            for name, t in tasks.items()
        }
        for name, fut in futures.items():
            results[name] = fut.result()
            
    # 3. Aggregate state vector parameters using PrimeScorer.score_mesh
    h_a = results.get("agent_3d", {}).get("h_t", h_prev)
    h_b = results.get("agent_semantic", {}).get("h_t", h_prev)
    h_c = results.get("agent_network", {}).get("h_t", h_prev)
    
    scorer = PrimeScorer(
        h_0=ham_cfg.get("h_0", 1.0),
        eta=ham_cfg.get("eta", 0.15),
        gamma=ham_cfg.get("gamma", 1.8),
        epsilon=ham_cfg.get("epsilon", 0.07),
        beta=ham_cfg.get("beta", 0.4),
        seed=777
    )
    
    consensus = scorer.score_mesh(h_a, h_b, h_c, context="mesh_orchestrator")
    serialized = consensus.serialize()
    
    # Save the binary PRIME artifact to prime_state.pb
    pb_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "prime_state.pb")
    try:
        with open(pb_path, "wb") as f:
            f.write(serialized)
    except Exception as e:
        print(f"Error writing prime_state.pb: {e}", file=sys.stderr)
        
    avg_h_t = consensus.consensus_score
    drift_breach = consensus.drift > const_cfg.get("drift_max_threshold", 10.0)
    
    # 4. Write transaction record to the mesh ledger
    ledger_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "mesh_ledger.jsonl")
    record = {
        "task_id": task_id,
        "avg_h_t": avg_h_t,
        "drift_breach": drift_breach,
        "jackpot": consensus.jackpot,
        "alerts": consensus.alerts,
        "serialized_len": len(serialized),
        "sub_agent_results": results
    }
    
    try:
        with open(ledger_path, "a") as f:
            f.write(json.dumps(record) + "\n")
    except Exception as e:
        print(f"Error writing to ledger: {e}", file=sys.stderr)
        
    return {
        "status": "CONVERGED",
        "task_id": task_id,
        "avg_h_t": avg_h_t,
        "drift_breach": drift_breach,
        "jackpot": consensus.jackpot,
        "alerts": consensus.alerts,
        "serialized_len": len(serialized),
        "sub_agents": results
    }

if __name__ == "__main__":
    uvicorn.run(app, host=orchestrator_cfg["host"], port=orchestrator_cfg["port"])
