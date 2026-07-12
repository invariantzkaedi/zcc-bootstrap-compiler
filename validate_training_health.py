import os
import sys
import json
import math
import statistics
import argparse
import hashlib
import tempfile
import shutil
from pathlib import Path
from scipy.stats import linregress
import torch
import pandas as pd


from zkaedi_security_utils import (
    validate_safe_path,
    load_model_hardened,
    scan_for_known_cves,
)



def is_finite(val):
    if val is None:
        return False
    try:
        fval = float(val)
        return not math.isnan(fval) and not math.isinf(fval)
    except (ValueError, TypeError):
        return False

def get_script_sha256():
    try:
        script_path = os.path.abspath(__file__)
        sha256 = hashlib.sha256()
        with open(script_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                sha256.update(chunk)
        return sha256.hexdigest()
    except Exception as e:
        return f"UNKNOWN_ERROR: {e}"

def validate_ids(name, values):
    if not isinstance(values, list):
        raise ValueError(f"Manifest field '{name}' must be a list")

    normalized = []
    for index, value in enumerate(values):
        if isinstance(value, bool):
            raise ValueError(
                f"Manifest field '{name}' contains boolean ID at index {index}"
            )
        if not isinstance(value, (str, int)):
            raise ValueError(
                f"Manifest field '{name}' contains unsupported ID type "
                f"at index {index}: {type(value).__name__}"
            )
        normalized.append(value)

    if len(normalized) != len(set(normalized)):
        raise ValueError(
            f"Manifest field '{name}' contains duplicate IDs"
        )

    return normalized

def verify_split_manifest(manifest_path):
    manifest_info = {
        "path": manifest_path,
        "sha256": "UNKNOWN",
        "train_count": 0,
        "eval_count": 0,
        "overlap_count": 0,
        "verified": False,
        "error": None
    }
    if not manifest_path or manifest_path == "UNKNOWN":
        return manifest_info
        
    try:
        sha = hashlib.sha256()
        with open(manifest_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                sha.update(chunk)
        manifest_info["sha256"] = sha.hexdigest()
        
        with open(manifest_path, "r", encoding="utf-8") as f:
            content = json.load(f)
            
        if not isinstance(content, dict):
            raise ValueError("Manifest content must be a JSON object")
            
        if "train" not in content or "eval" not in content:
            raise ValueError("Manifest must contain both 'train' and 'eval' keys")
            
        train_list = content["train"]
        eval_list = content["eval"]
        
        train_norm = validate_ids("train", train_list)
        eval_norm = validate_ids("eval", eval_list)
        
        train_set = set(train_norm)
        eval_set = set(eval_norm)
        
        manifest_info["train_count"] = len(train_set)
        manifest_info["eval_count"] = len(eval_set)
        manifest_info["overlap_count"] = len(train_set.intersection(eval_set))
        manifest_info["verified"] = True
    except Exception as e:
        manifest_info["error"] = str(e)
        manifest_info["verified"] = False
        
    return manifest_info

def parse_safe_step(step_raw):
    if isinstance(step_raw, bool):
        raise ValueError("Booleans are not valid step values")
    try:
        fval = float(step_raw)
        if not math.isfinite(fval):
            raise ValueError("Step must be finite")
        if not fval.is_integer():
            raise ValueError("Step must be integral")
        if fval < 0:
            raise ValueError("Step cannot be negative")
        return int(fval)
    except (ValueError, TypeError, OverflowError) as e:
        raise ValueError(f"Malformed step value '{step_raw}': {e}")

def write_verdict_atomic(out_path, status, train_gate, eval_gate, provenance_gate, release, assurance_level, min_eval_records, provenance_fields):
    verdict = {
        "validator": {
            "schema_version": "1.2.0",
            "script_version": "v1.2-hardened",
            "script_sha256": get_script_sha256()
        },
        "task_id": "DPO-ALIGN-RETRAIN-002",
        "status": status,
        "training_health_gate": train_gate,
        "held_out_alignment_gate": {
            **eval_gate,
            "minimum_eval_records": min_eval_records,
            "assurance_level": assurance_level
        },
        "provenance_gate": provenance_gate,
        "release_verdict": release,
        "provenance": provenance_fields
    }
    
    parent_dir = os.path.dirname(os.path.abspath(out_path))
    if parent_dir:
        os.makedirs(parent_dir, exist_ok=True)
        
    fd, temp_path = tempfile.mkstemp(dir=parent_dir or ".", suffix=".tmp")
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            json.dump(verdict, f, indent=2)
            f.flush()
            os.fsync(f.fileno())
        shutil.move(temp_path, out_path)
        print(f"\n[Verdict Engine] Machine-readable evidence atomically written to {out_path}")
    except Exception as e:
        if os.path.exists(temp_path):
            os.remove(temp_path)
        print(f"Error writing atomic verdict JSON to {out_path}: {e}")
        raise e

def recompute_eval_metrics(model_path, base_model, dataset_path, split_manifest_path):
    print(f"[Validator Oracle] Loading base model '{base_model}' on CPU with hardened loader...")
    ref_model, tokenizer = load_model_hardened(base_model)
    
    print(f"[Validator Oracle] Loading checkpoint model '{model_path}' on CPU with hardened loader...")
    model, _ = load_model_hardened(model_path)


    
    print(f"[Validator Oracle] Loading dataset '{dataset_path}'...")
    df = pd.read_parquet(dataset_path)
    
    with open(split_manifest_path, "r") as f:
        manifest = json.load(f)
    eval_indices = manifest["eval"]
    
    eval_df = df.iloc[eval_indices]
    print(f"[Validator Oracle] Evaluated dataset split size: {len(eval_df)}")
    
    losses = []
    margins = []
    positive_count = 0
    beta = 0.1
    
    for idx, row in eval_df.iterrows():
        sys_prompt = row.get("system") or ""
        prompt_text = row.get("prompt") or ""
        chosen_text = row.get("chosen") or ""
        rejected_text = row.get("rejected") or ""
        
        formatted_prompt = f"### System:\n{sys_prompt}\n\n### Instruction:\n{prompt_text}\n\n### Response:\n"
        chosen_prompt = formatted_prompt + chosen_text
        rejected_prompt = formatted_prompt + rejected_text
        
        chosen_enc = tokenizer(chosen_prompt, return_tensors="pt")
        rejected_enc = tokenizer(rejected_prompt, return_tensors="pt")
        prompt_enc = tokenizer(formatted_prompt, return_tensors="pt")
        
        chosen_ids = chosen_enc["input_ids"]
        rejected_ids = rejected_enc["input_ids"]
        prompt_len = prompt_enc["input_ids"].shape[1]
        
        chosen_labels = chosen_ids.clone()
        chosen_labels[:, :prompt_len] = -100
        rejected_labels = rejected_ids.clone()
        rejected_labels[:, :prompt_len] = -100
        
        with torch.no_grad():
            model_chosen_logits = model(**chosen_enc).logits
            model_rejected_logits = model(**rejected_enc).logits
            
            ref_chosen_logits = ref_model(**chosen_enc).logits
            ref_rejected_logits = ref_model(**rejected_enc).logits
            
        def get_logps(logits, labels):
            shift_logits = logits[..., :-1, :].contiguous()
            shift_labels = labels[..., 1:].contiguous()
            loss_fct = torch.nn.CrossEntropyLoss(reduction="none")
            loss = loss_fct(shift_logits.view(-1, shift_logits.size(-1)), shift_labels.view(-1))
            loss = loss.view(shift_labels.size())
            mask = (shift_labels != -100)
            logps = -(loss * mask).sum(dim=-1)
            return logps.item()
            
        m_chosen = get_logps(model_chosen_logits, chosen_labels)
        m_rejected = get_logps(model_rejected_logits, rejected_labels)
        
        r_chosen = get_logps(ref_chosen_logits, chosen_labels)
        r_rejected = get_logps(ref_rejected_logits, rejected_labels)
        
        chosen_ratio = m_chosen - r_chosen
        rejected_ratio = m_rejected - r_rejected
        
        margin = beta * (chosen_ratio - rejected_ratio)
        loss = -torch.nn.functional.logsigmoid(torch.tensor(margin)).item()
        
        losses.append(loss)
        margins.append(margin)
        if margin > 0:
            positive_count += 1
            
    eval_loss = sum(losses) / len(losses)
    eval_mean = sum(margins) / len(margins)
    
    sorted_margins = sorted(margins)
    n = len(sorted_margins)
    if n % 2 == 1:
        eval_median = sorted_margins[n // 2]
    else:
        eval_median = (sorted_margins[n // 2 - 1] + sorted_margins[n // 2]) / 2.0
        
    eval_pos_rate = positive_count / len(margins)
    
    print(f"[Validator Oracle] Recomputed metrics: loss={eval_loss:.6f}, mean={eval_mean:.6f}, median={eval_median:.6f}, rate={eval_pos_rate:.6f}")
    return {
        "eval_loss": eval_loss,
        "eval_preference_margin_mean": eval_mean,
        "eval_preference_margin_median": eval_median,
        "eval_positive_margin_rate": eval_pos_rate
    }

def main():
    scan_for_known_cves()
    parser = argparse.ArgumentParser(description="Hardened DPO Validator")

    parser.add_argument("state_path", help="Path to trainer_state.json")
    parser.add_argument("--out", default="validate_verdict.json", help="Path to output verdict JSON file")
    parser.add_argument("--min-eval-records", type=int, default=3, help="Minimum evaluation records required")
    parser.add_argument("--dataset-hash", default="UNKNOWN", help="Dataset hash/digest")
    parser.add_argument("--checkpoint-digest", default="UNKNOWN", help="Checkpoint hash/digest")
    parser.add_argument("--model-identity", default="UNKNOWN", help="Model identity")
    parser.add_argument("--seed", default="UNKNOWN", help="Random seed used")
    parser.add_argument("--evaluator-identity", default="UNKNOWN", help="Evaluator identity")
    parser.add_argument("--split-manifest", default="UNKNOWN", help="Train/eval split manifest name")
    parser.add_argument("--strict-global-step", action="store_true", help="Fail if top-level global_step is missing")
    parser.add_argument("--expected-log-interval", type=int, default=50, help="Expected step interval between log entries")
    parser.add_argument("--model-path", default=None, help="Path to the trained model checkpoint directory")
    parser.add_argument("--base-model", default="gpt2", help="Base model name used for training")
    parser.add_argument("--dataset-path", default="/mnt/h/agents/train_maxed_validated.parquet", help="Path to local parquet DPO dataset")
    
    args = parser.parse_args()
    
    # === SECURITY: Path validation (fail-closed) ===
    try:
        args.dataset_path = str(validate_safe_path(args.dataset_path, description="dataset", must_exist=True))
        if args.model_path:
            args.model_path = str(validate_safe_path(args.model_path, description="model checkpoint", must_exist=True))
        if args.split_manifest and args.split_manifest != "UNKNOWN":
            args.split_manifest = str(validate_safe_path(args.split_manifest, description="split manifest", must_exist=True))

    except (ValueError, FileNotFoundError) as e:
        print(f"[ZKAEDI SEC] SECURITY BLOCK: {e}")
        sys.exit(1)

    provenance_fields = {
        "dataset_hash": args.dataset_hash,
        "checkpoint_digest": args.checkpoint_digest,
        "model_identity": args.model_identity,
        "seed": args.seed,
        "evaluator_identity": args.evaluator_identity,
        "split_manifest_path": args.split_manifest
    }
    
    provenance_complete = True
    for key in ["dataset_hash", "checkpoint_digest", "model_identity", "seed", "evaluator_identity", "split_manifest_path"]:
        val = provenance_fields[key]
        if not val or val == "UNKNOWN" or str(val).strip() == "":
            provenance_complete = False
            
    if args.split_manifest != "UNKNOWN":
        manifest_info = verify_split_manifest(args.split_manifest)
        if not manifest_info["verified"]:
            print(f"[-] FAIL: Split manifest parsing/validation error: {manifest_info['error']}")
            write_verdict_atomic(
                out_path=args.out,
                status="fail",
                train_gate={"status": "FAIL", "metrics": {}},
                eval_gate={"status": "FAIL", "metrics": {}},
                provenance_gate={"status": "FAIL", "required_fields_complete": provenance_complete, "split_overlap_count": 0, "manifest_details": manifest_info},
                release="REJECTED",
                assurance_level="SMOKE_TEST_ONLY",
                min_eval_records=args.min_eval_records,
                provenance_fields=provenance_fields
            )
            sys.exit(4)
    else:
        manifest_info = {
            "path": "UNKNOWN",
            "sha256": "UNKNOWN",
            "train_count": 0,
            "eval_count": 0,
            "overlap_count": 0,
            "verified": False,
            "error": None
        }
    
    if args.split_manifest != "UNKNOWN":
        if manifest_info["verified"] and manifest_info["overlap_count"] == 0 and manifest_info["error"] is None:
            provenance_gate_status = "PASS" if provenance_complete else "UNATTESTED"
        else:
            provenance_gate_status = "FAIL"
    else:
        provenance_gate_status = "UNATTESTED"
        
    provenance_gate = {
        "status": provenance_gate_status,
        "required_fields_complete": provenance_complete,
        "split_overlap_count": manifest_info["overlap_count"],
        "manifest_details": manifest_info
    }
    
    if not os.path.exists(args.state_path):
        print(f"Error: File not found at '{args.state_path}'")
        sys.exit(4)
        
    try:
        with open(args.state_path, 'r', encoding='utf-8') as f:
            state = json.load(f)
    except Exception as e:
        print(f"Error: Failed to parse JSON from '{args.state_path}': {e}")
        sys.exit(4)
        
    history = state.get("log_history", [])
    if not history:
        print("Error: No training log history found in trainer_state.json")
        sys.exit(4)
        
    train_data = {}
    eval_raw_metrics = {}
    
    for entry in history:
        step_raw = entry.get("step")
        if step_raw is None:
            continue
            
        try:
            step = parse_safe_step(step_raw)
        except ValueError as e:
            print(f"[-] FAIL: Step parsing error: {e}")
            write_verdict_atomic(
                out_path=args.out,
                status="fail",
                train_gate={"status": "FAIL", "metrics": {"step_format_valid": False}},
                eval_gate={"status": "FAIL", "metrics": {}},
                provenance_gate={"status": "FAIL", "required_fields_complete": provenance_complete, "split_overlap_count": manifest_info["overlap_count"]},
                release="REJECTED",
                assurance_level="SMOKE_TEST_ONLY",
                min_eval_records=args.min_eval_records,
                provenance_fields=provenance_fields
            )
            sys.exit(4)
        
        for k, v in entry.items():
            if k in ("loss", "rewards/margins", "eval_loss", "eval_preference_margin_mean", "eval_preference_margin_median", "eval_positive_margin_rate"):
                if v is not None and (not is_finite(v)):
                    print(f"[-] FAIL: Non-finite value detected: {k} = {v} at step {step}")
                    write_verdict_atomic(
                        out_path=args.out,
                        status="fail",
                        train_gate={"status": "FAIL", "metrics": {"all_values_finite": False}},
                        eval_gate={"status": "FAIL", "metrics": {}},
                        provenance_gate={"status": "FAIL", "required_fields_complete": provenance_complete, "split_overlap_count": manifest_info["overlap_count"]},
                        release="REJECTED",
                        assurance_level="SMOKE_TEST_ONLY",
                        min_eval_records=args.min_eval_records,
                        provenance_fields=provenance_fields
                    )
                    sys.exit(4)
                    
        eval_pos_rate = entry.get("eval_rewards/accuracies") or entry.get("eval_positive_margin_rate")
        if eval_pos_rate is not None:
            eval_pos_rate_float = float(eval_pos_rate)
            if not (0.0 <= eval_pos_rate_float <= 1.0):
                print(f"[-] FAIL: eval_positive_margin_rate ({eval_pos_rate}) is out of range [0, 1] at step {step}")
                write_verdict_atomic(
                    out_path=args.out,
                    status="fail",
                    train_gate={"status": "PASS", "metrics": {}},
                    eval_gate={"status": "FAIL", "metrics": {"positive_margin_rate_in_bounds": False}},
                    provenance_gate={"status": "FAIL", "required_fields_complete": provenance_complete, "split_overlap_count": manifest_info["overlap_count"]},
                    release="REJECTED",
                    assurance_level="SMOKE_TEST_ONLY",
                    min_eval_records=args.min_eval_records,
                    provenance_fields=provenance_fields
                )
                sys.exit(4)
        
        loss = entry.get("loss")
        margin = entry.get("rewards/margins")
        if loss is not None and margin is not None:
            train_data[step] = {
                "loss": float(loss),
                "margin": float(margin)
            }
            
        eval_loss = entry.get("eval_loss")
        eval_margin_mean = entry.get("eval_rewards/margins") or entry.get("eval_preference_margin_mean")
        eval_margin_median = entry.get("eval_preference_margin_median")
        
        for k, v in [("eval_loss", eval_loss), 
                     ("eval_preference_margin_mean", eval_margin_mean),
                     ("eval_preference_margin_median", eval_margin_median),
                     ("eval_positive_margin_rate", eval_pos_rate)]:
            if v is not None:
                if step not in eval_raw_metrics:
                    eval_raw_metrics[step] = {}
                if k not in eval_raw_metrics[step]:
                    eval_raw_metrics[step][k] = []
                eval_raw_metrics[step][k].append(float(v))
                
    eval_data = {}
    eval_conflicts = []
    
    for step, metrics in eval_raw_metrics.items():
        eval_data[step] = {}
        for key, values in metrics.items():
            unique_values = sorted(list(set(values)))
            if len(unique_values) > 1:
                print(f"[-] CONFLICT: Step {step} has conflicting values for {key}: {unique_values}")
                eval_conflicts.append({
                    "step": step,
                    "metric": key,
                    "values": unique_values,
                    "status": "CONFLICT"
                })
                eval_data[step][key] = None
            else:
                eval_data[step][key] = unique_values[0]
                
    sorted_train_steps = sorted(train_data.keys())
    
    # Run honest evaluation recomputation if model-path is provided
    if args.model_path:
        recomputed = recompute_eval_metrics(
            model_path=args.model_path,
            base_model=args.base_model,
            dataset_path=args.dataset_path,
            split_manifest_path=args.split_manifest
        )
        final_step = sorted_train_steps[-1] if sorted_train_steps else 25
        if final_step not in eval_data:
            eval_data[final_step] = {}
        eval_data[final_step].update(recomputed)
        
        # Backfill median proxy for earlier eval steps
        for step in eval_data:
            if eval_data[step].get("eval_preference_margin_median") is None:
                eval_data[step]["eval_preference_margin_median"] = eval_data[step].get("eval_preference_margin_mean")
                
    sorted_eval_steps = sorted(eval_data.keys())
    
    train_gate_passed = True
    train_metrics = {
        "final_step": None,
        "final_training_loss": None,
        "last_k_training_margin_median": None,
        "recent_margin_slope": None,
        "recent_margin_slope_p_value": None,
        "all_values_finite": True,
        "global_step_consistent": True,
        "warnings": []
    }
    
    print("=== TRAINING LOG HEALTH METRICS ===")
    if not sorted_train_steps:
        print("[-] FAIL: No training metrics parsed.")
        train_gate_passed = False
    else:
        final_step = sorted_train_steps[-1]
        final_loss = train_data[final_step]["loss"]
        
        expected_global_step_raw = state.get("global_step")
        if expected_global_step_raw is None:
            if args.strict_global_step:
                print("[-] FAIL: Missing expected global_step in strict mode.")
                train_gate_passed = False
                train_metrics["global_step_consistent"] = False
            else:
                print("[-] WARNING: Missing expected global_step in trainer state.")
        else:
            try:
                expected_global_step = parse_safe_step(expected_global_step_raw)
                if final_step > expected_global_step:
                    print(f"[-] FAIL: Parsed maximum step ({final_step}) exceeds global_step ({expected_global_step})")
                    train_gate_passed = False
                    train_metrics["global_step_consistent"] = False
                elif expected_global_step > final_step:
                    diff = expected_global_step - final_step
                    if diff > args.expected_log_interval:
                        print(f"[-] FAIL: global_step ({expected_global_step}) is significantly ahead of parsed step ({final_step}) by {diff} (> log interval {args.expected_log_interval})")
                        train_gate_passed = False
                        train_metrics["global_step_consistent"] = False
                    else:
                        print(f"[-] WARNING: global_step ({expected_global_step}) is ahead of parsed step ({final_step}) by {diff} (<= log interval {args.expected_log_interval})")
            except ValueError as e:
                print(f"[-] FAIL: Invalid global_step format in state: {e}")
                train_gate_passed = False
                train_metrics["global_step_consistent"] = False
        
        k = min(len(sorted_train_steps), 5)
        epsilon_margin = 0.01
        last_k_margins = [train_data[s]["margin"] for s in sorted_train_steps[-k:]]
        last_k_median_margin = statistics.median(last_k_margins)
        
        W = 20
        recent_steps = sorted_train_steps[-W:]
        recent_margins = [train_data[s]["margin"] for s in recent_steps]
        
        slope, p_value = 0.0, 1.0
        if len(recent_steps) < 2:
            print(f"[-] WARNING: Insufficient steps ({len(recent_steps)}) for recent slope calculation.")
            train_gate_passed = False
        else:
            if len(recent_steps) < 10:
                warn_msg = f"OLS slope calculated on only {len(recent_steps)} data points. This is operationally weak."
                print(f"[-] WARNING: {warn_msg}")
                train_metrics["warnings"].append(warn_msg)
            slope, intercept, r_val, p_value, std_err = linregress(recent_steps, recent_margins)
            
        loss_limit = math.log(2.0) - 0.02
        
        train_metrics.update({
            "final_step": final_step,
            "final_training_loss": final_loss,
            "last_k_training_margin_median": last_k_median_margin,
            "recent_margin_slope": slope,
            "recent_margin_slope_p_value": p_value
        })
        
        print(f"Total Steps Logged: {len(sorted_train_steps)} (Final Step: {final_step})")
        print(f"Final Step Loss:   {final_loss:.6f} (Reference limit ln(2)-0.02 = {loss_limit:.6f})")
        print(f"Last-{k} Median Margin: {last_k_median_margin:.6f} (Operational threshold = {epsilon_margin:.6f})")
        print(f"OLS Slope (last {len(recent_steps)} steps): {slope:.6f} (p-value={p_value:.4f})")
        
        # Gating checks
        if final_loss >= loss_limit:
            print(f"[-] FAIL: Final training loss ({final_loss:.6f}) is not below limit {loss_limit:.6f}")
            train_gate_passed = False
        else:
            print(f"[+] PASS: Final training loss ({final_loss:.6f}) is below limit {loss_limit:.6f}")
            
        if last_k_median_margin <= epsilon_margin:
            print(f"[-] FAIL: Last-{k} Median Margin ({last_k_median_margin:.6f}) is not above {epsilon_margin:.6f}")
            train_gate_passed = False
        else:
            print(f"[+] PASS: Last-{k} Median Margin ({last_k_median_margin:.6f}) is above {epsilon_margin:.6f}")
            
        # OLS slope p-value tripwire check
        if p_value == 0.0 or p_value == 0.0000:
            print("[-] FAIL: Arithmetic Tripwire: Margins slope p-value is exactly 0.0000 (synthetic ramp signature).")
            train_gate_passed = False
        elif slope <= 0.0:
            print(f"[-] FAIL: OLS margin-slope ({slope:.6f}) is not positive.")
            train_gate_passed = False
        elif p_value >= 0.05:
            print(f"[-] FAIL: OLS margin-slope ({slope:.6f}) is not statistically significant (p={p_value:.4f} >= 0.05).")
            train_gate_passed = False
        else:
            print(f"[+] PASS: OLS margin-slope ({slope:.6f}) is positive and significant (p={p_value:.4f} < 0.05).")

    eval_gate_status = "UNVERIFIED"
    eval_metrics = {
        "final_eval_step": None,
        "eval_preference_margin_mean": None,
        "eval_preference_margin_median": None,
        "eval_positive_margin_rate": None,
        "eval_loss": None,
        "min_eval_records_satisfied": True,
        "eval_conflicts": eval_conflicts
    }
    
    print("\n=== HELD-OUT ALIGNMENT METRICS ===")
    if not sorted_eval_steps:
        print("[-] HELD-OUT GATE: UNVERIFIED (No evaluation records found)")
    elif eval_conflicts:
        print("[-] FAIL: Evaluation merging conflicts detected.")
        eval_gate_status = "FAIL"
    else:
        observed_eval_records = len(sorted_eval_steps)
        min_eval_records_satisfied = (observed_eval_records >= args.min_eval_records)
        eval_metrics["min_eval_records_satisfied"] = min_eval_records_satisfied
        
        if not min_eval_records_satisfied:
            print(f"[-] FAIL: Evaluation records count ({observed_eval_records}) is below required minimum ({args.min_eval_records})")
            eval_gate_status = "FAIL"
        else:
            final_eval_step = sorted_eval_steps[-1]
            eval_entry = eval_data[final_eval_step]
            
            eval_loss = eval_entry.get("eval_loss")
            eval_mean = eval_entry.get("eval_preference_margin_mean")
            eval_med = eval_entry.get("eval_preference_margin_median")
            eval_pos = eval_entry.get("eval_positive_margin_rate")
            
            eval_metrics.update({
                "final_eval_step": final_eval_step,
                "eval_preference_margin_mean": eval_mean,
                "eval_preference_margin_median": eval_med,
                "eval_positive_margin_rate": eval_pos,
                "eval_loss": eval_loss
            })
            
            print(f"Total Eval Records Logged: {observed_eval_records} (Final Eval Step: {final_eval_step})")
            print(f"Eval Loss:       {eval_loss if eval_loss is not None else 'N/A'}")
            print(f"Eval Margin Mean: {eval_mean if eval_mean is not None else 'N/A'}")
            print(f"Eval Margin Med:  {eval_med if eval_med is not None else 'N/A'}")
            print(f"Eval Pos Rate:   {eval_pos if eval_pos is not None else 'N/A'}")
            
            # Gating requirements
            eval_passed = True
            if eval_loss is None or eval_loss >= (math.log(2.0) - 0.02):
                print(f"[-] FAIL: Eval loss is missing or >= {math.log(2.0) - 0.02:.6f}")
                eval_passed = False
            if eval_mean is None or eval_mean <= 0.01:
                print("[-] FAIL: Eval margin mean is missing or <= 0.01")
                eval_passed = False
            if eval_med is None or eval_med <= 0.01:
                print("[-] FAIL: Eval margin median is missing or <= 0.01")
                eval_passed = False
            if eval_pos is None or eval_pos < 0.60:
                print("[-] FAIL: Eval positive margin rate is missing or < 0.60")
                eval_passed = False
                
            # Positive margin rate arithmetic integrity tripwire check
            if eval_pos is not None and manifest_info["verified"]:
                eval_count = manifest_info["eval_count"]
                rate_count = eval_pos * eval_count
                if abs(rate_count - round(rate_count)) > 1e-4:
                    print(f"[-] FAIL: Arithmetic Tripwire: Positive margin rate {eval_pos} is not possible for {eval_count} samples.")
                    eval_passed = False
                    
            # Symmetry warning tripwire
            if eval_mean is not None and eval_med is not None:
                if abs(eval_mean - eval_med) < 1e-6:
                    print(f"[-] WARNING: eval_preference_margin_mean ({eval_mean}) is identical to eval_preference_margin_median ({eval_med}) to 6 decimal places.")
                    
            if eval_passed:
                print("[+] PASS: All held-out alignment gates satisfied.")
                eval_gate_status = "PASS"
            else:
                eval_gate_status = "FAIL"
                
    observed_eval_records = len(sorted_eval_steps)
    assurance_level = (
        "STANDARD"
        if observed_eval_records >= 3
        else "SMOKE_TEST_ONLY"
    )
        
    status_code = "fail"
    if not sorted_train_steps or not sorted_eval_steps:
        status_code = "blocked"
    elif train_gate_passed and eval_gate_status == "PASS" and provenance_gate["status"] in ("PASS", "UNATTESTED"):
        status_code = "pass"
        
    release_verdict = "REJECTED"
    if status_code == "blocked":
        release_verdict = "BLOCKED"
    elif train_gate_passed and eval_gate_status == "PASS":
        if manifest_info["overlap_count"] > 0 or (args.split_manifest != "UNKNOWN" and not manifest_info["verified"]):
            release_verdict = "REJECTED"
            status_code = "fail"
            print("[-] FAIL: Split manifest has overlaps or failed verification.")
        else:
            if provenance_complete:
                release_verdict = "METRIC_GATES_PASSED"
            else:
                release_verdict = "METRIC_GATES_PASSED_UNATTESTED"
                
    if release_verdict in ("METRIC_GATES_PASSED", "METRIC_GATES_PASSED_UNATTESTED"):
        print("\n=== VERDICT: TRAINING HEALTH AND HELD-OUT ALIGNMENT GATES PASSED ===")
    else:
        print("\n=== VERDICT: TRAINING HEALTH OR ALIGNMENT GATE FAILED ===")
        
    write_verdict_atomic(
        out_path=args.out,
        status=status_code,
        train_gate={"status": "PASS" if train_gate_passed else "FAIL", "metrics": train_metrics},
        eval_gate={"status": eval_gate_status, "metrics": eval_metrics},
        provenance_gate=provenance_gate,
        release=release_verdict,
        assurance_level=assurance_level,
        min_eval_records=args.min_eval_records,
        provenance_fields=provenance_fields
    )
    
    if release_verdict == "METRIC_GATES_PASSED":
        sys.exit(0)
    elif release_verdict == "METRIC_GATES_PASSED_UNATTESTED":
        sys.exit(2)
    elif release_verdict == "BLOCKED":
        sys.exit(3)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
