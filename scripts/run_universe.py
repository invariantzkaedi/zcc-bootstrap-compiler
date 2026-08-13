import time
import threading
from implementation.core.persistent_universe import PersistentUniverse

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
    print("🌌 [Universe Tester] Starting Universe loop...")
    universe = PersistentUniverse(output_dir="output")
    
    stop_event = threading.Event()
    mock_request = MockRequest()
    
    # Start loop in a background thread
    t = threading.Thread(target=universe.execute_background_loop, args=(stop_event, 0.2, mock_request))
    t.start()
    
    # Let it tick for 2 seconds (10 ticks)
    time.sleep(2.0)
    
    print("🌌 [Universe Tester] Stopping Universe Engine daemon...")
    stop_event.set()
    t.join()
    
    print("✅ [Universe Tester] Test run complete.")
    print("📊 Generated universe_events.jsonl log size: ", universe.events_path.stat().st_size, "bytes")

if __name__ == "__main__":
    main()
