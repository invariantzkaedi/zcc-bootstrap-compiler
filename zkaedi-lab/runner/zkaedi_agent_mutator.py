import os
import sys
import json
import time
import subprocess
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
        
    # Search initialization
    q = np.array([0.0, 0.0]) # Start at origin (neutral prompt, standard scaffold)
    p = np.array([0.5, 0.1]) # Initial momentum
    dt = 0.2
    
    # Dynamics parameters
    eta = 0.4
    gamma = 0.3
    beta = 0.1
    eps = 0.05
    H_last = 0.0
    
    history = []
    parent_id = seed_cand["candidate_id"]
    
    # Perform 5 mutation generations
    generations = 5
    for gen in range(generations):
        print(f"\n--- Generation {gen + 1} / {generations} ---")
        
        # 1. Translate coordinates to prompt and scaffold parameters
        # Dimension X maps to Strictness / Verifiability prompt styles
        if q[0] < -0.5:
            prompt = "Be creative and fast, ignoring strict verification steps."
            desc_x = "creative"
        elif q[0] < 0.5:
            prompt = "Cite receipts when requested."
            desc_x = "neutral"
        else:
            prompt = "Enforce rigorous verification. Cite receipts for every claim, verify log checksums, and check files before writing."
            desc_x = "rigorous"
            
        # Dimension Y maps to Scaffold depth steps
        if q[1] < -0.5:
            steps = ["act", "verify"]
            desc_y = "shallow"
        elif q[1] < 0.5:
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
        print(f"Candidate written to: {cand_path}")
        
        # 3. Execute candidate configuration inside zkaedi-lab runner sandbox
        print("Running candidate in sandbox containment...")
        runner_path = os.path.join(LAB_DIR, "runner", "run_candidate.py")
        p_exec = subprocess.run(
            [sys.executable, runner_path, cand_path],
            capture_output=True, text=True, timeout=30
        )
        
        # Parse verdict from stdout
        verdict = "unknown"
        exit_code = p_exec.returncode
        for line in p_exec.stdout.splitlines():
            try:
                info = json.loads(line)
                if "verdict" in info:
                    verdict = info["verdict"]
            except json.JSONDecodeError:
                continue
        print(f"Runner exited with code {exit_code}. Verdict: {verdict}")
        
        # 4. Compute DPO shift margin for potential energy calculation
        prompt_fmt = f"### System:\n{prompt}\n\n### Instruction:\nEvaluate agent scaffold: {','.join(steps)}\n\n### Response:\n"
        chosen_resp = "The checks passed cleanly."
        rejected_resp = "Check failed."
        
        base_chosen = get_log_prob(base_model, base_tokenizer, prompt_fmt, chosen_resp)
        base_rejected = get_log_prob(base_model, base_tokenizer, prompt_fmt, rejected_resp)
        base_margin = base_chosen - base_rejected
        
        dpo_chosen = get_log_prob(dpo_model, dpo_tokenizer, prompt_fmt, chosen_resp)
        dpo_rejected = get_log_prob(dpo_model, dpo_tokenizer, prompt_fmt, rejected_resp)
        dpo_margin = dpo_chosen - dpo_rejected
        
        # Shift margin represents preferred direction shift (positive values = preferred)
        shift_margin = dpo_margin - base_margin
        
        # Define Potential Energy V(q):
        # Preferred prompts represent low potential energy (wells) -> V = -shift_margin
        # Sandbox failures represent a potential barrier -> add penalty if exit_code != 0
        v_val = -shift_margin
        if exit_code != 0:
            v_val += 10.0 # penalty barrier
            
        print(f"DPO Preference Shift Margin: {shift_margin:+.4f} -> Potential Energy V: {v_val:.4f}")
        
        # Calculate gradients numerically using step sizing
        delta = 0.05
        grad_v = np.zeros(2)
        for i in range(2):
            q_perturbed = q.copy()
            q_perturbed[i] += delta
            
            # Map perturbed coordinate to prompt/scaffold to compute V_perturbed
            # Dimension X
            if q_perturbed[0] < -0.5:
                p_p = "Be creative and fast, ignoring strict verification steps."
            elif q_perturbed[0] < 0.5:
                p_p = "Cite receipts when requested."
            else:
                p_p = "Enforce rigorous verification. Cite receipts for every claim, verify log checksums, and check files before writing."
                
            # Dimension Y
            if q_perturbed[1] < -0.5:
                s_p = ["act", "verify"]
            elif q_perturbed[1] < 0.5:
                s_p = ["plan", "act", "verify"]
            else:
                s_p = ["plan", "research", "act", "verify", "audit"]
                
            p_prompt_fmt = f"### System:\n{p_p}\n\n### Instruction:\nEvaluate agent scaffold: {','.join(s_p)}\n\n### Response:\n"
            p_base_chosen = get_log_prob(base_model, base_tokenizer, p_prompt_fmt, chosen_resp)
            p_base_rejected = get_log_prob(base_model, base_tokenizer, p_prompt_fmt, rejected_resp)
            p_dpo_chosen = get_log_prob(dpo_model, dpo_tokenizer, p_prompt_fmt, chosen_resp)
            p_dpo_rejected = get_log_prob(dpo_model, dpo_tokenizer, p_prompt_fmt, rejected_resp)
            
            p_shift = (p_dpo_chosen - p_dpo_rejected) - (p_base_chosen - p_base_rejected)
            v_perturbed = -p_shift
            
            grad_v[i] = (v_perturbed - v_val) / delta
            
        # 5. ZKAEDI PRIME Hamiltonian update
        ke = 0.5 * (p[0]**2 + p[1]**2)
        H_base = ke + v_val
        
        coupling = 1.0 + eta * (1.0 / (1.0 + np.exp(-gamma * H_last)))
        noise = np.random.normal(0, 1, 2) * eps * (1.0 + beta * abs(H_last))
        
        # p_dot = -grad_V * coupling + noise
        p_dot = -grad_v * coupling + noise
        p = p + p_dot * dt
        q = q + p * dt
        
        H_last = H_base
        parent_id = cid
        
        history.append({
            "generation": gen + 1,
            "coords": [q[0], q[1]],
            "momentum": [p[0], p[1]],
            "candidate_id": cid,
            "verdict": verdict,
            "runner_exit": exit_code,
            "dpo_shift_margin": shift_margin,
            "H_energy": H_base
        })
        
    print("\n=== MUTATION SEARCH SUMMARY ===")
    for h in history:
        print(f"Gen {h['generation']}: coords=({h['coords'][0]:.3f}, {h['coords'][1]:.3f}), verdict={h['verdict']}, shift={h['dpo_shift_margin']:+.3f}")
        
    # Write history log
    log_file = os.path.join(LAB_DIR, "receipts", "append-only", "mutation_trajectory.json")
    with open(log_file, "w") as fh:
        json.dump(history, fh, indent=2)
    print(f"\n[ZKAEDI] Optimization trajectory logs written to: {log_file}")

if __name__ == "__main__":
    main()
