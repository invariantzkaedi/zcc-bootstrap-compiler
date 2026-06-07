import os
import sys
import random
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import uvicorn

# Ensure the parent/current folder is on path for local imports
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from mesh_prime_scorer import PrimeScorer

# Defensive YAML parser fallback
def load_config():
    config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "mesh_config.yaml")
    try:
        import yaml
        with open(config_path, "r") as f:
            return yaml.safe_load(f)
    except ImportError:
        config = {"agents": {"agent_network": {"port": 8003, "host": "127.0.0.1"}},
                  "hamiltonian": {"eta": 0.15, "gamma": 1.8, "epsilon": 0.07, "beta": 0.4, "h_0": 1.0}}
        if os.path.exists(config_path):
            with open(config_path, "r") as f:
                lines = f.readlines()
            for line in lines:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if ":" in line:
                    k, v = line.split(":", 1)
                    k, v = k.strip(), v.strip()
                    if k == "port" and "agent_network" in "".join(lines):
                        try: config["agents"]["agent_network"]["port"] = int(v)
                        except: pass
        return config

config = load_config()
agent_cfg = config["agents"]["agent_network"]
ham_cfg = config["hamiltonian"]

app = FastAPI(title="ZKAEDI Agent C: Cloudflare Workers Network Monitor")

class NetworkPayload(BaseModel):
    task_id: str
    worker_count: int
    h_prev: float

@app.post("/run")
async def run_task(payload: NetworkPayload):
    try:
        # Initialize the PRIME Scorer
        scorer = PrimeScorer(
            h_0=ham_cfg.get("h_0", 1.0),
            eta=ham_cfg.get("eta", 0.15),
            gamma=ham_cfg.get("gamma", 1.8),
            epsilon=ham_cfg.get("epsilon", 0.07),
            beta=ham_cfg.get("beta", 0.4),
            seed=random.randint(1, 1000000)
        )
        
        # Propagate state
        h_t = scorer.step(payload.h_prev)
        
        # Calculate simulated latency and error rates influenced by state drift
        base_latency = 12.5  # ms
        drift_factor = max(0.1, abs(h_t))
        latency = base_latency * drift_factor + random.uniform(2.0, 8.0)
        error_rate = 0.001 * drift_factor + random.uniform(0.0001, 0.005)
        
        # Topology drift calculation: warn if drift exceeds limits
        topology_drift = drift_factor > 3.0
        
        return {
            "status": "COMPLETED",
            "agent": "Agent C (Network Mesh)",
            "task_id": payload.task_id,
            "h_t": h_t,
            "topology_drift": topology_drift,
            "stats": {
                "latency_ms": latency,
                "error_rate": error_rate,
                "active_workers": payload.worker_count,
                "scan_time_ms": random.randint(80, 200)
            }
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

if __name__ == "__main__":
    uvicorn.run(app, host=agent_cfg["host"], port=agent_cfg["port"])
