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
        # Fallback regex/line parsing if PyYAML is not installed
        config = {"agents": {"agent_3d": {"port": 8001, "host": "127.0.0.1"}},
                  "hamiltonian": {"eta": 0.15, "gamma": 1.8, "epsilon": 0.07, "beta": 0.4, "h_0": 1.0}}
        if os.path.exists(config_path):
            with open(config_path, "r") as f:
                lines = f.readlines()
            # Simple line parser
            for line in lines:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if ":" in line:
                    k, v = line.split(":", 1)
                    k, v = k.strip(), v.strip()
                    if k == "port" and "agent_3d" in "".join(lines):  # very simple stub
                        try: config["agents"]["agent_3d"]["port"] = int(v)
                        except: pass
        return config

config = load_config()
agent_cfg = config["agents"]["agent_3d"]
ham_cfg = config["hamiltonian"]

app = FastAPI(title="ZKAEDI Agent A: 3D Mesh Generator")

class TaskPayload(BaseModel):
    task_id: str
    prompt: str
    complexity: float  # 0.0 to 1.0
    h_prev: float      # previous state H_{t-1}

@app.post("/run")
async def run_task(payload: TaskPayload):
    try:
        # Initialize the PRIME Scorer with configuration hyperparameters
        scorer = PrimeScorer(
            h_0=ham_cfg.get("h_0", 1.0),
            eta=ham_cfg.get("eta", 0.15),
            gamma=ham_cfg.get("gamma", 1.8),
            epsilon=ham_cfg.get("epsilon", 0.07),
            beta=ham_cfg.get("beta", 0.4),
            seed=random.randint(1, 1000000)
        )
        
        # Propagate the state vector H_{t-1} -> H_t
        h_t = scorer.step(payload.h_prev)
        
        # Triage state vector into Legendary/Epic/Rare thresholds
        abs_h = abs(h_t)
        if abs_h > 2.5:
            tier = "LEGENDARY"
        elif abs_h > 1.2:
            tier = "EPIC"
        else:
            tier = "RARE"
            
        # Simulate LOD knapsack optimization stats
        triangles = int(payload.complexity * 50000 * (1.0 + abs(h_t) * 0.2))
        lod_levels = 3 if triangles > 10000 else 1
        
        return {
            "status": "COMPLETED",
            "agent": "Agent A (3D Mesh)",
            "task_id": payload.task_id,
            "prompt": payload.prompt,
            "h_t": h_t,
            "score_tier": tier,
            "stats": {
                "triangles": triangles,
                "lod_levels": lod_levels,
                "generation_time_ms": random.randint(150, 600)
            }
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

if __name__ == "__main__":
    uvicorn.run(app, host=agent_cfg["host"], port=agent_cfg["port"])
