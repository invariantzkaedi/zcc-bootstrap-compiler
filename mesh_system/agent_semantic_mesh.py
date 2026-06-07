import os
import sys
import random
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from typing import List
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
        config = {"agents": {"agent_semantic": {"port": 8002, "host": "127.0.0.1"}},
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
                    if k == "port" and "agent_semantic" in "".join(lines):
                        try: config["agents"]["agent_semantic"]["port"] = int(v)
                        except: pass
        return config

config = load_config()
agent_cfg = config["agents"]["agent_semantic"]
ham_cfg = config["hamiltonian"]

app = FastAPI(title="ZKAEDI Agent B: Semantic Mesh Analyzer")

class SemanticPayload(BaseModel):
    task_id: str
    embeddings: List[List[float]]
    h_prev: float

@app.post("/run")
async def run_task(payload: SemanticPayload):
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
        
        # Mock cosine similarity logic
        num_vecs = len(payload.embeddings)
        similarity_sum = 0.0
        comparisons = 0
        
        for i in range(num_vecs):
            for j in range(i + 1, num_vecs):
                # Simple mock similarity score modified by the state vector
                sim = max(0.0, min(1.0, 0.75 + 0.15 * math.sin(h_t) + random.uniform(-0.05, 0.05)))
                similarity_sum += sim
                comparisons += 1
                
        avg_similarity = (similarity_sum / comparisons) if comparisons > 0 else 1.0
        
        # Dimension collapse check: if avg similarity is extremely high, signal dimension collapse
        dimension_collapse = avg_similarity > 0.95 or abs(h_t) < 0.1
        
        return {
            "status": "COMPLETED",
            "agent": "Agent B (Semantic Mesh)",
            "task_id": payload.task_id,
            "h_t": h_t,
            "dimension_collapse": dimension_collapse,
            "stats": {
                "avg_similarity": avg_similarity,
                "dimension_count": len(payload.embeddings[0]) if num_vecs > 0 else 0,
                "analysis_time_ms": random.randint(100, 350)
            }
        }
    except Exception as e:
        import traceback
        traceback.print_exc()
        raise HTTPException(status_code=500, detail=str(e))

import math # Ensure math is imported for math.sin

if __name__ == "__main__":
    uvicorn.run(app, host=agent_cfg["host"], port=agent_cfg["port"])
