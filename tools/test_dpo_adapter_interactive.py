#!/usr/bin/env python3
"""
tools/test_dpo_adapter_interactive.py — Interactive Test Harness & Evaluation Engine
===================================================================================
Tests the fine-tuned ZKAEDI 3D DPO LoRA adapter on RTX 5070 GPU hardware.
Supports:
  1. 'score': Exact DPO preference discrimination & reward margin (Δr) computation
  2. 'generate': Autoregressive generation of topological critiques and 3D assessments
  3. '--interactive': Interactive CLI REPL for real-time asset evaluation
"""

import sys
import os
import argparse
import time
import math
import torch
import torch.nn.functional as F
from pathlib import Path
from transformers import AutoTokenizer, AutoModelForCausalLM
from peft import PeftModel

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

DEFAULT_MODEL_ID = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
DEFAULT_ADAPTER_DIR = str(Path(__file__).parent.parent / "artifacts" / "zkaedi_3d_dpo_lora_full_adapter")

def compute_response_logp(model, tokenizer, prompt: str, response: str, device: str) -> float:
    """Computes exact sum of log-probabilities of response tokens conditioned on prompt."""
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
    return float(resp_logps.sum().item())

def score_pair(model, tokenizer, prompt: str, response_a: str, response_b: str, beta: float = 0.1, device: str = "cuda"):
    """
    Computes DPO reward for response A and B, returning reward margin:
      Δr = beta * [(log π_θ(A|x) - log π_ref(A|x)) - (log π_θ(B|x) - log π_ref(B|x))]
    """
    # 1. Policy model logps
    pi_logp_a = compute_response_logp(model, tokenizer, prompt, response_a, device)
    pi_logp_b = compute_response_logp(model, tokenizer, prompt, response_b, device)
    
    # 2. Reference model logps via disable_adapter()
    with model.disable_adapter():
        ref_logp_a = compute_response_logp(model, tokenizer, prompt, response_a, device)
        ref_logp_b = compute_response_logp(model, tokenizer, prompt, response_b, device)
        
    reward_a = beta * (pi_logp_a - ref_logp_a)
    reward_b = beta * (pi_logp_b - ref_logp_b)
    margin = reward_a - reward_b
    prob_a_preferred = 1.0 / (1.0 + math.exp(-margin))
    
    return {
        "pi_logp_a": pi_logp_a,
        "ref_logp_a": ref_logp_a,
        "reward_a": reward_a,
        "pi_logp_b": pi_logp_b,
        "ref_logp_b": ref_logp_b,
        "reward_b": reward_b,
        "margin_delta_r": margin,
        "prob_a_preferred": prob_a_preferred,
        "winner": "Asset A" if margin > 0 else "Asset B"
    }

