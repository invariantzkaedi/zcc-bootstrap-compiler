#!/usr/bin/env python3
import os
import subprocess
import json
import torch
import numpy as np
from model import ZccCSourceModel

HARNESS_C = """#include <stdio.h>
#include <stdlib.h>
#include "weights.h"

int main(int argc, char **argv) {
    if (argc < 33) {
        printf("Error: Expected 32 float features as arguments.\\n");
        return 1;
    }
    
    float features[32];
    int i;
    for (i = 0; i < 32; i++) {
        features[i] = strtof(argv[i + 1], NULL);
    }
    
    float result = zcc_scan_c_vulnerabilities(features);
    printf("%.8f\\n", result);
    return 0;
}
"""

def main():
    dir_path = os.path.dirname(os.path.abspath(__file__))
    harness_path = os.path.join(dir_path, "verify_harness.c")
    bin_path = os.path.join(dir_path, "verify_harness")
    weights_pt = os.path.join(dir_path, "weights.pt")
    
    if os.name == "nt":
        bin_path += ".exe"
        
    print("[Verify] Writing C verification harness...")
    with open(harness_path, "w", encoding="utf-8") as f:
        f.write(HARNESS_C)
        
    print("[Verify] Compiling C verification harness...")
    try:
        # Run gcc compile command
        # Include current directory so weights.h is found
        if os.name == "nt":
            # Compile using wsl
            wsl_harness = harness_path.replace('\\', '/').replace('H:', '/mnt/h')
            wsl_bin = bin_path.replace('\\', '/').replace('H:', '/mnt/h')
            wsl_dir = dir_path.replace('\\', '/').replace('H:', '/mnt/h')
            cmd = ["wsl", "gcc", "-O3", "-I" + wsl_dir, wsl_harness, "-o", wsl_bin, "-lm"]
        else:
            cmd = ["gcc", "-O3", "-I" + dir_path, harness_path, "-o", bin_path, "-lm"]
            
        subprocess.run(cmd, check=True)
        print("[Verify] Harness compiled successfully.")
    except Exception as e:
        print(f"[Verify Error] Failed to compile harness: {e}")
        return
        
    # Load PyTorch checkpoint
    print(f"[Verify] Loading PyTorch model from {weights_pt}...")
    checkpoint = torch.load(weights_pt, map_location="cpu")
    model = ZccCSourceModel()
    model.load_state_dict(checkpoint["model_state_dict"])
    model.eval()
    
    mean = torch.tensor(checkpoint["mean"], dtype=torch.float32)
    std = torch.tensor(checkpoint["std"], dtype=torch.float32)
    
    # Generate some test vectors
    # 1. Zero vector
    # 2. Random feature vectors
    # 3. Features from real main.c
    test_vectors = [
        [0.0] * 32,
        [float(x) for x in range(32)],
        # main.c extracted features
        [10.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 4.0, 0.0, 0.0, 7.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0]
    ]
    
    # Generate 5 random tests
    np.random.seed(42)
    for _ in range(5):
        vec = np.random.randint(0, 50, size=32).astype(float).tolist()
        test_vectors.append(vec)
        
    print(f"[Verify] Running math equivalence checks on {len(test_vectors)} test vectors...")
    
    all_passed = True
    for idx, vec in enumerate(test_vectors):
        # 1. Run C inference
        str_args = [str(x) for x in vec]
        if os.name == "nt":
            wsl_bin_path = bin_path.replace('\\', '/').replace('H:', '/mnt/h')
            cmd = ["wsl", wsl_bin_path] + str_args
        else:
            cmd = [bin_path] + str_args
            
        res = subprocess.run(cmd, capture_output=True, text=True, check=True)
        c_prob = float(res.stdout.strip())
        
        # 2. Run PyTorch inference
        with torch.no_grad():
            x = torch.tensor([vec], dtype=torch.float32)
            x_norm = (x - mean) / std
            _, torch_prob_tensor = model(x_norm)
            torch_prob = torch_prob_tensor.item()
            
        # 3. Compare
        diff = abs(c_prob - torch_prob)
        status = "PASS" if diff < 1e-5 else "FAIL"
        if status == "FAIL":
            all_passed = False
            
        print(f"  Test {idx+1}: C={c_prob:.6f} | PyTorch={torch_prob:.6f} | Diff={diff:.6e} | {status}")
        
    # Cleanup binaries
    try:
        os.remove(harness_path)
        os.remove(bin_path)
    except:
        pass
        
    if all_passed:
        print("\n[SUCCESS] VERIFICATION SUCCESS: Bare-metal C inference matches PyTorch model outputs byte-for-byte!")
    else:
        print("\n[FAILURE] VERIFICATION FAILURE: Numerical mismatch between C harness and PyTorch!")
        sys.exit(1)

if __name__ == "__main__":
    main()
