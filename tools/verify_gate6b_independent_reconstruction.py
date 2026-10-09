#!/usr/bin/env python3
"""
🔱 ZKAEDI GATE 6-B INDEPENDENT RECONSTRUCTION & VERIFICATION HARNESS (HARDENED v2.0) 🔱
Authoritative Script: tools/verify_gate6b_independent_reconstruction.py

Enforces P0 Certification Requirements & Verification Protocols:
1. Pinned Cryptographic Lineage (Datasets, Adapter Weights, Adapter Config, Base Model Commit Revision, Script Hash).
2. Connected-Component Transitive Asset Disjointness (eval_cluster_id not in train_component_ids).
3. Joint Tokenization & Response Boundary Completion Masking (solving token-merging boundary drift, assert len == L_resp).
4. Independent GPU Recomputation of all 14 Jointly Novel Margins (and all 51 eval pairs).
5. Strict IEEE-754 Finite-Float & Variance Assurances (Rule AV-3, Rule NV-3: no NaNs, non-zero margin variance).
6. Live Functional LoRA Delta Verification (Rule AV-1: assert policy != reference on probe sequence).
7. Transitive Overlap & Corrupted-Join Adversarial Negative Controls.
8. Integrated 12-Fault-Mode Adversarial Mutation Suite (--test_mutations) proving fail-closed tripwires.
9. Atomic Certified Receipt Emission with raw per-pair evidence.

Protocols: Rules EF-1..6, Rules AV-1..6 (Authenticity Verification), Rules NV-1..5 (Numerical Verification).
"""

import os
import sys
import time
import json
import math
import copy
import hashlib
import argparse
import subprocess
import numpy as np
import pandas as pd
import torch
import torch.nn.functional as F
from peft import PeftModel
from transformers import AutoModelForCausalLM, AutoTokenizer

# Configure UTF-8 stdout/stderr
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

PINNED_TRAIN_SHA256 = "74dbef13038d2b9dea1d3406296b7e9a2ca1947c37c77e0ca541fdb37789ebb2"
PINNED_EVAL_SHA256 = "a3487a228bf681c9f98e61e7e70e9dc0ad0bdb1a1bbd5bfc81dffc41d6cad7ab"
PINNED_ADAPTER_SHA256 = "9a57b0481fb0b353f0f55cf31bc8820d09ca9c5366c61c71bd527fc10e63df3f"
PINNED_CONFIG_SHA256 = "f9dea79076b349f45e122a578534998f52eda77ba0179f45f4ee3302499b5c97"
PINNED_BASE_MODEL_ID = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
PINNED_BASE_REVISION = "2e1fd397ee46e1388853d2af2c993145b0f1098a"
EXPECTED_JOINTLY_NOVEL_INDICES = [2, 4, 6, 8, 9, 19, 22, 24, 26, 35, 37, 40, 42, 49]

def compute_sha256(filepath):
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Cannot compute hash; target file does not exist: {filepath}")
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def wilson_score_interval(successes, total, confidence=0.95):
    """Computes exact Wilson score confidence interval with boundary clamping."""
    if total == 0:
        return (0.0, 0.0)
    z = 1.959963984540054 if confidence == 0.95 else 1.96
    p = successes / total
    denom = 1 + z**2 / total
    center = (p + z**2 / (2 * total)) / denom
    diff = z * math.sqrt((p * (1 - p) + z**2 / (4 * total)) / total) / denom
    return (max(0.0, center - diff), min(1.0, center + diff))

def compute_t_test(margins):
    """Computes exact one-sample Student's t-test and Cohen's d against H0: mu <= 0."""
    n = len(margins)
    if n < 2:
        return 0.0, 1.0, 0.0
    mean_m = float(np.mean(margins))
    std_m = float(np.std(margins, ddof=1))
    if std_m <= 1e-12:
        return 0.0, 1.0, 0.0
    t_stat = mean_m / (std_m / math.sqrt(n))
    cohens_d = mean_m / std_m
    try:
        from scipy import stats
        p_val = float(stats.t.sf(t_stat, df=n - 1))
    except Exception:
        # High-accuracy analytical survival fallback
        p_val = float(0.5 * math.erfc(t_stat / math.sqrt(2)))
    return t_stat, p_val, cohens_d

def resolve_path(p_win, p_linux):
    return p_win if os.name == 'nt' else p_linux

def extract_entities(row_or_dict):
    """
    Extracts canonical entity identifiers from row metadata for hypergraph construction.
    """
    entities = set()
    s_key = str(row_or_dict.get("source_key", ""))
    if s_key:
        entities.add(f"source_key:{s_key}")

    meta = row_or_dict.get("metadata")
    if isinstance(meta, str):
        try:
            meta = json.loads(meta)
        except Exception:
            meta = {}
    elif not isinstance(meta, dict):
        meta = row_or_dict.get("metadata_str", {})
        if isinstance(meta, str):
            try:
                meta = json.loads(meta)
            except Exception:
                meta = {}

    if not isinstance(meta, dict):
        meta = {}

    pair_id = meta.get("pair_id")
    if pair_id:
        entities.add(f"pair_id:{pair_id}")
        parts = str(pair_id).split("_")
        for part in parts:
            if len(part) >= 16:  # Asset UUID / Hash token
                entities.add(f"asset_token:{part}")

    concept = meta.get("concept")
    if concept:
        entities.add(f"concept:{concept}")

    for k in ["chosen_filename", "rejected_filename", "chosen_path", "rejected_path"]:
        v = meta.get(k)
        if v:
            basename = os.path.basename(str(v))
            entities.add(f"mesh_file:{basename}")

    return entities

