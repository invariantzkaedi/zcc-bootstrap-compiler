import os
import json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import pandas as pd

def main():
    frames_dir = "artifacts/frames"
    os.makedirs(frames_dir, exist_ok=True)
    
    metrics_path = "living-tensor-heatmap/public/data/eval_metrics.jsonl"
    if not os.path.exists(metrics_path):
        print(f"Error: {metrics_path} not found.")
        return
        
    df = pd.read_json(metrics_path, lines=True)
    
    metrics = ["eval_loss", "eval_preference_margin_mean", "eval_preference_margin_median", "eval_positive_margin_rate"]
    steps = sorted(df["step"].unique())
    
    # Simple matplotlib heatmap rendering step-by-step
    for i, step in enumerate(steps):
        sub = df[df["step"] <= step]
        pivot = sub.pivot_table(index="metric", columns="step", values="value", aggfunc="mean").reindex(metrics)
        
        plt.figure(figsize=(10, 5))
        plt.imshow(pivot.values, aspect="auto", interpolation="nearest", cmap="viridis")
        plt.colorbar(label="Value")
        plt.yticks(range(len(metrics)), metrics)
        plt.xticks(range(len(pivot.columns)), pivot.columns, rotation=45)
        plt.title(f"Living Tensor Gating Frame - Step {step}")
        plt.tight_layout()
        
        frame_path = os.path.join(frames_dir, f"frame_{i:04d}.png")
        plt.savefig(frame_path, dpi=140)
        plt.close()
        
    print(f"[+] Heatmap frames successfully exported to {frames_dir}/")

if __name__ == "__main__":
    main()
