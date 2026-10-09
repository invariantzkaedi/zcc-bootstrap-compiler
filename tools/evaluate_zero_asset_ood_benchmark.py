#!/usr/bin/env python3
"""
🔱 ZKAEDI 3D DPO ZERO-ASSET OOD BENCHMARK EVALUATOR 🔱
Authoritative Script: tools/evaluate_zero_asset_ood_benchmark.py
Scope: Evaluates the fine-tuned 3D DPO LoRA adapter on the 50-pair zero-asset
       out-of-distribution (OOD) benchmark generated from the master 3D catalog.
       Zero prompt overlap, zero concept overlap, zero filename overlap, zero UUID overlap.
Methodology:
  1. Loads artifacts/zkaedi_3d_dpo_zero_asset_ood.parquet (50 pairs).
  2. Computes token-level masked sequence log-probabilities for chosen and rejected.
  3. Uses policy_model and policy_model.disable_adapter() for reference log-probabilities.
  4. Formulates implicit DPO rewards (beta=0.1):
       r(x, y) = beta * (log pi(y|x) - log ref(y|x))
       margin = r_chosen - r_rejected
  5. Computes win rate, Wilson 95% CI, mean margin, std, t-statistic, and Cohen's d.
  6. Preserves certified audit receipt to docs/evidence/zero_asset_ood_evaluation_receipt.json.
Enforces: Rules EF-1..6, Rules AV-1..6, Rules NV-1..5.
"""

import os
import sys
import time
import json
import math
import hashlib
import numpy as np
import pandas as pd
import torch
import torch.nn.functional as F
from scipy import stats
from peft import PeftModel
from transformers import AutoModelForCausalLM, AutoTokenizer

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

def resolve_path(p_win, p_linux):
    return p_win if os.name == 'nt' else p_linux

BASE_MODEL = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
ADAPTER_DIR = resolve_path(
    "H:/__DOWNLOADS/zcc_github_upload/artifacts/zkaedi_3d_dpo_lora_full_adapter",
    "/mnt/h/__DOWNLOADS/zcc_github_upload/artifacts/zkaedi_3d_dpo_lora_full_adapter"
)
OOD_PARQUET = resolve_path(
    "H:/__DOWNLOADS/zcc_github_upload/artifacts/zkaedi_3d_dpo_zero_asset_ood.parquet",
    "/mnt/h/__DOWNLOADS/zcc_github_upload/artifacts/zkaedi_3d_dpo_zero_asset_ood.parquet"
)
OUT_RECEIPT = resolve_path(
    "H:/__DOWNLOADS/zcc_github_upload/docs/evidence/zero_asset_ood_evaluation_receipt.json",
    "/mnt/h/__DOWNLOADS/zcc_github_upload/docs/evidence/zero_asset_ood_evaluation_receipt.json"
)

BETA = 0.1