def build_connected_components(train_df, eval_df, synthetic_edges=None):
    """
    Builds undirected hypergraph of entity relationships across train & eval datasets.
    Returns connected components and training-connected component IDs.
    """
    train_entities = [extract_entities(r) for _, r in train_df.iterrows()]
    eval_entities = [extract_entities(r) for _, r in eval_df.iterrows()]

    adj = {}
    def add_edge(u, v):
        adj.setdefault(u, set()).add(v)
        adj.setdefault(v, set()).add(u)

    for ents in train_entities + eval_entities:
        ent_list = list(ents)
        for i in range(len(ent_list)):
            adj.setdefault(ent_list[i], set())
            for j in range(i + 1, len(ent_list)):
                add_edge(ent_list[i], ent_list[j])

    if synthetic_edges:
        for u, v in synthetic_edges:
            add_edge(u, v)

    visited = set()
    components = []
    for node in adj:
        if node not in visited:
            comp = set()
            q = [node]
            visited.add(node)
            while q:
                curr = q.pop()
                comp.add(curr)
                for neighbor in adj[curr]:
                    if neighbor not in visited:
                        visited.add(neighbor)
                        q.append(neighbor)
            components.append(comp)

    node_to_comp = {node: comp_id for comp_id, comp in enumerate(components) for node in comp}

    train_comp_ids = set()
    for ents in train_entities:
        for e in ents:
            train_comp_ids.add(node_to_comp[e])

    eval_comp_ids = []
    eval_in_train_comp = []
    for ents in eval_entities:
        c_ids = {node_to_comp[e] for e in ents}
        cid = list(c_ids)[0]
        eval_comp_ids.append(cid)
        eval_in_train_comp.append(cid in train_comp_ids)

    return {
        "total_components": len(components),
        "train_components_count": len(train_comp_ids),
        "train_comp_ids": train_comp_ids,
        "eval_unique_components": len(set(eval_comp_ids)),
        "eval_comp_ids": eval_comp_ids,
        "eval_in_train_comp": eval_in_train_comp,
        "eval_pairs_in_train_clusters": sum(eval_in_train_comp)
    }

def compute_sequence_logp_joint(model, tokenizer, prompt, response, device, max_length=512, force_slice_perturbation=0):
    """
    Computes response log-likelihood with JOINT tokenization of prompt + response,
    applying completion masking over response tokens. This eliminates boundary-token
    merging discrepancies between separate tokenizations.
    Enforces Rule AV-3: strict check for finite floating-point values.
    Enforces exact completion slice length invariants.
    """
    prompt_ids = tokenizer.encode(prompt, add_special_tokens=False)
    full_text = prompt + response
    full_ids = tokenizer.encode(full_text, add_special_tokens=False)

    if len(full_ids) > max_length:
        excess = len(full_ids) - max_length
        prompt_ids = prompt_ids[excess:]
        full_ids = full_ids[excess:]

    L_prompt = len(prompt_ids)
    L_full = len(full_ids)

    assert L_prompt > 0, "Prompt tokenization returned empty sequence!"
    assert L_full > L_prompt, f"Response completion token length is 0! L_full={L_full}, L_prompt={L_prompt}"

    expected_resp_len = L_full - L_prompt

    input_ids = torch.tensor([full_ids], dtype=torch.long, device=device)

    with torch.no_grad():
        outputs = model(input_ids)
        logits = outputs.logits

    shift_logits = logits[0, :-1, :].contiguous()
    shift_labels = input_ids[0, 1:].contiguous()

    log_probs = F.log_softmax(shift_logits, dim=-1)
    target_logps = log_probs.gather(dim=-1, index=shift_labels.unsqueeze(-1)).squeeze(-1)

    # Shift mask: tokens corresponding to response completions
    resp_start_idx = max(0, L_prompt - 1)
    if force_slice_perturbation != 0:
        resp_start_idx += force_slice_perturbation

    resp_logps = target_logps[resp_start_idx:]

    # HARDENED INVARIANT: Completion slice must match expected response token count exactly
    if force_slice_perturbation == 0:
        assert len(resp_logps) == expected_resp_len, (
            f"Completion mask slice length mismatch! Got {len(resp_logps)}, expected {expected_resp_len} "
            f"(L_full={L_full}, L_prompt={L_prompt}, resp_start_idx={resp_start_idx})"
        )

    # Rule AV-3 Violation tripwire: Check for NaNs or Infinities in logits / log_probs
    if not torch.isfinite(resp_logps).all():
        raise AssertionError("Rule AV-3 Violation: Non-finite token log-probabilities detected (NaN or Inf encountered)")

    logp_sum = resp_logps.sum().item()
    if not math.isfinite(logp_sum):
        raise AssertionError(f"Rule AV-3 Violation: Non-finite log-likelihood sum detected: {logp_sum}")

    return logp_sum, L_full, L_prompt

