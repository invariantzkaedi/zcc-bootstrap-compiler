#!/usr/bin/env python3
"""
ZKAEDI PRIME Model Registry & Cryptographic Allow-list
Version: 1.2

Manages registration, allow-list checking, cryptographic validation,
and Ed25519 signature verification for LLM weights and adapters.
"""

from __future__ import annotations

import sys
import json
import hashlib
import argparse
import getpass
from pathlib import Path
from typing import Optional, Dict, Any, Tuple, List
from datetime import datetime, timezone

from zkaedi_security_utils import validate_safe_path


def get_registry_path() -> Path:
    """Locate the centralized registry file path under validated safe bases."""
    registry_file = Path(__file__).resolve().parent / "zkaedi_model_registry.json"
    return validate_safe_path(str(registry_file), must_exist=False, description="model registry database")


def _get_signature_path(registry_path: Path) -> Path:
    """Returns the matching .sig file path for the registry."""
    return registry_path.with_suffix(registry_path.suffix + ".sig")


def load_registry(verify_signature: bool = False, public_key_path: Optional[str] = None) -> Dict[str, Any]:
    """Loads the model registry database. Optionally verifies its Ed25519 signature."""
    reg_path = get_registry_path()
    if not reg_path.exists():
        return {"models": {}}

    try:
        with open(reg_path, "r", encoding="utf-8") as f:
            data = json.load(f)
            if "models" not in data:
                data["models"] = {}
    except Exception:
        # Fail-closed: return empty if corrupted/unreadable
        return {"models": {}}

    if verify_signature:
        if not public_key_path:
            raise ValueError("[ZKAEDI REG] public_key_path is required when verify_signature=True")

        sig_path = _get_signature_path(reg_path)
        if not sig_path.exists():
            raise FileNotFoundError(f"[ZKAEDI REG] Signature file not found: {sig_path}")

        with open(sig_path, "rb") as f:
            signature = f.read()

        if not verify_registry_signature(data, signature, public_key_path):
            raise ValueError("[ZKAEDI REG] Registry signature verification failed!")

        print("[ZKAEDI REG] Registry signature verified successfully.")

    return data


def save_registry(
    data: Dict[str, Any],
    sign: bool = False,
    private_key_path: Optional[str] = None,
    password: Optional[str] = None,
) -> None:
    """Saves the model registry database. Optionally signs it using Ed25519."""
    reg_path = get_registry_path()
    validate_safe_path(str(reg_path.parent), must_exist=True, description="registry parent directory")
    
    with open(reg_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)

    if sign:
        if not private_key_path:
            raise ValueError("[ZKAEDI REG] private_key_path is required when sign=True")
        
        signature = sign_registry(data, private_key_path, password=password)
        sig_path = _get_signature_path(reg_path)
        with open(sig_path, "wb") as f:
            f.write(signature)
        print(f"[ZKAEDI REG] Registry signed and saved to {sig_path}")


# =============================================================================
# CRYPTOGRAPHIC UTILITIES
# =============================================================================

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


# =============================================================================
# REGISTRY SIGNING (Ed25519)
# =============================================================================

def generate_ed25519_keypair(
    private_key_path: str,
    public_key_path: str,
    password: Optional[str] = None,
) -> None:
    """Generates a new Ed25519 keypair for registry signing."""
    try:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric import ed25519
    except ImportError:
        raise ImportError("[ZKAEDI REG] 'cryptography' package is required for keypair generation.")

    private_key = ed25519.Ed25519PrivateKey.generate()
    public_key = private_key.public_key()

    # Validate key save paths
    priv_path = validate_safe_path(private_key_path, must_exist=False, description="private key path")
    pub_path = validate_safe_path(public_key_path, must_exist=False, description="public key path")

    # Encryption configuration
    if password:
        enc_alg = serialization.BestAvailableEncryption(password.encode("utf-8"))
    else:
        enc_alg = serialization.NoEncryption()

    # Save private key
    with open(priv_path, "wb") as f:
        f.write(
            private_key.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.PKCS8,
                encryption_algorithm=enc_alg,
            )
        )

    # Save public key
    with open(pub_path, "wb") as f:
        f.write(
            public_key.public_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PublicFormat.SubjectPublicKeyInfo,
            )
        )

    print(f"[ZKAEDI REG] Ed25519 keypair generated successfully.")
    print(f"Private key: {priv_path}")
    print(f"Public key:  {pub_path}")


