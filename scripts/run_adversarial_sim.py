import json
import time
import random
from pathlib import Path
from implementation.core.persistent_universe import PersistentUniverse
from implementation.security.adversarial_simulation import AdversarialSimulation

class MockRequest:
    def __init__(self):
        self.story = {}
        self.world = {"locations": ["Deck A"]}
        self.scene = {
            "shot_id": "shot_01",
            "type": "Establishing Shot",
            "subject": "The Gate",
            "start_seconds": 0.0,
            "end_seconds": 5.0,
            "duration_seconds": 5.0,
            "lens": "35mm prime",
            "movement": "Pan"
        }
        self.character = {"name": "Operator", "vulnerability_index": 0.5}
        self.style = "Neon Noir"

def main():
    print("🚀 [Adversarial Sim] Launching 100-Year Adversarial Verification Run...")
    output_dir = Path("output")
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Clean up previous run outputs
    for fname in ["recovery_history.json", "security_state.json", "trust_ledger.jsonl", "causal_audit_log.jsonl", "security_world_graph.json"]:
        p = output_dir / fname
        if p.exists():
            p.unlink()
            
    # 1. Initialize persistent universe
    universe = PersistentUniverse(output_dir)
    adversarial_driver = AdversarialSimulation(universe.security_graph)
    
    nodes_pool = ["DirectorNode", "ResearchNode", "AssetNode", "RenderNode", "ValidationNode"]
    mock_request = MockRequest()
    
    breaches_injected = []
    
    start_time = time.time()
    
    # Run 100 simulated ticks
    for year in range(1, 101):
        # Tick the universe
        universe.run_universe_tick(mock_request)
        
        # Inject controlled compromise every 20 simulated years
        if year in [20, 40, 60, 80, 100]:
            active_nodes = [
                eid for eid, entity in universe.security_graph.identity_engine.identities.items()
                if entity.get("type") == "node" and entity.get("status") == "ACTIVE"
            ]
            target_node = random.choice(active_nodes)
            res = adversarial_driver.inject_compromise(target_node, year)
            breaches_injected.append(res)
            
    end_time = time.time()
    elapsed = end_time - start_time
    
    # Read recovery registry history
    rec_history_path = output_dir / "recovery_history.json"
    recoveries_count = 0
    if rec_history_path.exists():
        with open(rec_history_path, "r", encoding="utf-8") as f:
            rec_history = json.load(f)
            recoveries_count = len(rec_history)
            
    # Verify status
    status = "SUCCESS" if recoveries_count == len(breaches_injected) else "FAILED"
    
    report = {
        "status": status,
        "total_years_simulated": 100,
        "breaches_injected_count": len(breaches_injected),
        "recoveries_completed_count": recoveries_count,
        "elapsed_seconds": round(elapsed, 4),
        "breaches": breaches_injected
    }
    
    report_path = output_dir / "adversarial_simulation_report.json"
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
        
    print(f"✅ [Adversarial Sim] Run Complete. Status: {status}")
    print(f"📊 Total Injected: {len(breaches_injected)}")
    print(f"📊 Total Recoveries: {recoveries_count}")
    print(f"📊 Report saved to: {report_path}")

if __name__ == "__main__":
    main()
