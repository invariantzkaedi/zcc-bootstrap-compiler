#!/usr/bin/env python3
import os
import sys
import glob

# Filter out CUDA user warnings from torch if imported implicitly by shell environments
import warnings
warnings.filterwarnings("ignore", category=UserWarning)

try:
    import z3
except ImportError:
    print("ERROR: Python z3-solver package is not installed.")
    sys.exit(1)

def verify_proof(proof_path):
    s = z3.Solver()
    try:
        # Parse SMT-LIBv2 assertions from file
        assertions = z3.parse_smt2_file(proof_path)
        s.add(assertions)
        res = s.check()
        if res == z3.unsat:
            print(f"[PROVEN] {os.path.basename(proof_path)}")
            return True
        elif res == z3.sat:
            print(f"[FAILED] {os.path.basename(proof_path)} - Solver returned SAT (Counterexample found!)")
            m = s.model()
            print(f"  Model: {m}")
            return False
        else:
            print(f"[UNKNOWN] {os.path.basename(proof_path)} - Solver returned: {res}")
            return False
    except Exception as e:
        print(f"[ERROR] {os.path.basename(proof_path)} - Failed to parse/solve: {e}")
        return False

def main():
    proof_dir = "/tmp/zcc_proofs"
    if len(sys.argv) > 1:
        proof_dir = sys.argv[1]

    if not os.path.isdir(proof_dir):
        print(f"ERROR: Proof directory '{proof_dir}' does not exist.")
        sys.exit(0)

    proof_files = glob.glob(os.path.join(proof_dir, "proof_*.smt2"))
    if not proof_files:
        print(f"No SMT proofs found in '{proof_dir}'.")
        sys.exit(0)

    print(f"=== Running Z3 Formal SMT Verification Gauntlet on {len(proof_files)} proofs ===")
    failures = 0
    for f in sorted(proof_files):
        success = verify_proof(f)
        if not success:
            failures += 1

    print("==================================================")
    if failures == 0:
        print("=== ALL OPTIMIZATIONS FORMALLY PROVEN CORRECT ===")
        sys.exit(0)
    else:
        print(f"=== FORMAL VERIFICATION FAILED: {failures} DIVERGENCES FOUND ===")
        sys.exit(1)

if __name__ == "__main__":
    main()