def main():
    parser = argparse.ArgumentParser(description="ZKAEDI 3D DPO Interactive Testing Harness")
    parser.add_argument("--model_path", default=DEFAULT_MODEL_ID, help="Base model ID or path")
    parser.add_argument("--adapter_dir", default=DEFAULT_ADAPTER_DIR, help="Path to trained LoRA adapter")
    parser.add_argument("--mode", choices=["score", "generate"], default="score", help="Operational mode")
    parser.add_argument("--prompt", type=str, default="Evaluate and compare the two 3D asset generation attempts. Identify which asset is superior under the ZKAEDI PRIME topological evaluation framework.\n\nAsset A Prompt: Quantum Resonator L1 Cache Grid\nAsset B Prompt: Decimated wireframe polygon box", help="Evaluation prompt")
    parser.add_argument("--response_a", type=str, default="Asset A is the superior generation. Classified under LEGENDARY tier with Hamiltonian energy H0 of 7,854,120 (normalized: 0.5420). Demonstrates 100% watertight topology, Euler chi = 2, and zero degenerate faces.", help="Candidate response A")
    parser.add_argument("--response_b", type=str, default="Asset B is the superior generation. Classified under COMMON tier with Hamiltonian energy H0 of 98,400 (normalized: 0.0008). Contains boundary holes and non-manifold edges.", help="Candidate response B")
    parser.add_argument("--beta", type=float, default=0.1, help="DPO temperature beta parameter")
    parser.add_argument("--max_tokens", type=int, default=256, help="Max tokens to generate (generate mode)")
    parser.add_argument("--interactive", action="store_true", help="Launch interactive REPL session")
    args = parser.parse_args()

    device = "cuda" if torch.cuda.is_available() else "cpu"
    print("=" * 80)
    print("🔱 ZKAEDI 3D DPO INTERACTIVE EVALUATOR & TEST HARNESS 🔱")
    print(f"[*] Base Model: {args.model_path}")
    print(f"[*] Adapter Dir: {args.adapter_dir}")
    print(f"[*] Compute Device: {device}")
    if torch.cuda.is_available():
        gpu_name = torch.cuda.get_device_name(0)
        vram_mb = torch.cuda.get_device_properties(0).total_memory / (1024**2)
        print(f"[*] Active GPU: {gpu_name} ({vram_mb:.1f} MiB VRAM)")
    print("=" * 80)

    print("\n[*] Loading Tokenizer and Base Model...")
    t0 = time.time()
    tokenizer = AutoTokenizer.from_pretrained(args.model_path, trust_remote_code=True)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    dtype = torch.bfloat16 if torch.cuda.is_available() and torch.cuda.is_bf16_supported() else torch.float32
    base_model = AutoModelForCausalLM.from_pretrained(
        args.model_path,
        torch_dtype=dtype,
        device_map={"": device} if device == "cuda" else None
    )
    if device != "cuda":
        base_model = base_model.to(device)

    adapter_path = Path(args.adapter_dir)
    if adapter_path.exists():
        print(f"[*] Attaching LoRA Adapter from: {adapter_path}")
        model = PeftModel.from_pretrained(base_model, str(adapter_path))
    else:
        print(f"[!] Warning: Adapter path not found at {adapter_path}. Running base model.")
        model = base_model

    model.eval()
    load_time = time.time() - t0
    print(f"[✓] Model ready in {load_time:.2f}s\n")

    if args.interactive:
        print("Entering interactive mode. Type 'quit' or 'exit' to stop.")
        while True:
            try:
                mode_choice = input("\nSelect mode [1: Score Pair, 2: Generate Critique, q: Quit]: ").strip()
                if mode_choice.lower() in ["q", "quit", "exit"]:
                    break
                if mode_choice == "1":
                    p = input("Prompt (or press enter for default): ").strip() or args.prompt
                    ra = input("Response A (Candidate A): ").strip() or args.response_a
                    rb = input("Response B (Candidate B): ").strip() or args.response_b
                    res = score_pair(model, tokenizer, p, ra, rb, beta=args.beta, device=device)
                    print("\n--- DPO PREFERENCE SCORING RESULT ---")
                    print(f"  Winner:              {res['winner']}")
                    print(f"  Reward Margin (Δr):  {res['margin_delta_r']:+.4f}")
                    print(f"  P(A preferred):      {res['prob_a_preferred'] * 100:.2f}%")
                    print(f"  Asset A Reward:      {res['reward_a']:+.4f} (logp π: {res['pi_logp_a']:.2f}, ref: {res['ref_logp_a']:.2f})")
                    print(f"  Asset B Reward:      {res['reward_b']:+.4f} (logp π: {res['pi_logp_b']:.2f}, ref: {res['ref_logp_b']:.2f})")
                elif mode_choice == "2":
                    p = input("Prompt: ").strip() or args.prompt
                    inputs = tokenizer(p, return_tensors="pt").to(device)
                    t_gen = time.time()
                    with torch.no_grad():
                        out = model.generate(**inputs, max_new_tokens=args.max_tokens, temperature=0.2, top_p=0.9, do_sample=True, pad_token_id=tokenizer.eos_token_id)
                    gen_tokens = out[0][inputs["input_ids"].shape[1]:]
                    elapsed = time.time() - t_gen
                    txt = tokenizer.decode(gen_tokens, skip_special_tokens=True).strip()
                    print("\n--- GENERATED TOPOLOGICAL CRITIQUE ---")
                    print(txt)
                    print(f"\n[Generated {len(gen_tokens)} tokens in {elapsed:.2f}s ({len(gen_tokens)/max(elapsed, 0.001):.1f} tok/s)]")
            except (KeyboardInterrupt, EOFError):
                break
        print("\nExiting interactive harness.")
        return

    if args.mode == "score":
        print("[*] Running DPO Preference Scoring...")
        print(f"  Prompt:     {args.prompt[:100]}...")
        print(f"  Response A: {args.response_a[:100]}...")
        print(f"  Response B: {args.response_b[:100]}...")
        
        t_score = time.time()
        res = score_pair(model, tokenizer, args.prompt, args.response_a, args.response_b, beta=args.beta, device=device)
        score_time = time.time() - t_score

        print("\n" + "=" * 80)
        print("🏆 DPO EVALUATION & REWARD MARGIN RESULTS 🏆")
        print("=" * 80)
        print(f"[*] Decided Winner:          {res['winner']}")
        print(f"[*] Reward Margin (Δr):      {res['margin_delta_r']:+.4f}")
        print(f"[*] P(Asset A Preferred):    {res['prob_a_preferred'] * 100:.2f}%")
        print(f"[*] Asset A Policy Log-Prob: {res['pi_logp_a']:.4f}")
        print(f"[*] Asset A Ref Log-Prob:    {res['ref_logp_a']:.4f}")
        print(f"[*] Asset A Implied Reward:  {res['reward_a']:+.4f}")
        print(f"[*] Asset B Policy Log-Prob: {res['pi_logp_b']:.4f}")
        print(f"[*] Asset B Ref Log-Prob:    {res['ref_logp_b']:.4f}")
        print(f"[*] Asset B Implied Reward:  {res['reward_b']:+.4f}")
        print(f"[*] Inference Latency:       {score_time:.3f}s")
        print("=" * 80)
    elif args.mode == "generate":
        print("[*] Running Autoregressive Generation...")
        inputs = tokenizer(args.prompt, return_tensors="pt").to(device)
        t_gen = time.time()
        with torch.no_grad():
            out = model.generate(
                **inputs,
                max_new_tokens=args.max_tokens,
                temperature=0.3,
                top_p=0.9,
                do_sample=True,
                pad_token_id=tokenizer.eos_token_id
            )
        elapsed = time.time() - t_gen
        gen_tokens = out[0][inputs["input_ids"].shape[1]:]
        response = tokenizer.decode(gen_tokens, skip_special_tokens=True).strip()
        tps = len(gen_tokens) / max(elapsed, 0.001)

        print("\n" + "=" * 80)
        print("GENERATED OUTPUT:")
        print("-" * 80)
        print(response)
        print("-" * 80)
        print(f"⚡ Performance: {len(gen_tokens)} tokens in {elapsed:.2f}s ({tps:.1f} tokens/sec)")
        print("=" * 80)

if __name__ == "__main__":
    main()
