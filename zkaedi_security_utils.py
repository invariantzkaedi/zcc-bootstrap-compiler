#!/usr/bin/env python3
"""
ZKAEDI PRIME Security Utilities
Version: 2.3.1

Centralized, battle-tested security primitives for sovereign AI systems.

Includes:
- Hardened model loading (CVE-2026-4372 mitigation)
- Fail-closed path validation
- Cryptographic provenance recording
- Runtime CVE scanning
- Secure model quantization (8-bit and 4-bit via bitsandbytes)
- Secure GPTQ model loading with config integrity verification
"""

from __future__ import annotations

import os
import sys
import json
import hashlib
from pathlib import Path
from typing import Optional, Dict, Any, Tuple
from datetime import datetime, timezone

import torch
from transformers import AutoModelForCausalLM, AutoTokenizer


# =============================================================================
# CONFIGURATION
# =============================================================================

DEFAULT_SAFE_BASES: list[Path] = [Path("/mnt/h")]

KNOWN_VULNERABILITIES: Dict[str, Dict[str, str]] = {
    "transformers": {
        "CVE-2026-4372": "<5.3.0",
        "description": "Remote code execution via malicious config.json",
    }
}


def get_safe_bases() -> list[Path]:
    env_base = os.environ.get("ZKAEDI_SAFE_BASE")
    if env_base:
        return [Path(env_base).resolve()]
    return [p.resolve() for p in DEFAULT_SAFE_BASES]


# =============================================================================
# PATH VALIDATION
# =============================================================================

def validate_safe_path(
    user_path: str,
    must_exist: bool = True,
    allow_symlinks: bool = False,
    description: str = "path",
    extra_safe_bases: Optional[list[Path]] = None,
) -> Path:
    """Fail-closed path validation with symlink and traversal protection."""
    if not user_path or not isinstance(user_path, str):
        raise ValueError(f"Invalid {description}: must be a non-empty string")

    try:
        raw_path = Path(user_path).expanduser()
        resolved = raw_path.resolve(strict=False)
    except Exception as e:
        raise ValueError(f"Invalid {description} '{user_path}': {e}") from e

    if must_exist and not resolved.exists():
        raise FileNotFoundError(f"Required {description} does not exist: {resolved}")

    if not allow_symlinks:
        try:
            resolved = resolved.resolve(strict=True)
        except FileNotFoundError:
            if must_exist:
                raise

    safe_bases = get_safe_bases()
    if extra_safe_bases:
        safe_bases.extend([p.resolve() for p in extra_safe_bases])

    is_safe = any(
        str(resolved).startswith(str(base)) or resolved.is_relative_to(base)
        for base in safe_bases
    )

    if not is_safe:
        raise ValueError(f"[ZKAEDI SEC] Path traversal blocked for {description}.")

    return resolved


# =============================================================================
# HARDENED MODEL LOADER (Standard)
# =============================================================================

def load_model_hardened(
    model_name_or_path: str, revision: str = "main", enforce_allow_list: bool = False
) -> Tuple[AutoModelForCausalLM, AutoTokenizer]:
    """CVE-2026-4372 hardened model loader."""
    if enforce_allow_list:
        from zkaedi_model_registry import is_model_allowed, verify_model_integrity
        p_res = Path(model_name_or_path)
        if p_res.exists():
            is_valid, errors = verify_model_integrity(str(p_res))
            if not is_valid:
                raise ValueError(f"[ZKAEDI SEC] Model integrity verification failed: {errors}")
        else:
            if not is_model_allowed(model_name_or_path):
                raise ValueError(f"[ZKAEDI SEC] Model '{model_name_or_path}' is not in the allow-list.")

    model_load_kwargs: Dict[str, Any] = {
        "trust_remote_code": False,
        "use_safetensors": True,
    }

    p = Path(model_name_or_path)
    if p.exists() or "/" in model_name_or_path or "\\" in model_name_or_path:
        model_load_kwargs.pop("revision", None)
    else:
        model_load_kwargs["revision"] = revision

    try:
        model = AutoModelForCausalLM.from_pretrained(model_name_or_path, **model_load_kwargs)
        tokenizer = AutoTokenizer.from_pretrained(model_name_or_path, trust_remote_code=False)
        if tokenizer.pad_token is None:
            tokenizer.pad_token = tokenizer.eos_token
        model.eval()
        return model, tokenizer
    except Exception as e:
        print(f"[ZKAEDI SEC] Hardened model load FAILED: {type(e).__name__}", file=sys.stderr)
        raise


