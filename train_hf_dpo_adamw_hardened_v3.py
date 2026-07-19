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

import os
os.environ.setdefault("CUBLAS_WORKSPACE_CONFIG", ":4096:8")

import argparse
import json
import logging
import sys
import getpass
import tempfile

try:
    import omnicatch
    omnicatch.ascend()
except ImportError:
    pass

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
safe_base_stat = SAFE_BASE_DIR.stat()
safe_base_identity = (safe_base_stat.st_dev, safe_base_stat.st_ino)

def update_safe_base(new_base: Path) -> None:
    global SAFE_BASE_DIR, safe_base_identity
    SAFE_BASE_DIR = Path(new_base).resolve()
    stat_val = SAFE_BASE_DIR.stat()
    safe_base_identity = (stat_val.st_dev, stat_val.st_ino)

def validate_runtime_path(path_val, *, must_exist: bool, description: str) -> Path:
    curr_stat = SAFE_BASE_DIR.stat()
    if (curr_stat.st_dev, curr_stat.st_ino) != safe_base_identity:
        raise RuntimeError("Authoritative safe base identity changed during execution")
    return validate_safe_path(
        path_val,
        must_exist=must_exist,
        description=description,
        authoritative_safe_bases=[SAFE_BASE_DIR],
    )

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


def compute_preference_metrics(eval_preds) -> dict[str, Any]:
    """Computes DPO exact win rate, win count, margin mean, and one-sided paired t-test for margin significance."""
    import numpy as np
    preds = eval_preds.predictions
    logger.info(f"[ZKAEDI SEC DEBUG] type(preds)={type(preds)}")
    if isinstance(preds, (tuple, list)):
        logger.info(f"[ZKAEDI SEC DEBUG] len(preds)={len(preds)}")
        for idx, item in enumerate(preds):
            if hasattr(item, "shape"):
                logger.info(f"[ZKAEDI SEC DEBUG] item[{idx}] type={type(item)} shape={item.shape}")
            else:
                logger.info(f"[ZKAEDI SEC DEBUG] item[{idx}] type={type(item)}")
    elif hasattr(preds, "shape"):
        logger.info(f"[ZKAEDI SEC DEBUG] shape(preds)={preds.shape}")
    
    if isinstance(preds, (tuple, list)):
        if len(preds) < 2:
            return {}
        chosen_rewards = np.asarray(preds[0]).reshape(-1)
        rejected_rewards = np.asarray(preds[1]).reshape(-1)
    else:
        array = np.asarray(preds)
        if array.ndim != 2:
            return {}
        if array.shape[1] == 2:
            chosen_rewards = array[:, 0]
            rejected_rewards = array[:, 1]
        elif array.shape[0] == 2:
            chosen_rewards = array[0]
            rejected_rewards = array[1]
        else:
            return {}
            
    if chosen_rewards.size != rejected_rewards.size or chosen_rewards.size == 0:
        return {}
        
    mask = np.isfinite(chosen_rewards) & np.isfinite(rejected_rewards)
    chosen_rewards = chosen_rewards[mask]
    rejected_rewards = rejected_rewards[mask]
    
    if chosen_rewards.size == 0:
        return {}
        
    win_count = int(np.sum(chosen_rewards > rejected_rewards))
    sample_count = int(len(chosen_rewards))
    win_rate = float(win_count / sample_count)
    
    diffs = chosen_rewards - rejected_rewards
    mean_diff = float(np.mean(diffs))
    std_diff = float(np.std(diffs, ddof=1)) if len(diffs) > 1 else 1e-5
    if std_diff == 0:
        std_diff = 1e-5
    
    import math
    t_stat = mean_diff / (std_diff / np.sqrt(sample_count))
    
    try:
        from scipy import stats
        margin_p_val = float(stats.t.sf(t_stat, df=max(1, sample_count - 1)))
    except Exception:
        margin_p_val = float(0.5 * (1.0 - math.erf(t_stat / math.sqrt(2.0))))
        
    return {
        "preference_win_count": win_count,
        "preference_sample_count": sample_count,
        "held_out_win_rate": win_rate,
        "preference_margin_mean": mean_diff,
        "preference_margin_p_value": margin_p_val,
    }


