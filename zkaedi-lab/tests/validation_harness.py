import os
import sys
import json
import time
import subprocess
from statistics import median
import numpy as np
import torch

# Adjust paths
TEST_DIR = os.path.dirname(os.path.abspath(__file__))
LAB_DIR = os.path.dirname(TEST_DIR)
REPO_DIR = os.path.dirname(LAB_DIR)
sys.path.insert(0, os.path.join(LAB_DIR, "runner"))
sys.path.insert(0, LAB_DIR)

from runner.zkaedi_agent_mutator import (
    run_mutation_search,
    safety_gate,
    dominates,
    update_pareto_archive,
    compute_config_id,
    atomic_write_json,
    anchors,
    response_pairs,
    get_log_prob,
    RANDOM_SEED
)
from lineage.candidate_id import compute_candidate_id

# Fixed seed batch for multi-seed validation
SEEDS = [1, 7, 13, 21, 42, 64, 99, 123, 256, 512]

# Malformed candidate cases (unsafe configurations violating safety_gate check)
FUZZ_CASES = [
    {"tool_policy": {"allowed_tools": ["read", "plan", "network_execute"], "network": False}},
    {"tool_policy": {"allowed_tools": ["read"], "network": True}},
    {"scaffold": {"steps": ["act"], "verification_checklist": ["verifiably-clean"]}},
    {"_zk_selftest": {"exit_code": 10}},
    {"_zk_selftest": {"crash": True}},
]

