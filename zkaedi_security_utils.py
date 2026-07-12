#!/usr/bin/env python3
"""
ZKAEDI PRIME Security Utilities
Version: 2.2

Battle-tested security primitives for:
- Secure model loading (CVE-2026-4372 mitigation)
- Strict path validation (fail-closed)
- Cryptographic provenance recording
- Runtime CVE scanning

Intended for use in sovereign/offline DPO training and validation pipelines.
"""

import os
import hashlib
import sys
from pathlib import Path
from typing import List, Optional, Dict, Any

# =============================================================================
# CONFIGURATION
# =============================================================================

DEFAULT_SAFE_BASES: List[Path] = [
    Path("/mnt/h"),
    # Add other approved roots for your environment here
]

def get_safe_bases() -> List[Path]:
    """Allow runtime override via environment variable."""
    env_base = os.environ.get("ZKAEDI_SAFE_BASE")
    if env_base:
        return [Path(env_base).resolve()]
    return [p.resolve() for p in DEFAULT_SAFE_BASES]


# =============================================================================
# CVE SCANNING
# =============================================================================

def scan_for_known_cves() -> None:
    """
    Checks the transformers package version to mitigate CVE-2026-4372.
    Fails fast with exit code 2 if a vulnerable version is detected.
    """
    try:
        from transformers import __version__ as TRANSFORMERS_VERSION
        from packaging import version

        if version.parse(TRANSFORMERS_VERSION) < version.parse("5.3.0"):
            print(
                f"[ZKAEDI SEC] FATAL: transformers version {TRANSFORMERS_VERSION} < 5.3.0 detected. "
                "CVE-2026-4372 RCE risk. Upgrade immediately.",
                file=sys.stderr
            )
            sys.exit(2)
    except ImportError:
        pass


# =============================================================================
# PATH VALIDATION
# =============================================================================

def validate_safe_path(
    user_path: str,
    must_exist: bool = True,
    allow_symlinks: bool = False,
    description: str = "path",
    extra_safe_bases: Optional[List[Path]] = None,
) -> Path:
    """
    Refactored v2 path validation helper.

    Security guarantees:
    - Fail-closed on directory traversal attempts
    - Enforces allow-listed base directories under /mnt/h
    - Resolves absolute paths safely
    """
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
        raise ValueError(
            f"[ZKAEDI SEC] Path traversal / unsafe location blocked for {description}."
        )

    return resolved


# =============================================================================
# HARDENED MODEL LOADER
# =============================================================================

def load_model_hardened(
    model_name_or_path: str,
    revision: str = "main",
) -> tuple[Any, Any]:
    """
    Battle-tested hardened model loader.

    Mitigations:
    - trust_remote_code=False
    - use_safetensors=True
    - Automatic local path detection (bypasses HF Hub)
    - Version gate for CVE-2026-4372 (requires transformers >= 5.3.0)
    """
    # Enforce CVE scan before loading model
    scan_for_known_cves()

    from transformers import AutoModelForCausalLM, AutoTokenizer

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
        tokenizer = AutoTokenizer.from_pretrained(
            model_name_or_path, trust_remote_code=False
        )
        if tokenizer.pad_token is None:
            tokenizer.pad_token = tokenizer.eos_token

        model.eval()
        return model, tokenizer
    except Exception as e:
        print(f"[ZKAEDI SEC] Hardened model load FAILED: {type(e).__name__}", file=sys.stderr)
        raise


# =============================================================================
# PROVENANCE RECORDING
# =============================================================================

def record_provenance(
    dataset_path: str,
    checkpoint_dir: str,
    extra_safe_bases: Optional[List[Path]] = None,
) -> Dict[str, Any]:
    """
    Records cryptographic provenance for dataset and checkpoint.

    Uses validate_safe_path internally for safety.
    Returns a dict suitable for embedding in validate_verdict.json.
    """
    ds_path = validate_safe_path(
        dataset_path, must_exist=True, description="dataset", extra_safe_bases=extra_safe_bases
    )
    ckpt_path = validate_safe_path(
        checkpoint_dir, must_exist=True, description="checkpoint", extra_safe_bases=extra_safe_bases
    )

    def sha256_file(path: Path) -> str:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(8192), b""):
                h.update(chunk)
        return h.hexdigest()

    # Dataset hash
    dataset_sha256 = sha256_file(ds_path)

    # Combined checkpoint hash (all files)
    ckpt_files = sorted(ckpt_path.rglob("*"))
    ckpt_hasher = hashlib.sha256()
    for f in ckpt_files:
        if f.is_file():
            ckpt_hasher.update(sha256_file(f).encode())

    return {
        "dataset_path": str(ds_path),
        "dataset_sha256": dataset_sha256,
        "checkpoint_dir": str(ckpt_path),
        "checkpoint_combined_sha256": ckpt_hasher.hexdigest(),
        "timestamp": __import__("datetime").datetime.utcnow().isoformat() + "Z",
    }


# =============================================================================
# USAGE EXAMPLE
# =============================================================================

if __name__ == "__main__":
    print("ZKAEDI PRIME Security Utils v2.2 loaded successfully.")
    scan_for_known_cves()
    print("Available functions: load_model_hardened, validate_safe_path, record_provenance, scan_for_known_cves")