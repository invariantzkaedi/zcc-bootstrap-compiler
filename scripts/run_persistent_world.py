import time
import threading
from implementation.core.persistent_world_engine import PersistentWorldEngine

def main():
    print("🌍 [Persistent World Tester] Booting World Engine loop...")
    engine = PersistentWorldEngine(history_log_path="output/world_history.jsonl")
    
    stop_event = threading.Event()
    
    # Start loop in a background thread
    t = threading.Thread(target=engine.execute_background_loop, args=(stop_event, 0.2))
    t.start()
    
    # Let it tick for 2 seconds (10 ticks)
    time.sleep(2.0)
    
    print("🌍 [Persistent World Tester] Stopping World Engine daemon...")
    stop_event.set()
    t.join()
    
    print("✅ [Persistent World Tester] Test run complete.")
    print("📊 Generated world_history.jsonl log size: ", engine.history_log_path.stat().st_size, "bytes")

if __name__ == "__main__":
    main()
