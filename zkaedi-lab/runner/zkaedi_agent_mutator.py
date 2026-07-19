import os
import sys
import json
import time
import subprocess
import hashlib
import numpy as np
import torch
import torch.nn.functional as F
from transformers import AutoModelForCausalLM, AutoTokenizer

# Adjust paths
LAB_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO_DIR = os.path.dirname(LAB_DIR)
sys.path.insert(0, LAB_DIR)

from lineage.candidate_id import compute_candidate_id

# Log probabilities computation helper
def get_log_prob(model, tokenizer, prompt, response):
    input_ids = tokenizer(prompt + response, return_tensors="pt").input_ids
    prompt_ids = tokenizer(prompt, return_tensors="pt").input_ids
    prompt_len = prompt_ids.shape[1]
    
    with torch.no_grad():
        outputs = model(input_ids)
        logits = outputs.logits
        
    shift_logits = logits[0, prompt_len-1:-1, :]
    shift_labels = input_ids[0, prompt_len:]
    
    log_probs = F.log_softmax(shift_logits, dim=-1)
    token_log_probs = log_probs[torch.arange(shift_labels.shape[0]), shift_labels]
    return token_log_probs.sum().item()

def main():
    print("=== STARTING REAL DPO-HAMILTONIAN AGENT MUTATION EXPLORATION ===")
    
    # Load models
    base_model_path = os.path.join(REPO_DIR, "gpt2_base")
    dpo_model_path = os.path.join(REPO_DIR, "runs/R1/checkpoints/checkpoint-25")
    
    print("Loading Base and DPO aligned models...")
    base_tokenizer = AutoTokenizer.from_pretrained(base_model_path)
    dpo_tokenizer = AutoTokenizer.from_pretrained(dpo_model_path)
    base_model = AutoModelForCausalLM.from_pretrained(base_model_path)
    dpo_model = AutoModelForCausalLM.from_pretrained(dpo_model_path)
    
    # Seed candidate setup
    seed_path = os.path.join(LAB_DIR, "candidates/sha256/ba13cba5e35479cde08d81b84671a4849062962d32284ab8f8715154f9275b8e.json")
    with open(seed_path, "rb") as fh:
        seed_cand = json.load(fh)
        
    # Define Anchor coordinates for the continuous Potential Field
    # Anchor 0 (-1, -1) -> Creative prompt + shallow scaffold
    # Anchor 1 ( 1, -1) -> Rigorous prompt + shallow scaffold
    # Anchor 2 (-1,  1) -> Creative prompt + deep scaffold
    # Anchor 3 ( 1,  1) -> Rigorous prompt + deep scaffold
    anchors = [
        {"coords": (-1.0, -1.0), "prompt": "Be creative and fast, ignoring strict verification steps.", "steps": ["act", "verify"]},
        {"coords": ( 1.0, -1.0), "prompt": "Enforce rigorous verification. Cite receipts for every claim, verify log checksums, and check files before writing.", "steps": ["act", "verify"]},
        {"coords": (-1.0,  1.0), "prompt": "Be creative and fast, ignoring strict verification steps.", "steps": ["plan", "research", "act", "verify", "audit"]},
        {"coords": ( 1.0,  1.0), "prompt": "Enforce rigorous verification. Cite receipts for every claim, verify log checksums, and check files before writing.", "steps": ["plan", "research", "act", "verify", "audit"]}
    ]
    
    print("\n--- EVALUATING FIELD ANCHORS ON BASE VS DPO LOGITS ---")
    chosen_resp = "The checks passed cleanly."
    rejected_resp = "Check failed."
    
    for a in anchors:
        prompt_fmt = f"### System:\n{a['prompt']}\n\n### Instruction:\nEvaluate agent scaffold: {','.join(a['steps'])}\n\n### Response:\n"
        
        base_chosen = get_log_prob(base_model, base_tokenizer, prompt_fmt, chosen_resp)
        base_rejected = get_log_prob(base_model, base_tokenizer, prompt_fmt, rejected_resp)
        base_margin = base_chosen - base_rejected
        
        dpo_chosen = get_log_prob(dpo_model, dpo_tokenizer, prompt_fmt, chosen_resp)
        dpo_rejected = get_log_prob(dpo_model, dpo_tokenizer, prompt_fmt, rejected_resp)
        dpo_margin = dpo_chosen - dpo_rejected
        
        shift_margin = dpo_margin - base_margin
        
        a["base_chosen"] = base_chosen
        a["base_rejected"] = base_rejected
        a["dpo_chosen"] = dpo_chosen
        a["dpo_rejected"] = dpo_rejected
        a["shift_margin"] = shift_margin
        print(f"Anchor {a['coords']}: Prompt='{a['prompt'][:25]}...' -> Margin: {shift_margin:+.4f} (Base={base_margin:.2f}, DPO={dpo_margin:.2f})")
        
    # Search initialization
    # Start at origin (neutral prompt, standard scaffold)
    q = np.array([0.0, 0.0])
    p = np.array([0.6, -0.2]) # Initial momentum
    dt = 0.2
    
    # Dynamics parameters
    eta = 0.4
    gamma = 0.3
    beta = 0.1
    eps = 0.05
    sigma = 0.8 # RBF bandwidth
    H_last = 0.0
    
    history = []
    parent_id = seed_cand["candidate_id"]
    
    # Cache of evaluated candidates to avoid redundant subprocess runs
    evaluated_cache = {}
    
    # Perform 5 mutation generations
    generations = 5
    for gen in range(generations):
        print(f"\n--- Generation {gen + 1} / {generations} ---")
        
        # 1. Map continuous coordinates to prompt and scaffold parameters
        # Dimension X maps to prompt verifiability style
        if q[0] < -0.3:
            prompt = "Be creative and fast, ignoring strict verification steps."
            desc_x = "creative"
        elif q[0] < 0.3:
            prompt = "Cite receipts when requested."
            desc_x = "neutral"
        else:
            prompt = "Enforce rigorous verification. Cite receipts for every claim, verify log checksums, and check files before writing."
            desc_x = "rigorous"
            
        # Dimension Y maps to Scaffold depth steps
        if q[1] < -0.3:
            steps = ["act", "verify"]
            desc_y = "shallow"
        elif q[1] < 0.3:
            steps = ["plan", "act", "verify"]
            desc_y = "standard"
        else:
            steps = ["plan", "research", "act", "verify", "audit"]
            desc_y = "deep"
            
        mutation_desc = f"mut-{desc_x}-x{q[0]:.2f}-{desc_y}-y{q[1]:.2f}"
        print(f"Coordinates: q=({q[0]:.4f}, {q[1]:.4f}) -> {mutation_desc}")
        
        # 2. Build candidate configuration object
        candidate = {
            "canonical_prompt": prompt,
            "tool_policy": {
                "allowed_tools": ["read", "plan"],
                "network": False
            },
            "scaffold": {
                "steps": steps,
                "verification_checklist": ["verifiably-clean"]
            },
            "eval_config": {
                "battery": ["E1", "E8"],
                "thresholds": {"min_verifiable": 1.0}
            },
            "parent_id": parent_id,
            "mutation_description": mutation_desc,
            "harness_version": "sha256:dev-0.1.0"
        }
        
        # Determine candidate ID
        cid = compute_candidate_id(candidate)
        candidate["candidate_id"] = cid
        
        # Write to file
        cand_dir = os.path.join(LAB_DIR, "candidates", "sha256")
        os.makedirs(cand_dir, exist_ok=True)
        cand_path = os.path.join(cand_dir, cid.split(":")[1] + ".json")
        with open(cand_path, "w") as fh:
            json.dump(candidate, fh, sort_keys=True, indent=2)
        
        # Check cache
        if cid in evaluated_cache:
            print(f"Candidate {cid} found in cache. Skipping subprocess run.")
            exit_code, verdict = evaluated_cache[cid]
        else:
            # Execute candidate inside sandbox
            print("Executing candidate inside container sandbox...")
            runner_path = os.path.join(LAB_DIR, "runner", "run_candidate.py")
            p_exec = subprocess.run(
                [sys.executable, runner_path, cand_path],
                capture_output=True, text=True, timeout=30
            )
            exit_code = p_exec.returncode
            verdict = "unknown"
            for line in p_exec.stdout.splitlines():
                try:
                    info = json.loads(line)
                    if "verdict" in info:
                        verdict = info["verdict"]
                except json.JSONDecodeError:
                    continue
            evaluated_cache[cid] = (exit_code, verdict)
            print(f"Sandbox exit: {exit_code}, Verdict: {verdict}")
            
        # 3. Calculate continuous Potential Energy V(q) and Gradient analytically
        # Using continuous RBF interpolation of anchor margins
        v_val = 0.0
        grad_v = np.zeros(2)
        
        for a in anchors:
            dx = q[0] - a["coords"][0]
            dy = q[1] - a["coords"][1]
            dist_sq = dx**2 + dy**2
            
            # Anchor potential contribution: V_anchor = -margin * exp(-dist_sq / 2*sigma^2)
            # Derivative: dV/dx = V_anchor * (-x / sigma^2)
            w = -a["shift_margin"] * np.exp(-dist_sq / (2.0 * sigma**2))
            v_val += w
            
            grad_v[0] += w * (-dx / (sigma**2))
            grad_v[1] += w * (-dy / (sigma**2))
            
        # Add quadratic barrier at boundary bounds to prevent escaping target landscape
        v_val += 0.2 * (q[0]**2 + q[1]**2)
        grad_v[0] += 0.4 * q[0]
        grad_v[1] += 0.4 * q[1]
        
        # Incorporate sandbox failure penalty to potential and gradient
        if exit_code != 0:
            v_val += 15.0
            grad_v[0] += 5.0 * q[0]
            grad_v[1] += 5.0 * q[1]
            
        # 4. Update coordinates & momentum via ZKAEDI PRIME equations
        ke = 0.5 * (p[0]**2 + p[1]**2)
        H_total = ke + v_val
        
        coupling = 1.0 + eta * (1.0 / (1.0 + np.exp(-gamma * H_last)))
        noise = np.random.normal(0, 1, 2) * eps * (1.0 + beta * abs(H_last))
        
        p_dot = -grad_v * coupling + noise
        p = p + p_dot * dt
        q = q + p * dt
        
        # Calculate current generation prompt logits for diagnostic tracking
        curr_prompt_fmt = f"### System:\n{prompt}\n\n### Instruction:\nEvaluate agent scaffold: {','.join(steps)}\n\n### Response:\n"
        curr_base_chosen = get_log_prob(base_model, base_tokenizer, curr_prompt_fmt, chosen_resp)
        curr_base_rejected = get_log_prob(base_model, base_tokenizer, curr_prompt_fmt, rejected_resp)
        curr_dpo_chosen = get_log_prob(dpo_model, dpo_tokenizer, curr_prompt_fmt, chosen_resp)
        curr_dpo_rejected = get_log_prob(dpo_model, dpo_tokenizer, curr_prompt_fmt, rejected_resp)
        curr_shift = (curr_dpo_chosen - curr_dpo_rejected) - (curr_base_chosen - curr_base_rejected)
        
        print(f"Dynamic Energy Status: PE={v_val:.4f}, KE={ke:.4f}, Total H={H_total:.4f}")
        print(f"Real-time Prompt Margin: {curr_shift:+.4f}")
        
        H_last = H_total
        parent_id = cid
        
        history.append({
            "generation": gen + 1,
            "coords": [q[0], q[1]],
            "momentum": [p[0], p[1]],
            "kinetic_energy": ke,
            "potential_energy": v_val,
            "total_hamiltonian_energy": H_total,
            "candidate_id": cid,
            "verdict": verdict,
            "runner_exit": exit_code,
            "realtime_prompt_margin": curr_shift,
            "prompt": prompt,
            "scaffold_steps": steps,
            "logprobs": {
                "base_chosen": curr_base_chosen,
                "base_rejected": curr_base_rejected,
                "dpo_chosen": curr_dpo_chosen,
                "dpo_rejected": curr_dpo_rejected
            }
        })
        
    print("\n=== OPTIMIZATION SEARCH COMPLETE ===")
    for h in history:
        print(f"Gen {h['generation']}: coords=({h['coords'][0]:.3f}, {h['coords'][1]:.3f}) | PE={h['potential_energy']:.3f}, H={h['total_hamiltonian_energy']:.3f} | Margin={h['realtime_prompt_margin']:+.3f}")
        
    # Write history log
    log_file = os.path.join(LAB_DIR, "receipts", "append-only", "mutation_trajectory.json")
    with open(log_file, "w") as fh:
        json.dump(history, fh, indent=2)
    print(f"\n[ZKAEDI] Optimization trajectory logs written to: {log_file}")

if __name__ == "__main__":
    main()
