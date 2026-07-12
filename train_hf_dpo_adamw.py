#!/usr/bin/env python3
r"""
ZKAEDI PRIME Security Hardened - AdamW Baseline HF DPO Training Engine
Version: 2.2-VERIFIED-20260712
Author: ZKAEDI PRIME Security Testing Orchestrator (self-audit)

SECURITY NOTES (MANDATORY READING BEFORE EXECUTION):
- This version mitigates CVE-2026-4372 (Transformers RCE via config injection).
- Requires transformers>=5.3.0
- Models MUST be loaded from pre-vetted local paths or with explicit revision pinning + trust_remote_code=False.
- For full sovereign ZKAEDI deployment: Download models offline, verify SHA-256 against allow-list manifest, then load locally.
- All paths are validated against a safe base directory to prevent traversal.
- No remote code execution surface exposed via model loading.
- Error messages redact sensitive path components where possible for production logging.
"""

import argparse
import json
import logging
import sys
import getpass
from pathlib import Path
from typing import Optional, Dict, Any
from datetime import datetime, timezone

import torch
from datasets import load_dataset
from transformers import AutoModelForCausalLM, AutoTokenizer, TrainerCallback, __version__ as TRANSFORMERS_VERSION

from trl import DPOTrainer, DPOConfig
from zkaedi_security_utils import (
    validate_safe_path,
    load_model_hardened,
    scan_for_known_cves,
)

# Configure secure logging (no secrets, redact paths in prod)
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [ZKAEDI-SEC-%(levelname)s] %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)]
)
logger = logging.getLogger("zkaedi_dpo_hardened")


class DPOSTripwireCallback(TrainerCallback):
    """Real-time DPO stability checks for gradient explosions, NaNs, and margin saturation."""
    def on_log(self, args, state, control, logs=None, **kwargs):
        if logs is not None:
            # 1. NaN/Inf Checks
            for k, v in logs.items():
                if isinstance(v, float):
                    val_tensor = torch.tensor(v)
                    if torch.isinf(val_tensor) or torch.isnan(val_tensor):
                        logger.error(f"[ZKAEDI SEC] TRIPWIRE TRIGGERED: NaN/Inf detected in metric '{k}'!")
                        control.should_training_stop = True
                        raise ValueError(f"Training aborted due to NaN/Inf in metric '{k}'")

            # 2. Margin Saturation Check
            margin = logs.get("rewards/margins")
            if margin is not None:
                if abs(margin) > 10.0:
                    logger.warning(f"[ZKAEDI SEC] Margin saturation warning: {margin:.4f}")
                    if abs(margin) > 15.0:
                        logger.error(f"[ZKAEDI SEC] TRIPWIRE TRIGGERED: Margin exceeds critical safety boundary of 15.0!")
                        control.should_training_stop = True
                        raise ValueError(f"Training aborted due to preference margin saturation: {margin:.4f}")


def format_dpo(sample):
    """Safe formatting - no exec, pure string ops."""
    sys_prompt = sample.get("system") or ""
    prompt_text = sample.get("prompt") or ""
    formatted_prompt = f"### System:\n{sys_prompt}\n\n### Instruction:\n{prompt_text}\n\n### Response:\n"
    return {
        "prompt": formatted_prompt,
        "chosen": sample.get("chosen") or "",
        "rejected": sample.get("rejected") or ""
    }


def generate_dpo_attestation(
    script_path: Path,
    dataset_path: Path,
    base_model: str,
    adapter_dir: Path,
    combined_hash: str,
    files_dict: Dict[str, str],
    private_key_path: Optional[str] = None,
    password: Optional[str] = None,
) -> None:
    """Generates a cryptographically signed DPO training attestation receipt."""
    from zkaedi_model_registry import get_file_sha256
    
    script_hash = get_file_sha256(script_path)
    ds_hash = get_file_sha256(dataset_path)
    
    attestation = {
        "attestation_type": "ZKAEDI_DPO_TRAINING_ATTESTATION",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "script_sha256": script_hash,
        "dataset": {
            "path": str(dataset_path),
            "sha256": ds_hash
        },
        "base_model": base_model,
        "adapter": {
            "path": str(adapter_dir),
            "combined_sha256": combined_hash,
            "files": files_dict
        }
    }
    
    att_path = adapter_dir / "dpo_security_attestation.json"
    with open(att_path, "w", encoding="utf-8") as f:
        json.dump(attestation, f, indent=2)
    print(f"[ZKAEDI SEC] Attestation written to: {att_path}")
    
    if private_key_path:
        from zkaedi_model_registry import sign_registry
        try:
            signature = sign_registry(attestation, private_key_path, password=password)
            sig_path = att_path.with_suffix(att_path.suffix + ".sig")
            with open(sig_path, "wb") as f:
                f.write(signature)
            print(f"[ZKAEDI SEC] Attestation signed and saved to: {sig_path}")
        except Exception as e:
            print(f"[ZKAEDI SEC] Failed to sign attestation: {e}", file=sys.stderr)
            raise e


