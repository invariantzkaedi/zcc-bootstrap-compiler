#!/usr/bin/env python3
r"""
ZKAEDI PRIME Security Hardened - AdamW Baseline HF DPO Training Engine
Version: 2.6-RELEASE-20260712
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
import tempfile
import os

# Fix transformers v5 compatibility with llm_blender
try:
    import transformers.utils.hub
    if not hasattr(transformers.utils.hub, "TRANSFORMERS_CACHE"):
        transformers.utils.hub.TRANSFORMERS_CACHE = os.getenv("HF_HOME", os.path.expanduser("~/.cache/huggingface/hub"))
except ImportError:
    pass
import math
import numbers
from pathlib import Path
from typing import Optional, Dict, Any
from datetime import datetime, timezone

import torch
from datasets import load_dataset
from transformers import TrainerCallback, __version__ as TRANSFORMERS_VERSION

from trl import DPOTrainer, DPOConfig
from zkaedi_security_utils import (
    validate_safe_path,
    load_model_hardened,
    scan_for_known_cves,
    get_safe_bases,
)

SAFE_BASE_DIR = get_safe_bases()[0]

# Configure secure logging (no secrets, redact paths in prod)
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [ZKAEDI-SEC-%(levelname)s] %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)]
)
logger = logging.getLogger("zkaedi_dpo_hardened")


def write_atomic_json(target_path: Path, data: dict) -> None:
    """Atomic write for JSON payloads using fsync, directory fsync, and temporary replacement."""
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
        # Replace
        os.replace(temp_path, target_path)
        temp_path = None
        # Fsync parent directory for power-loss metadata durability (wrapped gracefully for Windows/etc.)
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


def write_atomic_binary(target_path: Path, data: bytes) -> None:
    """Atomic write for raw signatures using fsync, directory fsync, and temporary replacement."""
    target_path = Path(target_path)
    parent = target_path.parent
    parent.mkdir(parents=True, exist_ok=True)
    temp_path = None
    try:
        with tempfile.NamedTemporaryFile("wb", dir=str(parent), delete=False) as f:
            temp_path = Path(f.name)
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.replace(temp_path, target_path)
        temp_path = None
        # Fsync parent directory
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


def get_relative_safe_path(path: Path, base_dir: Path) -> str:
    """Redacts absolute system path leaks by resolving paths relative to safe workspace base.
    Fails closed if the path lies outside the safe workspace.
    """
    resolved_path = path.resolve()
    resolved_base = base_dir.resolve()
    try:
        return resolved_path.relative_to(resolved_base).as_posix()
    except ValueError as exc:
        raise ValueError(
            f"Path is outside the declared safe workspace: {resolved_path.name}"
        ) from exc


def normalize_scalar_metric(name: str, value: Any) -> Optional[float]:
    """Safely normalizes values to floats from scalars, numpy values, or single-element tensors.
    Raises ValueError on non-scalar / multi-element metrics.
    """
    if isinstance(value, numbers.Real):
        return float(value)

    if torch.is_tensor(value):
        if value.numel() != 1:
            raise ValueError(
                f"Metric '{name}' must be scalar; received {value.numel()} values"
            )
        return float(value.detach().cpu().item())

    try:
        import numpy as np
        if isinstance(value, np.ndarray):
            if value.size != 1:
                raise ValueError(
                    f"Metric '{name}' must be scalar; received {value.size} values"
                )
            return float(value.item())
    except ImportError:
        pass

    return None


class DPOSTripwireCallback(TrainerCallback):
    """Real-time DPO stability checks for gradient explosions, NaNs, and margin saturation."""
    def on_log(self, args, state, control, logs=None, **kwargs):
        if logs is not None:
            # 1. NaN/Inf Checks (supporting tensors, numpy, float, and nonnumeric edge cases)
            for k, v in logs.items():
                is_nan_inf = False
                val_norm = normalize_scalar_metric(k, v)
                if val_norm is not None:
                    if not math.isfinite(val_norm):
                        is_nan_inf = True
                
                if is_nan_inf:
                    logger.error(f"[ZKAEDI SEC] TRIPWIRE TRIGGERED: NaN/Inf detected in metric '{k}'!")
                    control.should_training_stop = True
                    raise ValueError(f"Training aborted due to NaN/Inf in metric '{k}'")

            # 2. Margin Saturation Check
            margin = logs.get("rewards/margins")
            if margin is not None:
                margin_val = normalize_scalar_metric("rewards/margins", margin)
                if margin_val is not None:
                    if abs(margin_val) > 10.0:
                        logger.warning(f"[ZKAEDI SEC] Margin saturation warning: {margin_val:.4f}")
                        if abs(margin_val) > 15.0:
                            logger.error("[ZKAEDI SEC] TRIPWIRE TRIGGERED: Margin exceeds critical safety boundary of 15.0!")
                            control.should_training_stop = True
                            raise ValueError(f"Training aborted due to preference margin saturation: {margin_val:.4f}")


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
    base_model_hash: str,
    checkpoint_dir: Path,
    model_payload_sha256: str,
    files_dict: Dict[str, str],
    safe_base_dir: Path,
    private_key_path: Optional[str] = None,
    password: Optional[str] = None,
    num_train_samples: Optional[int] = None,
    num_eval_samples: Optional[int] = None,
    training_config: Optional[Dict[str, Any]] = None,
    attestation_id: Optional[str] = None,
    manifest_sha256: Optional[str] = None,
) -> None:
    """Generates a cryptographically signed DPO training attestation receipt."""
    import uuid
    import secrets
    from zkaedi_model_registry import get_file_sha256
    
    script_hash = get_file_sha256(script_path)
    ds_hash = get_file_sha256(dataset_path)

    try:
        import trl
        trl_version = trl.__version__
    except Exception:
        trl_version = "unknown"

    attestation = {
        "attestation_type": "ZKAEDI_DPO_TRAINING_ATTESTATION",
        "attestation_id": attestation_id or str(uuid.uuid4()),
        "nonce": secrets.token_hex(16),
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "environment": {
            "python_version": sys.version.split()[0],
            "torch_version": torch.__version__,
            "transformers_version": TRANSFORMERS_VERSION,
            "trl_version": trl_version,
            "cuda_available": torch.cuda.is_available(),
            "cuda_device_count": torch.cuda.device_count()
        },
        "script_sha256": script_hash,
        "dataset": {
            "path": get_relative_safe_path(dataset_path, safe_base_dir),
            "sha256": ds_hash,
            "train_samples": num_train_samples,
            "eval_samples": num_eval_samples
        },
        "base_model": {
            "identity": base_model,
            "allow_list_sha256": base_model_hash
        },
        "training_config": training_config or {},
        "model_payload_sha256": model_payload_sha256,
        "training_manifest": {
            "sha256": manifest_sha256 or "unknown",
            "relative_path": "training_manifest.json"
        },
        "checkpoint": {
            "path": get_relative_safe_path(checkpoint_dir, safe_base_dir),
            "files": files_dict
        }
    }
    
    att_path = checkpoint_dir / "dpo_security_attestation.json"
    write_atomic_json(att_path, attestation)
    print(f"[ZKAEDI SEC] Attestation written atomically to: {att_path}")
    
    if private_key_path:
        from zkaedi_model_registry import sign_registry
        try:
            # Sign the attestation file
            signature = sign_registry(attestation, private_key_path, password=password)
            sig_path = att_path.with_suffix(att_path.suffix + ".sig")
            write_atomic_binary(sig_path, signature)
            print(f"[ZKAEDI SEC] Attestation signed and saved to: {sig_path}")
            
            # Sign the training manifest file if it exists
            manifest_file = checkpoint_dir / "training_manifest.json"
            if manifest_file.exists():
                with open(manifest_file, "r") as mf:
                    manifest_data = json.load(mf)
                manifest_sig = sign_registry(manifest_data, private_key_path, password=password)
                manifest_sig_path = manifest_file.with_suffix(manifest_file.suffix + ".sig")
                write_atomic_binary(manifest_sig_path, manifest_sig)
                print(f"[ZKAEDI SEC] Training manifest signed and saved to: {manifest_sig_path}")
        except Exception as e:
            print(f"[ZKAEDI SEC] Failed to sign attestation/manifest: {e}", file=sys.stderr)
            raise e


def verify_release_receipt(receipt_path: Path, public_key_path: Path, safe_base: Optional[Path] = None) -> bool:
    """Verifies a detached release receipt signature, checks file digests, and validates bundle integrity (REL-03)."""
    from zkaedi_model_registry import verify_registry_signature, get_model_hashes
    from zkaedi_security_utils import validate_safe_path
    
    extra_bases = [Path(safe_base)] if safe_base else None
    
    # 1. Path Containment check for paths (SEC-17)
    # Validate receipt file, public key, and signature file against safe base
    validated_receipt = validate_safe_path(str(receipt_path), must_exist=True, extra_safe_bases=extra_bases, description="receipt path")
    validated_public_key = validate_safe_path(str(public_key_path), must_exist=True, extra_safe_bases=extra_bases, description="public key path")
    
    sig_path = Path(str(validated_receipt) + ".sig")
    validated_sig_path = validate_safe_path(str(sig_path), must_exist=True, extra_safe_bases=extra_bases, description="receipt signature path")
    
    # Resolve and check safe_base explicitly if provided
    if safe_base:
        trusted_base = Path(safe_base).resolve()
        for p in (validated_receipt, validated_public_key, validated_sig_path):
            try:
                p.resolve().relative_to(trusted_base)
            except ValueError as exc:
                raise ValueError(f"Path {p} escapes trusted safe base {trusted_base}") from exc
                
    with open(validated_receipt, "r", encoding="utf-8") as f:
        receipt_data = json.load(f)
        
    with open(validated_sig_path, "rb") as f:
        signature = f.read()
        
    # Verify signature
    if not verify_registry_signature(receipt_data, signature, str(validated_public_key)):
        raise ValueError("Release receipt signature verification failed")
        
    # 2. Recalculate and verify files in receipt
    rel_art_path = receipt_data.get("relative_artifact_path", "checkpoint")
    if Path(rel_art_path).is_absolute():
        raise ValueError("Absolute artifact paths are not permitted in release receipts")
        
    receipt_root = validated_receipt.parent.resolve()
    checkpoint_dir = (receipt_root / rel_art_path).resolve()
    
    try:
        checkpoint_dir.relative_to(receipt_root)
    except ValueError as exc:
        raise ValueError("Receipt artifact path escapes the release directory") from exc
        
    # Validate artifact directory against safe base
    validated_checkpoint_dir = validate_safe_path(str(checkpoint_dir), must_exist=True, extra_safe_bases=extra_bases, description="artifact directory")
    
    if safe_base:
        trusted_base = Path(safe_base).resolve()
        try:
            validated_checkpoint_dir.resolve().relative_to(trusted_base)
        except ValueError as exc:
            raise ValueError(f"Path {validated_checkpoint_dir} escapes trusted safe base {trusted_base}") from exc

        
    # 3. Recalculate file digests
    actual_bundle_hash, actual_files = get_model_hashes(validated_checkpoint_dir)
    
    # 4. Compare expected files and hashes
    expected_files = receipt_data.get("files", {})
    if set(expected_files.keys()) != set(actual_files.keys()):
        raise ValueError(
            f"Release bundle file mismatch. Expected: {list(expected_files.keys())}, Actual: {list(actual_files.keys())}"
        )
        
    for rel_path, expected_hash in expected_files.items():
        if actual_files[rel_path] != expected_hash:
            raise ValueError(f"File integrity mismatch for '{rel_path}'")
            
    # 5. Verify bundle sha256
    if receipt_data.get("bundle_sha256") != actual_bundle_hash:
        raise ValueError("Bundle aggregate hash mismatch")
        
    # 6. Cross-check attestation_id with dpo_security_attestation.json inside checkpoint (REL-03)
    attestation_file = validated_checkpoint_dir / "dpo_security_attestation.json"
    if not attestation_file.exists():
        raise FileNotFoundError(f"Attestation document not found inside checkpoint: {attestation_file}")
    with open(attestation_file, "r", encoding="utf-8") as f:
        attestation_data = json.load(f)
    if attestation_data.get("attestation_id") != receipt_data.get("attestation_id"):
        raise ValueError("Cross-check failed: attestation_id mismatch between receipt and attestation document")
        
    return True


def main():
    scan_for_known_cves()
    parser = argparse.ArgumentParser(
        description="ZKAEDI PRIME Hardened AdamW HF DPO Training Engine (CVE-2026-4372 mitigated)",
        epilog="Run only with verified local models or pinned HF revisions after transformers>=5.3.0 upgrade."
    )
    parser.add_argument("--mode", type=str, choices=["dev", "release"], default="dev",
                        help="Execution mode: 'dev' for rapid prototyping (unsigned), 'release' for strict cryptographic verification.")
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
    parser.add_argument("--public-key", help="Path to Ed25519 public key for registry verification")
    
    # Registration & signing arguments
    parser.add_argument("--sign", action="store_true", help="Sign DPO attestation and registry entries")
    parser.add_argument("--private-key", help="Path to Ed25519 private key (required for sign)")
    parser.add_argument("--password", help="[DEPRECATED/INSECURE] Passphrase for private key decryption")
    parser.add_argument("--prompt-password", action="store_true", help="Prompt for private key passphrase")
    parser.add_argument("--register", action="store_true", help="Register the DPO adapter to allow-list")
    parser.add_argument("--artifact-name", help="Registered name identifier for the DPO artifact")

    args = parser.parse_args()

    # === SECURITY: mode validation gates (SEC-09 & SEC-10) ===
    if args.mode == "release":
        if not args.sign:
            parser.error("In release mode, --sign is required to establish cryptographic provenance.")
        if not args.private_key:
            parser.error("In release mode, --private-key is required to sign release artifacts.")
        if not args.public_key:
            parser.error("In release mode, --public-key is required to verify signatures.")
        p_base = Path(args.model_name)
        if not p_base.exists() or not p_base.is_dir():
            parser.error("In release mode, --model-name must be an existing local directory containing pre-downloaded model weights.")
    else:
        # Development mode blocks auto-registration
        if args.register:
            parser.error("Auto-registration is blocked in development mode. Set --mode release to register artifacts.")

    # === SECURITY: signing flag semantics (SEC-01) ===
    if args.sign and not args.private_key:
        parser.error("--private-key is required when --sign is enabled")
    
    attestation_key = args.private_key if args.sign else None

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

    # === SECURITY: Base Model allow-list & integrity verification (SEC-06) ===
    base_model_hash = "unknown"
    logger.info("[ZKAEDI SEC] Resolving base model allow-list hash...")
    try:
        from zkaedi_model_registry import load_registry, verify_model_integrity
        # Load registry with signature checking if public key is passed
        verify_sig = (args.public_key is not None) or (args.mode == "release")
        registry = load_registry(verify_signature=verify_sig, public_key_path=args.public_key)
        
        p_base = Path(args.model_name)
        if p_base.exists():
            # Verify base model directory files strictly against allowlist
            is_valid, errors = verify_model_integrity(str(p_base), verify_signature=verify_sig, public_key_path=args.public_key)
            if not is_valid:
                raise ValueError(f"Integrity verification failed: {errors}")
            
            # Extract combined hash of local path from registry
            resolved_abs_base = p_base.resolve()
            for entry in registry.get("models", {}).values():
                entry_path = Path(entry.get("path", "")).resolve()
                if entry_path == resolved_abs_base:
                    base_model_hash = entry.get("combined_sha256", "unknown")
                    break
        else:
            # Check by identifier in allow-list
            if args.model_name in registry.get("models", {}):
                base_model_hash = registry["models"][args.model_name].get("combined_sha256", "unknown")
    except Exception as e:
        logger.error(f"[ZKAEDI SEC] Base model allow-list verification failed: {e}")
        sys.exit(3)

    if base_model_hash == "unknown":
        logger.error(f"[ZKAEDI SEC] FAIL: Base model '{args.model_name}' has no verified allow-list digest!")
        sys.exit(3)

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

    logger.info("[ZKAEDI SEC] Saving hardened checkpoint...")
    checkpoint_dir = output_dir / "checkpoint"
    checkpoint_dir.mkdir(exist_ok=True)
    
    # Enforce safe_serialization=True
    model.save_pretrained(checkpoint_dir, safe_serialization=True)
    tokenizer.save_pretrained(checkpoint_dir)

    # Post-save validations
    scan_for_known_cves()
    validate_safe_path(str(checkpoint_dir), must_exist=True, description="saved checkpoint directory")
    
    from zkaedi_model_registry import get_model_hashes
    model_payload_sha256, files_dict = get_model_hashes(checkpoint_dir)
    logger.info(f"[ZKAEDI SEC] Model payload weights cryptographically hashed: {model_payload_sha256}")

    # Determine adapter vs model type
    is_peft = False
    try:
        from peft import PeftModel
        if isinstance(model, PeftModel):
            is_peft = True
    except ImportError:
        pass
    artifact_type = "PEFT_adapter" if is_peft else "fine_tuned_model"

    # UUID replay protection / detection token
    import uuid
    attestation_id = str(uuid.uuid4())

    # Optional: write integrity manifest
    manifest_file = checkpoint_dir / "training_manifest.json"
    manifest_data = {
        "model_name": args.model_name,
        "revision": args.model_revision,
        "transformers_version": TRANSFORMERS_VERSION,
        "dataset": {
            "path": get_relative_safe_path(dataset_path, SAFE_BASE_DIR),
            "logical_name": dataset_path.stem
        },
        "train_samples": len(train_dataset),
        "eval_samples": len(eval_dataset),
        "model_payload_sha256": model_payload_sha256,
        "artifact_type": artifact_type,
        "attestation_id": attestation_id,
        "security_note": "Saved in safe tensors format. Hashed and ready for swarm ingestion."
    }
    write_atomic_json(manifest_file, manifest_data)

    # Calculate manifest sha256
    from zkaedi_model_registry import get_file_sha256
    manifest_sha256 = get_file_sha256(manifest_file)

    # Password extraction
    pwd = None
    if args.password:
        logger.warning("[ZKAEDI SEC] WARNING: Passing password via plaintext CLI arguments is deprecated and insecure. Use --prompt-password or environment-based key managers.")
        pwd = args.password
    elif args.prompt_password:
        pwd = getpass.getpass("Enter private key passphrase: ")

    # Extract training hyperparameters
    config_dict = {
        "learning_rate": training_args.learning_rate,
        "lr_scheduler_type": training_args.lr_scheduler_type,
        "weight_decay": training_args.weight_decay,
        "seed": training_args.seed,
        "max_steps": training_args.max_steps,
        "per_device_train_batch_size": training_args.per_device_train_batch_size,
        "gradient_accumulation_steps": training_args.gradient_accumulation_steps,
        "optim": training_args.optim,
        "fp16": training_args.fp16,
        "bf16": training_args.bf16
    }

    # Generate and sign attestation
    generate_dpo_attestation(
        script_path=Path(__file__).resolve(),
        dataset_path=dataset_path,
        base_model=args.model_name,
        base_model_hash=base_model_hash,
        checkpoint_dir=checkpoint_dir,
        model_payload_sha256=model_payload_sha256,
        files_dict=files_dict,
        safe_base_dir=SAFE_BASE_DIR,
        private_key_path=attestation_key,
        password=pwd,
        num_train_samples=len(train_dataset),
        num_eval_samples=len(eval_dataset),
        training_config=config_dict,
        attestation_id=attestation_id,
        manifest_sha256=manifest_sha256
    )

    # Compute complete final release-bundle digest of checkpoint directory
    final_bundle_hash, final_bundle_files = get_model_hashes(checkpoint_dir)
    
    # Write detached release receipt outside checkpoint_dir
    receipt_data = {
        "artifact": "checkpoint",
        "relative_artifact_path": "checkpoint",
        "bundle_sha256": final_bundle_hash,
        "files": final_bundle_files,
        "attestation_id": attestation_id,
        "timestamp": datetime.now(timezone.utc).isoformat()
    }
    receipt_path = output_dir / "release_receipt.json"
    write_atomic_json(receipt_path, receipt_data)
    logger.info(f"[ZKAEDI SEC] Detached release receipt written atomically to: {receipt_path}")
    
    # Sign the detached receipt if private key is active
    if attestation_key:
        from zkaedi_model_registry import sign_registry
        try:
            receipt_sig = sign_registry(receipt_data, attestation_key, password=pwd)
            receipt_sig_path = receipt_path.with_suffix(receipt_path.suffix + ".sig")
            write_atomic_binary(receipt_sig_path, receipt_sig)
            logger.info(f"[ZKAEDI SEC] Detached release receipt signed: {receipt_sig_path}")
        except Exception as e:
            logger.error(f"Failed to sign detached release receipt: {e}")
            sys.exit(1)

    # Mandatory receipt verification gate (REL-03 / SEC-09)
    if args.mode == "release":
        logger.info("[ZKAEDI SEC] Running mandatory release receipt verification gate...")
        try:
            verify_release_receipt(receipt_path, Path(args.public_key), safe_base=SAFE_BASE_DIR)
            logger.info("[ZKAEDI SEC] Mandatory verification gate: PASSED (Signature, file digests, and bundle hash matching precisely)")
        except Exception as e:
            logger.error(f"[ZKAEDI SEC] Mandatory verification gate FAILED: {e}")
            sys.exit(1)
    elif attestation_key and args.public_key:
        # Dev mode best-effort verification if keys are passed
        logger.info("[ZKAEDI SEC] Running dev mode best-effort release receipt verification gate...")
        try:
            verify_release_receipt(receipt_path, Path(args.public_key), safe_base=SAFE_BASE_DIR)
            logger.info("[ZKAEDI SEC] Dev verification gate: PASSED")
        except Exception as e:
            logger.warning(f"[ZKAEDI SEC] Dev verification gate FAILED: {e}")

    if args.register:
        try:
            from zkaedi_model_registry import register_model
            reg_name = args.artifact_name or checkpoint_dir.name
            reg_desc = f"DPO {artifact_type} trained on dataset {dataset_path.name}"
            # Structured metadata registration (SEC-07)
            meta = {
                "artifact_type": artifact_type,
                "release_bundle_sha256": final_bundle_hash,
                "release_receipt_path": get_relative_safe_path(receipt_path, SAFE_BASE_DIR),
                "attestation_id": attestation_id
            }
            register_model(
                model_name=reg_name,
                model_path=str(checkpoint_dir),
                author="DPO Training Engine",
                description=reg_desc,
                sign=args.sign,
                private_key_path=args.private_key,
                password=pwd,
                metadata=meta
            )
            logger.info(f"[ZKAEDI SEC] Checkpoint model '{reg_name}' registered successfully.")
        except Exception as e:
            logger.error(f"Registry auto-registration failed: {e}")
            sys.exit(1)

    logger.info(f"[ZKAEDI SEC] Training complete. Checkpoint + manifest + attestation saved to {checkpoint_dir}")


if __name__ == "__main__":
    main()