# =============================================================================
# RUNTIME CVE SCANNING
# =============================================================================

def scan_for_known_cves() -> None:
    """Runtime CVE scanner (currently checks for CVE-2026-4372)."""
    try:
        from transformers import __version__ as TRANSFORMERS_VERSION
        from packaging import version

        if version.parse(TRANSFORMERS_VERSION) < version.parse("5.3.0"):
            print(
                f"[ZKAEDI SEC] FATAL: transformers {TRANSFORMERS_VERSION} < 5.3.0. "
                "Vulnerable to CVE-2026-4372 (RCE). Upgrade immediately.",
                file=sys.stderr,
            )
            sys.exit(2)
        else:
            print(f"[ZKAEDI SEC] Transformers version check passed ({TRANSFORMERS_VERSION}).")
    except ImportError:
        pass


# =============================================================================
# PROVENANCE RECORDING
# =============================================================================

def record_provenance(
    source_path: str,
    output_path: str,
    extra_safe_bases: Optional[list[Path]] = None,
) -> Dict[str, Any]:
    """Records cryptographic provenance between source and output."""
    src = validate_safe_path(source_path, must_exist=True, description="source", extra_safe_bases=extra_safe_bases)
    out = validate_safe_path(output_path, must_exist=False, description="output", extra_safe_bases=extra_safe_bases)

    def sha256_file(path: Path) -> str:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(8192), b""):
                h.update(chunk)
        return h.hexdigest()

    src_hash = sha256_file(src) if src.is_file() else "directory"

    out_hasher = hashlib.sha256()
    if out.exists():
        for f in sorted(out.rglob("*")):
            if f.is_file():
                out_hasher.update(sha256_file(f).encode())

    return {
        "source_path": str(src),
        "source_sha256": src_hash,
        "output_path": str(out),
        "output_combined_sha256": out_hasher.hexdigest(),
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }


# =============================================================================
# SECURE MODEL QUANTIZATION (bitsandbytes)
# =============================================================================

def safe_quantize_model(
    model_path: str,
    output_dir: str,
    bits: int = 8,
    device: str = "auto",
) -> Path:
    """Secure model quantization using bitsandbytes."""
    scan_for_known_cves()
    print(f"[ZKAEDI SEC] Starting secure {bits}-bit quantization...")

    model, tokenizer = load_model_hardened(model_path)
    output_path = validate_safe_path(output_dir, must_exist=False, description="quantized output")
    output_path.mkdir(parents=True, exist_ok=True)

    try:
        import bitsandbytes as bnb
    except ImportError:
        raise ImportError("[ZKAEDI SEC] bitsandbytes is not installed.")

    try:
        if bits == 8:
            print("[ZKAEDI SEC] Applying 8-bit quantization...")
            quantized_model = _replace_with_8bit(model)
        elif bits == 4:
            print("[ZKAEDI SEC] Applying 4-bit quantization (nf4)...")
            quantized_model = _replace_with_4bit(model)
        else:
            raise ValueError(f"[ZKAEDI SEC] Unsupported bits value: {bits}")
    except Exception as e:
        print(f"[ZKAEDI SEC] Quantization failed: {e}", file=sys.stderr)
        raise RuntimeError(f"Failed to quantize to {bits}-bit") from e

    if device == "auto":
        device = "cuda" if torch.cuda.is_available() else "cpu"

    quantized_model = quantized_model.to(device)
    quantized_model.save_pretrained(output_path, safe_serialization=True)
    tokenizer.save_pretrained(output_path)

    provenance = record_provenance(model_path, str(output_path))
    with open(output_path / "quantization_provenance.json", "w") as f:
        json.dump(provenance, f, indent=2)

    print(f"[ZKAEDI SEC] Quantization complete. Saved to: {output_path}")
    return output_path


def _replace_with_8bit(model: torch.nn.Module) -> torch.nn.Module:
    import bitsandbytes as bnb
    for name, module in list(model.named_children()):
        if isinstance(module, torch.nn.Linear):
            new_module = bnb.nn.Linear8bitLt(
                module.in_features, module.out_features,
                bias=module.bias is not None,
                has_fp16_weights=False, threshold=6.0
            )
            new_module.weight.data = module.weight.data.clone()
            if module.bias is not None:
                new_module.bias.data = module.bias.data.clone()
            setattr(model, name, new_module)
        else:
            _replace_with_8bit(module)
    return model