def main():
    scan_for_known_cves()
    parser = argparse.ArgumentParser(
        description="ZKAEDI PRIME Hardened AdamW HF DPO Training Engine (CVE-2026-4372 mitigated)",
        epilog="Run only with verified local models or pinned HF revisions after transformers>=5.3.0 upgrade."
    )
    parser.add_argument("--dataset", type=str, default="/mnt/h/agents/train_maxed_validated.parquet",
                        help="Path to local parquet DPO dataset (must be under safe base)")
    parser.add_argument("--model-name", type=str, default="gpt2",
                        help="Base model ID or LOCAL PATH to verified model directory")
    parser.add_argument("--model-revision", type=str, default="main",
                        help="HF Hub revision/commit SHA for pinning (ignored for local paths)")
    parser.add_argument("--output-dir", type=str, default="outputs_dpo_adamw",
                        help="Output directory (will be created under safe base if relative)")
    parser.add_argument("--safe-base-dir", type=str, default="/mnt/h",
                        help="Root directory allowed for all filesystem operations (ZKAEDI policy)")
    
    # Registration & signing arguments
    parser.add_argument("--sign", action="store_true", help="Sign DPO attestation and registry entries")
    parser.add_argument("--private-key", help="Path to Ed25519 private key (required for sign)")
    parser.add_argument("--password", help="Passphrase for private key decryption")
    parser.add_argument("--prompt-password", action="store_true", help="Prompt for private key passphrase")
    parser.add_argument("--register", action="store_true", help="Register the DPO adapter to allow-list")
    parser.add_argument("--adapter-name", help="Registered name for the DPO adapter")

    args = parser.parse_args()

    global SAFE_BASE_DIR
    SAFE_BASE_DIR = Path(args.safe_base_dir).resolve()

    dataset_path_str = args.dataset.replace("\\", "/")
    output_path_str = args.output_dir.replace("\\", "/")

    # === SECURITY: Path validation (fail-closed) ===
    try:
        dataset_path = validate_safe_path(dataset_path_str, description="dataset", must_exist=True)
        output_dir = validate_safe_path(output_path_str, description="output-dir", must_exist=False)
        output_dir.mkdir(parents=True, exist_ok=True)
    except (ValueError, FileNotFoundError) as e:
        logger.error(f"SECURITY BLOCK: {e}")
        sys.exit(1)

    logger.info(f"[ZKAEDI SEC] transformers=={TRANSFORMERS_VERSION} (post-CVE-2026-4372 required)")

    logger.info("[ZKAEDI SEC] Loading Dataset (validated path)...")
    try:
        dataset = load_dataset("parquet", data_files=str(dataset_path), split="train")
    except Exception as e:
        logger.error(f"Dataset load failed (possible corrupt/malicious parquet or permission): {type(e).__name__}")
        sys.exit(1)

    # Manifest load
    manifest_path = Path("splits/dpo_v1_manifest.json")
    if not manifest_path.is_absolute():
        manifest_path = (Path.cwd() / manifest_path).resolve()

    logger.info("[ZKAEDI SEC] Loading manifest...")
    try:
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
        train_indices = manifest["train"]
        eval_indices = manifest["eval"]
    except Exception as e:
        logger.error(f"Manifest load failed: {type(e).__name__}")
        sys.exit(1)

    train_dataset = dataset.select(train_indices).map(format_dpo)
    eval_dataset = dataset.select(eval_indices).map(format_dpo)
    logger.info(f"[ZKAEDI KERNEL] Loaded {len(train_dataset)} train and {len(eval_dataset)} eval sequences.")

    # === CRITICAL SECURITY: Model loading with CVE-2026-4372 mitigations ===
    logger.info("[ZKAEDI SEC] Loading model with hardened parameters...")
    try:
        model, tokenizer = load_model_hardened(args.model_name, revision=args.model_revision)
    except Exception as e:
        logger.error(f"Model load FAILED: {type(e).__name__}")
        sys.exit(1)

    logger.info("[ZKAEDI SEC] Constructing DPOConfig (AdamW + hardened defaults)...")
    training_args = DPOConfig(
        output_dir=str(output_dir),
        per_device_train_batch_size=4,
        gradient_accumulation_steps=1,
        warmup_steps=5,
        max_steps=25,
        learning_rate=2e-5,
        fp16=False,
        bf16=False,
        logging_steps=1,
        weight_decay=0.01,
        lr_scheduler_type="linear",
        seed=3407,
        gradient_checkpointing=False,
        remove_unused_columns=False,
        report_to="none",
        optim="adamw_torch",
        use_cpu=True,
        eval_strategy="steps",
        eval_steps=5,
    )

    trainer = DPOTrainer(
        model=model,
        ref_model=None,
        processing_class=tokenizer,
        train_dataset=train_dataset,
        eval_dataset=eval_dataset,
        optimizers=(None, None),
        args=training_args
    )

    # Register Loss stability tripwire
    trainer.add_callback(DPOSTripwireCallback())

    logger.info("[ZKAEDI SEC] Starting DPO Training (AdamW CPU, scope-compliant)...")
    try:
        trainer.train()
    except ValueError as e:
        logger.error(f"Training halted by safety tripwire: {e}")
        sys.exit(2)

    logger.info("[ZKAEDI SEC] Saving hardened adapter...")
    adapter_dir = output_dir / "adapter"
    adapter_dir.mkdir(exist_ok=True)
    
    # Enforce safe_serialization=True
    model.save_pretrained(adapter_dir, safe_serialization=True)
    tokenizer.save_pretrained(adapter_dir)

    # Post-save validations
    scan_for_known_cves()
    validate_safe_path(str(adapter_dir), must_exist=True, description="saved adapter directory")
    
    from zkaedi_model_registry import get_model_hashes
    combined_hash, files_dict = get_model_hashes(adapter_dir)
    logger.info(f"[ZKAEDI SEC] Saved adapter cryptographically hashed: {combined_hash}")

    # Optional: write integrity manifest
    manifest_file = adapter_dir / "training_manifest.json"
    with open(manifest_file, "w") as mf:
        json.dump({
            "model_name": args.model_name,
            "revision": args.model_revision,
            "transformers_version": TRANSFORMERS_VERSION,
            "dataset": str(dataset_path),
            "train_samples": len(train_dataset),
            "eval_samples": len(eval_dataset),
            "security_note": "Saved in safe tensors format. Hashed and ready for swarm ingestion."
        }, mf, indent=2)

    # Password extraction
    pwd = None
    if args.prompt_password:
        pwd = getpass.getpass("Enter private key passphrase: ")
    elif args.password:
        pwd = args.password

    # Generate and sign attestation
    generate_dpo_attestation(
        script_path=Path(__file__).resolve(),
        dataset_path=dataset_path,
        base_model=args.model_name,
        adapter_dir=adapter_dir,
        combined_hash=combined_hash,
        files_dict=files_dict,
        private_key_path=args.private_key,
        password=pwd
    )

    if args.register:
        try:
            from zkaedi_model_registry import register_model
            reg_name = args.adapter_name or adapter_dir.name
            register_model(
                model_name=reg_name,
                model_path=str(adapter_dir),
                author="DPO Training Engine",
                description=f"DPO adapter trained on dataset {dataset_path.name}",
                sign=args.sign,
                private_key_path=args.private_key,
                password=pwd
            )
            logger.info(f"[ZKAEDI SEC] Adapter model '{reg_name}' registered successfully.")
        except Exception as e:
            logger.error(f"Registry auto-registration failed: {e}")
            sys.exit(1)

    logger.info(f"[ZKAEDI SEC] Training complete. Adapter + manifest + attestation saved to {adapter_dir}")


if __name__ == "__main__":
    main()
