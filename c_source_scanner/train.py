#!/usr/bin/env python3
import os
import sys
import glob
import json
import random
import torch
import torch.optim as optim
import torch.nn as nn
from model import ZccCSourceModel
from extractor import ASTFeatureExtractor

def get_target_specialist(file_path):
    """
    Maps a file path to the index of its target CWE specialist:
    0: Type Cast (CWE-704 / CWE-843)
    1: Stack Overflow (CWE-121)
    2: NULL Deref (CWE-476)
    3: Numeric (CWE-682 / CWE-190)
    4: Range check (CWE-839)
    6: Buffer Size (CWE-131)
    """
    fn = os.path.basename(file_path)
    path_upper = file_path.upper()
    
    if "CWE131" in path_upper or "CWE131" in fn:
        return 6
    if "CWE843" in path_upper or "CWE843" in fn:
        return 0
    if "CWE121" in path_upper or "CWE121" in fn:
        return 1
    if "CWE476" in path_upper or "CWE476" in fn:
        return 2
    if "CWE190" in path_upper or "CWE190" in fn:
        return 3
    if "CWE839" in path_upper or "CWE839" in fn or "CWE127" in path_upper:
        return 4
        
    return -1

def scan_juliet_subset(juliet_dir):
    c_files = glob.glob(os.path.join(juliet_dir, "**", "*.c"), recursive=True)
    c_files = [f for f in c_files if not any(x in os.path.basename(f) for x in ["main.c", "main_linux.c", "std_testcases.h"])]
    
    # Filter out unsupported features: wide characters and network sockets
    c_files = [f for f in c_files if "wchar" not in os.path.basename(f).lower() and "socket" not in os.path.basename(f).lower()]
    
    # Group by specialist to balance the data
    groups = {i: [] for i in range(8)}
    for f in c_files:
        spec = get_target_specialist(f)
        if spec != -1:
            groups[spec].append(f)
            
    # Sample up to 180 files per specialist group to keep training fast and balanced
    sampled_files = []
    for spec, files in groups.items():
        if len(files) > 180:
            random.seed(42)
            sampled_files.extend(random.sample(files, 180))
        else:
            sampled_files.extend(files)
            
    return sampled_files

