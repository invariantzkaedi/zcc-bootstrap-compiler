import json
from implementation.services.learning_service.learning_service import SelfEvolutionLearningService

def main():
    print("🧠 [Self Evolution Tester] Booting Learning Service...")
    service = SelfEvolutionLearningService(memory_path="output/strategy_memory.json")
    
    print("🧠 [Self Evolution Tester] Recording trial runs...")
    # Simulate Noir success
    service.record_production_run("NoirDirector", 8.8, success=True)
    # Simulate Action failure
    service.record_production_run("ActionDirector", 5.2, success=False)
    # Simulate Noir success
    service.record_production_run("NoirDirector", 9.1, success=True)
    
    # Reload and print
    service.load_memory()
    print("✅ [Self Evolution Tester] Evolved strategy weights:")
    print(json.dumps(service.strategy, indent=2))

if __name__ == "__main__":
    main()