def sign_registry(data: Dict[str, Any], private_key_path: str, password: Optional[str] = None) -> bytes:
    """
    Signs the registry data using Ed25519.
    Returns the raw signature bytes.
    """
    try:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric import ed25519
    except ImportError:
        raise ImportError("[ZKAEDI REG] 'cryptography' package is required for registry signing.")

    # Canonical JSON (sorted keys for determinism)
    canonical_json = json.dumps(data, sort_keys=True, separators=(",", ":")).encode("utf-8")

    priv_path = validate_safe_path(private_key_path, must_exist=True, description="private key path")
    
    password_bytes = password.encode("utf-8") if password else None
    with open(priv_path, "rb") as f:
        private_key = serialization.load_pem_private_key(f.read(), password=password_bytes)

    if not isinstance(private_key, ed25519.Ed25519PrivateKey):
        raise ValueError("[ZKAEDI REG] Private key must be an Ed25519 key.")

    return private_key.sign(canonical_json)


def verify_registry_signature(data: Dict[str, Any], signature: bytes, public_key_path: str) -> bool:
    """Verifies the registry signature using Ed25519."""
    try:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric import ed25519
        from cryptography.exceptions import InvalidSignature
    except ImportError:
        raise ImportError("[ZKAEDI REG] 'cryptography' package is required for registry verification.")

    canonical_json = json.dumps(data, sort_keys=True, separators=(",", ":")).encode("utf-8")

    pub_path = validate_safe_path(public_key_path, must_exist=True, description="public key path")
    with open(pub_path, "rb") as f:
        public_key = serialization.load_pem_public_key(f.read())

    if not isinstance(public_key, ed25519.Ed25519PublicKey):
        raise ValueError("[ZKAEDI REG] Public key must be an Ed25519 key.")

    try:
        public_key.verify(signature, canonical_json)
        return True
    except InvalidSignature:
        return False


# =============================================================================
# REGISTRY CORE MANAGEMENT
# =============================================================================

