import pandas as pd
import json
import os
import argparse

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()
    
    os.makedirs(args.out_dir, exist_ok=True)
    
    # 1. Generate 64 mock DPO sequences
    data = []
    for i in range(64):
        data.append({
            "prompt": f"Prompt {i}",
            "chosen": f"Chosen response {i}",
            "rejected": f"Rejected response {i}"
        })
        
    df = pd.DataFrame(data)
    parquet_path = os.path.join(args.out_dir, "dataset.parquet")
    df.to_parquet(parquet_path, index=False)
    print(f"Generated mock parquet dataset at: {parquet_path}")
    
    # 2. Generate split manifest with 48 train and 16 eval indices
    train_indices = list(range(48))
    eval_indices = list(range(48, 64))
    
    splits = {
        "train": train_indices,
        "eval": eval_indices
    }
    
    splits_path = os.path.join(args.out_dir, "split_manifest.json")
    with open(splits_path, "w", encoding="utf-8") as f:
        json.dump(splits, f, indent=2)
    print(f"Generated split manifest at: {splits_path}")

if __name__ == "__main__":
    main()
