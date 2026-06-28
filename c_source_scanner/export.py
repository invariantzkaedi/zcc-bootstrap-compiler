#!/usr/bin/env python3
import os
import json
import torch

def export_to_c_header(state, header_path):
    mean = state["mean"]
    std = state["std"]
    f1_weights = state["f1_weights"]
    sd = state["model_state_dict"]
    
    with open(header_path, "w", encoding="utf-8") as f:
        f.write("/* ========================================================================= */\n")
        f.write("/* ZCC C SOURCE CODE SCANNER WEIGHTS & INFERENCE                             */\n")
        f.write("/* Generated dynamically from trained specialists                            */\n")
        f.write("/* ========================================================================= */\n\n")
        f.write("#ifndef ZCC_C_SCANNER_WEIGHTS_H\n")
        f.write("#define ZCC_C_SCANNER_WEIGHTS_H\n\n")
        f.write("#include <math.h>\n\n")
        
        # 1. Normalization parameters
        f.write("/* Input normalization mean and standard deviation */\n")
        f.write(f"static const float ZCC_SCANNER_MEAN[32] = {{\n  " + ", ".join(f"{x:.8f}f" for x in mean) + "\n};\n\n")
        f.write(f"static const float ZCC_SCANNER_STD[32] = {{\n  " + ", ".join(f"{x:.8f}f" for x in std) + "\n};\n\n")
        
        # 2. Ensemble voting weights
        f.write("/* Reliability-weighted voting F1 coefficients */\n")
        f.write(f"static const float ZCC_SCANNER_F1_WEIGHTS[8] = {{\n  " + ", ".join(f"{x:.8f}f" for x in f1_weights) + "\n};\n\n")
        
        # 3. Weights and biases for the 8 specialists
        for i in range(8):
            fc1_w = sd[f"specialists.{i}.fc1.weight"].tolist()
            fc1_b = sd[f"specialists.{i}.fc1.bias"].tolist()
            fc2_w = sd[f"specialists.{i}.fc2.weight"].squeeze().tolist()
            fc2_b = sd[f"specialists.{i}.fc2.bias"].tolist()
            
            f.write(f"/* SPECIALIST {i} PARAMETERS */\n")
            
            # fc1 weight (32, 32)
            f.write(f"static const float ZCC_SPECIALIST_{i}_FC1_W[32][32] = {{\n")
            for r in fc1_w:
                f.write("  {" + ", ".join(f"{val:.8f}f" for val in r) + "},\n")
            f.write("};\n\n")
            
            # fc1 bias (32)
            f.write(f"static const float ZCC_SPECIALIST_{i}_FC1_B[32] = {{\n  " + ", ".join(f"{val:.8f}f" for val in fc1_b) + "\n};\n\n")
            
            # fc2 weight (32)
            f.write(f"static const float ZCC_SPECIALIST_{i}_FC2_W[32] = {{\n  " + ", ".join(f"{val:.8f}f" for val in fc2_w) + "\n};\n\n")
            
            # fc2 bias (1)
            f.write(f"static const float ZCC_SPECIALIST_{i}_FC2_B = {fc2_b[0]:.8f}f;\n\n")
            
        # 4. Embedded inference function
        f.write("/* Math helper functions */\n")
        f.write("static float zcc_scanner_sigmoid(float x) {\n")
        f.write("  return 1.0f / (1.0f + expf(-x));\n")
        f.write("}\n\n")
        
        f.write("static float zcc_scanner_relu(float x) {\n")
        f.write("  return x > 0.0f ? x : 0.0f;\n")
        f.write("}\n\n")
        
        f.write("/* Main scanning inference entry point. Takes a 32-dim feature vector and\n")
        f.write("   returns the reliability-weighted consensus vulnerability probability. */\n")
        f.write("static float zcc_scan_c_vulnerabilities(const float *features) {\n")
        f.write("  float norm_feat[32];\n")
        f.write("  int i, j, k;\n")
        f.write("  float consensus_prob = 0.0f;\n")
        f.write("  float total_weight = 0.0f;\n\n")
        
        f.write("  /* Normalize features */\n")
        f.write("  for (i = 0; i < 32; i++) {\n")
        f.write("    norm_feat[i] = (features[i] - ZCC_SCANNER_MEAN[i]) / ZCC_SCANNER_STD[i];\n")
        f.write("  }\n\n")
        
        f.write("  /* Evaluate all 8 specialists */\n")
        f.write("  for (k = 0; k < 8; k++) {\n")
        f.write("    float hidden[32];\n")
        f.write("    float fc2_out = 0.0f;\n")
        f.write("    float spec_prob = 0.0f;\n\n")
        
        f.write("    /* Layer 1: linear projection + ReLU */\n")
        f.write("    for (i = 0; i < 32; i++) {\n")
        f.write("      float sum = 0.0f;\n")
        f.write("      for (j = 0; j < 32; j++) {\n")
        f.write("        /* Select the correct specialist matrix pointer statically */\n")
        f.write("        switch(k) {\n")
        for idx in range(8):
            f.write(f"          case {idx}: sum += norm_feat[j] * ZCC_SPECIALIST_{idx}_FC1_W[i][j]; break;\n")
        f.write("        }\n")
        f.write("      }\n")
        f.write("      switch(k) {\n")
        for idx in range(8):
            f.write(f"        case {idx}: sum += ZCC_SPECIALIST_{idx}_FC1_B[i]; break;\n")
        f.write("      }\n")
        f.write("      hidden[i] = zcc_scanner_relu(sum);\n")
        f.write("    }\n\n")
        
        f.write("    /* Layer 2: output linear projection + Sigmoid */\n")
        f.write("    for (i = 0; i < 32; i++) {\n")
        f.write("      switch(k) {\n")
        for idx in range(8):
            f.write(f"        case {idx}: fc2_out += hidden[i] * ZCC_SPECIALIST_{idx}_FC2_W[i]; break;\n")
        f.write("      }\n")
        f.write("    }\n")
        f.write("    switch(k) {\n")
        for idx in range(8):
            f.write(f"      case {idx}: fc2_out += ZCC_SPECIALIST_{idx}_FC2_B; break;\n")
        f.write("    }\n")
        f.write("    spec_prob = zcc_scanner_sigmoid(fc2_out);\n\n")
        
        f.write("    /* Accumulate weighted voting */\n")
        f.write("    consensus_prob += spec_prob * ZCC_SCANNER_F1_WEIGHTS[k];\n")
        f.write("    total_weight += ZCC_SCANNER_F1_WEIGHTS[k];\n")
        f.write("  }\n\n")
        
        f.write("  return total_weight > 0.0f ? (consensus_prob / total_weight) : 0.0f;\n")
        f.write("}\n\n")
        
        f.write("#endif /* ZCC_C_SCANNER_WEIGHTS_H */\n")