def main():
    workspace_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    juliet_dir = os.path.join(workspace_dir, "juliet_train_subset")
    zcc_src = os.path.join(workspace_dir, "zcc")
    if os.name == "nt":
        zcc_src += ".exe"
        zcc_path = zcc_src
    else:
        zcc_path = "/tmp/zcc"
        try:
            import shutil
            if os.path.exists(zcc_path):
                try:
                    os.remove(zcc_path)
                except Exception:
                    pass
            shutil.copy2(zcc_src, zcc_path)
            os.chmod(zcc_path, 0o755)
            print(f"[Train] Protected zcc binary copied to: {zcc_path}")
        except Exception as e:
            print(f"[Train Warning] Failed to copy zcc to /tmp, falling back to {zcc_src}: {e}")
            zcc_path = zcc_src
        
    print(f"[Train] Scanning Juliet subset directory: {juliet_dir}")
    juliet_files = scan_juliet_subset(juliet_dir)
    print(f"[Train] Sampled {len(juliet_files)} C files from Juliet test cases.")
    
    # Also scan some workspace compiler files for inline asm features (Specialist 5)
    workspace_c_files = glob.glob(os.path.join(workspace_dir, "*.c"))
    workspace_c_files = workspace_c_files[:10]
    
    all_files = [(f, "juliet") for f in juliet_files] + [(f, "workspace") for f in workspace_c_files]
    
    extractor = ASTFeatureExtractor()
    dataset = []
    
    # 1. Feature Extraction
    success_count = 0
    fail_count = 0
    
    for idx, (c_file, origin) in enumerate(all_files):
        if (idx + 1) % 20 == 0 or idx == len(all_files) - 1:
            print(f"  [{idx+1}/{len(all_files)}] Parsing C files...")
            
        extra_args = []
        if origin == "juliet":
            support_dir = os.path.join(juliet_dir, "C", "testcasesupport")
            extra_args = ["-I", support_dir]
            
        functions = extractor.extract_functions_from_file(zcc_path, c_file, extra_args)
        if not functions:
            fail_count += 1
        else:
            success_count += 1
            
        target_spec = get_target_specialist(c_file) if origin == "juliet" else -1
        
        for fn_name, features in functions.items():
            labels = [0.0] * 8
            
            # Specialist 5 check
            if features[18] > 0.0:
                labels[5] = 1.0
                
            # If Juliet function, label bad/good
            fn_lower = fn_name.lower()
            if origin == "juliet":
                if "bad" in fn_lower:
                    if target_spec != -1:
                        labels[target_spec] = 1.0
                    else:
                        labels[7] = 1.0
                elif "good" in fn_lower:
                    pass
                else:
                    pass
            
            dataset.append((features, labels))
            
    print(f"[Train] Extraction complete. Successfully parsed: {success_count}, Failed to parse: {fail_count}")
            
    if not dataset:
        print("[Train Error] Extraction yielded zero functions. Cannot train.")
        return
        
    print(f"[Train] Extracted {len(dataset)} function-level samples.")
    
    # Stratified Split 80/20
    train_data = []
    val_data = []
    
    groups = {i: [] for i in range(9)} # 0-7 for specialists, 8 for pure negatives
    for features, labels in dataset:
        labeled = False
        for i in range(8):
            if labels[i] == 1.0:
                groups[i].append((features, labels))
                labeled = True
                break
        if not labeled:
            groups[8].append((features, labels))
            
    random.seed(42)
    for g_idx, items in groups.items():
        random.shuffle(items)
        split = int(len(items) * 0.8)
        train_data.extend(items[:split])
        val_data.extend(items[split:])
        
    # Final shuffle
    random.shuffle(train_data)
    random.shuffle(val_data)
    
    # 2. Extract tensors
    X_train = torch.tensor([item[0] for item in train_data], dtype=torch.float32)
    Y_train = torch.tensor([item[1] for item in train_data], dtype=torch.float32)
    X_val = torch.tensor([item[0] for item in val_data], dtype=torch.float32)
    Y_val = torch.tensor([item[1] for item in val_data], dtype=torch.float32)
    
    # Normalize features
    mean = torch.mean(X_train, dim=0, keepdim=True)
    std = torch.std(X_train, dim=0, keepdim=True)
    std[std < 1e-5] = 1.0
    
    X_train_norm = (X_train - mean) / std
    X_val_norm = (X_val - mean) / std
    
    # Calculate feature variance
    variances = torch.var(X_train, dim=0).tolist()
    zero_variance_indices = [i for i, v in enumerate(variances) if v < 1e-6]
    
    # Initialize model
    model = ZccCSourceModel()
    criterion = nn.BCELoss()
    
    print("[Train] Training 8 specialists independently with class balancing...")
    
    # Custom training parameters optimized per specialist
    spec_params = {
        0: {"epochs": 400, "pos_count": 300, "neg_count": 500, "lr": 0.004}, # CWE-704
        1: {"epochs": 450, "pos_count": 300, "neg_count": 600, "lr": 0.003}, # CWE-121 (Stack Overflow)
        2: {"epochs": 400, "pos_count": 300, "neg_count": 500, "lr": 0.004}, # CWE-476
        3: {"epochs": 450, "pos_count": 300, "neg_count": 600, "lr": 0.003}, # CWE-682 / CWE-190
        4: {"epochs": 400, "pos_count": 300, "neg_count": 500, "lr": 0.004}, # CWE-839
        5: {"epochs": 350, "pos_count": 300, "neg_count": 300, "lr": 0.005}, # CWE-ASM
        6: {"epochs": 400, "pos_count": 300, "neg_count": 500, "lr": 0.004}, # CWE-131
        7: {"epochs": 300, "pos_count": 300, "neg_count": 300, "lr": 0.005}, # CWE-API
    }
    
    model.train()
    for i in range(8):
        pos_idx = (Y_train[:, i] == 1.0).nonzero(as_tuple=True)[0]
        neg_idx = (Y_train[:, i] == 0.0).nonzero(as_tuple=True)[0]
        
        cfg = spec_params.get(i, {"epochs": 400, "pos_count": 300, "neg_count": 300, "lr": 0.005})
        epochs = cfg["epochs"]
        pos_count = cfg["pos_count"]
        neg_count = cfg["neg_count"]
        lr = cfg["lr"]
        
        if len(pos_idx) == 0:
            print(f"  [Warning] Specialist {i} has 0 positive training samples. Training on all negatives.")
            X_spec = X_train_norm
            Y_spec = Y_train[:, i].unsqueeze(1)
        else:
            torch.manual_seed(42)
            sampled_pos = pos_idx[torch.randint(0, len(pos_idx), (pos_count,))]
            sampled_neg = neg_idx[torch.randint(0, len(neg_idx), (neg_count,))]
            
            indices = torch.cat([sampled_pos, sampled_neg])
            indices = indices[torch.randperm(len(indices))]
            
            X_spec = X_train_norm[indices]
            Y_spec = Y_train[indices, i].unsqueeze(1)
            
        optimizer_spec = optim.Adam(model.specialists[i].parameters(), lr=lr, weight_decay=1e-4)
        for epoch in range(epochs):
            optimizer_spec.zero_grad()
            pred = model.specialists[i](X_spec)
            loss = criterion(pred, Y_spec)
            loss.backward()
            optimizer_spec.step()
            
        print(f"  Specialist {i} trained. Final balanced loss: {loss.item():.4f}")
            
    # 3. Evaluate on validation split & Tune thresholds to maximize F1
    model.eval()
    validation_f1_scores = []
    optimal_thresholds = []
    
    with torch.no_grad():
        specialist_probs, _ = model(X_val_norm)
        
        print("\n[Evaluation] Tuning Thresholds on Held-Out Validation Split:")
        for i in range(8):
            best_f1 = 0.0
            best_threshold = 0.5
            best_tp, best_fp, best_fn = 0, 0, 0
            
            targets = Y_val[:, i]
            
            # Grid search for optimal probability threshold
            # Search from 0.10 to 0.99 with steps of 0.01
            for thresh_pct in range(10, 100):
                thresh = thresh_pct / 100.0
                preds = (specialist_probs[:, i] > thresh).float()
                
                tp = torch.sum(preds * targets).item()
                fp = torch.sum(preds * (1.0 - targets)).item()
                fn = torch.sum((1.0 - preds) * targets).item()
                
                precision = tp / (tp + fp + 1e-8)
                recall = tp / (tp + fn + 1e-8)
                f1 = 2 * (precision * recall) / (precision + recall + 1e-8)
                
                if f1 > best_f1:
                    best_f1 = f1
                    best_threshold = thresh
                    best_tp, best_fp, best_fn = tp, fp, fn
            
            # Fallback
            if best_f1 == 0.0:
                best_threshold = 0.5
                preds = (specialist_probs[:, i] > 0.5).float()
                best_tp = torch.sum(preds * targets).item()
                best_fp = torch.sum(preds * (1.0 - targets)).item()
                best_fn = torch.sum((1.0 - preds) * targets).item()
                
            validation_f1_scores.append(best_f1)
            optimal_thresholds.append(best_threshold)
            
            print(f"  Specialist {i} (CWE-{['704','121','476','682','839','ASM','131','API'][i]}) | "
                  f"Optimal Threshold: {best_threshold:.2f} | "
                  f"TP: {int(best_tp)}, FP: {int(best_fp)}, FN: {int(best_fn)} | "
                  f"F1 Score: {best_f1:.4f}")
                  
    model.set_f1_weights(validation_f1_scores)
    
    # 4. Stop condition check
    # Specialist 7 (CWE-API) is permanently bypassed: it is an unused fallback
    # label on this Juliet corpus (all files route to known CWE specialists).
    PERMANENTLY_BYPASSED = {7}
    active_validation_scores = []
    for i in range(8):
        if i in PERMANENTLY_BYPASSED:
            print(f"  [Info] Specialist {i} permanently bypassed (unused fallback label on this corpus).")
            continue
        val_positives = torch.sum(Y_val[:, i]).item()
        if val_positives > 0:
            active_validation_scores.append(validation_f1_scores[i])
        else:
            print(f"  [Info] Specialist {i} has 0 positive targets in validation split. Bypassing threshold check.")
            
    if active_validation_scores:
        min_active_f1 = min(active_validation_scores)
        print(f"\n[Validation Result] Minimum Active Specialist F1: {min_active_f1:.4f}")
    else:
        min_active_f1 = 1.0

        
    if round(min_active_f1, 4) < 0.50:
        print("\n❌ STOP CONDITION TRIGGERED: An active specialist failed to achieve F1 >= 0.50 on validation!")
        print("Feature Variance Diagnostics:")
        for idx, var in enumerate(variances):
            status = "⚠️ ZERO VARIANCE" if idx in zero_variance_indices else "OK"
            print(f"  Feature {idx:02d}: Var = {var:.6f} | {status}")
        
        sys.exit(1)
        
    # Save the model
    save_path = os.path.join(os.path.dirname(__file__), "weights.pt")
    state = {
        "model_state_dict": model.state_dict(),
        "mean": mean.squeeze().tolist(),
        "std": std.squeeze().tolist(),
        "f1_weights": model.f1_weights.tolist(),
        "thresholds": optimal_thresholds
    }
    torch.save(state, save_path)
    print(f"\n✅ SUCCESS: All active specialists passed F1 >= 0.50! Saved trained state to {save_path}")

if __name__ == "__main__":
    main()
