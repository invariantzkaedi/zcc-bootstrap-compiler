import json

import luckycomm
from agent_dispatcher import AgentDispatcher


def main():
    # 1. Initialize the dispatcher
    print("Initializing Agent Dispatcher...")
    dispatcher = AgentDispatcher("agents-skills-grouped.json")

    # 2. Simulate a crash in EVM lifter
    print("\nSimulating EVM Lifter Crash...")
    file_path = "evm_lifter.c"
    error_log = "segmentation fault during stack overflow check in MSTORE"
    phase = "codegen"

    # 3. Match the appropriate agent
    profile = dispatcher.match_agent(
        file_path=file_path, 
        error_log=error_log, 
        phase=phase
    )
    print("\nDispatcher Output:")
    print(json.dumps(profile, indent=2))

    # 4. Generate the verified LuckyComm Handoff
    print("\nGenerating LuckyComm Handoff Protocol V1...")
    
    # Mock verified artifacts
    artifacts = [
        {"path": "evm_lifter.c", "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "role": "source_target"},
        {"path": "corpusagents/corpus/mutated_evm_lifter/seed_047.json", "sha256": "8f434346648f6b96df89dda901c5176b10a6d83961dd3c1ac88b59b2dc327aa4", "role": "repro_seed"}
    ]
    
    handoff = luckycomm.create_handoff(
        dispatcher_output=profile,
        artifacts=artifacts,
        directive="run_crash_reproduction_with_token_stream"
    )

    print("\nValidated LuckyComm Output:")
    print(json.dumps(handoff, indent=2))

if __name__ == "__main__":
    main()