def main():
    dir_path = os.path.dirname(os.path.abspath(__file__))
    weights_pt = os.path.join(dir_path, "weights.pt")
    
    if not os.path.exists(weights_pt):
        print(f"[Export Error] Weights file {weights_pt} not found. Run train.py first.")
        return
        
    print(f"[Export] Loading PyTorch checkpoint from {weights_pt}...")
    state = torch.load(weights_pt, map_location="cpu")
    
    # 1. Export C Header
    header_path = os.path.join(dir_path, "weights.h")
    print(f"[Export] Formatting weights to C header: {header_path}...")
    export_to_c_header(state, header_path)
    
    # 2. Export JSON representation for JS/WASM
    json_path = os.path.join(dir_path, "weights.json")
    print(f"[Export] Exporting weights to JSON: {json_path}...")
    
    # Convert state dict to regular lists
    serializable_sd = {}
    for key, tensor in state["model_state_dict"].items():
        serializable_sd[key] = tensor.tolist()
        
    serializable_state = {
        "mean": state["mean"],
        "std": state["std"],
        "f1_weights": state["f1_weights"],
        "model_state_dict": serializable_sd
    }
    
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(serializable_state, f, indent=2)
        
    # Check if safetensors is available, try to save
    try:
        from safetensors.torch import save_file
        safetensors_path = os.path.join(dir_path, "weights.safetensors")
        save_file(state["model_state_dict"], safetensors_path)
        print(f"[Export] Saved safetensors to {safetensors_path}")
    except ImportError:
        print("[Export] safetensors library not installed. Skipping .safetensors output.")
        
    print("[Export Complete] All targets exported successfully.")

if __name__ == "__main__":
    main()
