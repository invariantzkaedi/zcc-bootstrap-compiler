import json
import os
from pathlib import Path
from implementation.swarm.cluster_node import DistributedFabric

def main():
    print("🌐 [Cluster Fabric Tester] Booting distributed node cluster simulation...")
    fabric = DistributedFabric()
    cluster_state = fabric.execute_distributed_run("Ashes of Avalon")
    
    # Save output/cluster_state.json
    output_dir = Path("output")
    output_dir.mkdir(parents=True, exist_ok=True)
    
    report_path = output_dir / "cluster_state.json"
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(cluster_state, f, indent=2)
        
    print("✅ [Cluster Fabric Tester] Simulation run finished.")
    print(f"📊 Report written to: {report_path}")

if __name__ == "__main__":
    main()
