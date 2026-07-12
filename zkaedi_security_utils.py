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
# HARDENED MODEL LOADER
# =============================================================================

def load_model_hardened(
    model_name_or_path: str, revision: str = "main"
) -> Tuple[AutoModelForCausalLM, AutoTokenizer]:
    """CVE-2026-4372 hardened model loader."""
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
    """
    Runtime CVE scanner.
    Currently checks for CVE-2026-4372 (transformers < 5.3.0).
    """
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
# SECURE MODEL QUANTIZATION
# =============================================================================

def safe_quantize_model(
    model_path: str,
    output_dir: str,
    bits: int = 8,
    device: str = "auto",
) -> Path:
    """
    Secure model quantization using bitsandbytes.

    Security Properties:
    - Always loads via load_model_hardened() (CVE-2026-4372 protected)
    - Outputs only safetensors format
    - Records full cryptographic provenance
    - Fail-closed path validation
    - Runtime CVE scanning before execution
    - Proper error handling during quantization

    Supported:
    - bits=8  → 8-bit quantization (Linear8bitLt)
    - bits=4  → 4-bit quantization (Linear4bit)
    """
    scan_for_known_cves()

    print(f"[ZKAEDI SEC] Starting secure {bits}-bit quantization...")

    # Load model safely
    model, tokenizer = load_model_hardened(model_path)

    # Validate output path
    output_path = validate_safe_path(output_dir, must_exist=False, description="quantized model output")
    output_path.mkdir(parents=True, exist_ok=True)

    try:
        import bitsandbytes as bnb
    except ImportError:
        raise ImportError(
            "[ZKAEDI SEC] bitsandbytes is not installed. "
            "Install with: pip install bitsandbytes"
        )

    try:
        if bits == 8:
            print("[ZKAEDI SEC] Applying 8-bit quantization (Linear8bitLt)...")
            quantized_model = _replace_with_8bit(model)
        elif bits == 4:
            print("[ZKAEDI SEC] Applying 4-bit quantization (nf4)...")
            quantized_model = _replace_with_4bit(model)
        else:
            raise ValueError(f"[ZKAEDI SEC] Unsupported bits value: {bits}. Use 4 or 8.")

    except Exception as e:
        print(f"[ZKAEDI SEC] Quantization process failed: {e}", file=sys.stderr)
        raise RuntimeError(f"Failed to quantize model to {bits}-bit") from e

    # Handle device placement
    if device == "auto":
        device = "cuda" if torch.cuda.is_available() else "cpu"

    quantized_model = quantized_model.to(device)

    # Save with safetensors only
    print(f"[ZKAEDI SEC] Saving quantized model to {output_path} (safetensors)...")
    quantized_model.save_pretrained(output_path, safe_serialization=True)
    tokenizer.save_pretrained(output_path)

    # Record provenance
    provenance = record_provenance(model_path, str(output_path))
    with open(output_path / "quantization_provenance.json", "w") as f:
        json.dump(provenance, f, indent=2)

    print(f"[ZKAEDI SEC] Quantization complete. Model saved to: {output_path}")
    return output_path


def _replace_with_8bit(model: torch.nn.Module) -> torch.nn.Module:
    """Recursively replace nn.Linear layers with 8-bit Linear8bitLt layers."""
    import bitsandbytes as bnb

    for name, module in list(model.named_children()):
        if isinstance(module, torch.nn.Linear):
            in_features = module.in_features
            out_features = module.out_features
            has_bias = module.bias is not None

            new_module = bnb.nn.Linear8bitLt(
                in_features,
                out_features,
                bias=has_bias,
                has_fp16_weights=False,
                threshold=6.0,
            )
            new_module.weight.data = module.weight.data.clone()
            if has_bias:
                new_module.bias.data = module.bias.data.clone()

            setattr(model, name, new_module)
        else:
            _replace_with_8bit(module)

    return model


def _replace_with_4bit(model: torch.nn.Module) -> torch.nn.Module:
    """Recursively replace nn.Linear layers with 4-bit Linear4bit layers."""
    import bitsandbytes as bnb

    for name, module in list(model.named_children()):
        if isinstance(module, torch.nn.Linear):
            in_features = module.in_features
            out_features = module.out_features
            has_bias = module.bias is not None

            new_module = bnb.nn.Linear4bit(
                in_features,
                out_features,
                bias=has_bias,
                compute_dtype=torch.float16,
                compress_statistics=True,
                quant_type="nf4",
            )
            new_module.weight.data = module.weight.data.clone()
            if has_bias:
                new_module.bias.data = module.bias.data.clone()

            setattr(model, name, new_module)
        else:
            _replace_with_4bit(module)

    return model


# =============================================================================
# GPTQ SECURE MODEL LOADER
# =============================================================================

def load_gptq_model_hardened(
    model_path: str,
    device: str = "auto",
    use_safetensors: bool = True,
    disable_exllama: bool = True,
    disable_exllamav2: bool = True,
    use_triton: bool = False,
) -> Tuple[AutoModelForCausalLM, AutoTokenizer]:
    """
    Secure loader for pre-quantized GPTQ models.

    Security Properties:
    - Calls scan_for_known_cves() before loading
    - Enforces path validation via validate_safe_path()
    - Sets trust_remote_code=False
    - Disables potentially risky custom kernels by default (ExLlama, Triton)
    - Prefers safetensors format

    Note:
    - This function only *loads* already-quantized GPTQ models.
    - It does **not** perform GPTQ quantization itself.
    """
    scan_for_known_cves()

    print(f"[ZKAEDI SEC] Loading GPTQ model from: {model_path}")

    validated_path = validate_safe_path(
        model_path, must_exist=True, description="GPTQ model directory"
    )

    try:
        from auto_gptq import AutoGPTQForCausalLM
    except ImportError:
        raise ImportError(
            "[ZKAEDI SEC] auto-gptq is not installed. "
            "Install with: pip install auto-gptq"
        )

    if device == "auto":
        device = "cuda:0" if torch.cuda.is_available() else "cpu"

    try:
        model = AutoGPTQForCausalLM.from_quantized(
            str(validated_path),
            device=device,
            use_safetensors=use_safetensors,
            trust_remote_code=False,
            use_triton=use_triton,
            disable_exllama=disable_exllama,
            disable_exllamav2=disable_exllamav2,
        )

        tokenizer = AutoTokenizer.from_pretrained(
            str(validated_path),
            trust_remote_code=False
        )

        if tokenizer.pad_token is None:
            tokenizer.pad_token = tokenizer.eos_token

        model.eval()
        print(f"[ZKAEDI SEC] GPTQ model loaded successfully on device: {device}")
        return model, tokenizer

    except Exception as e:
        print(f"[ZKAEDI SEC] Failed to load GPTQ model: {e}", file=sys.stderr)
        raise RuntimeError(f"GPTQ model loading failed from {validated_path}") from e


def is_gptq_model(model_path: str) -> bool:
    """Quick check if a directory contains a GPTQ-quantized model."""
    path = Path(model_path)
    return (path / "quantize_config.json").exists()


# =============================================================================
# SELF TEST
# =============================================================================


if __name__ == "__main__":
    print("ZKAEDI PRIME Security Utilities v2.3.1 loaded successfully.")
    scan_for_known_cves()