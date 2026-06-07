import os
import sys
import subprocess

# Ensure local modules can be loaded
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from mesh_prime_scorer import PrimeScorer, PrimeConsensus

def main():
    dir_path = os.path.dirname(os.path.abspath(__file__))
    pb_path = os.path.join(dir_path, "prime_state_c.pb")
    
    # 1. Initialize and score mesh
    print("[PY-TEST] Initializing PrimeScorer...")
    scorer = PrimeScorer(h_0=1.0, eta=0.15, gamma=1.8, epsilon=0.07, beta=0.4, seed=777)
    consensus = scorer.score_mesh(0.92, 0.88, 0.95, context="mesh_c_interop_verify")
    
    print(f"[PY-TEST] Consensus Score: {consensus.consensus_score:.6f}")
    print(f"[PY-TEST] Jackpot Score  : {consensus.jackpot}")
    
    # 2. Serialize to C struct bytes
    print("[PY-TEST] Serializing to C binary struct format...")
    c_bytes = consensus.serialize_c_struct()
    
    with open(pb_path, "wb") as f:
        f.write(c_bytes)
    print(f"[PY-TEST] Wrote {len(c_bytes)} bytes to {pb_path}")
    
    # 3. Roundtrip test in Python
    print("[PY-TEST] Testing Python deserialization of C-style payload...")
    restored = PrimeConsensus.deserialize(c_bytes)
    assert abs(restored.consensus_score - consensus.consensus_score) < 1e-9
    assert restored.jackpot == consensus.jackpot
    assert restored.state.context == "mesh_c_interop_verify"
    print("OK: Python roundtrip validation PASSED!")
    
    # 4. Invoke C test runner inside WSL to verify cross-runtime compatibility
    print("[PY-TEST] Invoking C verification runner in WSL...")
    c_runner = "./src/test_prime_serialization"
    c_arg = "mesh_system/prime_state_c.pb"
    
    repo_root = os.path.abspath(os.path.join(dir_path, ".."))
    
    proc = subprocess.run(
        ["wsl", c_runner, c_arg],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    print("=== C Runner Output ===")
    print(proc.stdout)
    if proc.stderr:
        print("=== C Runner Error ===")
        print(proc.stderr)
        
    if proc.returncode == 0:
        print("OK: Cross-runtime C / Python binary inter-op verified successfully!")
    else:
        print(f"ERROR: C verification runner exited with code {proc.returncode}")
        sys.exit(proc.returncode)

if __name__ == "__main__":
    main()
