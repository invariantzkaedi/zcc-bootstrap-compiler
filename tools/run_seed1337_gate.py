#!/usr/bin/env python3
"""
🔱 ZKAEDI 3D DPO SECOND-SEED REPEATABILITY EXPERIMENT (SEED 1337) 🔱
Authoritative Script: tools/run_seed1337_gate.py
Scope: Executes a second, independent DPO training run with SEED=1337
       in an isolated Python process to evaluate cross-seed parameter
       stability and margin variance across the 51 held-out test pairs.
"""

import os
import sys
import time
import json
import random
import numpy as np
import pandas as pd
import torch
from datasets import Dataset
from peft import LoraConfig
from transformers import AutoModelForCausalLM, AutoTokenizer, set_seed
from trl import DPOTrainer, DPOConfig

def extract_metric(metrics_dict, suffix):
    if not isinstance(metrics_dict, dict):
        return None
    for k, v in metrics_dict.items():
        if k == suffix or k.endswith(suffix):
            return v
    return None

def run_seed1337():
    print("=" * 80)
    print("🔱 ZKAEDI 3D DPO SECOND-SEED EXPERIMENT (SEED 1337) 🔱")
    print("=" * 80)

    SEED = 1337
    random.seed(SEED)
    np.random.seed(SEED)
    torch.manual_seed(SEED)
    if torch.cuda.is_available():
        torch.cuda.manual_seed_all(SEED)
    set_seed(SEED)
    print(f"[*] Deterministic Seed Locked: {SEED}")

    train_parquet_path = "/mnt/c/Users/zkaed/.gemini/antigravity-ide/scratch/zkaedi_3d_dpo_train.parquet"
    eval_parquet_path = "/mnt/c/Users/zkaed/.gemini/antigravity-ide/scratch/zkaedi_3d_dpo_eval.parquet"
    output_dir = "/tmp/zkaedi_dpo_seed1337_output"
    os.makedirs(output_dir, exist_ok=True)

    train_df = pd.read_parquet(train_parquet_path)
    eval_df = pd.read_parquet(eval_parquet_path)
    train_sample_df = train_df.iloc[:40].copy()

    train_ds = Dataset.from_pandas(train_sample_df)
    eval_ds = Dataset.from_pandas(eval_df)
    print(f"[*] Train Pairs: {len(train_ds)} | Held-Out Eval Pairs: {len(eval_ds)}")

    model_id = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
    tokenizer = AutoTokenizer.from_pretrained(model_id, local_files_only=True)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    print(f"[*] Loading Foundation Model in bfloat16...")
    model = AutoModelForCausalLM.from_pretrained(
        model_id,
        dtype=torch.bfloat16,
        device_map="auto",
        local_files_only=True
    )

    peft_config = LoraConfig(
        r=8,
        lora_alpha=16,
        target_modules=["q_proj", "v_proj"],
        lora_dropout=0.05,
        bias="none",
        task_type="CAUSAL_LM"
    )

    dpo_config = DPOConfig(
        output_dir=output_dir,
        beta=0.1,
        max_length=512,
        per_device_train_batch_size=1,
        per_device_eval_batch_size=1,
        learning_rate=5e-5,
        max_steps=10,
        logging_steps=1,
        eval_strategy="no",
        report_to="none",
        save_strategy="no",
        seed=SEED,
        data_seed=SEED
    )

    trainer = DPOTrainer(
        model=model,
        ref_model=None,
        args=dpo_config,
        train_dataset=train_ds,
        eval_dataset=eval_ds,
        processing_class=tokenizer,
        peft_config=peft_config,
    )

    print("[*] Executing 10 DPO Optimization Steps under Seed 1337...")
    start_time = time.time()
    train_res = trainer.train()
    runtime = time.time() - start_time
    print(f"[*] Training Complete in {runtime:.2f} s. Final Loss: {train_res.training_loss:.4f}")

    print("[*] Evaluating Seed 1337 on all 51 Held-Out Pairs...")
    eval_metrics = trainer.evaluate()
    
    loss = extract_metric(eval_metrics, "loss")
    margin = extract_metric(eval_metrics, "rewards/margins")
    accuracy = extract_metric(eval_metrics, "rewards/accuracies")
    
    print(f"[*] Seed 1337 Eval Loss:            {loss:.4f}")
    print(f"[*] Seed 1337 Eval Reward Margin:   {margin:+.4f}")
    print(f"[*] Seed 1337 Eval Reward Accuracy: {accuracy * 100:.2f}%")

    out_file = "/mnt/h/__DOWNLOADS/zcc_github_upload/docs/evidence/seed1337_eval_metrics.json"
    with open(out_file, "w", encoding="utf-8") as f:
        json.dump({
            "seed": SEED,
            "training_loss": train_res.training_loss,
            "eval_loss": loss,
            "eval_margin": margin,
            "eval_accuracy": accuracy,
            "runtime_sec": runtime,
            "all_metrics": eval_metrics
        }, f, indent=2)

    print(f"[*] Seed 1337 metrics written to: {out_file}")
    print("=" * 80)
    print("🔱 SEED 1337 EXPERIMENT: COMPLETE 🔱")
    print("=" * 80)

if __name__ == "__main__":
    run_seed1337()
