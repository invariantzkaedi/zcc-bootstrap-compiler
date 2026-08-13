#!/usr/bin/env python3
import os
import json
import time
import random

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT_DIR = os.path.join(REPO_ROOT, "output")
STATE_PATH = os.path.join(OUTPUT_DIR, "render_farm_state.json")
EVENTS_PATH = os.path.join(OUTPUT_DIR, "render_farm_events.jsonl")

def ensure_output_dir():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

def generate_telemetry():
    ensure_output_dir()
    
    # Simulate GPU Nodes
    gpu_nodes = [
        {
            "id": "GPU_Worker_01",
            "type": "GPU",
            "status": "BUSY" if random.random() > 0.3 else "IDLE",
            "load": round(random.uniform(70.0, 95.0), 1) if random.random() > 0.3 else round(random.uniform(0.0, 5.0), 1),
            "vram_used_gb": round(random.uniform(8.0, 15.0), 1),
            "vram_total_gb": 16.0,
            "temperature_c": round(random.uniform(65.0, 80.0), 1)
        },
        {
            "id": "GPU_Worker_02",
            "type": "GPU",
            "status": "BUSY" if random.random() > 0.5 else "IDLE",
            "load": round(random.uniform(50.0, 90.0), 1) if random.random() > 0.5 else round(random.uniform(0.0, 5.0), 1),
            "vram_used_gb": round(random.uniform(4.0, 12.0), 1),
            "vram_total_gb": 16.0,
            "temperature_c": round(random.uniform(55.0, 75.0), 1)
        }
    ]
    
    # Simulate CPU Nodes
    cpu_nodes = [
        {
            "id": "CPU_Worker_01",
            "type": "CPU",
            "status": "BUSY",
            "load": round(random.uniform(40.0, 85.0), 1),
            "cores_used": random.randint(12, 28),
            "cores_total": 32,
            "temperature_c": round(random.uniform(50.0, 68.0), 1)
        }
    ]
    
    worker_nodes = gpu_nodes + cpu_nodes
    
    # Render Job queue metrics
    completed = random.randint(100, 500)
    failed = random.randint(0, 5)
    pending = random.randint(1, 10)
    total = completed + failed + pending
    
    state = {
        "status": "OPERATIONAL",
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "worker_nodes": worker_nodes,
        "render_queue": {
            "total_jobs": total,
            "completed_jobs": completed,
            "failed_jobs": failed,
            "pending_jobs": pending,
            "avg_frame_time_sec": round(random.uniform(8.5, 14.0), 2)
        }
    }
    
    # Write State
    with open(STATE_PATH, "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)
    
    # Write events to log (jsonl)
    events = [
        {
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "worker": "GPU_Worker_01",
            "event": "FRAME_COMPLETED",
            "job_id": f"job_{random.randint(1000, 9999)}",
            "frame": random.randint(0, 100),
            "duration_sec": round(random.uniform(9.0, 12.5), 2)
        },
        {
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "worker": "CPU_Worker_01",
            "event": "COMPRESS_START",
            "job_id": f"job_{random.randint(1000, 9999)}",
            "frame": random.randint(0, 100),
            "duration_sec": 0.0
        }
    ]
    
    with open(EVENTS_PATH, "a", encoding="utf-8") as f:
        for ev in events:
            f.write(json.dumps(ev) + "\n")
            
    print(f"Render farm telemetry compiled and updated successfully at {STATE_PATH}")

if __name__ == "__main__":
    generate_telemetry()
