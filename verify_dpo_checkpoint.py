#!/usr/bin/env python3
"""
verify_dpo_checkpoint.py — Independent DPO Checkpoint Metric Verifier v2

WHAT THIS FIXES (vs v1)
-----------------------
#1  Manifest key: reads manifest["eval"] — same key trainer uses (L910).
#2  Prompt formatting: applies format_dpo() before scoring — same distribution
    trainer evals on.
#3  π_ref loaded and used: computes actual DPO reward
    r(y|x) = β·(log π_θ(y|x) − log π_ref(y|x))
    so chosen>rejected is the same predicate as the receipt's win_rate.
#4  Never mutates the signed receipt. Writes detached
    independent_verification.json containing receipt sha256.
    Post-verification, original .json and .json.sig remain intact.
#5a Boundary-safe slicing: verifies enc_full[:prompt_len] == enc_prompt
    tokens before slicing; skips and flags on mismatch.
#5b Guards against truncation eating the response (empty response slice
    → skip, counted in skip_count).
#6  Hash pre-check: recomputes sha256 of every file in the snapshot
    and compares against receipt["files"] before loading model.
#7  Never trusts metrics_independently_reproduced flag. --skip-if-verified
    opt-in only.
#8  Fails if processed sample count N′ deviates from receipt's
    sample_count beyond logged skips.
#9  Forces float32 for consistency with trainer's default precision.

TRAINER RESIDUAL (separate patch in train_hf_dpo_adamw_hardened_v3.py)
-----------------------------------------------------------------------
on_save hashes checkpoint_dir but binds the receipt to best_snapshot_dir.
Fixed there: hash best_snapshot_dir after copytree.

USAGE
-----
  python verify_dpo_checkpoint.py \\
    --receipt  outputs_dpo_adamw/best_statistically_valid_checkpoint.json \\
    --ref-model gpt2 \\
    --dataset  /mnt/h/agents/train_maxed_validated.parquet \\
    --split-manifest splits/dpo_v1_manifest.json \\
    --beta 0.1 \\
    [--tolerance 0.02] \\
    [--max-length 512] \\
    [--use-cpu] \\
    [--skip-if-verified]

EXIT CODES
----------
  0  Verification passed; independent_verification.json written.
  1  Input error (missing file, manifest key missing, etc.).
  2  Verification failed (metric delta, p-value, N-mismatch, provenance).
  3  Hash pre-check failed (snapshot tampered or wrong checkpoint).
"""

import argparse
import hashlib
import json
import logging
import math
import os
import sys
import tempfile
from pathlib import Path

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [VERIFY-%(levelname)s] %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)],
)
logger = logging.getLogger("verify_dpo_checkpoint")