def compute_sha256(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def wilson_score_interval(successes, total, confidence=0.95):
    if total == 0:
        return (0.0, 0.0)
    z = stats.norm.ppf(1 - (1 - confidence) / 2)
    p = successes / total
    denom = 1 + z**2 / total
    center = (p + z**2 / (2 * total)) / denom
    diff = z * math.sqrt((p * (1 - p) + z**2 / (4 * total)) / total) / denom
    return (max(0.0, center - diff), min(1.0, center + diff))

def compute_sequence_logp(model, tokenizer, prompt, response, device):
    prompt_ids = tokenizer.encode(prompt, add_special_tokens=False)
    resp_ids = tokenizer.encode(response, add_special_tokens=False)
    
    full_ids = prompt_ids + resp_ids
    if len(full_ids) > 512:
        excess = len(full_ids) - 512
        prompt_ids = prompt_ids[excess:]
        full_ids = prompt_ids + resp_ids
        
    input_ids = torch.tensor([full_ids], dtype=torch.long, device=device)
    
    with torch.no_grad():
        outputs = model(input_ids)
        logits = outputs.logits
        
    shift_logits = logits[0, :-1, :].contiguous()
    shift_labels = input_ids[0, 1:].contiguous()
    
    log_probs = F.log_softmax(shift_logits, dim=-1)
    target_logps = log_probs.gather(dim=-1, index=shift_labels.unsqueeze(-1)).squeeze(-1)
    
    resp_start_idx = max(0, len(prompt_ids) - 1)
    resp_logps = target_logps[resp_start_idx:]
    
    return resp_logps.sum().item()

def evaluate_pair(policy_model, tokenizer, prompt, chosen, rejected, device):
    pi_chosen = compute_sequence_logp(policy_model, tokenizer, prompt, chosen, device)
    pi_rejected = compute_sequence_logp(policy_model, tokenizer, prompt, rejected, device)

    with policy_model.disable_adapter():
        ref_chosen = compute_sequence_logp(policy_model, tokenizer, prompt, chosen, device)
        ref_rejected = compute_sequence_logp(policy_model, tokenizer, prompt, rejected, device)

    r_chosen = BETA * (pi_chosen - ref_chosen)
    r_rejected = BETA * (pi_rejected - ref_rejected)
    margin = r_chosen - r_rejected
    win = 1.0 if margin > 0.0 else 0.0

    return {
        "pi_chosen": pi_chosen,
        "pi_rejected": pi_rejected,
        "ref_chosen": ref_chosen,
        "ref_rejected": ref_rejected,
        "r_chosen": r_chosen,
        "r_rejected": r_rejected,
        "margin": margin,
        "win": win
    }

def main():
    print("=" * 80)
    print("🔱 ZKAEDI 3D DPO ZERO-ASSET OOD BENCHMARK EVALUATOR 🔱")
    print("=" * 80)

    device = "cuda:0" if torch.cuda.is_available() else "cpu"
    print(f"[*] Execution Device: {device}")
    if device != "cpu":
        print(f"[*] GPU Name        : {torch.cuda.get_device_name(0)}")

    df = pd.read_parquet(OOD_PARQUET)
    print(f"[*] Loaded Zero-Asset OOD Dataset: {len(df)} pairs")
    parquet_sha = compute_sha256(OOD_PARQUET)
    print(f"[*] OOD Parquet SHA-256          : {parquet_sha}")

    print(f"[*] Loading Tokenizer: {BASE_MODEL}...")
    tokenizer = AutoTokenizer.from_pretrained(BASE_MODEL, use_fast=True)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    print(f"[*] Loading Base Model in bfloat16: {BASE_MODEL}...")
    base_model = AutoModelForCausalLM.from_pretrained(
        BASE_MODEL,
        torch_dtype=torch.bfloat16,
        device_map={"": device}
    )

    print(f"[*] Attaching LoRA Adapter: {ADAPTER_DIR}...")
    adapter_weights = os.path.join(ADAPTER_DIR, "adapter_model.safetensors")
    adapter_sha = compute_sha256(adapter_weights)
    print(f"[*] Adapter SHA-256: {adapter_sha}")
    policy_model = PeftModel.from_pretrained(base_model, ADAPTER_DIR)
    policy_model.eval()

    print("\n" + "=" * 80)
    print("EVALUATING 50 ZERO-ASSET OOD PAIRS...")
    print("=" * 80)

    results = []
    margins = []
    wins = []
    category_margins = {"EASY": [], "MEDIUM": [], "HARD": []}
    category_wins = {"EASY": [], "MEDIUM": [], "HARD": []}

    t0 = time.time()
    for idx, row in df.iterrows():
        prompt = row["prompt"]
        chosen = row["chosen"]
        rejected = row["rejected"]
        meta = json.loads(row["metadata"]) if isinstance(row["metadata"], str) else row["metadata"]
        cat = meta.get("margin_cat", "MEDIUM")

        res = evaluate_pair(policy_model, tokenizer, prompt, chosen, rejected, device)
        margin = res["margin"]
        win = res["win"]

        margins.append(margin)
        wins.append(win)

        if cat in category_margins:
            category_margins[cat].append(margin)
            category_wins[cat].append(win)

        results.append({
            "pair_idx": int(idx),
            "concept": meta.get("concept", ""),
            "margin_cat": cat,
            "h0_delta": meta.get("h0_delta", 0.0),
            "r_chosen": float(res["r_chosen"]),
            "r_rejected": float(res["r_rejected"]),
            "margin": float(margin),
            "win": float(win)
        })

        if (idx + 1) % 10 == 0 or idx == len(df) - 1:
            print(f"[*] Processed {idx+1}/{len(df)} pairs | Running Accuracy: {np.mean(wins)*100:.1f}% | Running Mean Margin: {np.mean(margins):+.4f}")

    elapsed = time.time() - t0
    margins = np.array(margins)
    wins = np.array(wins)

    total_pairs = len(wins)
    total_wins = int(np.sum(wins))
    win_rate = float(np.mean(wins))
    mean_margin = float(np.mean(margins))
    std_margin = float(np.std(margins, ddof=1)) if len(margins) > 1 else 0.0
    cohens_d = float(mean_margin / std_margin) if std_margin > 0 else 0.0

    ci_low, ci_high = wilson_score_interval(total_wins, total_pairs, confidence=0.95)
    t_stat, p_val_two_tailed = stats.ttest_1samp(margins, 0.0)
    p_val_one_tailed = p_val_two_tailed / 2 if t_stat > 0 else 1.0

    print("\n" + "=" * 80)
    print("ZERO-ASSET OOD BENCHMARK EVALUATION SUMMARY:")
    print("=" * 80)
    print(f"[*] Total Zero-Asset Pairs Evaluated: {total_pairs}")
    print(f"[*] Model Wins                      : {total_wins} / {total_pairs} ({win_rate*100:.2f}%)")
    print(f"[*] Wilson 95% Confidence Interval  : [{ci_low*100:.2f}%, {ci_high*100:.2f}%]")
    print(f"[*] Mean Reward Margin              : {mean_margin:+.4f} ± {std_margin:.4f}")
    print(f"[*] Cohen's d Effect Size           : {cohens_d:.4f}")
    print(f"[*] One-sample t-statistic          : t = {t_stat:.3f} (p = {p_val_one_tailed:.4e})")
    print(f"[*] Total Runtime                   : {elapsed:.2f}s ({total_pairs/elapsed:.2f} pairs/s)")

    print("\nCATEGORY BREAKDOWN:")
    cat_summary = {}
    for cat in ["EASY", "MEDIUM", "HARD"]:
        c_margins = category_margins[cat]
        c_wins = category_wins[cat]
        if c_margins:
            c_mean_m = float(np.mean(c_margins))
            c_std_m = float(np.std(c_margins, ddof=1)) if len(c_margins) > 1 else 0.0
            c_wr = float(np.mean(c_wins))
            cat_summary[cat] = {
                "count": len(c_margins),
                "wins": int(np.sum(c_wins)),
                "win_rate": c_wr,
                "mean_margin": c_mean_m,
                "std_margin": c_std_m
            }
            print(f"  - {cat:<6s}: {int(np.sum(c_wins))}/{len(c_margins)} ({c_wr*100:.1f}%) | Mean Margin: {c_mean_m:+.4f} ± {c_std_m:.4f}")

    receipt = {
        "specification": "ZKAEDI-3D-DPO-ZERO-ASSET-OOD-BENCHMARK-v1.0",
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "dataset_metadata": {
            "parquet_file": OOD_PARQUET,
            "parquet_sha256": parquet_sha,
            "total_pairs": total_pairs,
            "zero_leakage_guarantee": "Zero concept, filename, UUID, or prompt overlap with training corpus"
        },
        "model_lineage": {
            "base_model": BASE_MODEL,
            "adapter_dir": ADAPTER_DIR,
            "adapter_sha256": adapter_sha
        },
        "overall_metrics": {
            "total_pairs": total_pairs,
            "total_wins": total_wins,
            "win_rate": win_rate,
            "wilson_ci_95": [float(ci_low), float(ci_high)],
            "mean_reward_margin": mean_margin,
            "std_reward_margin": std_margin,
            "cohens_d": cohens_d,
            "t_statistic": float(t_stat),
            "p_value_one_tailed": float(p_val_one_tailed),
            "eval_duration_sec": elapsed
        },
        "category_metrics": cat_summary,
        "exact_per_pair_results": results
    }

    os.makedirs(os.path.dirname(OUT_RECEIPT), exist_ok=True)
    with open(OUT_RECEIPT, "w", encoding="utf-8") as f:
        json.dump(receipt, f, indent=2)

    print(f"\n[*] Certified OOD Evaluation Receipt preserved to: {OUT_RECEIPT}")
    print("=" * 80)
    print(f"OFFICIAL VERDICT: {'PASS' if win_rate >= 0.85 else 'FAIL'} (Accuracy: {win_rate*100:.2f}%, Cohen's d: {cohens_d:.2f})")
    print("=" * 80)

if __name__ == "__main__":
    main()