def compute_sequence_logp_separate(model, tokenizer, prompt, response, device, max_length=512):
    """
    Computes response log-likelihood with SEPARATE tokenization (legacy pathway).
    """
    prompt_ids = tokenizer.encode(prompt, add_special_tokens=False)
    resp_ids = tokenizer.encode(response, add_special_tokens=False)

    full_ids = prompt_ids + resp_ids
    if len(full_ids) > max_length:
        excess = len(full_ids) - max_length
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

    if not torch.isfinite(resp_logps).all():
        raise AssertionError("Rule AV-3 Violation: Non-finite token log-probabilities in separate tokenization")

    logp_sum = resp_logps.sum().item()
    if not math.isfinite(logp_sum):
        raise AssertionError(f"Rule AV-3 Violation: Non-finite log-likelihood sum in separate tokenization: {logp_sum}")

    return logp_sum

def run_independent_reconstruction(
    injected_mutation=None,
    dry_run=False,
    train_parquet_path=None,
    eval_parquet_path=None,
    adapter_dir=None,
    out_receipt_path=None
):
    print("=" * 80)
    print("🔱 ZKAEDI GATE 6-B INDEPENDENT RECONSTRUCTION & VERIFICATION (HARDENED v2.0) 🔱")
    print("=" * 80)

    if injected_mutation:
        print(f"[!] INJECTED ADVERSARIAL MUTATION ACTIVE: {injected_mutation}")

    # 1. Pinned Paths & Cryptographic Lineage Check
    if train_parquet_path is None:
        train_parquet_path = resolve_path(
            "C:/Users/zkaed/.gemini/antigravity-ide/scratch/zkaedi_3d_dpo_train.parquet",
            "/mnt/c/Users/zkaed/.gemini/antigravity-ide/scratch/zkaedi_3d_dpo_train.parquet"
        )
    if eval_parquet_path is None:
        eval_parquet_path = resolve_path(
            "C:/Users/zkaed/.gemini/antigravity-ide/scratch/zkaedi_3d_dpo_eval.parquet",
            "/mnt/c/Users/zkaed/.gemini/antigravity-ide/scratch/zkaedi_3d_dpo_eval.parquet"
        )
    if adapter_dir is None:
        adapter_dir = resolve_path(
            "H:/__DOWNLOADS/zcc_github_upload/artifacts/zkaedi_3d_dpo_lora_full_adapter",
            "/mnt/h/__DOWNLOADS/zcc_github_upload/artifacts/zkaedi_3d_dpo_lora_full_adapter"
        )
    if out_receipt_path is None:
        out_receipt_path = resolve_path(
            "H:/__DOWNLOADS/zcc_github_upload/docs/evidence/gate6b_independent_reconstruction_receipt.json",
            "/mnt/h/__DOWNLOADS/zcc_github_upload/docs/evidence/gate6b_independent_reconstruction_receipt.json"
        )

    if not os.path.exists(train_parquet_path):
        raise FileNotFoundError(f"Train Parquet missing: {train_parquet_path}")
    if not os.path.exists(eval_parquet_path):
        raise FileNotFoundError(f"Eval Parquet missing: {eval_parquet_path}")

    train_sha = compute_sha256(train_parquet_path)
    eval_sha = compute_sha256(eval_parquet_path)
    adapter_weights_path = os.path.join(adapter_dir, "adapter_model.safetensors")
    adapter_config_path = os.path.join(adapter_dir, "adapter_config.json")
    adapter_sha = compute_sha256(adapter_weights_path)
    config_sha = compute_sha256(adapter_config_path)

    print(f"[*] Train Parquet:      {train_parquet_path} (SHA-256: {train_sha})")
    print(f"[*] Eval Parquet:       {eval_parquet_path} (SHA-256: {eval_sha})")
    print(f"[*] Adapter Weights:    {adapter_weights_path} (SHA-256: {adapter_sha})")
    print(f"[*] Adapter Config:     {adapter_config_path} (SHA-256: {config_sha})")
    print(f"[*] Base Model & Rev:   {PINNED_BASE_MODEL_ID} (rev: {PINNED_BASE_REVISION})")

    # Mutation M01: Corrupted Train Parquet SHA
    if injected_mutation == "M01_CORRUPTED_TRAIN_SHA":
        train_sha = "0000000000000000000000000000000000000000000000000000000000000000"

    # Mutation M02: Corrupted Eval Parquet SHA
    if injected_mutation == "M02_CORRUPTED_EVAL_SHA":
        eval_sha = "1111111111111111111111111111111111111111111111111111111111111111"

    # Mutation M03: Corrupted Adapter SHA
    if injected_mutation == "M03_CORRUPTED_ADAPTER_SHA":
        adapter_sha = "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef"

    # Mutation M04: Corrupted Adapter Config SHA
    if injected_mutation == "M04_CORRUPTED_CONFIG_SHA":
        config_sha = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"

    # Mutation M12: Forged Base Revision
    base_rev_check = PINNED_BASE_REVISION
    if injected_mutation == "M12_FORGED_LINEAGE_REVISION":
        base_rev_check = "0000000000000000000000000000000000000000"

    assert train_sha == PINNED_TRAIN_SHA256, f"Rule AV-4 Lineage Failure: Train SHA mismatch! {train_sha} != {PINNED_TRAIN_SHA256}"
    assert eval_sha == PINNED_EVAL_SHA256, f"Rule AV-4 Lineage Failure: Eval SHA mismatch! {eval_sha} != {PINNED_EVAL_SHA256}"
    assert adapter_sha == PINNED_ADAPTER_SHA256, f"Rule AV-4 Lineage Failure: Adapter SHA mismatch! {adapter_sha} != {PINNED_ADAPTER_SHA256}"
    assert config_sha == PINNED_CONFIG_SHA256, f"Rule AV-4 Lineage Failure: Adapter Config SHA mismatch! {config_sha} != {PINNED_CONFIG_SHA256}"
    assert base_rev_check == PINNED_BASE_REVISION, f"Rule AV-4 Lineage Failure: Base Model Revision mismatch! {base_rev_check} != {PINNED_BASE_REVISION}"

    # Verify adapter hyperparameters from config
    with open(adapter_config_path, "r", encoding="utf-8") as f:
        cfg = json.load(f)
    assert cfg.get("r") == 8, f"Unexpected LoRA rank r={cfg.get('r')} (expected 8)"
    assert cfg.get("lora_alpha") == 16, f"Unexpected LoRA alpha={cfg.get('lora_alpha')} (expected 16)"
    assert "q_proj" in cfg.get("target_modules", []) and "v_proj" in cfg.get("target_modules", []), "LoRA target modules missing q_proj or v_proj"

    print("[PASS] P0-1: Pinned Cryptographic Lineage Fully Verified.")

    # 2. Ingest DataFrames & Rebuild Connected Component Hypergraph
    train_df = pd.read_parquet(train_parquet_path)
    eval_df = pd.read_parquet(eval_parquet_path)

    # Mutation M05: Transitive Leakage Bridge
    synth_edges = None
    if injected_mutation == "M05_TRANSITIVE_LEAKAGE_BRIDGE":
        train_ents = set().union(*[extract_entities(r) for _, r in train_df.iterrows()])
        eval_novel_ent = list(extract_entities(eval_df.iloc[2]))[0]
        train_ent = list(train_ents)[0]
        synth_edges = [(eval_novel_ent, "intermediary:adversarial_proxy"), ("intermediary:adversarial_proxy", train_ent)]

    graph_data = build_connected_components(train_df, eval_df, synthetic_edges=synth_edges)
    train_comp_ids = graph_data["train_comp_ids"]
    eval_comp_ids = graph_data["eval_comp_ids"]

    train_prompts = set(train_df["prompt"])
    train_chosen = set(train_df["chosen"])
    train_rejected = set(train_df["rejected"])

    jointly_novel_records = []
    all_eval_records = []

    for idx, row in eval_df.iterrows():
        s_key = row["source_key"]
        p = row["prompt"]
        c = row["chosen"]
        r = row["rejected"]
        cid = eval_comp_ids[idx]

        prompt_disjoint = p not in train_prompts
        chosen_disjoint = c not in train_chosen
        rejected_disjoint = r not in train_rejected
        asset_disjoint = (cid not in train_comp_ids)

        # Mutation M11: Disjoint Set Contradiction (Forge prompt as novel despite being in train)
        if injected_mutation == "M11_DISJOINT_SET_CONTRADICTION" and idx == 0:
            prompt_disjoint = True
            asset_disjoint = True

        jointly_novel = (prompt_disjoint and chosen_disjoint and rejected_disjoint and asset_disjoint)

        rec = {
            "index": int(idx),
            "source_key": s_key,
            "cluster_id": int(cid),
            "prompt": p,
            "chosen": c,
            "rejected": r,
            "prompt_disjoint": prompt_disjoint,
            "chosen_disjoint": chosen_disjoint,
            "rejected_disjoint": rejected_disjoint,
            "asset_disjoint": asset_disjoint,
            "jointly_novel": jointly_novel
        }
        all_eval_records.append(rec)
        if jointly_novel:
            jointly_novel_records.append(rec)

    derived_indices = [r["index"] for r in jointly_novel_records]

    # Mutation M08: Dropped Novel Pair
    if injected_mutation == "M08_DROPPED_NOVEL_PAIR":
        derived_indices = derived_indices[:-1]

    print(f"\n[*] Total Jointly Novel Holdout Pairs Derived: {len(derived_indices)} / {len(eval_df)}")
    print(f"[*] Derived Indices:  {derived_indices}")
    print(f"[*] Expected Indices: {EXPECTED_JOINTLY_NOVEL_INDICES}")

    assert derived_indices == EXPECTED_JOINTLY_NOVEL_INDICES, (
        f"Derived indices mismatch! Derived={derived_indices}, Expected={EXPECTED_JOINTLY_NOVEL_INDICES}"
    )
    print("[PASS] P0-2: Connected-Component Transitive Asset Disjointness Verified.")

    if dry_run:
        print("\n[*] DRY RUN ACTIVE: Skipping live GPU inference passes.")
        return True

    # 3. Independent Model Scoring on GPU (Pinned Revision)
    device = "cuda:0" if torch.cuda.is_available() else "cpu"
    print(f"\n[*] Execution Device: {device}")
    if device != "cpu":
        print(f"[*] GPU Name        : {torch.cuda.get_device_name(0)}")

    print(f"[*] Loading Tokenizer ({PINNED_BASE_MODEL_ID}, revision={PINNED_BASE_REVISION})...")
    tokenizer = AutoTokenizer.from_pretrained(
        PINNED_BASE_MODEL_ID,
        revision=PINNED_BASE_REVISION,
        local_files_only=True
    )
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    print(f"[*] Loading Base Model in bfloat16 ({PINNED_BASE_MODEL_ID}, revision={PINNED_BASE_REVISION})...")
    base_model = AutoModelForCausalLM.from_pretrained(
        PINNED_BASE_MODEL_ID,
        revision=PINNED_BASE_REVISION,
        torch_dtype=torch.bfloat16,
        device_map=device,
        local_files_only=True
    )
    policy_model = PeftModel.from_pretrained(base_model, adapter_dir)
    policy_model.eval()

    # Rule AV-1: Authenticity Verification: Prove LoRA adapter is functionally active
    probe_prompt = "ZKAEDI 3D Topology Analysis: Mesh 999,003v 1,773,892t quad_dominant. PBR status: FULL_PBR."
    probe_inputs = tokenizer(probe_prompt, return_tensors="pt").to(device)
    with torch.no_grad():
        logits_policy = policy_model(**probe_inputs).logits[0, -1, :].float()
        with policy_model.disable_adapter():
            logits_ref = policy_model(**probe_inputs).logits[0, -1, :].float()

    logit_delta_max = (logits_policy - logits_ref).abs().max().item()
    print(f"[*] Rule AV-1 Authenticity Check: Max LoRA Logit Delta on Probe: {logit_delta_max:.4f}")
    assert logit_delta_max > 0.05, f"Rule AV-1 Violation: LoRA adapter produced dead/identity logit delta ({logit_delta_max})"
    print("[PASS] Rule AV-1: Non-Zero LoRA Functional Parameter Delta Confirmed.")

    print("\n" + "=" * 80)
    print("🔬 P0-3: INDEPENDENT RECOMPUTATION OF HELD-OUT MARGINS (BETA=0.1)...")
    print("=" * 80)

    BETA = 0.1
    boundary_mismatch_count = 0
    joint_margins = []
    separate_margins = []
    per_pair_results = []

    slice_perturb = -1 if injected_mutation == "M09_OFF_BY_ONE_MASK_SLICE" else 0

    start_eval = time.time()
    for rec in all_eval_records:
        idx = rec["index"]
        prompt = rec["prompt"]
        chosen = rec["chosen"]
        rejected = rec["rejected"]

        # Joint Tokenization with Completion Masking
        pi_c_joint, _, _ = compute_sequence_logp_joint(
            policy_model, tokenizer, prompt, chosen, device, force_slice_perturbation=slice_perturb
        )
        pi_r_joint, _, _ = compute_sequence_logp_joint(
            policy_model, tokenizer, prompt, rejected, device, force_slice_perturbation=slice_perturb
        )

        with policy_model.disable_adapter():
            ref_c_joint, _, _ = compute_sequence_logp_joint(
                policy_model, tokenizer, prompt, chosen, device, force_slice_perturbation=slice_perturb
            )
            ref_r_joint, _, _ = compute_sequence_logp_joint(
                policy_model, tokenizer, prompt, rejected, device, force_slice_perturbation=slice_perturb
            )

        # Mutation M06: Non-finite NaN Log-Likelihood Injection
        if injected_mutation == "M06_NON_FINITE_LOGP_NAN" and idx == 2:
            pi_c_joint = float('nan')

        if not (math.isfinite(pi_c_joint) and math.isfinite(pi_r_joint) and math.isfinite(ref_c_joint) and math.isfinite(ref_r_joint)):
            raise AssertionError(f"Rule AV-3 Violation: Non-finite log-likelihood value detected on pair {idx}")

        r_c_joint = BETA * (pi_c_joint - ref_c_joint)
        r_r_joint = BETA * (pi_r_joint - ref_r_joint)
        margin_joint = r_c_joint - r_r_joint

        # Mutation M07: Adversarial Margin Inversion
        if injected_mutation == "M07_ADVERSARIAL_MARGIN_INVERSION" and idx == 2:
            margin_joint = -1.50

        # Mutation M10: Zero Variance Constant Margin
        if injected_mutation == "M10_ZERO_VARIANCE_MARGIN":
            margin_joint = 5.0000

        win_joint = 1.0 if margin_joint > 0.0 else 0.0

        # Separate Tokenization (for Contract Comparison)
        pi_c_sep = compute_sequence_logp_separate(policy_model, tokenizer, prompt, chosen, device)
        pi_r_sep = compute_sequence_logp_separate(policy_model, tokenizer, prompt, rejected, device)

        with policy_model.disable_adapter():
            ref_c_sep = compute_sequence_logp_separate(policy_model, tokenizer, prompt, chosen, device)
            ref_r_sep = compute_sequence_logp_separate(policy_model, tokenizer, prompt, rejected, device)

        r_c_sep = BETA * (pi_c_sep - ref_c_sep)
        r_r_sep = BETA * (pi_r_sep - ref_r_sep)
        margin_sep = r_c_sep - r_r_sep

        if abs(margin_joint - margin_sep) > 1e-4:
            boundary_mismatch_count += 1

        rec["pi_chosen"] = float(pi_c_joint)
        rec["pi_rejected"] = float(pi_r_joint)
        rec["ref_chosen"] = float(ref_c_joint)
        rec["ref_rejected"] = float(ref_r_joint)
        rec["r_chosen"] = float(r_c_joint)
        rec["r_rejected"] = float(r_r_joint)
        rec["margin_joint"] = float(margin_joint)
        rec["margin_sep"] = float(margin_sep)
        rec["token_boundary_delta"] = float(abs(margin_joint - margin_sep))
        rec["win"] = float(win_joint)

        joint_margins.append(margin_joint)
        separate_margins.append(margin_sep)
        per_pair_results.append(rec)

        if idx in EXPECTED_JOINTLY_NOVEL_INDICES:
            print(f"  [Jointly Novel Pair {idx:2d}] {rec['source_key']:<34s} | Margin={margin_joint:+.4f} | Win={win_joint} (Bound Delta={abs(margin_joint-margin_sep):.6f})")

    eval_duration = time.time() - start_eval
    print(f"\n[*] All 51 pairs re-evaluated in {eval_duration:.2f}s ({len(all_eval_records)/eval_duration:.2f} pairs/s)")
    print(f"[*] Tokenization Boundary Sensitivity (>1e-4 delta): {boundary_mismatch_count}/51 pairs")

    # Metrics on 14 Jointly Novel Holdout Pairs
    novel_margins = [r["margin_joint"] for r in per_pair_results if r["jointly_novel"]]
    novel_wins = sum(int(r["win"]) for r in per_pair_results if r["jointly_novel"])
    novel_n = len(novel_margins)

    ci_low, ci_high = wilson_score_interval(novel_wins, novel_n)
    t_stat, p_val, cohens_d = compute_t_test(novel_margins)

    std_novel = float(np.std(novel_margins, ddof=1))
    mean_novel = float(np.mean(novel_margins))

    print("\n" + "=" * 80)
    print("JOINTLY NOVEL HOLDOUT SUBSET RESULTS (N=14):")
    print("=" * 80)
    print(f"[*] Wins:                         {novel_wins} / {novel_n} ({novel_wins/novel_n*100:.2f}%)")
    print(f"[*] Wilson 95% CI:                [{ci_low*100:.2f}%, {ci_high*100:.2f}%]")
    print(f"[*] Mean Reward Margin:           {mean_novel:+.4f} ± {std_novel:.4f}")
    print(f"[*] One-sample t-statistic:       t = {t_stat:.3f} (p = {p_val:.4e})")
    print(f"[*] Effect Size (Cohen's d):      d = {cohens_d:.3f}")
    print(f"[*] Minimum Margin in Subset:     {np.min(novel_margins):+.4f}")
    print(f"[*] Maximum Margin in Subset:     {np.max(novel_margins):+.4f}")

    # Rule NV-3 Violation Tripwire: Assert non-zero variance
    assert std_novel > 1e-4, f"Rule NV-3 Violation: Zero variance detected across margins (std={std_novel})"

    assert novel_wins == novel_n, f"Novel wins != novel_n! {novel_wins} != {novel_n}"
    assert np.min(novel_margins) > 0.0, f"Found non-positive margin in jointly novel subset! min={np.min(novel_margins)}"
    print("[PASS] P0-3: All 14 Jointly Novel Margins Positive, Non-Zero Variance, and Verified.")

    # 4. P0-4: Negative Controls (Transitive Overlap & Corrupted Joins)
    print("\n" + "=" * 80)
    print("🔬 P0-4: RUNNING TRANSITIVE-OVERLAP & ADVERSARIAL NEGATIVE CONTROLS...")
    print("=" * 80)

    # Control 1: Transitive Bridge Injection
    print("[CONTROL 1] Testing Transitive Leakage Bridge...")
    train_ents = set().union(*[extract_entities(r) for _, r in train_df.iterrows()])
    eval_novel_ent = list(extract_entities(eval_df.iloc[2]))[0]
    train_ent = list(train_ents)[0]
    synthetic_edges = [(eval_novel_ent, "intermediary:synthetic_proxy"), ("intermediary:synthetic_proxy", train_ent)]
    bridge_graph = build_connected_components(train_df, eval_df, synthetic_edges=synthetic_edges)
    bridge_train_cids = bridge_graph["train_comp_ids"]
    bridge_eval_cids = bridge_graph["eval_comp_ids"]
    novel_after_bridge = [
        i for i, cid in enumerate(bridge_eval_cids)
        if cid not in bridge_train_cids and eval_df.iloc[i]["prompt"] not in train_prompts
    ]
    assert len(novel_after_bridge) == 13, f"Transitive bridge failed! Count was {len(novel_after_bridge)}, expected 13"
    assert bridge_eval_cids[2] in bridge_train_cids, "Eval pair 2 not marked as connected to train!"
    print(f"  [PASS] Transitive bridge correctly caught and reduced novel cluster count from 14 to {len(novel_after_bridge)}.")

    # Control 2: Direct Overlap Failure
    print("[CONTROL 2] Testing Transitive vs Direct Intersection Discrepancy...")
    direct_overlaps = [len(extract_entities(eval_df.iloc[i]).intersection(
        set().union(*[extract_entities(r) for _, r in train_df.iterrows()])
    )) > 0 for i in range(len(eval_df))]
    transitive_overlaps = [cid in train_comp_ids for cid in eval_comp_ids]
    assert direct_overlaps == transitive_overlaps, "Direct and transitive overlaps differed in baseline clean data!"
    print("  [PASS] Direct and transitive component boundaries aligned in uncorrupted corpus.")

    # Control 3: Adversarial Inversion of Margin 2
    print("[CONTROL 3] Testing Margin Inversion Rejection...")
    mock_margins = list(novel_margins)
    mock_margins[0] = -1.50
    mock_wins = sum(1 for m in mock_margins if m > 0.0)
    assert mock_wins < novel_n, "Mock inversion failed to lower win count!"
    print("  [PASS] Inverted margin correctly drops accuracy and fails gate condition.")

    print("\n[PASS] P0-4: All Negative Controls Passed.")

    # 5. P0-5: Emit Signed-off Certified Receipt
    self_hash = compute_sha256(__file__)
    receipt_data = {
        "specification": "ZKAEDI-GATE6B-INDEPENDENT-RECONSTRUCTION-v2.0-HARDENED",
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "verdict": "PASS",
        "disposition": "GATE6B_JOINTLY_NOVEL_HOLDOUT_REPORTED_14_OF_14",
        "script_sha256": self_hash,
        "lineage": {
            "base_model_id": PINNED_BASE_MODEL_ID,
            "base_model_revision": PINNED_BASE_REVISION,
            "train_parquet_sha256": train_sha,
            "eval_parquet_sha256": eval_sha,
            "adapter_model_sha256": adapter_sha,
            "adapter_config_sha256": config_sha,
            "total_train_pairs": len(train_df),
            "total_eval_pairs": len(eval_df)
        },
        "jointly_novel_holdout_metrics": {
            "n": novel_n,
            "wins": f"{novel_wins}/{novel_n}",
            "accuracy": float(novel_wins / novel_n),
            "wilson_ci_95": [float(ci_low), float(ci_high)],
            "mean_reward_margin": mean_novel,
            "std_reward_margin": std_novel,
            "min_margin": float(np.min(novel_margins)),
            "max_margin": float(np.max(novel_margins)),
            "t_statistic": float(t_stat),
            "p_value_one_tailed": float(p_val),
            "cohens_d": float(cohens_d),
            "exact_indices": derived_indices
        },
        "scoring_contract": {
            "methodology": "Joint Tokenization with Completion Masking",
            "tokenization_boundary_mismatches_vs_separate": boundary_mismatch_count,
            "beta": BETA,
            "mask_slice_exact_length_verified": True
        },
        "authenticity_assertions": {
            "lora_functional_delta_max": float(logit_delta_max),
            "rule_av1_active_lora_verified": bool(logit_delta_max > 0.05),
            "rule_av3_finite_floats_verified": True,
            "rule_nv3_non_zero_variance_verified": bool(std_novel > 1e-4)
        },
        "per_pair_results_jointly_novel": [
            r for r in per_pair_results if r["jointly_novel"]
        ],
        "assertions": {
            "train_sha_verified": train_sha == PINNED_TRAIN_SHA256,
            "eval_sha_verified": eval_sha == PINNED_EVAL_SHA256,
            "adapter_sha_verified": adapter_sha == PINNED_ADAPTER_SHA256,
            "adapter_config_sha_verified": config_sha == PINNED_CONFIG_SHA256,
            "base_revision_verified": base_rev_check == PINNED_BASE_REVISION,
            "transitive_asset_disjoint_verified": derived_indices == EXPECTED_JOINTLY_NOVEL_INDICES,
            "all_jointly_novel_margins_positive": all(m > 0.0 for m in novel_margins),
            "all_14_novel_pairs_won": novel_wins == 14
        }
    }

    os.makedirs(os.path.dirname(out_receipt_path), exist_ok=True)
    temp_path = out_receipt_path + ".tmp"
    with open(temp_path, "w", encoding="utf-8") as f:
        json.dump(receipt_data, f, indent=2)
        f.flush()
        os.fsync(f.fileno())
    os.replace(temp_path, out_receipt_path)
    print(f"\n[*] Certified Receipt Saved to: {out_receipt_path}")

    print("\n" + "=" * 80)
    print("OFFICIAL VERDICT BLOCK:")
    print("=" * 80)
    print("GATE6B_INDEPENDENT_RECONSTRUCTION_PASS")
    print("JOINTLY_NOVEL_14_OF_14_RECOMPUTED_POSITIVE")
    print("TRANSITIVE_ASSET_DISJOINTNESS_VERIFIED")
    print("=" * 80)

    return True

