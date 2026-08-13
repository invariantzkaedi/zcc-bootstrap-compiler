import json
import os
from pathlib import Path
from implementation.director.director_jury import DirectorJury

class MockRequest:
    def __init__(self):
        self.story = {
            "story_id": "story_mock_noir",
            "title": "The Glass Terminal",
            "genre": "Noir Drama",
            "themes": ["memory", "betrayal"]
        }
        self.world = {
            "locations": ["Terminal Deck B", "Sub-Alley 09"],
            "climate": "Corrosive Acid Rain"
        }
        self.scene = {
            "shot_id": "shot_mock_01",
            "type": "Establishing Shot",
            "subject": "The Glass Gate",
            "start_seconds": 0.0,
            "end_seconds": 5.0,
            "duration_seconds": 5.0,
            "lens": "35mm prime",
            "movement": "Slow Pan Right"
        }
        self.character = {
            "name": "Operator K",
            "vulnerability_index": 0.72,
            "pose": "standing close to wall"
        }
        self.style = "Neon Noir Cyberpunk"

def main():
    print("🎭 [Director Tournament] Starting Tournament Selection process...")
    
    request = MockRequest()
    assembly = {
        "world_graph": {
            "nodes": [
                {
                    "id": "node_env",
                    "type": "environment",
                    "name": "Environment Terminal B",
                    "uri": "r2://models/environment.glb",
                    "local_path": "assets/environment.glb"
                }
            ]
        },
        "placements": [
            {"node_id": "node_env", "x": 0.0, "y": 0.0, "z": 0.0, "rotation_y": 0.0, "scale": 1.0}
        ]
    }
    
    jury = DirectorJury()
    report = jury.evaluate_tournament(request.story, request, assembly)
    
    # Save output/director_tournament.json
    output_dir = Path("output")
    output_dir.mkdir(parents=True, exist_ok=True)
    
    report_path = output_dir / "director_tournament.json"
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
        
    print(f"✅ [Director Tournament] Tournament complete. Winner: {report['winner']}")
    print(f"📊 Report written to: {report_path}")

if __name__ == "__main__":
    main()