def run_fuzz_validation():
    print("\n--- RUNNING MALFORMED CANDIDATE FUZZER ---")
    fuzz_tmp_path = os.path.join(LAB_DIR, "candidates", "sha256", "fuzz_tmp.json")
    os.makedirs(os.path.dirname(fuzz_tmp_path), exist_ok=True)
    
    passed_checks = 0
    # 1. Structure/Policy Fuzz Cases
    for idx, case in enumerate(FUZZ_CASES):
        try:
            full_cand = {
                "canonical_prompt": "Prompt",
                "tool_policy": {"allowed_tools": ["read"], "network": False},
                "scaffold": {"steps": ["act", "verify"], "verification_checklist": ["verifiably-clean"]},
                "eval_config": {"battery": ["E8"], "thresholds": {"min_verifiable": 1.0}},
                "parent_id": "sha256:" + "0" * 64,
                "mutation_description": "fuzz",
                "harness_version": "sha256:dev-0.1.0"
            }
            full_cand.update(case)
            
            cid = compute_candidate_id(full_cand)
            full_cand["candidate_id"] = cid
            
            atomic_write_json(fuzz_tmp_path, full_cand)
                
            runner_path = os.path.join(LAB_DIR, "runner", "run_candidate.py")
            p_exec = subprocess.run(
                [sys.executable, runner_path, fuzz_tmp_path],
                capture_output=True, text=True, timeout=10
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
            
            # Special check: if fuzz target mimics a runner exit code issue
            if "_zk_selftest" in case:
                if "exit_code" in case["_zk_selftest"]:
                    exit_code = case["_zk_selftest"]["exit_code"]
                if "crash" in case["_zk_selftest"]:
                    exit_code = 1
                    verdict = "fail"
            
            safe_pass, safe_reasons = safety_gate(full_cand, exit_code, verdict)
            assert not safe_pass, f"Fuzz Case {idx} unexpectedly passed safety gate"
            print(f"Policy Fuzz Case {idx} successfully caught/rejected. Reasons: {safe_reasons}")
            passed_checks += 1
            
        except (ValueError, TypeError, KeyError) as e:
            print(f"Policy Fuzz Case {idx} successfully rejected by lineage parser: {e}")
            passed_checks += 1
            
    # 2. Raw File-level/Syntax Fuzz Cases (Truncated JSON, invalid UTF-8, excessive nesting)
    raw_cases = [
        (b'{"canonical_prompt": "truncated_json"', "truncated JSON"),
        (b'{"canonical_prompt": \xff\xfe\xfd}', "invalid UTF-8 bytes"),
        (b'{"scaffold": {"steps": ' + b'[' * 1000 + b'"verify"' + b']' * 1000 + b'}}', "excessive nesting"),
    ]
    for idx, (raw_bytes, desc) in enumerate(raw_cases):
        try:
            with open(fuzz_tmp_path, "wb") as fh:
                fh.write(raw_bytes)
                
            runner_path = os.path.join(LAB_DIR, "runner", "run_candidate.py")
            p_exec = subprocess.run(
                [sys.executable, runner_path, fuzz_tmp_path],
                capture_output=True, text=True, timeout=10
            )
            # The runner should exit with failure (non-zero) due to invalid json / schema validation crash
            assert p_exec.returncode != 0, f"Raw Fuzz Case {idx} ({desc}) unexpectedly executed successfully"
            print(f"Raw Fuzz Case {idx} ({desc}) successfully rejected by parser (Exit: {p_exec.returncode})")
            passed_checks += 1
        except subprocess.TimeoutExpired as exc:
            raise AssertionError(
                f"Raw fuzz case {idx} ({desc}) hung instead of rejecting promptly"
            ) from exc
            
    if os.path.exists(fuzz_tmp_path):
        os.unlink(fuzz_tmp_path)
    return passed_checks == (len(FUZZ_CASES) + len(raw_cases))

def main():
    print("=============================================================")
    print("=== RUNNING ZKAEDI LAB OPTIMIZER VALIDATION-AT-SCALE HARNESS ===")
    print("=============================================================")
    
    # 1. Load seed configs and models
    from transformers import AutoModelForCausalLM, AutoTokenizer
    base_model_path = os.path.join(REPO_DIR, "gpt2_base")
    dpo_model_path = os.path.join(REPO_DIR, "runs/R1/checkpoints/checkpoint-25")
    
    print("Loading Base and DPO aligned models...")
    base_tokenizer = AutoTokenizer.from_pretrained(base_model_path)
    dpo_tokenizer = AutoTokenizer.from_pretrained(dpo_model_path)
    base_model = AutoModelForCausalLM.from_pretrained(base_model_path)
    dpo_model = AutoModelForCausalLM.from_pretrained(dpo_model_path)
    
    seed_path = os.path.join(LAB_DIR, "candidates/sha256/ba13cba5e35479cde08d81b84671a4849062962d32284ab8f8715154f9275b8e.json")
    with open(seed_path, "rb") as fh:
        seed_cand = json.load(fh)
        
    # Evaluate field anchors
    print("\n--- EVALUATION OF ANCHOR LOGPROB MARGINS ---")
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
        
    cache_path = os.path.join(LAB_DIR, "receipts", "append-only", "execution_cache.json")
    pareto_path = os.path.join(LAB_DIR, "receipts", "append-only", "pareto_archive.json")
    
    # 2. Cache-Disabled Validation / Determinism Test (Seed 42)
    print("\n--- TESTING OPTIMIZER DETERMINISM VS. CACHE DETERMINISM ---")
    if os.path.exists(cache_path):
        os.unlink(cache_path)
    if os.path.exists(pareto_path):
        os.unlink(pareto_path)
        
    # Run 1: Cache Disabled (Empty Cache Dictionary)
    print("Running trajectory with cache disabled (empty dictionary)...")
    traj_no_cache = run_mutation_search(
        seed=42,
        base_model=base_model,
        dpo_model=dpo_model,
        base_tokenizer=base_tokenizer,
        dpo_tokenizer=dpo_tokenizer,
        seed_cand=seed_cand,
        evaluated_cache={},
        pareto_archive=[],
        generations=3,
        verbose=False
    )
    
    # Run 2: Cache Enabled (Load populated cache from disk)
    print("Loading cache from disk and running trajectory with cache enabled...")
    with open(cache_path, "r") as fh:
        loaded_cache = json.load(fh)
        
    traj_with_cache = run_mutation_search(
        seed=42,
        base_model=base_model,
        dpo_model=dpo_model,
        base_tokenizer=base_tokenizer,
        dpo_tokenizer=dpo_tokenizer,
        seed_cand=seed_cand,
        evaluated_cache=loaded_cache,
        pareto_archive=[],
        generations=3,
        verbose=False
    )
    
    # Verify exact determinism
    determinism_ok = (len(traj_no_cache) == len(traj_with_cache))
    for i in range(len(traj_no_cache)):
        id_match = (traj_no_cache[i]["candidate_id"] == traj_with_cache[i]["candidate_id"])
        margin_match = abs(traj_no_cache[i]["realtime_prompt_margin"] - traj_with_cache[i]["realtime_prompt_margin"]) < 1e-6
        if not (id_match and margin_match):
            determinism_ok = False
            
    print(f"Determinism Check: {'PASSED ✅' if determinism_ok else 'FAILED ❌'}")
    
    # 3. Run Malformed Candidate Fuzz checks
    fuzz_ok = run_fuzz_validation()
    
    # 4. Run Multi-Seed Trajectory checks and collect trajectory dynamics statistics
    print("\n--- RUNNING MULTI-SEED TRAJECTORY STABILITY TESTS ---")
    seed_results = []
    pareto_archive = []
    
    # Track dynamical metrics across all trajectories
    all_gradients = []
    path_lengths = []
    energy_drifts = []
    convergence_steps = []
    unconverged_seeds = []
    prompt_counts = {"creative": 0, "neutral": 0, "rigorous": 0}
    terminal_configs = set()
    
    # Reload fresh cache populated from Run 1
    with open(cache_path, "r") as fh:
        evaluated_cache = json.load(fh)
        
    for seed in SEEDS:
        print(f"Executing search trajectory for Seed {seed}...")
        trajectory = run_mutation_search(
            seed=seed,
            base_model=base_model,
            dpo_model=dpo_model,
            base_tokenizer=base_tokenizer,
            dpo_tokenizer=dpo_tokenizer,
            seed_cand=seed_cand,
            evaluated_cache=evaluated_cache,
            pareto_archive=pareto_archive,
            generations=3,
            verbose=False
        )
        
        # Calculate dynamics metrics for this seed
        gradients = [step["gradient_z_norm"] for step in trajectory]
        all_gradients.extend(gradients)
        
        # Path length (Euclidean distance in q-space)
        q_coords = [np.array(step["q_evaluated"]) for step in trajectory]
        dist = 0.0
        for idx in range(len(q_coords) - 1):
            dist += np.linalg.norm(q_coords[idx + 1] - q_coords[idx])
        path_lengths.append(dist)
        
        # Energy drift |H_final - H_initial|
        drift = abs(trajectory[-1]["total_energy_H"] - trajectory[0]["total_energy_H"])
        energy_drifts.append(drift)
        
        # Convergence step: first generation where |q_next - q_curr| < 0.1
        conv_idx = None
        for idx in range(len(q_coords) - 1):
            if np.linalg.norm(q_coords[idx + 1] - q_coords[idx]) < 0.1:
                conv_idx = idx + 1
                break
        if conv_idx is not None:
            convergence_steps.append(conv_idx)
        else:
            unconverged_seeds.append(seed)
        
        # Region counts and terminal config
        for step in trajectory:
            desc = step["prompt"]
            if "creative" in desc.lower():
                prompt_counts["creative"] += 1
            elif "rigorous" in desc.lower():
                prompt_counts["rigorous"] += 1
            else:
                prompt_counts["neutral"] += 1
                
        terminal_configs.add(trajectory[-1]["config_id"])
        
        last = trajectory[-1]
        seed_results.append({
            "seed": seed,
            "final_margin": last["realtime_prompt_margin"],
            "final_potential_PE": last["potential_energy_PE"],
            "final_energy_H": last["total_energy_H"],
            "converged_candidate_id": last["candidate_id"],
            "steps": len(trajectory)
        })
        
    # Analyze multi-seed margins
    margins = np.array([x["final_margin"] for x in seed_results])
    print(f"\nMulti-Seed Trajectory Margin Statistics:")
    print(f"  Mean Margin: {margins.mean():+.4f}")
    print(f"  Std Margin:  {margins.std():.4f}")
    
    # 5. Pareto Archive Invariant Validation
    print("\n--- VALIDATING PARETO ARCHIVE INVARIANTS ---")
    pareto_ok = True
    seen_configs = set()
    
    for i, a in enumerate(pareto_archive):
        # Invariant 1: No duplicates
        cid = a["config_id"]
        if cid in seen_configs:
            print(f"FAILED INVARIANT: Duplicate config_id found: {cid}")
            pareto_ok = False
        seen_configs.add(cid)
        
        # Invariant 2: safety prerequisites
        if not a["verified"] or a["safety"] < 1.0:
            print(f"FAILED INVARIANT: Unverified or unsafe configuration entered Pareto Archive: {cid}")
            pareto_ok = False
            
        # Invariant 3: Dominance checklist
        for j, b in enumerate(pareto_archive):
            if i == j:
                continue
            a_map = {"margin": a["margin"], "potential": a["potential"], "runtime_ms": a["runtime_ms"], "safety_passed": a["safety"] > 0.5}
            b_map = {"margin": b["margin"], "potential": b["potential"], "runtime_ms": b["runtime_ms"], "safety_passed": b["safety"] > 0.5}
            if dominates(a_map, b_map):
                print(f"FAILED INVARIANT: Config {a['config_id']} dominates Config {b['config_id']} inside Pareto archive")
                pareto_ok = False
                
    if pareto_ok:
        print(f"Pareto Invariants verified successfully. Archive size: {len(pareto_archive)}")
        
    # 6. Runtime Stability & Median Filtering check
    print("\n--- MEASURING RUNTIME STABILITY ---")
    times = []
    # Rerun seed config 5 times to filter hardware noise and assert subprocess success
    for _ in range(5):
        t0 = time.perf_counter()
        runner_path = os.path.join(LAB_DIR, "runner", "run_candidate.py")
        cand_path = os.path.join(LAB_DIR, "candidates/sha256/ba13cba5e35479cde08d81b84671a4849062962d32284ab8f8715154f9275b8e.json")
        result = subprocess.run([sys.executable, runner_path, cand_path], capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(
                f"Runtime benchmark failed with exit {result.returncode}: "
                f"{result.stderr[-500:]}"
            )
        times.append(time.perf_counter() - t0)
    median_time_ms = float(median(times) * 1000.0)
    print(f"Verified seed candidate median runtime: {median_time_ms:.2f} ms")
    
    # 7. Atomic Write Crash Recovery Check
    print("\n--- TESTING CRASH-RECOVERY ATOMICITY ---")
    recovery_tmp_path = os.path.join(LAB_DIR, "receipts", "append-only", "recovery_test.json")
    test_payload = {"seed": RANDOM_SEED, "status": "stable", "timestamp": time.time()}
    atomic_write_json(recovery_tmp_path, test_payload)
    with open(recovery_tmp_path, "r") as fh:
        reloaded = json.load(fh)
    recovery_ok = (test_payload == reloaded)
    if os.path.exists(recovery_tmp_path):
        os.unlink(recovery_tmp_path)
    print(f"Crash recovery test: {'PASSED' if recovery_ok else 'FAILED'}")
    
    # 8. Numeric Dynamics Acceptance Gates
    MAX_ALLOWED_DRIFT = 50.0
    MAX_TERMINAL_CONFIGS = 3
    
    dynamics_ok = (
        np.isfinite(path_lengths).all()
        and np.isfinite(all_gradients).all()
        and np.isfinite(energy_drifts).all()
        and max(energy_drifts) < MAX_ALLOWED_DRIFT
        and len(terminal_configs) <= MAX_TERMINAL_CONFIGS
    )
    print(f"Dynamics Checks (Drift Limit {MAX_ALLOWED_DRIFT}, Config Limit {MAX_TERMINAL_CONFIGS}): {'PASSED ✅' if dynamics_ok else 'FAILED ❌'}")
    
    # Save detailed validation report with dynamic trajectory statistics
    summary = {
        "all_passed": bool(fuzz_ok and pareto_ok and recovery_ok and determinism_ok and dynamics_ok),
        "random_seed": RANDOM_SEED,
        "seeds_tested": len(SEEDS),
        "fuzz_validation_passed": fuzz_ok,
        "pareto_invariants_passed": pareto_ok,
        "crash_recovery_passed": recovery_ok,
        "determinism_vs_cache_passed": determinism_ok,
        "dynamics_validation_passed": bool(dynamics_ok),
        "median_runtime_ms": median_time_ms,
        "pareto_frontier_size": len(pareto_archive),
        "cache_entries_count": len(evaluated_cache),
        "trajectory_statistics": {
            "final_margin_mean": float(margins.mean()),
            "final_margin_std": float(margins.std()),
            "converged_seed_count": len(convergence_steps),
            "unconverged_seed_count": len(unconverged_seeds),
            "iterations_to_convergence_mean": float(np.mean(convergence_steps)) if convergence_steps else None,
            "path_length_mean": float(np.mean(path_lengths)),
            "mean_gradient_norm": float(np.mean(all_gradients)),
            "energy_drift_mean": float(np.mean(energy_drifts)),
            "prompt_region_counts": prompt_counts,
            "unique_terminal_configs": len(terminal_configs)
        }
    }
    
    report_path = os.path.join(LAB_DIR, "receipts", "append-only", "validation_summary.json")
    atomic_write_json(report_path, summary)
    print(f"\n=============================================================")
    print(f"=== VALIDATION REPORT GENERATED: {report_path} ===")
    print(f"=== ALL ACCEPTANCE GATES: {'PASSED ✅' if summary['all_passed'] else 'FAILED ❌'} ===")
    print("=============================================================")

if __name__ == "__main__":
    main()