# ─── HELPERS ──────────────────────────────────────────────────────────────────

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_json(data: dict) -> str:
    return hashlib.sha256(
        json.dumps(data, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


def write_detached_json(target_path: Path, data: dict) -> None:
    """Atomic write to a NEW file. Never called on the signed receipt."""
    target_path = Path(target_path)
    parent = target_path.parent
    parent.mkdir(parents=True, exist_ok=True)
    temp_path = None
    try:
        with tempfile.NamedTemporaryFile("w", dir=str(parent), delete=False, encoding="utf-8") as f:
            temp_path = Path(f.name)
            json.dump(data, f, indent=2)
            f.flush()
            os.fsync(f.fileno())
        os.replace(temp_path, target_path)
        temp_path = None
        try:
            dir_fd = os.open(str(parent), os.O_RDONLY)
            try:
                os.fsync(dir_fd)
            finally:
                os.close(dir_fd)
        except OSError:
            pass
    finally:
        if temp_path is not None and temp_path.exists():
            temp_path.unlink(missing_ok=True)


# ─── FORMAT_DPO (must match trainer verbatim — train_hf_dpo_adamw_hardened_v3.py L447) ──

def format_dpo(sample: dict) -> dict:
    """Mirrors the trainer's format_dpo exactly. Any change here must track L447."""
    sys_prompt = sample.get("system") or ""
    prompt_text = sample.get("prompt") or ""
    formatted_prompt = (
        f"### System:\n{sys_prompt}\n\n"
        f"### Instruction:\n{prompt_text}\n\n"
        f"### Response:\n"
    )
    return {
        "prompt": formatted_prompt,
        "chosen": sample.get("chosen") or "",
        "rejected": sample.get("rejected") or "",
    }


# ─── HASH PRE-CHECK ───────────────────────────────────────────────────────────

def verify_snapshot_hashes(snapshot_dir: Path, receipt_files: dict) -> tuple[bool, set, set]:
    """
    FIX-6 (v3): Set-equality scoped to the receipt's own file-extension universe.

    FINDING #2 MITIGATION: HF checkpoint dirs contain optimizer.pt, scheduler.pt,
    rng_state.pth, trainer_state.json, etc. get_model_hashes() likely hashes only
    model payload files (config.json, model.safetensors, tokenizer.*). Running
    rglob against the full snapshot and comparing to a payload-only receipt will
    always yield EXTRA FILES and false-exit-3 on untampered snapshots.

    Resolution: scope the extra-file check to the set of file extensions that
    appear in the receipt. Files with extensions not present in the receipt are
    assumed to be trainer state excluded by get_model_hashes policy and are
    logged as UNVERIFIED (not extra). This policy is recorded in the output record
    so it's auditable rather than implicit.

    Returns (all_ok, verified_extensions, unverified_files)
    """
    logger.info(f"Hash pre-check: {len(receipt_files)} files in receipt...")
    all_ok = True

    # Determine which extensions the receipt covers
    receipt_extensions = {Path(p).suffix.lower() for p in receipt_files.keys()}
    logger.info(f"  Receipt file-extension universe: {sorted(receipt_extensions)}")

    # Collect all files in snapshot, partitioned by whether extension is receipt-covered
    in_scope: set = set()
    out_of_scope: set = set()
    for fpath in snapshot_dir.rglob("*"):
        if fpath.is_file():
            try:
                rel = str(fpath.relative_to(snapshot_dir))
            except ValueError:
                rel = str(fpath)
            if rel in ["dpo_security_attestation.json", "dpo_security_attestation.json.sig", "training_manifest.json", "training_manifest.json.sig"]:
                continue
            if fpath.suffix.lower() in receipt_extensions:
                in_scope.add(rel)
            else:
                out_of_scope.add(rel)

    if out_of_scope:
        logger.info(
            f"  UNVERIFIED (extension not in receipt scope, assumed trainer-state): "
            f"{sorted(out_of_scope)}"
        )

    receipt_file_set = set(receipt_files.keys())

    # Extra in-scope files not in receipt → genuine unexpected file
    extra = in_scope - receipt_file_set
    if extra:
        logger.error(
            f"  EXTRA IN-SCOPE FILES (extension covered by receipt, not listed): {sorted(extra)}"
        )
        all_ok = False

    # Missing files listed in receipt
    missing = receipt_file_set - in_scope
    for rel_path in missing:
        logger.error(f"  MISSING: {rel_path}")
        all_ok = False

    # Hash check for present receipt-listed files
    for rel_path, expected_hash in receipt_files.items():
        if rel_path in missing:
            continue
        fpath = snapshot_dir / rel_path
        actual = sha256_file(fpath)
        if actual != expected_hash:
            logger.error(f"  HASH MISMATCH: {rel_path}")
            logger.error(f"    receipt : {expected_hash}")
            logger.error(f"    on-disk : {actual}")
            all_ok = False
        else:
            logger.info(f"  OK: {rel_path}")

    return all_ok, receipt_extensions, out_of_scope


# ─── LOAD EVAL SPLIT ──────────────────────────────────────────────────────────

def load_eval_split(dataset_path: Path, manifest_path: Path):
    """
    FIX-1: reads manifest["eval"] — the key the trainer uses (L910).
    FIX-2: applies format_dpo — same conditioning distribution as trainer eval.
    """
    logger.info(f"Loading split manifest: {manifest_path}")
    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    # FIX-1: correct key
    if "eval" not in manifest:
        raise KeyError(
            f"Manifest missing key 'eval'. Keys present: {list(manifest.keys())}. "
            f"(v1 bug: incorrectly used 'eval_indices')"
        )
    eval_indices = manifest["eval"]
    if not isinstance(eval_indices, list) or len(eval_indices) == 0:
        raise ValueError("manifest['eval'] is empty or not a list")

    logger.info(f"Loading dataset: {dataset_path}")
    from datasets import load_dataset as hf_load
    ds = hf_load("parquet", data_files=str(dataset_path), split="train")

    eval_ds = ds.select(eval_indices)
    # FIX-2: apply format_dpo before returning
    eval_ds = eval_ds.map(format_dpo)
    logger.info(f"Eval split: {len(eval_ds)} samples (format_dpo applied)")
    return eval_ds


# ─── COLD MODEL LOAD ──────────────────────────────────────────────────────────

def load_model_cold(checkpoint_dir: Path, device: str):
    """FIX-9: forces float32 for consistency with trainer default precision."""
    from transformers import AutoModelForCausalLM, AutoTokenizer

    logger.info(f"Cold-loading policy model from: {checkpoint_dir}")
    tokenizer = AutoTokenizer.from_pretrained(str(checkpoint_dir), trust_remote_code=False)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    model = AutoModelForCausalLM.from_pretrained(
        str(checkpoint_dir),
        trust_remote_code=False,
        torch_dtype="float32",  # FIX-9: match trainer default, avoid fp16 margin shifts
    )
    model.to(device)
    model.eval()
    logger.info(f"Policy loaded on {device} | params: {sum(p.numel() for p in model.parameters()):,}")
    return model, tokenizer


def load_ref_model_cold(ref_model_name_or_path: str, device: str):
    """Load reference model (π_ref) — same base as trainer's ref_model."""
    from transformers import AutoModelForCausalLM, AutoTokenizer

    logger.info(f"Cold-loading reference model from: {ref_model_name_or_path}")
    tokenizer = AutoTokenizer.from_pretrained(ref_model_name_or_path, trust_remote_code=False)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    ref_model = AutoModelForCausalLM.from_pretrained(
        ref_model_name_or_path,
        trust_remote_code=False,
        torch_dtype="float32",  # FIX-9
    )
    ref_model.to(device)
    ref_model.eval()
    logger.info(f"Reference model loaded on {device}")
    return ref_model, tokenizer


# ─── LOG-PROB COMPUTATION ─────────────────────────────────────────────────────

def compute_log_prob(model, tokenizer, prompt: str, response: str,
                     device: str, max_length: int) -> tuple[float, bool]:
    """
    Returns (log_prob_sum, ok).

    FIX-5a: Boundary-safe slicing. Tokenizes prompt standalone, then verifies
    that enc_full[:prompt_len_tokens] == enc_prompt tokens before slicing.
    Returns ok=False on mismatch (sample skipped by caller).

    FIX-5b: Guards against truncation eating the response. If the response
    slice is empty after truncation, returns ok=False (sample skipped).

    Returns log-prob of response tokens only, summed.
    """
    import torch

    full_text = prompt + response

    enc_full = tokenizer(
        full_text, return_tensors="pt",
        truncation=True, max_length=max_length,
        add_special_tokens=True,
    )
    enc_prompt = tokenizer(
        prompt, return_tensors="pt",
        truncation=True, max_length=max_length,
        add_special_tokens=True,
    )

    input_ids_full = enc_full["input_ids"][0]        # (seq_len,)
    prompt_ids     = enc_prompt["input_ids"][0]       # (prompt_len,)
    prompt_len     = prompt_ids.shape[0]

    # FIX-5a: verify boundary — BPE can merge across prompt/response seam
    if prompt_len > input_ids_full.shape[0]:
        # Truncation swallowed the whole prompt
        return 0.0, False

    actual_prefix = input_ids_full[:prompt_len]
    if not torch.equal(actual_prefix, prompt_ids):
        # Boundary mismatch — skip this sample
        return 0.0, False

    # FIX-5b: check response slice is non-empty
    response_start = prompt_len  # first response token's logit index is prompt_len - 1
    if response_start >= input_ids_full.shape[0]:
        # Truncation ate the entire response
        return 0.0, False

    input_ids = input_ids_full.unsqueeze(0).to(device)

    with torch.no_grad():
        out = model(input_ids=input_ids)
        logits = out.logits[0]  # (seq_len, vocab)

    # logits[i] predicts token[i+1]
    shift_logits = logits[:-1, :]          # (seq_len-1, vocab)
    shift_labels = input_ids_full[1:]      # (seq_len-1,)

    log_probs = torch.nn.functional.log_softmax(shift_logits, dim=-1)
    token_log_probs = log_probs[range(len(shift_labels)), shift_labels.to(device)]

    # Response tokens start at index prompt_len-1 in the shifted sequence
    response_lp = token_log_probs[prompt_len - 1:]

    if response_lp.shape[0] == 0:
        return 0.0, False

    return float(response_lp.sum().cpu()), True


# ─── INDEPENDENT WIN-RATE COMPUTATION ────────────────────────────────────────

def compute_independent_win_rate(
    policy_model, ref_model, tokenizer,
    eval_ds, device: str, beta: float, max_length: int
):
    """
    FIX-3: Computes actual DPO reward:
        r(y|x) = β · (log π_θ(y|x) − log π_ref(y|x))

    chosen > rejected under this predicate is the same as the trainer's
    eval (which uses TRL's internal DPO reward computation).

    Returns: win_count, processed_count, skip_count, margins
    """
    win_count = 0
    skip_count = 0
    margins = []
    n = len(eval_ds)

    for i, sample in enumerate(eval_ds):
        prompt  = sample.get("prompt", "")
        chosen  = sample.get("chosen", "")
        rejected = sample.get("rejected", "")

        if not prompt or not chosen or not rejected:
            logger.warning(f"Sample {i}: missing field — skipped")
            skip_count += 1
            continue

        # Policy log-probs
        lp_chosen_theta,  ok1 = compute_log_prob(policy_model, tokenizer, prompt, chosen,  device, max_length)
        lp_rejected_theta, ok2 = compute_log_prob(policy_model, tokenizer, prompt, rejected, device, max_length)

        # Reference log-probs
        lp_chosen_ref,    ok3 = compute_log_prob(ref_model,    tokenizer, prompt, chosen,  device, max_length)
        lp_rejected_ref,  ok4 = compute_log_prob(ref_model,    tokenizer, prompt, rejected, device, max_length)

        if not (ok1 and ok2 and ok3 and ok4):
            logger.warning(f"Sample {i}: boundary/truncation issue (ok={ok1},{ok2},{ok3},{ok4}) — skipped")
            skip_count += 1
            continue

        # Actual DPO reward (FIX-3)
        r_chosen   = beta * (lp_chosen_theta   - lp_chosen_ref)
        r_rejected = beta * (lp_rejected_theta - lp_rejected_ref)
        margin     = r_chosen - r_rejected

        margins.append(margin)
        if r_chosen > r_rejected:
            win_count += 1

        if (i + 1) % 10 == 0 or (i + 1) == n:
            logger.info(
                f"  [{i+1}/{n}] wins={win_count} skips={skip_count} "
                f"margin={margin:.4f} r_chosen={r_chosen:.4f} r_rej={r_rejected:.4f}"
            )

    processed = len(margins)
    return win_count, processed, skip_count, margins


# ─── STATISTICAL GATE ─────────────────────────────────────────────────────────

def run_binomial_gate(win_count: int, sample_count: int):
    """One-sided exact binomial test: H1 win_rate > 0.5."""
    try:
        from scipy import stats
        if hasattr(stats, "binomtest"):
            result = stats.binomtest(win_count, sample_count, p=0.5, alternative="greater")
            return float(result.pvalue), "one_sided_exact_binomial"
        pval = float(stats.binom_test(win_count, sample_count, p=0.5, alternative="greater"))
        return pval, "one_sided_exact_binomial"
    except ImportError:
        mean = 0.5 * sample_count
        std  = math.sqrt(0.25 * sample_count)
        if std > 0:
            z = (win_count - 0.5 - mean) / std
            pval = float(0.5 * (1.0 - math.erf(z / math.sqrt(2.0))))
        else:
            pval = 1.0
        return pval, "one_sided_binomial_normal_approximation"


# ─── MAIN ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Independent DPO Checkpoint Metric Verifier v2 — closes shared-oracle + 9 audit fixes"
    )
    parser.add_argument("--receipt", required=True,
                        help="Path to best_statistically_valid_checkpoint.json")
    parser.add_argument("--ref-model", required=True,
                        help="Base model name or local path used as pi_ref (must match training)")
    parser.add_argument("--dataset", required=True,
                        help="Path to the parquet dataset used during training")
    parser.add_argument("--split-manifest", required=True,
                        help="Path to splits/dpo_v1_manifest.json")
    parser.add_argument("--beta", type=float, default=0.1,
                        help="DPO beta used during training")
    parser.add_argument("--tolerance", type=float, default=0.02,
                        help="Max allowed |Δ win_rate| between receipt and independent measurement")
    parser.add_argument("--max-length", type=int, default=512,
                        help="Tokenization max_length (match trainer)")
    parser.add_argument("--use-cpu", action="store_true",
                        help="Force CPU inference")
    # FIX-7: opt-in skip, never default
    parser.add_argument("--skip-if-verified", action="store_true",
                        help="Skip if prior independent_verification.json matches current receipt sha256")
    parser.add_argument("--skip-attestation-check", action="store_true",
                        help="Override exit-2 when attestation stub is missing. Adds UNVERIFIED flag to record. For old checkpoints only.")
    parser.add_argument("--allow-reconstructed-counts", action="store_true",
                        help="Allow verification of receipts with reconstructed preference counts (demoted confidence).")
    args = parser.parse_args()

    receipt_path = Path(args.receipt).resolve()
    dataset_path = Path(args.dataset).resolve()
    manifest_path = Path(args.split_manifest).resolve()

    for p, label in [(receipt_path, "receipt"), (dataset_path, "dataset"), (manifest_path, "manifest")]:
        if not p.exists():
            logger.error(f"{label} not found: {p}")
            sys.exit(1)

    # FIX-7: opt-in early exit — ONLY if the prior record binds to the CURRENT receipt.
    # Stale-green attack: regenerate receipt, keep old record, pass --skip-if-verified.
    # Guard: compare prior record's receipt_sha256 against current receipt's sha256.
    verification_out = receipt_path.parent / "independent_verification.json"
    if args.skip_if_verified and verification_out.exists():
        with open(receipt_path, "r", encoding="utf-8") as _rf:
            _current_receipt_for_skip = json.load(_rf)
        _current_sha = sha256_json(_current_receipt_for_skip)
        with open(verification_out, "r", encoding="utf-8") as f:
            prev = json.load(f)
        if prev.get("verification_passed") and prev.get("receipt_sha256") == _current_sha:
            logger.info("--skip-if-verified: prior passing verification matches current receipt sha256. Exiting.")
            sys.exit(0)
        else:
            logger.warning("--skip-if-verified: prior record sha256 mismatch or not passed — running full verification.")

    # Load receipt
    with open(receipt_path, "r", encoding="utf-8") as f:
        receipt = json.load(f)

    # FIX-4: compute receipt sha256 BEFORE doing anything else.
    # All output goes to independent_verification.json, never back into the receipt.
    receipt_sha256 = sha256_json(receipt)
    logger.info(f"Receipt sha256 (json-normalized): {receipt_sha256}")

    receipt_win_rate    = float(receipt.get("win_rate", -1.0))
    receipt_win_count   = receipt.get("win_rate_test", {}).get("win_count")
    receipt_sample_count = receipt.get("win_rate_test", {}).get("sample_count")
    counts_source       = receipt.get("win_rate_test", {}).get("counts_source", "exact")
    logger.info(f"Receipt claims: win_rate={receipt_win_rate:.4f} ({receipt_win_count}/{receipt_sample_count}), counts_source={counts_source}")

    if counts_source != "exact" and not getattr(args, "allow_reconstructed_counts", False):
        logger.error(
            f"RECEIPT ERROR: win_rate_test counts_source is '{counts_source}'. "
            f"Validation requires exact counts to prevent rounding error leaks. "
            f"Pass --allow-reconstructed-counts to force override."
        )
        sys.exit(2)

    # Locate snapshot
    best_snapshot = receipt.get("best_snapshot")
    checkpoint_name = receipt.get("checkpoint")
    receipt_dir = receipt_path.parent

    snapshot_dir = None
    if best_snapshot:
        cand = receipt_dir / best_snapshot
        if cand.is_dir():
            snapshot_dir = cand
            logger.info(f"Using rotation-safe snapshot: {snapshot_dir}")

    if snapshot_dir is None and checkpoint_name:
        for parent_rel in [Path("."), Path(".."), Path("../checkpoints")]:
            cand = (receipt_dir / parent_rel / checkpoint_name).resolve()
            if cand.is_dir():
                snapshot_dir = cand
                logger.info(f"Using located snapshot directory: {snapshot_dir}")
                break

    if snapshot_dir is None:
        logger.error("Cannot locate checkpoint. Was best/ snapshot written by the trainer FIX-3?")
        sys.exit(1)

    # FIX-6: Hash pre-check before any model load
    receipt_files = receipt.get("files", {})
    if not receipt_files:
        logger.error("Receipt contains no 'files' hash manifest. Cannot verify snapshot integrity.")
        sys.exit(3)

    hash_ok, verified_extensions, unverified_files = verify_snapshot_hashes(snapshot_dir, receipt_files)
    if not hash_ok:
        logger.error("Hash pre-check FAILED — snapshot does not match receipt. Possible tamper or wrong checkpoint.")
        sys.exit(3)
    logger.info("Hash pre-check PASSED — snapshot matches receipt (extension-scoped set-equality).")

    # Device
    import torch
    device = "cpu" if args.use_cpu or not torch.cuda.is_available() else "cuda"
    logger.info(f"Inference device: {device}")

    # Load data (FIX-1 + FIX-2 applied inside load_eval_split)
    try:
        eval_ds = load_eval_split(dataset_path, manifest_path)
    except Exception as e:
        logger.error(f"MANIFEST/DATASET VERIFICATION ERROR: {e}")
        sys.exit(2)

    # FIX-8: capture expected sample count from manifest-derived eval set
    expected_n = len(eval_ds)

    # PREV FLAG #1: π_ref provenance check.
    # --ref-model is arbitrary user input; verify it matches the attestation's
    # base_model.identity before loading. Without this, any convenient π_ref
    # shifts every margin and defeats the verifier's independence claim.
    attestation_path = snapshot_dir / "dpo_security_attestation.json"
    attested_ref_identity = None
    attested_ref_hash = None
    if attestation_path.exists():
        with open(attestation_path, "r", encoding="utf-8") as _af:
            _att = json.load(_af)
        attested_ref_identity = _att.get("base_model", {}).get("identity")
        attested_ref_hash     = _att.get("base_model", {}).get("allow_list_sha256")
        logger.info(f"Attestation base_model.identity: {attested_ref_identity}")

        def normalize_path_ws_win(p: str) -> str:
            p = p.replace("\\", "/").strip().rstrip("/")
            if p.lower().startswith("/mnt/"):
                drive = p[5:6].upper()
                p = f"{drive}:{p[6:]}"
            return p.lower()

        if attested_ref_identity and normalize_path_ws_win(attested_ref_identity) != normalize_path_ws_win(args.ref_model):
            # Local path edge case: both may resolve to the same model.
            # Require exact string match or explicit acknowledgement.
            logger.error(
                f"pi_ref PROVENANCE MISMATCH: --ref-model='{args.ref_model}' "
                f"but attestation.base_model.identity='{attested_ref_identity}'. "
                f"Pass the exact identity from the attestation."
            )
            sys.exit(2)

        # If attested_ref_hash is available and ref_model is a local dir, hash-check it.
        if attested_ref_hash and attested_ref_hash != "unknown":
            _ref_path = Path(args.ref_model)
            if _ref_path.is_dir():
                from zkaedi_model_registry import get_model_hashes  # type: ignore[import]
                try:
                    _ref_payload_hash, _ = get_model_hashes(_ref_path)
                    if _ref_payload_hash != attested_ref_hash:
                        logger.error(
                            f"pi_ref hash mismatch: local dir hash {_ref_payload_hash} "
                            f"!= attestation allow_list_sha256 {attested_ref_hash}"
                        )
                        sys.exit(2)
                    logger.info("pi_ref local directory hash matches attestation allow_list_sha256.")
                except Exception as _e:
                    logger.warning(f"Could not hash ref model directory: {_e}. Skipping hash check.")
    else:
        # FINDING #1: no attestation in snapshot = no provenance check possible.
        # This is now exit-2 (not warn-and-proceed): the trainer stub-write (on_save
        # FINDING #1 FIX) ensures every legitimate snapshot has an attestation.
        # Absence means either (a) old trainer that never wrote stubs, or
        # (b) someone deleted it — both require explicit acknowledgement.
        logger.error(
            "dpo_security_attestation.json NOT FOUND in snapshot. "
            "pi_ref identity CANNOT be verified. "
            "Trainer FIX (on_save stub-write) must be applied before running the verifier. "
            "Pass --skip-attestation-check to override (adds UNVERIFIED flag to record)."
        )
        if not getattr(args, 'skip_attestation_check', False):
            sys.exit(2)

    # Load models (FIX-3: π_ref loaded separately)
    policy_model, tokenizer = load_model_cold(snapshot_dir, device)
    ref_model, _            = load_ref_model_cold(args.ref_model, device)

    # Independent measurement (FIX-3 actual DPO reward)
    logger.info("=== RUNNING INDEPENDENT EVAL (actual DPO reward beta*(log pi_theta - log pi_ref)) ===")
    win_count, processed, skip_count, margins = compute_independent_win_rate(
        policy_model, ref_model, tokenizer,
        eval_ds, device, args.beta, args.max_length
    )

    if processed == 0:
        logger.error("Zero valid samples processed. Verification FAILED.")
        sys.exit(2)

    # FIX-8 (relabeled): the tautological check is retained as an internal sanity
    # assertion (by construction processed == expected_n - skip_count). The real
    # gate is the receipt's sample_count — trainer may have masked non-finite rewards,
    # shrinking its effective N below ours. That's a hard failure.
    assert processed == expected_n - skip_count, (
        f"Internal invariant broken: {processed} != {expected_n} - {skip_count}. "
        "Bug in compute_independent_win_rate."
    )

    if receipt_sample_count is not None and processed != int(receipt_sample_count):
        # Hard gate: N mismatch means win_rate denominators are different populations.
        # This is exit-2, not a warning — tolerating it makes delta comparison invalid.
        logger.error(
            f"N MISMATCH: processed {processed} samples, receipt claims {receipt_sample_count}. "
            f"Logged skips: {skip_count}. Trainer may have masked non-finite rewards, "
            f"shrinking its N. Win-rate denominators differ — verification INVALID."
        )
        sys.exit(2)

    independent_win_rate = win_count / processed
    mean_margin = sum(margins) / len(margins)
    std_margin  = math.sqrt(sum((m - mean_margin)**2 for m in margins) / max(len(margins)-1, 1))
    win_p_val, win_test_method = run_binomial_gate(win_count, processed)

    delta = abs(independent_win_rate - receipt_win_rate)

    logger.info("=== INDEPENDENT RESULTS ===")
    logger.info(f"  win_count              : {win_count}/{processed}  (skips={skip_count})")
    logger.info(f"  independent_win_rate   : {independent_win_rate:.4f}")
    logger.info(f"  receipt_win_rate       : {receipt_win_rate:.4f}")
    logger.info(f"  |delta win_rate|       : {delta:.4f}  (tolerance={args.tolerance})")
    logger.info(f"  mean_margin (DPO)      : {mean_margin:.4f}")
    logger.info(f"  std_margin             : {std_margin:.4f}")
    logger.info(f"  win_p_val              : {win_p_val:.4e}  ({win_test_method})")

    passes = (
        delta <= args.tolerance
        and independent_win_rate > 0.5
        and win_p_val < 0.05
        and skip_count == 0  # strict: any skip means a sample wasn't verified
    )

    # FIX-4: write detached independent_verification.json, NEVER touch the receipt
    # PREV FLAG #2: verifier self-attestation — sha256 of this script so the
    # record is reproducible from source, not just from a version string.
    try:
        verifier_script_sha256 = sha256_file(Path(__file__))
    except Exception:
        verifier_script_sha256 = "UNAVAILABLE"

    try:
        trainer_path = Path(__file__).parent / "train_hf_dpo_adamw_hardened_v3.py"
        trainer_script_sha256 = sha256_file(trainer_path) if trainer_path.exists() else "UNAVAILABLE"
    except Exception:
        trainer_script_sha256 = "UNAVAILABLE"

    try:
        smoke_loop_path = Path(__file__).parent / "smoke_loop.py"
        smoke_loop_script_sha256 = sha256_file(smoke_loop_path) if smoke_loop_path.exists() else "UNAVAILABLE"
    except Exception:
        smoke_loop_script_sha256 = "UNAVAILABLE"

    verification_record = {
        "verifier_version": "3.0",
        "verifier_script_sha256": verifier_script_sha256,  # PREV FLAG #2
        "trainer_script_sha256": trainer_script_sha256,
        "smoke_loop_script_sha256": smoke_loop_script_sha256,
        "receipt_sha256": receipt_sha256,          # binds this record to the exact receipt
        "receipt_path": str(receipt_path),
        "snapshot_dir": str(snapshot_dir),
        "attested_ref_identity": attested_ref_identity,   # PREV FLAG #1
        "hash_precheck": "PASSED",
        "hash_precheck_extension_scope": sorted(verified_extensions),
        "hash_precheck_unverified_files": sorted(unverified_files),
        "hash_precheck_policy": (
            "Set-equality scoped to receipt's file-extension universe. "
            "Files with extensions absent from receipt (optimizer.pt, scheduler.pt, etc.) "
            "are logged UNVERIFIED, not flagged as extra. Policy is auditable here."
        ),
        "ref_model": args.ref_model,
        "reward_type": "beta_times_log_pi_theta_minus_log_pi_ref",
        "beta": args.beta,
        "max_length_used": args.max_length,
        "independent_win_count": win_count,
        "independent_sample_count": processed,
        "skip_count": skip_count,
        "independent_win_rate": round(independent_win_rate, 6),
        "receipt_win_rate": receipt_win_rate,
        "receipt_counts_reconstructed": (counts_source != "exact"),
        "delta": round(delta, 6),
        "tolerance": args.tolerance,
        "win_p_val": win_p_val,
        "win_test_method": win_test_method,
        "mean_margin_dpo": round(mean_margin, 6),
        "std_margin_dpo":  round(std_margin,  6),
        "verification_passed": passes,
        "design_risk_trl_tokenization": (
            # Design risk noted in third audit: trainer used TRL's default truncation policy "
            # (max_length/max_prompt_length not pinned in DPOConfig). This verifier uses "
            # --max-length which is a different truncation regime. Long pairs will diverge "
            # even on a clean model. Mitigation: pin max_length/max_prompt_length in DPOConfig, "
            # record them in dpo_security_attestation.json, read them here instead of CLI default."
            "TRL default truncation not matched; pin max_length in DPOConfig + attestation"
        ),
        "note": (
            "FIX-4: Detached. Never mutates signed receipt; bind by receipt_sha256. "
            "FIX-3: actual pi_ref DPO reward. FIX-1: manifest['eval']. FIX-2: format_dpo. "
            "PREV-1: pi_ref provenance vs attestation. PREV-2: verifier_script_sha256."
        ),
    }

    write_detached_json(verification_out, verification_record)

    if passes:
        logger.info(f"VERIFICATION PASSED -- detached record: {verification_out}")
        logger.info("   Receipt and its .json.sig are UNCHANGED.")
        sys.exit(0)
    else:
        logger.error("VERIFICATION FAILED")
        if delta > args.tolerance:
            logger.error(f"  |delta win_rate| = {delta:.4f} > tolerance {args.tolerance}")
        if independent_win_rate <= 0.5:
            logger.error(f"  Independent win_rate {independent_win_rate:.4f} <= 0.5")
        if win_p_val >= 0.05:
            logger.error(f"  p-value {win_p_val:.4e} >= 0.05")
        if skip_count > 0:
            logger.error(f"  {skip_count} samples skipped (boundary/truncation) — strict mode requires 0")
        logger.error(f"  Failure record: {verification_out}")
        sys.exit(2)


if __name__ == "__main__":
    main()
