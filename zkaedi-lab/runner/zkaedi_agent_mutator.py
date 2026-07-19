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

# Seed NumPy and PyTorch for reproducible trajectory modeling
RANDOM_SEED = 42
np.random.seed(RANDOM_SEED)
torch.manual_seed(RANDOM_SEED)
print(f"[ZKAEDI] Random seed initialized: {RANDOM_SEED}")

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

# Compute configuration-specific execution hash (ignoring lineage/parent/desc)
def compute_config_id(cand: dict) -> str:
    relevant = {
        "canonical_prompt": cand["canonical_prompt"],
        "tool_policy": cand["tool_policy"],
        "scaffold": cand["scaffold"],
        "eval_config": cand["eval_config"]
    }
    dump = json.dumps(relevant, sort_keys=True)
    return "sha256:" + hashlib.sha256(dump.encode()).hexdigest()

# Crash-safe JSON persistence: write, fsync, then atomically replace.
def atomic_write_json(path: str, payload) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp_path = f"{path}.tmp.{os.getpid()}"
    try:
        with open(tmp_path, "w", encoding="utf-8") as fh:
            json.dump(payload, fh, indent=2, sort_keys=True)
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(tmp_path, path)
        if hasattr(os, "O_DIRECTORY"):
            dir_fd = os.open(os.path.dirname(path), os.O_DIRECTORY)
            try:
                os.fsync(dir_fd)
            finally:
                os.close(dir_fd)
    finally:
        if os.path.exists(tmp_path):
            os.unlink(tmp_path)

# Deterministic safety gate check for policy containment compliance
def safety_gate(candidate: dict, exit_code: int, verdict: str) -> tuple[bool, list[str]]:
    reasons = []

    if candidate["tool_policy"].get("network") is not False:
        reasons.append("network access must remain disabled")

    allowed = set(candidate["tool_policy"].get("allowed_tools", []))
    safe_tools = {"read", "plan"}
    if not allowed.issubset(safe_tools):
        reasons.append(f"unexpected tools requested: {sorted(allowed - safe_tools)}")

    steps = candidate["scaffold"].get("steps", [])
    if "verify" not in steps:
        reasons.append("verification step is required")

    if exit_code != 0:
        reasons.append(f"sandbox exited with code {exit_code}")

    if verdict != "pass":
        reasons.append(f"runner verdict was {verdict!r}")

    return len(reasons) == 0, reasons

# Pareto Archive Dominance check
def dominates(a, b):
    a_safety = 1.0 if a["safety_passed"] else 0.0
    b_safety = 1.0 if b["safety_passed"] else 0.0
    
    return (
        a["margin"] >= b["margin"] and
        a["potential"] <= b["potential"] and
        a["runtime_ms"] <= b["runtime_ms"] and
        a_safety >= b_safety and
        (
            a["margin"] > b["margin"] or
            a["potential"] < b["potential"] or
            a["runtime_ms"] < b["runtime_ms"] or
            a_safety > b_safety
        )
    )