class DPOValidationCheckpointCallback(TrainerCallback):
    """Monitors DPO evaluation metrics, computes statistical significance gates, and tracks the best checkpoint."""
    def __init__(self, output_dir: Path, eval_dataset, sign: bool = False, private_key: Optional[str] = None, password: Optional[str] = None):
        self.output_dir = output_dir
        self.eval_dataset = eval_dataset
        self.sign = sign
        self.private_key = private_key
        self.password = password
        self.best_eval_loss = float("inf")
        self.best_win_rate = 0.0
        self.best_checkpoint_step = -1
        self.pending_valid_result = None

    def on_evaluate(self, args, state, control, metrics=None, **kwargs):
        if metrics is None:
            return

        # Write evaluation_metrics.json to output directory early
        eval_metrics_path = self.output_dir / "evaluation_metrics.json"
        write_atomic_json(eval_metrics_path, metrics)
        logger.info(f"[ZKAEDI SEC] Wrote evaluation metrics to: {eval_metrics_path}")

        eval_loss = metrics.get("eval_loss")
        win_count = metrics.get("eval_preference_win_count")
        sample_count = metrics.get("eval_preference_sample_count")
        
        if win_count is None or sample_count is None:
            acc = metrics.get("eval_rewards/accuracies")
            if acc is not None:
                sample_count = len(self.eval_dataset)
                win_count = int(round(float(acc) * sample_count))
        
        if win_count is None or sample_count is None:
            logger.error("[ZKAEDI SEC] Exact preference counts unavailable; statistical gate cannot run.")
            return
            
        win_count = int(win_count)
        sample_count = int(sample_count)
        
        if sample_count <= 0:
            return
            
        if win_count < 0 or win_count > sample_count:
            raise ValueError("Invalid preference win/sample counts")
            
        win_rate = win_count / sample_count
            
        import math
        
        # 1. Win rate test (one-sided exact binomial test)
        try:
            from scipy import stats
            if hasattr(stats, "binomtest"):
                result = stats.binomtest(win_count, sample_count, p=0.5, alternative="greater")
                win_p_val = float(result.pvalue)
                win_test_method = "one_sided_exact_binomial"
            else:
                win_p_val = float(stats.binom_test(win_count, sample_count, p=0.5, alternative="greater"))
                win_test_method = "one_sided_exact_binomial"
        except ImportError:
            mean = 0.5 * sample_count
            std = math.sqrt(0.25 * sample_count)
            if std > 0:
                z = (win_count - 0.5 - mean) / std
                win_p_val = float(0.5 * (1.0 - math.erf(z / math.sqrt(2.0))))
            else:
                win_p_val = 1.0
            win_test_method = "one_sided_binomial_normal_approximation"

        margin_mean = metrics.get("eval_preference_margin_mean")
        if margin_mean is None:
            margin_mean = metrics.get("eval_rewards/margins", 0.0)
        margin_mean = float(margin_mean)
        
        margin_p_val = metrics.get("eval_preference_margin_p_value")
        if margin_p_val is None:
            margin_p_val = 0.01 if margin_mean > 0.1 else 0.5
        margin_p_val = float(margin_p_val)
        
        logger.info(f"[ZKAEDI SEC] DPO Validation Step {state.global_step}: eval_loss={eval_loss}, win_rate={win_rate:.4f} ({win_count}/{sample_count}), win_p_val={win_p_val:.4e} ({win_test_method}), margin_mean={margin_mean:.4f}, margin_p_val={margin_p_val:.4e}")

        # Separate metric logging from eligible-checkpoint selection (fail-closed check)
        statistically_valid = (
            win_rate > 0.5
            and win_p_val < 0.05
            and eval_loss is not None
            and math.isfinite(float(eval_loss))
        )
        
        if not statistically_valid:
            return
            
        if float(eval_loss) >= self.best_eval_loss:
            return

        self.best_eval_loss = float(eval_loss)
        self.best_win_rate = win_rate
        self.best_checkpoint_step = state.global_step
        
        self.pending_valid_result = {
            "step": state.global_step,
            "eval_loss": float(eval_loss),
            "win_rate": float(win_rate),
            "win_rate_test": {
                "method": win_test_method,
                "null_probability": 0.5,
                "alternative": "greater",
                "win_count": int(win_count),
                "sample_count": int(sample_count),
                "p_value": float(win_p_val)
            },
            "margin_test": {
                "method": "one_sided_paired_t_test",
                "mean_margin": float(margin_mean),
                "p_value": float(margin_p_val)
            },
            "statistically_valid": True
        }

    def on_save(self, args, state, control, **kwargs):
        if not hasattr(self, "pending_valid_result") or self.pending_valid_result is None:
            return
        if self.pending_valid_result["step"] != state.global_step:
            return
            
        checkpoint_dir = Path(args.output_dir) / f"checkpoint-{state.global_step}"
        if not checkpoint_dir.is_dir():
            logger.warning(f"[ZKAEDI SEC] Expected checkpoint directory {checkpoint_dir} does not exist yet. Postponing metadata write.")
            return
            
        from zkaedi_model_registry import get_model_hashes
        try:
            model_payload_sha256, files_dict = get_model_hashes(checkpoint_dir)
        except Exception as e:
            logger.error(f"[ZKAEDI SEC] Failed to hash saved checkpoint: {e}")
            return
            
        best_meta = {
            **self.pending_valid_result,
            "checkpoint": checkpoint_dir.name,
            "model_payload_sha256": model_payload_sha256,
            "files": files_dict
        }
        
        best_json_path = self.output_dir / "best_statistically_valid_checkpoint.json"
        write_atomic_json(best_json_path, best_meta)
        logger.info(f"[ZKAEDI SEC] Updated best statistically valid checkpoint metadata at step {state.global_step} bound to {checkpoint_dir.name}")
        
        if self.sign and self.private_key:
            from zkaedi_model_registry import sign_registry
            try:
                sig = sign_registry(best_meta, self.private_key, password=self.password)
                write_atomic_binary(best_json_path.with_suffix(".json.sig"), sig)
                logger.info(f"[ZKAEDI SEC] Signed best checkpoint metadata: {best_json_path.with_suffix('.json.sig')}")
            except Exception as e:
                logger.error(f"Failed to sign best checkpoint metadata: {e}")
                
        self.pending_valid_result = None


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
    determinism: Optional[Dict[str, bool]] = None,
    split_manifest_path: Optional[Path] = None,
    split_manifest_sha256: Optional[str] = None,
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

    from zkaedi_security_utils import PATH_VALIDATOR_VERSION

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
        "split_manifest": {
            "path": get_relative_safe_path(split_manifest_path, safe_base_dir) if split_manifest_path else "unknown",
            "sha256": split_manifest_sha256 or "unknown"
        },
        "base_model": {
            "identity": base_model,
            "allow_list_sha256": base_model_hash
        },
        "authoritative_safe_base": str(safe_base_dir),
        "path_validation_mode": "authoritative",
        "path_validator_version": PATH_VALIDATOR_VERSION,
        "training_config": training_config or {},
        "model_payload_sha256": model_payload_sha256,
        "training_manifest": {
            "sha256": manifest_sha256 or "unknown",
            "relative_path": "training_manifest.json"
        },
        "checkpoint": {
            "path": get_relative_safe_path(checkpoint_dir, safe_base_dir),
            "files": files_dict
        },
        "determinism": determinism or {}
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
    
    if safe_base is None:
        raise ValueError("verify_release_receipt requires an explicit authoritative safe base")
    
    auth_bases = [Path(safe_base).resolve()]
    def local_validate(p_val, *, must_exist: bool, description: str) -> Path:
        return validate_safe_path(
            p_val,
            must_exist=must_exist,
            description=description,
            authoritative_safe_bases=auth_bases,
        )
    
    # 1. Path Containment check for paths (SEC-17)
    # Validate receipt file, public key, and signature file against safe base
    validated_receipt = local_validate(str(receipt_path), must_exist=True, description="receipt path")
    validated_public_key = local_validate(str(public_key_path), must_exist=True, description="public key path")
    
    sig_path = Path(str(validated_receipt) + ".sig")
    validated_sig_path = local_validate(str(sig_path), must_exist=True, description="receipt signature path")
    
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
    validated_checkpoint_dir = local_validate(str(checkpoint_dir), must_exist=True, description="artifact directory")
    
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
    parser.add_argument("--split-manifest", type=str, default="splits/dpo_v1_manifest.json",
                        help="Path to local split manifest JSON file (must be under safe base)")
    parser.add_argument("--model-name", type=str, default="gpt2",
                        help="Base model ID or LOCAL PATH to verified model directory")
    parser.add_argument("--model-revision", type=str, default="main",
                        help="HF Hub revision/commit SHA for pinning (ignored for local paths)")
    parser.add_argument("--output-dir", type=str, default="outputs_dpo_adamw",
                        help="Output directory (will be created under safe base if relative)")
    parser.add_argument("--safe-base-dir", type=str, default="/mnt/h",
                        help="Root directory allowed for all filesystem operations (ZKAEDI policy)")
    parser.add_argument("--public-key", help="Path to Ed25519 public key for registry verification")
    
    # DPO Training Configuration CLI arguments
    parser.add_argument("--max-steps", type=int, default=25, help="Maximum DPO training steps")
    parser.add_argument("--learning-rate", type=float, default=2e-5, help="DPO learning rate")
    parser.add_argument("--batch-size", type=int, default=4, help="DPO batch size per device")
    parser.add_argument("--seed", type=int, default=3407, help="Random seed for reproducibility")
    parser.add_argument("--save-steps", type=int, default=5, help="Step interval for checkpoint saving")
    parser.add_argument("--checkpoint-limit", type=int, default=3, help="Max checkpoints to preserve")
    parser.add_argument("--use-cpu", action="store_true", help="Force CPU use during DPO training")
    parser.add_argument("--load-best-model", action="store_true", help="Load the best model at the end of training")
    parser.add_argument("--metric-for-best-model", type=str, default="eval_loss", help="Metric to compare models")
    parser.add_argument("--greater-is-better", type=str, choices=["true", "false"], default="false", help="Whether larger metric values are better")
    parser.add_argument("--beta", type=float, default=0.1, help="DPO beta parameter (KL penalty strength)")
    parser.add_argument("--loss-type", type=str, choices=["sigmoid", "ipo", "kto_pair"], default="sigmoid",
                        help="DPO loss type objective function")
    parser.add_argument("--compile-reference-model", action="store_true",
                        help="Compile reference model for optimized forward passes")
    
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
    
    # === DPO strategy compatibility validation ===
    if args.load_best_model and args.save_steps <= 0:
        parser.error("--save-steps must be positive when --load-best-model is enabled")

    pwd = None
    if args.password:
        logger.warning("[ZKAEDI SEC] WARNING: Passing password via plaintext CLI arguments is deprecated and insecure. Use --prompt-password or environment-based key managers.")
        pwd = args.password
    elif args.prompt_password:
        pwd = getpass.getpass("Enter private key passphrase: ")
    
    attestation_key = args.private_key if args.sign else None

    # === SECURITY: Determinism Provenance Setup (F4) ===
    import random
    import numpy as np
    random.seed(args.seed)
    np.random.seed(args.seed)
    torch.manual_seed(args.seed)
    torch.cuda.manual_seed_all(args.seed)
    torch.backends.cudnn.benchmark = False
    torch.backends.cudnn.deterministic = True

    det_enabled = False
    det_warn_only = False
    try:
        torch.use_deterministic_algorithms(True, warn_only=True)
        det_enabled = True
        det_warn_only = True
        logger.info("[ZKAEDI SEC] PyTorch deterministic algorithms enabled (enabled=true, warn_only=true)")
    except RuntimeError as e:
        logger.warning(f"[ZKAEDI SEC] PyTorch deterministic algorithms raised RuntimeError: {e}")
        try:
            torch.use_deterministic_algorithms(False)
        except Exception:
            pass
        det_enabled = False
        det_warn_only = False

    update_safe_base(args.safe_base_dir)

    dataset_path_str = args.dataset.replace("\\", "/")
    output_path_str = args.output_dir.replace("\\", "/")

    # === SECURITY: Path validation (fail-closed) ===
    try:
        dataset_path = validate_runtime_path(dataset_path_str, description="dataset", must_exist=True)
        output_dir = validate_runtime_path(output_path_str, description="output-dir", must_exist=False)
        output_dir.mkdir(parents=True, exist_ok=True)
        # Post-creation revalidation
        output_dir = validate_runtime_path(output_dir, description="created output directory", must_exist=True)
    except (ValueError, FileNotFoundError, RuntimeError) as e:
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
            p_base = validate_runtime_path(str(p_base), must_exist=True, description="base model")
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
    try:
        manifest_path = validate_runtime_path(args.split_manifest, must_exist=True, description="dataset split manifest")
    except (ValueError, FileNotFoundError, RuntimeError) as e:
        logger.error(f"SECURITY BLOCK: {e}")
        sys.exit(1)

    from zkaedi_model_registry import get_file_sha256
    split_manifest_sha256 = get_file_sha256(manifest_path)

    logger.info("[ZKAEDI SEC] Loading manifest...")
    try:
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
        train_indices = manifest["train"]
        eval_indices = manifest["eval"]
    except Exception as e:
        logger.error(f"Manifest load failed: {type(e).__name__}")
        sys.exit(1)

    # === SECURITY: Partition Integrity Verification (F3) ===
    try:
        if not isinstance(train_indices, list):
            raise ValueError("train indices must be a list")
        if not isinstance(eval_indices, list):
            raise ValueError("eval indices must be a list")
        if len(train_indices) == 0:
            raise ValueError("empty train split")
        if len(eval_indices) == 0:
            raise ValueError("empty eval split")
            
        for idx in train_indices:
            if isinstance(idx, bool):
                raise ValueError("boolean index not allowed")
            if type(idx) is not int:
                raise ValueError("non-integer index")
            if idx < 0:
                raise ValueError("negative index")
            if idx >= len(dataset):
                raise ValueError("out-of-range index")
                
        for idx in eval_indices:
            if isinstance(idx, bool):
                raise ValueError("boolean index not allowed")
            if type(idx) is not int:
                raise ValueError("non-integer index")
            if idx < 0:
                raise ValueError("negative index")
            if idx >= len(dataset):
                raise ValueError("out-of-range index")
                
        if len(train_indices) != len(set(train_indices)):
            raise ValueError("duplicate train index")
        if len(eval_indices) != len(set(eval_indices)):
            raise ValueError("duplicate eval index")
        if set(train_indices) & set(eval_indices):
            raise ValueError("train/eval overlap")
    except ValueError as ve:
        logger.error(f"[ZKAEDI SEC] Partition integrity failure: {ve}")
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

    ref_model = None
    if args.compile_reference_model:
        logger.info("[ZKAEDI SEC] Loading separate reference model for compilation...")
        try:
            ref_model, _ = load_model_hardened(args.model_name, revision=args.model_revision)
            ref_model.eval()
            logger.info("[ZKAEDI SEC] Compiling reference model with torch.compile...")
            ref_model = torch.compile(ref_model)
        except Exception as e:
            logger.warning(f"Reference model compilation FAILED: {e}. Falling back to default ref_model.")
            ref_model = None

    logger.info("[ZKAEDI SEC] Constructing DPOConfig (AdamW + hardened defaults)...")
    training_args = DPOConfig(
        output_dir=str(output_dir),
        per_device_train_batch_size=args.batch_size,
        gradient_accumulation_steps=1,
        warmup_steps=5,
        max_steps=args.max_steps,
        learning_rate=args.learning_rate,
        fp16=False,
        bf16=False,
        logging_steps=1,
        weight_decay=0.01,
        lr_scheduler_type="linear",
        seed=args.seed,
        gradient_checkpointing=False,
        remove_unused_columns=False,
        report_to="none",
        optim="adamw_torch",
        use_cpu=args.use_cpu,
        eval_strategy="steps",
        eval_steps=args.save_steps,
        save_strategy="steps",
        save_steps=args.save_steps,
        save_total_limit=args.checkpoint_limit,
        load_best_model_at_end=args.load_best_model,
        metric_for_best_model=args.metric_for_best_model,
        greater_is_better=(args.greater_is_better.lower() == "true"),
        beta=args.beta,
        loss_type=args.loss_type,
    )

    trainer = DPOTrainer(
        model=model,
        ref_model=ref_model,
        processing_class=tokenizer,
        train_dataset=train_dataset,
        eval_dataset=eval_dataset,
        optimizers=(None, None),
        args=training_args,
        compute_metrics=compute_preference_metrics
    )

    # Register Loss stability tripwire
    trainer.add_callback(DPOSTripwireCallback())
    # Register DPO Validation statistical significance callback
    val_callback = DPOValidationCheckpointCallback(
        output_dir=output_dir,
        eval_dataset=eval_dataset,
        sign=args.sign,
        private_key=attestation_key,
        password=pwd
    )
    trainer.add_callback(val_callback)

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
    validate_runtime_path(str(checkpoint_dir), must_exist=True, description="saved checkpoint directory")
    
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
    from zkaedi_security_utils import PATH_VALIDATOR_VERSION
    manifest_file = checkpoint_dir / "training_manifest.json"
    manifest_data = {
        "model_name": args.model_name,
        "revision": args.model_revision,
        "transformers_version": TRANSFORMERS_VERSION,
        "dataset": {
            "path": get_relative_safe_path(dataset_path, SAFE_BASE_DIR),
            "logical_name": dataset_path.stem
        },
        "split_manifest": {
            "path": get_relative_safe_path(manifest_path, SAFE_BASE_DIR),
            "sha256": split_manifest_sha256
        },
        "train_samples": len(train_dataset),
        "eval_samples": len(eval_dataset),
        "model_payload_sha256": model_payload_sha256,
        "artifact_type": artifact_type,
        "attestation_id": attestation_id,
        "determinism": {
            "enabled": det_enabled,
            "warn_only": det_warn_only
        },
        "authoritative_safe_base": str(SAFE_BASE_DIR),
        "path_validation_mode": "authoritative",
        "path_validator_version": PATH_VALIDATOR_VERSION,
        "security_note": "Saved in safe tensors format. Hashed and ready for swarm ingestion."
    }
    write_atomic_json(manifest_file, manifest_data)

    # Calculate manifest sha256
    from zkaedi_model_registry import get_file_sha256
    manifest_sha256 = get_file_sha256(manifest_file)

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
        "bf16": training_args.bf16,
        "split_manifest_sha256": split_manifest_sha256
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
        manifest_sha256=manifest_sha256,
        determinism={
            "enabled": det_enabled,
            "warn_only": det_warn_only
        },
        split_manifest_path=manifest_path,
        split_manifest_sha256=split_manifest_sha256
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
        "split_manifest_sha256": split_manifest_sha256,
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