def register_model(
    model_name: str,
    model_path: str,
    author: str = "unknown",
    description: str = "",
    sign: bool = False,
    private_key_path: Optional[str] = None,
    password: Optional[str] = None,
    metadata: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """
    Registers a model directory or file in the allow-list registry.
    
    Calculates cryptographic hashes, records metadata, and optionally signs.
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
        "metadata": metadata or {},
    }
    
    registry_data["models"][model_name] = entry
    save_registry(registry_data, sign=sign, private_key_path=private_key_path, password=password)
    
    print(f"[ZKAEDI REG] Successfully registered model '{model_name}' (Hash: {combined_sha256[:16]}...)")
    return entry


def deregister_model(
    model_name: str,
    sign: bool = False,
    private_key_path: Optional[str] = None,
    password: Optional[str] = None,
) -> bool:
    """Removes a model entry from the registry allow-list."""
    registry_data = load_registry()
    if model_name in registry_data["models"]:
        del registry_data["models"][model_name]
        save_registry(registry_data, sign=sign, private_key_path=private_key_path, password=password)
        print(f"[ZKAEDI REG] Removed model '{model_name}' from the registry.")
        return True
    return False


def list_registered_models(
    verify_signature: bool = False,
    public_key_path: Optional[str] = None,
) -> Dict[str, Dict[str, Any]]:
    """Lists all registered models in the database."""
    return load_registry(verify_signature=verify_signature, public_key_path=public_key_path).get("models", {})


def is_model_allowed(
    model_name_or_hash: str,
    verify_signature: bool = False,
    public_key_path: Optional[str] = None,
) -> bool:
    """Checks if a model's name or combined SHA-256 hash is in the allow-list."""
    if not model_name_or_hash:
        return False
        
    models = list_registered_models(verify_signature=verify_signature, public_key_path=public_key_path)
    if model_name_or_hash in models:
        return True
        
    # Check by combined hash
    for m_info in models.values():
        if m_info.get("combined_sha256") == model_name_or_hash:
            return True
            
    return False


def verify_model_integrity(
    model_path: str,
    verify_signature: bool = False,
    public_key_path: Optional[str] = None,
) -> Tuple[bool, List[str]]:
    """
    Verifies that all files in the model path match their registered hashes.
    
    Returns:
        Tuple: (is_valid, list of verification errors/warnings)
    """
    validated_path = validate_safe_path(model_path, must_exist=True, description="model path to verify")
    
    # Compute current hashes
    current_combined, current_files = get_model_hashes(validated_path)
    
    models = list_registered_models(verify_signature=verify_signature, public_key_path=public_key_path)
    
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


# =============================================================================
# COMMAND LINE INTERFACE (CLI)
# =============================================================================

def main() -> None:
    parser = argparse.ArgumentParser(
        description="ZKAEDI PRIME sovereign model registry command-line utility."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    # Keygen Command
    keygen_parser = subparsers.add_parser("keygen", help="Generate Ed25519 keypair for registry signing")
    keygen_parser.add_argument("--private-key", required=True, help="Path to save private key file")
    keygen_parser.add_argument("--public-key", required=True, help="Path to save public key file")
    keygen_parser.add_argument("--password", help="Passphrase for private key encryption")
    keygen_parser.add_argument("--prompt-password", action="store_true", help="Prompt for private key passphrase")

    # Register Command
    reg_parser = subparsers.add_parser("register", help="Register a model in the allow-list")
    reg_parser.add_argument("--name", required=True, help="Model name identifier")
    reg_parser.add_argument("--path", required=True, help="Path to the model directory or weight file")
    reg_parser.add_argument("--author", default="unknown", help="Author metadata")
    reg_parser.add_argument("--description", default="", help="Description metadata")
    reg_parser.add_argument("--sign", action="store_true", help="Sign the registry after update")
    reg_parser.add_argument("--private-key", help="Path to Ed25519 private key (required for sign)")
    reg_parser.add_argument("--password", help="Passphrase for private key decryption")
    reg_parser.add_argument("--prompt-password", action="store_true", help="Prompt for private key passphrase")

    # Deregister Command
    dereg_parser = subparsers.add_parser("deregister", help="Deregister a model from the allow-list")
    dereg_parser.add_argument("--name", required=True, help="Model name identifier")
    dereg_parser.add_argument("--sign", action="store_true", help="Sign the registry after update")
    dereg_parser.add_argument("--private-key", help="Path to Ed25519 private key (required for sign)")
    dereg_parser.add_argument("--password", help="Passphrase for private key decryption")
    dereg_parser.add_argument("--prompt-password", action="store_true", help="Prompt for private key passphrase")

    # List Command
    subparsers.add_parser("list", help="List all registered models")

    # Verify Command
    ver_parser = subparsers.add_parser("verify", help="Verify a model directory integrity against the registry")
    ver_parser.add_argument("--path", required=True, help="Path to the model directory to verify")
    ver_parser.add_argument("--verify-sig", action="store_true", help="Enforce registry signature verification")
    ver_parser.add_argument("--public-key", help="Path to Ed25519 public key (required for signature verification)")

    args = parser.parse_args()

    # Password extraction helper
    def get_pwd(cli_args, parser_obj) -> Optional[str]:
        if getattr(cli_args, "prompt_password", False):
            return getpass.getpass("Enter private key passphrase: ")
        return getattr(cli_args, "password", None)

    try:
        if args.command == "keygen":
            pwd = get_pwd(args, keygen_parser)
            generate_ed25519_keypair(args.private_key, args.public_key, password=pwd)
            
        elif args.command == "register":
            if args.sign and not args.private_key:
                reg_parser.error("--private-key is required when --sign is set.")
            pwd = get_pwd(args, reg_parser)
            register_model(
                model_name=args.name,
                model_path=args.path,
                author=args.author,
                description=args.description,
                sign=args.sign,
                private_key_path=args.private_key,
                password=pwd
            )
            
        elif args.command == "deregister":
            if args.sign and not args.private_key:
                dereg_parser.error("--private-key is required when --sign is set.")
            pwd = get_pwd(args, dereg_parser)
            deregister_model(
                model_name=args.name,
                sign=args.sign,
                private_key_path=args.private_key,
                password=pwd
            )
            
        elif args.command == "list":
            models = list_registered_models()
            if not models:
                print("Registry is empty.")
            else:
                print(json.dumps(models, indent=2))
                
        elif args.command == "verify":
            if args.verify_sig and not args.public_key:
                ver_parser.error("--public-key is required when --verify-sig is set.")
            valid, errors = verify_model_integrity(
                args.path,
                verify_signature=args.verify_sig,
                public_key_path=args.public_key
            )
            if valid:
                print("[+] Integrity check passed: Model matches registry entry precisely.")
            else:
                print("[-] Integrity verification FAILED:")
                for err in errors:
                    print(f"  - {err}")
                sys.exit(1)
                
    except Exception as e:
        print(f"Error executing command '{args.command}': {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