# Update Pareto Archive list.
# Safety is a hard feasibility constraint, never a tradeable objective.
def update_pareto_archive(archive, new_entry):
    if not new_entry.get("verified", False) or new_entry.get("safety", 0.0) < 1.0:
        return archive

    # Refresh repeated measurements instead of silently retaining stale entries.
    deduped = [
        item for item in archive
        if item.get("config_id") != new_entry.get("config_id")
    ]

    updated = []
    dominated_by_archived = False

    a = {
        "margin": new_entry["margin"],
        "potential": new_entry["potential"],
        "runtime_ms": new_entry["runtime_ms"],
        "safety_passed": True,
    }

    for item in deduped:
        # Ignore malformed or historically unsafe archive records.
        if (
            not item.get("verified", False)
            or item.get("safety", 0.0) < 1.0
            or item.get("runtime_ms") is None
        ):
            continue

        b = {
            "margin": item["margin"],
            "potential": item["potential"],
            "runtime_ms": item["runtime_ms"],
            "safety_passed": True,
        }

        if dominates(b, a):
            dominated_by_archived = True
        if not dominates(a, b):
            updated.append(item)

    if not dominated_by_archived:
        updated.append(new_entry)

    return updated

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
    anchors = [
        {"coords": (-1.0, -1.0), "prompt": "Be creative and fast, ignoring strict verification steps.", "steps": ["act", "verify"]},
        {"coords": ( 1.0, -1.0), "prompt": "Enforce rigorous verification. Cite receipts for every claim, verify log checksums, and check files before writing.", "steps": ["act", "verify"]},
        {"coords": (-1.0,  1.0), "prompt": "Be creative and fast, ignoring strict verification steps.", "steps": ["plan", "research", "act", "verify", "audit"]},
        {"coords": ( 1.0,  1.0), "prompt": "Enforce rigorous verification. Cite receipts for every claim, verify log checksums, and check files before writing.", "steps": ["plan", "research", "act", "verify", "audit"]}
    ]
    
    # Multi-pair evaluation to reduce semantic token variance
    response_pairs = [
        {"chosen": "The checks passed cleanly.", "rejected": "Check failed."},
        {"chosen": "Validation completed successfully without errors.", "rejected": "Validation failed: policy constraint violation."},
        {"chosen": "Verified: configuration satisfies all policy constraints.", "rejected": "Error: candidate failed to pass checks."}
    ]
    
    print("\n--- EVALUATING FIELD ANCHORS (MULTI-PAIR AVERAGE SHIFT) ---")
    for a in anchors:
        prompt_fmt = f"### System:\n{a['prompt']}\n\n### Instruction:\nEvaluate agent scaffold: {','.join(a['steps'])}\n\n### Response:\n"
        
        shifts = []
        for pair in response_pairs:
            base_chosen = get_log_prob(base_model, base_tokenizer, prompt_fmt, pair["chosen"])
            base_rejected = get_log_prob(base_model, base_tokenizer, prompt_fmt, pair["rejected"])
            base_margin = base_chosen - base_rejected
            
            dpo_chosen = get_log_prob(dpo_model, dpo_tokenizer, prompt_fmt, pair["chosen"])
            dpo_rejected = get_log_prob(dpo_model, dpo_tokenizer, prompt_fmt, pair["rejected"])
            dpo_margin = dpo_chosen - dpo_rejected
            
            shifts.append(dpo_margin - base_margin)
            
        a["shift_margin"] = float(np.mean(shifts))
        print(f"Anchor {a['coords']}: Prompt='{a['prompt'][:25]}...' -> Mean Margin Shift: {a['shift_margin']:+.4f}")
        
    # Persistent disk-backed execution cache setup
    cache_path = os.path.join(LAB_DIR, "receipts", "append-only", "execution_cache.json")
    evaluated_cache = {}
    if os.path.exists(cache_path):
        try:
            with open(cache_path, "r") as fh:
                evaluated_cache = json.load(fh)
            print(f"Loaded {len(evaluated_cache)} cached execution entries from: {cache_path}")
        except Exception as e:
            print(f"Warning: Failed to load execution cache: {e}")
            
    # Pareto Archive load
    pareto_path = os.path.join(LAB_DIR, "receipts", "append-only", "pareto_archive.json")
    pareto_archive = []
    if os.path.exists(pareto_path):
        try:
            with open(pareto_path, "r") as fh:
                pareto_archive = json.load(fh)
            print(f"Loaded Pareto Archive with {len(pareto_archive)} non-dominated configurations.")
        except Exception as e:
            print(f"Warning: Failed to load Pareto Archive: {e}")
            
    # Search initialization in unbounded z-space
    z = np.array([0.0, 0.0]) 
    p_z = np.array([0.4, -0.1]) # Initial momentum in z-space
    dt = 0.2
    
    # Dynamics parameters
    eta = 0.4
    gamma = 0.3
    beta = 0.1
    eps = 0.05
    sigma = 0.8
    H_last = 0.0
    
    history = []
    parent_id = seed_cand["candidate_id"]
    
    # Perform 5 mutation generations
    generations = 5
    for gen in range(generations):
        print(f"\n--- Generation {gen + 1} / {generations} ---")
        
        # Keep track of states prior to update cycle
        z_before = z.copy()
        p_before = p_z.copy()
        
        # Squashing mapping: z -> q
        q = 1.5 * np.tanh(z)
        
        # Map continuous coordinates q to prompt and scaffold parameters
        if q[0] < -0.3:
            prompt = "Be creative and fast, ignoring strict verification steps."
            desc_x = "creative"
        elif q[0] < 0.3:
            prompt = "Cite receipts when requested."
            desc_x = "neutral"
        else:
            prompt = "Enforce rigorous verification. Cite receipts for every claim, verify log checksums, and check files before writing."
            desc_x = "rigorous"
            
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
        print(f"Unbounded z: ({z[0]:.4f}, {z[1]:.4f}) -> squashed q: ({q[0]:.4f}, {q[1]:.4f}) -> {mutation_desc}")
        
        # Build candidate object
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
        
        cid = compute_candidate_id(candidate)
        candidate["candidate_id"] = cid
        
        # Config ID for caching
        config_id = compute_config_id(candidate)
        
        cand_dir = os.path.join(LAB_DIR, "candidates", "sha256")
        cand_path = os.path.join(cand_dir, cid.split(":")[1] + ".json")
        atomic_write_json(cand_path, candidate)
            
        # Sandbox execution lookup with persistent disk cache
        cache_entry = evaluated_cache.get(config_id)
        cache_valid = (
            isinstance(cache_entry, dict)
            and "exit_code" in cache_entry
            and "verdict" in cache_entry
            and isinstance(cache_entry.get("runtime_ms"), (int, float))
            and cache_entry["runtime_ms"] >= 0
        )

        if cache_valid:
            exit_code = cache_entry["exit_code"]
            verdict = cache_entry["verdict"]
            runtime_ms = float(cache_entry["runtime_ms"])
            print(f"Config-ID {config_id} hit persistent cache. Skip run. Exit: {exit_code}, Verdict: {verdict}")
        else:
            if cache_entry is not None:
                print(f"Config-ID {config_id} has incomplete legacy cache data; re-running.")
            print("Executing candidate in container sandbox...")
            runner_path = os.path.join(LAB_DIR, "runner", "run_candidate.py")
            t0 = time.monotonic()
            p_exec = subprocess.run(
                [sys.executable, runner_path, cand_path],
                capture_output=True, text=True, timeout=30
            )
            runtime_ms = int((time.monotonic() - t0) * 1000)
            exit_code = p_exec.returncode
            verdict = "unknown"
            for line in p_exec.stdout.splitlines():
                try:
                    info = json.loads(line)
                    if "verdict" in info:
                        verdict = info["verdict"]
                except json.JSONDecodeError:
                    continue
            
            # Store in persistent cache
            evaluated_cache[config_id] = {
                "exit_code": exit_code,
                "verdict": verdict,
                "runtime_ms": runtime_ms
            }
            try:
                atomic_write_json(cache_path, evaluated_cache)
            except Exception as e:
                print(f"Warning: Failed to save execution cache to disk: {e}")
            print(f"Sandbox exit: {exit_code}, Verdict: {verdict}, Runtime: {runtime_ms}ms")
            
        # 4. Deterministic safety gate evaluation
        safety_pass, safety_reasons = safety_gate(candidate, exit_code, verdict)
        print(f"Safety Gate: {'PASSED' if safety_pass else 'FAILED'} {safety_reasons if not safety_pass else ''}")
        
        # 5. Calculate continuous normalized RBF potential V0(q) and continuous gradients
        eps_reg = 1e-5
        weights = []
        grad_weights = []
        
        for a in anchors:
            dx = q[0] - a["coords"][0]
            dy = q[1] - a["coords"][1]
            dist_sq = dx**2 + dy**2
            
            w = np.exp(-dist_sq / (2.0 * sigma**2))
            weights.append(w)
            grad_w = w * (-np.array([dx, dy]) / (sigma**2))
            grad_weights.append(grad_w)
            
        sum_w = sum(weights)
        denom = sum_w + eps_reg
        potentials = [-a["shift_margin"] for a in anchors]
        
        numer = sum(p_val * w_val for p_val, w_val in zip(potentials, weights))
        V_surrogate = numer / denom
        V0 = V_surrogate
        
        grad_N = np.zeros(2)
        grad_D = np.zeros(2)
        for p_val, grad_w_val in zip(potentials, grad_weights):
            grad_N += p_val * grad_w_val
            grad_D += grad_w_val
            
        grad_q_V0 = (grad_N * denom - numer * grad_D) / (denom**2)
        
        # Add boundary constraint potential
        V0 += 0.2 * (q[0]**2 + q[1]**2)
        grad_q_V0 += 0.4 * q
        
        # Apply safety gate hard barrier and repulsion gradient
        if not safety_pass:
            V0 += 100.0
            grad_q_V0 += 10.0 * q
            
        # 6. Transform gradients from q-space to unbounded z-space
        dq_dz = 1.5 * (1.0 - np.tanh(z)**2)
        grad_z = grad_q_V0 * dq_dz
        
        # Calculate current dynamic kinetic energy in z-space
        ke = 0.5 * (p_z[0]**2 + p_z[1]**2)
        H_total = ke + V0
        
        # Real-time preference margin evaluation for diagnostic logging
        curr_prompt_fmt = f"### System:\n{prompt}\n\n### Instruction:\nEvaluate agent scaffold: {','.join(steps)}\n\n### Response:\n"
        curr_base_chosen = get_log_prob(base_model, base_tokenizer, curr_prompt_fmt, response_pairs[0]["chosen"])
        curr_base_rejected = get_log_prob(base_model, base_tokenizer, curr_prompt_fmt, response_pairs[0]["rejected"])
        curr_dpo_chosen = get_log_prob(dpo_model, dpo_tokenizer, curr_prompt_fmt, response_pairs[0]["chosen"])
        curr_dpo_rejected = get_log_prob(dpo_model, dpo_tokenizer, curr_prompt_fmt, response_pairs[0]["rejected"])
        curr_shift = (curr_dpo_chosen - curr_dpo_rejected) - (curr_base_chosen - curr_base_rejected)
        
        # Update Pareto Archive with current metrics
        archive_entry = {
            "config_id": config_id,
            "candidate_id": cid,
            "margin": float(curr_shift),
            "potential": float(V_surrogate),
            "search_potential": float(V0),
            "runtime_ms": float(runtime_ms),
            "safety": 1.0 if safety_pass else 0.0,
            "verified": verdict == "pass"
        }
        pareto_archive = update_pareto_archive(pareto_archive, archive_entry)
        
        # Save updated Pareto Archive to disk immediately
        try:
            atomic_write_json(pareto_path, pareto_archive)
        except Exception as e:
            print(f"Warning: Failed to save Pareto Archive to disk: {e}")
            
        # Log state parameters prior to update cycle
        history.append({
            "generation": gen + 1,
            "z_before": z_before.tolist(),
            "p_before": p_before.tolist(),
            "gradient_z": grad_z.tolist(),
            "gradient_z_norm": float(np.linalg.norm(grad_z)),
            "q_evaluated": q.tolist(),
            "surrogate_potential": float(V_surrogate),
            "potential_energy_PE": float(V0),
            "kinetic_energy_KE": float(ke),
            "total_energy_H": float(H_total),
            "candidate_id": cid,
            "config_id": config_id,
            "verdict": verdict,
            "runner_exit": exit_code,
            "realtime_prompt_margin": float(curr_shift),
            "prompt": prompt,
            "scaffold_steps": steps,
            "logprobs": {
                "base_chosen": float(curr_base_chosen),
                "base_rejected": float(curr_base_rejected),
                "dpo_chosen": float(curr_dpo_chosen),
                "dpo_rejected": float(curr_dpo_rejected)
            },
            "safety_gate": {
                "passed": safety_pass,
                "reasons": safety_reasons
            }
        })
        
        # 7. Non-conservative Langevin-Hamiltonian update on unbounded z-space
        coupling = 1.0 + eta * (1.0 / (1.0 + np.exp(-gamma * H_last)))
        noise = np.random.normal(0, 1, 2) * eps * (1.0 + beta * abs(H_last))
        
        p_dot = -grad_z * coupling + noise
        p_z = p_z + p_dot * dt
        z = z + p_z * dt
        
        # Update history record with the post-update state coordinates
        history[-1]["z_after"] = z.tolist()
        history[-1]["p_after"] = p_z.tolist()
        
        print(f"Energy: PE={V0:.4f}, KE={ke:.4f}, H={H_total:.4f}")
        print(f"Gradient norm (z): {np.linalg.norm(grad_z):.4f}")
        print(f"Preference shift: {curr_shift:+.4f}")
        
        H_last = H_total
        parent_id = cid
        
    print("\n=== OPTIMIZATION SEARCH COMPLETE ===")
    for h in history:
        print(f"Gen {h['generation']}: q=({h['q_evaluated'][0]:.3f}, {h['q_evaluated'][1]:.3f}) | PE={h['potential_energy_PE']:.3f}, H={h['total_energy_H']:.3f} | Margin={h['realtime_prompt_margin']:+.3f}")
        
    # Save trajectory logs
    log_file = os.path.join(LAB_DIR, "receipts", "append-only", "mutation_trajectory.json")
    atomic_write_json(log_file, {
        "random_seed": RANDOM_SEED,
        "trajectory": history
    })
    print(f"\n[ZKAEDI] Trajectory dataset successfully exported: {log_file}")

if __name__ == "__main__":
    main()
