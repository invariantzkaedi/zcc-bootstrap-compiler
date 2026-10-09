#!/usr/bin/env python3
"""
🔱 ZKAEDI 3D DPO NEAR-BOUNDARY EPSILON-MARGIN EVALUATOR (IDEA 5) 🔱
Authoritative Script: tools/evaluate_epsilon_hard_negatives.py
Scope: Evaluates the live 229-step LoRA policy against the 100 near-boundary epsilon-margin
       hard-negative pairs mined from 3d_assets_catalog.db.
Enforces: Rules EF-1..6, Rules AV-1..6 (Authenticity Verification), and Rules NV-1..5 (Numerical Verification).
"""

import os
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

import time
import json
import math
import hashlib
import argparse
import numpy as np
import pandas as pd
from scipy import stats
import torch
import torch.nn.functional as F
from peft import PeftModel
from transformers import AutoModelForCausalLM, AutoTokenizer

def resolve_path(p_win, p_linux):
    return p_win if os.name == 'nt' else p_linux

PINNED_BASE_MODEL_ID = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
PINNED_BASE_REVISION = "2e1fd397ee46e1388853d2af2c993145b0f1098a"

DEFAULT_PARQUET = resolve_path(
    r"H:\__DOWNLOADS\zcc_github_upload\artifacts\zkaedi_3d_dpo_epsilon_hard_negatives.parquet",
    "/mnt/h/__DOWNLOADS/zcc_github_upload/artifacts/zkaedi_3d_dpo_epsilon_hard_negatives.parquet"
)
DEFAULT_ADAPTER = resolve_path(
    r"H:\__DOWNLOADS\zcc_github_upload\artifacts\zkaedi_3d_dpo_lora_full_adapter",
    "/mnt/h/__DOWNLOADS/zcc_github_upload/artifacts/zkaedi_3d_dpo_lora_full_adapter"
)
DEFAULT_RECEIPT = resolve_path(
    r"H:\__DOWNLOADS\zcc_github_upload\docs\evidence\epsilon_hard_negatives_evaluation_receipt.json",
    "/mnt/h/__DOWNLOADS/zcc_github_upload/docs/evidence/epsilon_hard_negatives_evaluation_receipt.json"
)