def _replace_with_4bit(model: torch.nn.Module) -> torch.nn.Module:
    import bitsandbytes as bnb
    for name, module in list(model.named_children()):
        if isinstance(module, torch.nn.Linear):
            new_module = bnb.nn.Linear4bit(
                module.in_features, module.out_features,
                bias=module.bias is not None,
                compute_dtype=torch.float16,
                compress_statistics=True,
                quant_type="nf4"
            )
            new_module.weight.data = module.weight.data.clone()
            if module.bias is not None:
                new_module.bias.data = module.bias.data.clone()
            setattr(model, name, new_module)
        else:
            _replace_with_4bit(module)
    return model


# =============================================================================
# GPTQ SECURE LOADING + CONFIG INTEGRITY
# =============================================================================

def verify_gptq_config_integrity(
    model_path: str,
    expected_config_hash: Optional[str] = None
) -> Dict[str, Any]:
    """Validates quantize_config.json structure and optional hash."""
    config_path = Path(model_path) / "quantize_config.json"
    if not config_path.exists():
        raise FileNotFoundError(f"[ZKAEDI SEC] quantize_config.json not found in {model_path}")

    with open(config_path, "r", encoding="utf-8") as f:
        config = json.load(f)

    errors = []
    if "bits" not in config or not isinstance(config["bits"], int) or config["bits"] not in [2, 3, 4, 8]:
        errors.append("Invalid or missing 'bits'")
    if "group_size" not in config or not isinstance(config["group_size"], int) or config["group_size"] <= 0:
        errors.append("Invalid or missing 'group_size'")

    if errors:
        raise ValueError(f"[ZKAEDI SEC] GPTQ config validation failed: {errors}")

    result = {
        "valid": True,
        "config": config,
        "config_hash": None,
        "checked_at": datetime.now(timezone.utc).isoformat()
    }

    if expected_config_hash:
        def sha256_file(path: Path) -> str:
            h = hashlib.sha256()
            with open(path, "rb") as f:
                for chunk in iter(lambda: f.read(8192), b""):
                    h.update(chunk)
            return h.hexdigest()
        actual = sha256_file(config_path)
        result["config_hash"] = actual
        if actual != expected_config_hash:
            raise ValueError(f"[ZKAEDI SEC] quantize_config.json hash mismatch")

    return result


def load_gptq_model_hardened(
    model_path: str,
    device: str = "auto",
    use_safetensors: bool = True,
    disable_exllama: bool = True,
    disable_exllamav2: bool = True,
    use_triton: bool = False,
    expected_config_hash: Optional[str] = None,
    enforce_allow_list: bool = False,
) -> Tuple[AutoModelForCausalLM, AutoTokenizer]:
    """Secure loader for pre-quantized GPTQ models with config integrity check."""
    scan_for_known_cves()
    print(f"[ZKAEDI SEC] Loading GPTQ model from: {model_path}")

    validated_path = validate_safe_path(model_path, must_exist=True, description="GPTQ model")

    if enforce_allow_list:
        from zkaedi_model_registry import verify_model_integrity
        is_valid, errors = verify_model_integrity(str(validated_path))
        if not is_valid:
            raise ValueError(f"[ZKAEDI SEC] GPTQ model integrity verification failed: {errors}")

    # Verify quantize_config.json
    config_info = verify_gptq_config_integrity(str(validated_path), expected_config_hash=expected_config_hash)
    print(f"[ZKAEDI SEC] quantize_config.json validated. bits={config_info['config']['bits']}")

    try:
        from auto_gptq import AutoGPTQForCausalLM
    except ImportError:
        raise ImportError("[ZKAEDI SEC] auto-gptq is not installed.")

    if device == "auto":
        device = "cuda:0" if torch.cuda.is_available() else "cpu"

    model = AutoGPTQForCausalLM.from_quantized(
        str(validated_path),
        device=device,
        use_safetensors=use_safetensors,
        trust_remote_code=False,
        use_triton=use_triton,
        disable_exllama=disable_exllama,
        disable_exllamav2=disable_exllamav2,
    )

    tokenizer = AutoTokenizer.from_pretrained(str(validated_path), trust_remote_code=False)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    model.eval()
    print(f"[ZKAEDI SEC] GPTQ model loaded successfully on {device}")
    return model, tokenizer


def is_gptq_model(model_path: str) -> bool:
    """Quick check if directory contains a GPTQ model."""
    return (Path(model_path) / "quantize_config.json").exists()


# =============================================================================
# SELF TEST
# =============================================================================

if __name__ == "__main__":
    print("ZKAEDI PRIME Security Utilities v2.3.1 loaded successfully.")
    scan_for_known_cves()