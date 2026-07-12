#!/usr/bin/env python3
"""
ZKAEDI PRIME Model Registry & Cryptographic Allow-list
Version: 1.0

Manages registration, allow-list checking, and cryptographic validation
for LLM weights and adapters in the sovereign ZKAEDI PRIME swarm.
"""

from __future__ import annotations

import json
import hashlib
from pathlib import Path
from typing import Optional, Dict, Any, Tuple, List
from datetime import datetime, timezone

from zkaedi_security_utils import validate_safe_path


def get_registry_path() -> Path:
    """Locate the centralized registry file path under validated safe bases."""
    # Place registry in the same folder as this module, validated as safe
    registry_file = Path(__file__).resolve().parent / "zkaedi_model_registry.json"
    return validate_safe_path(str(registry_file), must_exist=False, description="model registry database")


def load_registry() -> Dict[str, Any]:
    """Loads the model registry database."""
    reg_path = get_registry_path()
    if not reg_path.exists():
        return {"models": {}}

    try:
        with open(reg_path, "r", encoding="utf-8") as f:
            data = json.load(f)
            if "models" not in data:
                data["models"] = {}
            return data
    except Exception:
        # Fail-closed: return empty if corrupted/unreadable
        return {"models": {}}


def save_registry(data: Dict[str, Any]) -> None:
    """Saves the model registry database."""
    reg_path = get_registry_path()
    # Ensure parent directory is validated and exists
    validate_safe_path(str(reg_path.parent), must_exist=True, description="registry parent directory")
    
    with open(reg_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


def get_file_sha256(file_path: Path) -> str:
    """Computes the SHA-256 hash of a single file."""
    h = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


def get_model_hashes(model_path: Path) -> Tuple[str, Dict[str, str]]:
    """
    Computes hashes for a model directory or a single model file.
    
    Returns:
        Tuple of (combined_sha256, files_dict)
    """
    if model_path.is_file():
        file_hash = get_file_sha256(model_path)
        return file_hash, {model_path.name: file_hash}
        
    files_hashes = {}
    combined_hasher = hashlib.sha256()
    for f in sorted(model_path.rglob("*")):
        if f.is_file():
            # Exclude provenance metadata itself to prevent circular hashing
            if f.name == "quantization_provenance.json":
                continue
            rel_path = f.relative_to(model_path).as_posix()
            f_hash = get_file_sha256(f)
            files_hashes[rel_path] = f_hash
            combined_hasher.update(f_hash.encode())
            
    return combined_hasher.hexdigest(), files_hashes


def register_model(
    model_name: str,
    model_path: str,
    author: str = "unknown",
    description: str = "",
) -> Dict[str, Any]:
    """
    Registers a model directory or file in the allow-list registry.
    
    Calculates cryptographic hashes and records metadata.
    """
    if not model_name or not isinstance(model_name, str):
        raise ValueError("model_name must be a non-empty string")
        
    validated_path = validate_safe_path(model_path, must_exist=True, description="model path to register")
    
    print(f"[ZKAEDI REG] Hashing model assets at: {validated_path}")
    combined_sha256, files_dict = get_model_hashes(validated_path)
    
    registry_data = load_registry()
    
    entry = {
        "name": model_name,
        "path": str(validated_path),
        "author": author,
        "description": description,
        "combined_sha256": combined_sha256,
        "files": files_dict,
        "registered_at": datetime.now(timezone.utc).isoformat(),
    }
    
    registry_data["models"][model_name] = entry
    save_registry(registry_data)
    
    print(f"[ZKAEDI REG] Successfully registered model '{model_name}' (Hash: {combined_sha256[:16]}...)")
    return entry


def deregister_model(model_name: str) -> bool:
    """Removes a model entry from the registry allow-list."""
    registry_data = load_registry()
    if model_name in registry_data["models"]:
        del registry_data["models"][model_name]
        save_registry(registry_data)
        print(f"[ZKAEDI REG] Removed model '{model_name}' from the registry.")
        return True
    return False


def list_registered_models() -> Dict[str, Dict[str, Any]]:
    """Lists all registered models in the database."""
    return load_registry().get("models", {})


def is_model_allowed(model_name_or_hash: str) -> bool:
    """
    Checks if a model's name or its combined SHA-256 hash is in the allow-list.
    """
    if not model_name_or_hash:
        return False
        
    models = list_registered_models()
    if model_name_or_hash in models:
        return True
        
    # Check by combined hash
    for m_info in models.values():
        if m_info.get("combined_sha256") == model_name_or_hash:
            return True
            
    return False


def verify_model_integrity(model_path: str) -> Tuple[bool, List[str]]:
    """
    Verifies that all files in the model path match their registered hashes.
    
    Returns:
        Tuple: (is_valid, list of verification errors/warnings)
    """
    validated_path = validate_safe_path(model_path, must_exist=True, description="model path to verify")
    
    # Compute current hashes
    current_combined, current_files = get_model_hashes(validated_path)
    
    models = list_registered_models()
    
    # Try to find corresponding entry by path or combined hash
    matched_entry: Optional[Dict[str, Any]] = None
    for entry in models.values():
        if entry.get("combined_sha256") == current_combined or entry.get("path") == str(validated_path):
            matched_entry = entry
            break
            
    if not matched_entry:
        return False, [f"No registry entry found matching path '{validated_path}' or hash '{current_combined}'"]
        
    errors = []
    registered_files = matched_entry.get("files", {})
    
    # Check for missing or tampered files
    for rel_path, expected_hash in registered_files.items():
        actual_file_path = validated_path / rel_path
        if not actual_file_path.exists():
            errors.append(f"Missing file: {rel_path}")
            continue
            
        actual_hash = get_file_sha256(actual_file_path)
        if actual_hash != expected_hash:
            errors.append(f"Hash mismatch for {rel_path}. Expected: {expected_hash}, Actual: {actual_hash}")
            
    # Check for untracked files
    for rel_path in current_files.keys():
        if rel_path not in registered_files:
            errors.append(f"Untracked file found in model directory: {rel_path}")
            
    return len(errors) == 0, errors