def compute_sha256(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def wilson_score_interval(successes, total, confidence=0.95):
    if total == 0:
        return (0.0, 0.0)
    z = 1.959963984540054 if confidence == 0.95 else 1.96
    p = successes / total
    denom = 1 + z**2 / total
    center = (p + z**2 / (2 * total)) / denom
    diff = z * math.sqrt((p * (1 - p) + z**2 / (4 * total)) / total) / denom
    return (max(0.0, center - diff), min(1.0, center + diff))

def compute_sequence_logp_joint(model, tokenizer, prompt, response, device, max_length=512):
    eos = tokenizer.eos_token
    resp_text = response + (eos if not response.endswith(eos) else "")
    joint_text = prompt + resp_text
    
    p_char_len = len(prompt)
    enc = tokenizer(joint_text, return_offsets_mapping=True, add_special_tokens=False)
    full_ids = enc.input_ids
    offsets = enc.offset_mapping
    
    if len(full_ids) > max_length:
        excess = len(full_ids) - max_length
        full_ids = full_ids[excess:]
        offsets = offsets[excess:]
        
    offset_mask = [1 if s >= p_char_len else 0 for s, e in offsets]
    input_ids = torch.tensor([full_ids], dtype=torch.long, device=device)
    
    with torch.no_grad():
        out = model(input_ids)
        logits = out.logits
        
    shift_logits = logits[0, :-1, :].contiguous()
    shift_labels = input_ids[0, 1:].contiguous()
    shift_mask = torch.tensor(offset_mask[1:], dtype=torch.float32, device=device)
    
    log_probs = F.log_softmax(shift_logits, dim=-1)
    target_logps = log_probs.gather(dim=-1, index=shift_labels.unsqueeze(-1)).squeeze(-1)
    resp_logps = target_logps * shift_mask
    
    assert torch.isfinite(resp_logps).all(), "Rule AV-3 Violation: Non-finite token log-probabilities encountered"
    logp_sum = resp_logps.sum().item()
    assert math.isfinite(logp_sum), f"Rule AV-3 Violation: Non-finite log-likelihood sum: {logp_sum}"
    
    return logp_sum

def run_evaluation(parquet_path=None, adapter_path=None, out_receipt_path=None, dry_run=False):
    print("=" * 80)
    print("🔱 ZKAEDI 3D DPO NEAR-BOUNDARY EPSILON-MARGIN HARD-NEGATIVE EVALUATION 🔱")
    print("=========================================================================")
    
    if parquet_path is None:
        parquet_path = DEFAULT_PARQUET
    if adapter_path is None:
        adapter_path = DEFAULT_ADAPTER
    if out_receipt_path is None:
        out_receipt_path = DEFAULT_RECEIPT
        
    assert os.path.exists(parquet_path), f"Parquet dataset missing: {parquet_path}"
    assert os.path.exists(adapter_path), f"Adapter path missing: {adapter_path}"
    
    parquet_sha = compute_sha256(parquet_path)
    print(f"[*] Epsilon Parquet Path:   {parquet_path}")
    print(f"[*] Epsilon Parquet SHA:    {parquet_sha}")
    print(f"[*] Adapter Path:           {adapter_path}")
    
    df = pd.read_parquet(parquet_path)
    total_pairs = len(df)
    print(f"[*] Loaded {total_pairs} near-boundary epsilon-margin pairs.")
    
    tokenizer = AutoTokenizer.from_pretrained(
        PINNED_BASE_MODEL_ID,
        revision=PINNED_BASE_REVISION,
        local_files_only=True
    )
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token
        
    if dry_run:
        print("\n[*] DRY RUN ACTIVE: Simulating evaluation...")
        mock_margins = [0.15 + np.random.normal(0.0, 0.03) for _ in range(total_pairs)]
        print(f"[*] Simulated Mean Margin: {np.mean(mock_margins):+.4f}")
        print("[PASS] DRY RUN COMPLETE.")
        return True

    device = "cuda:0" if torch.cuda.is_available() else "cpu"
    print(f"[*] Target GPU Device: {device}")
    
    print("[*] Loading Base Model...")
    base_model = AutoModelForCausalLM.from_pretrained(
        PINNED_BASE_MODEL_ID,
        revision=PINNED_BASE_REVISION,
        torch_dtype=torch.bfloat16,
        device_map=device,
        local_files_only=True
    )
    
    print("[*] Attaching LoRA Adapter...")
    model = PeftModel.from_pretrained(base_model, adapter_path)
    model.eval()
    
    BETA = 0.1
    margins = []
    pair_results = []
    
    print(f"[*] Computing implicit DPO rewards (β={BETA}) across {total_pairs} pairs...")
    t0 = time.time()
    
    for idx, row in df.iterrows():
        p = row["prompt"]
        c = row["chosen"]
        r = row["rejected"]
        meta = json.loads(row["metadata"]) if isinstance(row["metadata"], str) else row["metadata"]
        
        pi_c = compute_sequence_logp_joint(model, tokenizer, p, c, device)
        pi_r = compute_sequence_logp_joint(model, tokenizer, p, r, device)
        
        with model.disable_adapter():
            ref_c = compute_sequence_logp_joint(model, tokenizer, p, c, device)
            ref_r = compute_sequence_logp_joint(model, tokenizer, p, r, device)
            
        r_c = BETA * (pi_c - ref_c)
        r_r = BETA * (pi_r - ref_r)
        m = r_c - r_r
        margins.append(m)
        
        pair_results.append({
            "idx": idx,
            "concept": meta.get("concept", ""),
            "winner_file": meta.get("winner_file", ""),
            "loser_file": meta.get("loser_file", ""),
            "size_ratio": meta.get("size_ratio", 1.0),
            "delta_h0_norm": meta.get("delta_h0_norm", 0.0),
            "margin": float(m),
            "r_chosen": float(r_c),
            "r_rejected": float(r_r),
            "win": bool(m > 0.0)
        })
        
        if (idx + 1) % 25 == 0 or (idx + 1) == total_pairs:
            cur_wins = sum(1 for res in pair_results if res["win"])
            print(f"  Processed {idx + 1:3d}/{total_pairs}: Current Wins = {cur_wins}/{idx + 1} ({cur_wins/(idx + 1)*100:.1f}%), Mean Margin = {np.mean(margins):+.4f}")
            
    eval_time = time.time() - t0
    m_arr = np.array(margins)
    wins = int(sum(1 for m in m_arr if m > 0.0))
    acc = wins / total_pairs
    ci_low, ci_high = wilson_score_interval(wins, total_pairs)
    
    mean_m = float(np.mean(m_arr))
    std_m = float(np.std(m_arr, ddof=1))
    t_stat = mean_m / (std_m / math.sqrt(total_pairs))
    p_val = float(1.0 - stats.t.cdf(t_stat, df=total_pairs - 1))
    cohens_d = mean_m / std_m
    
    print("\n" + "=" * 80)
    print("📊 NEAR-BOUNDARY EPSILON-MARGIN HARD-NEGATIVE EVALUATION RESULTS:")
    print("=" * 80)
    print(f"[*] Total Evaluated Pairs:         {total_pairs}")
    print(f"[*] Total Evaluation Runtime:      {eval_time:.2f}s ({eval_time/total_pairs:.2f}s/pair)")
    print(f"[*] Policy Win Rate:               {wins} / {total_pairs} ({acc * 100:.2f}%)")
    print(f"[*] Wilson 95% Confidence Interval:[{ci_low * 100:.2f}%, {ci_high * 100:.2f}%]")
    print(f"[*] Mean Implicit Reward Margin:   {mean_m:+.4f} ± {std_m:.4f}")
    print(f"[*] Min / Max Margin:              [{np.min(m_arr):+.4f}, {np.max(m_arr):+.4f}]")
    print(f"[*] One-Sample t-Statistic:        t = {t_stat:.4f} (p = {p_val:.4e})")
    print(f"[*] Cohen's d Effect Size:         d = {cohens_d:.4f}")
    print("=========================================================================")
    
    # Save certified receipt
    receipt = {
        "specification": "ZKAEDI-3D-DPO-EPSILON-HARD-NEGATIVE-EVALUATION-v1.0",
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "verdict": "PASS" if acc >= 0.70 and mean_m > 0.0 else "FAIL",
        "lineage": {
            "base_model": PINNED_BASE_MODEL_ID,
            "revision": PINNED_BASE_REVISION,
            "adapter_path": adapter_path,
            "parquet_sha256": parquet_sha
        },
        "evaluation_metrics": {
            "total_pairs": total_pairs,
            "wins": wins,
            "accuracy": float(acc),
            "wilson_ci_95": [float(ci_low), float(ci_high)],
            "mean_margin": float(mean_m),
            "std_margin": float(std_m),
            "t_statistic": float(t_stat),
            "p_value": float(p_val),
            "cohens_d": float(cohens_d),
            "runtime_sec": float(eval_time)
        },
        "per_pair_results": pair_results
    }
    
    os.makedirs(os.path.dirname(out_receipt_path), exist_ok=True)
    with open(out_receipt_path, "w", encoding="utf-8") as f:
        json.dump(receipt, f, indent=2)
    print(f"[*] Certified Receipt Saved: {out_receipt_path}")
    
    # Assertions
    assert np.std(m_arr, ddof=1) > 1e-4, "Rule NV-3 Violation: Zero variance detected across margins"
    assert mean_m > 0.0, f"Rule EF-2 Violation: Non-positive mean margin: {mean_m}"
    assert acc >= 0.70, f"Policy accuracy below 70% threshold on epsilon pairs: {acc*100:.2f}%"
    
    print("\n🏆 EPSILON-MARGIN EVALUATION COMPLETE: OFFICIAL VERDICT PASS")
    return True

def main():
    parser = argparse.ArgumentParser(description="🔱 ZKAEDI 3D DPO Epsilon Hard-Negative Evaluator 🔱")
    parser.add_argument("--dry_run", action="store_true", help="Lineage test without GPU inference")
    args = parser.parse_args()
    
    success = run_evaluation(dry_run=args.dry_run)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