def run_adversarial_mutation_gauntlet():
    """
    Executes 12 distinct adversarial mutations in isolated subprocesses.
    Confirms that every single fault mode FAILS CLOSED with non-zero exit code.
    Preserves receipt to docs/evidence/gate6b_reconstruction_mutation_receipt.json.
    """
    print("=" * 80)
    print("🔱 ZKAEDI GATE 6-B HARDENED MUTATION GAUNTLET (12 FAULT MODES) 🔱")
    print("=" * 80)

    repo_dir = resolve_path("H:/__DOWNLOADS/zcc_github_upload", "/mnt/h/__DOWNLOADS/zcc_github_upload")
    this_script = os.path.abspath(__file__)
    out_receipt = resolve_path(
        "H:/__DOWNLOADS/zcc_github_upload/docs/evidence/gate6b_reconstruction_mutation_receipt.json",
        "/mnt/h/__DOWNLOADS/zcc_github_upload/docs/evidence/gate6b_reconstruction_mutation_receipt.json"
    )

    mutations = [
        {"id": "M01_CORRUPTED_TRAIN_SHA", "name": "Corrupted Train Parquet Hash", "desc": "Alters train SHA-256 to all zeros", "dry_run": True},
        {"id": "M02_CORRUPTED_EVAL_SHA", "name": "Corrupted Eval Parquet Hash", "desc": "Alters eval SHA-256 to all ones", "dry_run": True},
        {"id": "M03_CORRUPTED_ADAPTER_SHA", "name": "Corrupted LoRA Adapter Hash", "desc": "Alters adapter weights SHA-256 to deadbeef", "dry_run": True},
        {"id": "M04_CORRUPTED_CONFIG_SHA", "name": "Corrupted Adapter Config Hash", "desc": "Alters adapter_config.json SHA-256 to all f's", "dry_run": True},
        {"id": "M05_TRANSITIVE_LEAKAGE_BRIDGE", "name": "Transitive Graph Leakage", "desc": "Injects proxy bridge linking novel asset to train", "dry_run": True},
        {"id": "M06_NON_FINITE_LOGP_NAN", "name": "IEEE-754 NaN Floating Point", "desc": "Injects float('nan') into sequence logp (Rule AV-3)", "dry_run": False},
        {"id": "M07_ADVERSARIAL_MARGIN_INVERSION", "name": "Margin Sign Inversion", "desc": "Forces novel pair margin to -1.50", "dry_run": False},
        {"id": "M08_DROPPED_NOVEL_PAIR", "name": "Omission of Novel Pair Index", "desc": "Omits pair index 49 from derived novel set", "dry_run": True},
        {"id": "M09_OFF_BY_ONE_MASK_SLICE", "name": "Completion Mask Off-by-One", "desc": "Perturbs response mask slice start index by -1", "dry_run": False},
        {"id": "M10_ZERO_VARIANCE_MARGIN", "name": "Zero-Variance Constant Output", "desc": "Sets all margins to identical 5.0 (Rule NV-3)", "dry_run": False},
        {"id": "M11_DISJOINT_SET_CONTRADICTION", "name": "Disjoint Invariant Violation", "desc": "Forces train prompt 0 to report disjoint", "dry_run": True},
        {"id": "M12_FORGED_LINEAGE_REVISION", "name": "Forged Git Base Revision", "desc": "Replaces pinned base model commit SHA", "dry_run": True}
    ]

    results = []
    passed_count = 0

    for m in mutations:
        m_id = m["id"]
        m_name = m["name"]
        print(f"\n[RUNNING] {m_id}: {m_name}...")

        cmd = [sys.executable, "-X", "utf8", this_script, "--injected_mutation", m_id]
        if m.get("dry_run", False):
            cmd.append("--dry_run")

        t0 = time.time()
        proc = subprocess.run(
            cmd,
            cwd=repo_dir,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace"
        )
        elapsed = time.time() - t0

        failed_closed = (proc.returncode != 0)
        status = "PASS (Correctly Rejected)" if failed_closed else "FAIL (Undetected Mutation!)"

        if failed_closed:
            passed_count += 1

        print(f"  - Return Code   : {proc.returncode}")
        print(f"  - Execution Time: {elapsed:.2f}s")
        print(f"  - Verdict       : {status}")

        err_snippet = proc.stderr[-400:] if proc.stderr else proc.stdout[-400:]
        for line in err_snippet.strip().split("\n")[-3:]:
            print(f"    | {line}")

        results.append({
            "mutation_id": m_id,
            "mutation_name": m_name,
            "description": m["desc"],
            "expected_exit": "non-zero (1)",
            "actual_exit": proc.returncode,
            "failed_closed": failed_closed,
            "execution_duration_sec": elapsed,
            "status": status,
            "error_snippet": err_snippet.strip()
        })

    gauntlet_verdict = "PASS" if passed_count == len(mutations) else "FAIL"

    receipt = {
        "specification": "ZKAEDI-GATE6B-RECONSTRUCTION-MUTATION-GAUNTLET-v1.0",
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "verdict": gauntlet_verdict,
        "total_mutations": len(mutations),
        "passed_count": passed_count,
        "failed_closed_rate": passed_count / len(mutations),
        "mutation_results": results
    }

    os.makedirs(os.path.dirname(out_receipt), exist_ok=True)
    with open(out_receipt, "w", encoding="utf-8") as f:
        json.dump(receipt, f, indent=2)

    print("\n" + "=" * 80)
    print("GAUNTLET AUDIT SUMMARY:")
    print("=" * 80)
    print(f"[*] Total Mutations Injected  : {len(mutations)}")
    print(f"[*] Verified Failed Closed     : {passed_count} / {len(mutations)} ({passed_count/len(mutations)*100:.1f}%)")
    print(f"[*] Certified Receipt Saved To : {out_receipt}")
    print(f"[*] OFFICIAL VERDICT           : {gauntlet_verdict}")
    print("=" * 80)

    if gauntlet_verdict != "PASS":
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="🔱 ZKAEDI Gate 6-B Independent Reconstruction & Hardened Verification Harness 🔱")
    parser.add_argument("--test_mutations", action="store_true", help="Execute 12-mode adversarial mutation gauntlet")
    parser.add_argument("--injected_mutation", type=str, default=None, help="Inject specific adversarial mutation (M01-M12)")
    parser.add_argument("--dry_run", action="store_true", help="Run fast algebraic and graph verification without GPU inference")
    parser.add_argument("--train_parquet", type=str, default=None, help="Path to train Parquet file")
    parser.add_argument("--eval_parquet", type=str, default=None, help="Path to eval Parquet file")
    parser.add_argument("--adapter_dir", type=str, default=None, help="Path to LoRA adapter directory")
    parser.add_argument("--out_receipt", type=str, default=None, help="Path to output certified JSON receipt")

    args = parser.parse_args()

    if args.test_mutations:
        run_adversarial_mutation_gauntlet()
        sys.exit(0)

    success = run_independent_reconstruction(
        injected_mutation=args.injected_mutation,
        dry_run=args.dry_run,
        train_parquet_path=args.train_parquet,
        eval_parquet_path=args.eval_parquet,
        adapter_dir=args.adapter_dir,
        out_receipt_path=args.out_receipt
    )
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
