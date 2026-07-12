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
from typing import Optional, Dict, Any, Tuple, List, TYPE_CHECKING
from datetime import datetime, timezone

from zkaedi_security_utils import validate_safe_path

class RegistrySignatureMissingError(ValueError):
    pass

class RegistrySignatureValidationError(ValueError):
    pass

class ModelNotInRegistryError(ValueError):
    pass

class ModelIntegrityMismatchError(ValueError):
    pass

import time

if TYPE_CHECKING:
    from cryptography.hazmat.primitives.asymmetric import ed25519

def is_pid_alive(pid: int) -> bool:
    """Checks if a process with the given PID is currently running."""
    if pid <= 0:
        return False
    import os
    import sys
    try:
        if sys.platform != "win32":
            os.kill(pid, 0)
            return True
        else:
            import ctypes
            PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
            handle = ctypes.windll.kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
            if handle:
                ctypes.windll.kernel32.CloseHandle(handle)
                return True
            return False
    except OSError:
        return False


def validate_generation_value(raw_generation: Any) -> int:
    """Validates and returns the generation as a safe non-negative integer (SEC-22 / SEC-23)."""
    if type(raw_generation) is not int:
        try:
            if isinstance(raw_generation, str):
                raw_generation = raw_generation.strip()
            generation = int(raw_generation)
        except (TypeError, ValueError) as exc:
            raise ValueError(f"Invalid registry generation value: {raw_generation}") from exc
    else:
        generation = raw_generation
        
    if generation < 0:
        raise ValueError(f"Registry generation must be non-negative: {generation}")
    return generation


def _restore_quarantine_no_clobber(quarantine_path: Path, lock_path: Path) -> None:
    """Restores a quarantined lock file back to the active lock_path without clobbering an existing active lock."""
    import os
    try:
        if hasattr(os, "link"):
            try:
                os.link(str(quarantine_path), str(lock_path))
                quarantine_path.unlink(missing_ok=True)
            except FileExistsError:
                # Another owner acquired the active lock; clean up quarantine
                quarantine_path.unlink(missing_ok=True)
        else:
            # Windows/non-POSIX fallback: os.rename raises FileExistsError on Windows if dst exists
            try:
                os.rename(str(quarantine_path), str(lock_path))
            except FileExistsError:
                # Another owner acquired the active lock; clean up quarantine
                quarantine_path.unlink(missing_ok=True)
    except Exception:
        quarantine_path.unlink(missing_ok=True)


def _safe_break_stale_lock(lock_path: Path, expected_token: str) -> bool:
    """Atomically breaks a stale lock using quarantine rename to prevent races (REL-17)."""
    import uuid
    import json
    import os
    if not lock_path.exists():
        return True
    
    quarantine_path = lock_path.with_name(f"write.lock.quar.{uuid.uuid4().hex}")
    try:
        # Atomically move lock file to quarantine path
        os.rename(str(lock_path), str(quarantine_path))
    except Exception:
        # If lock was already renamed, deleted or acquired, we lost the race
        return False
        
    try:
        with open(quarantine_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        if data.get("owner_token") == expected_token:
            quarantine_path.unlink(missing_ok=True)
            return True
        else:
            # Token changed: restore original lock file without clobbering
            _restore_quarantine_no_clobber(quarantine_path, lock_path)
            return False
    except Exception:
        # Restore on any parse failure to be safe without clobbering
        _restore_quarantine_no_clobber(quarantine_path, lock_path)
        return False


class RegistryLock:
    def __init__(self, lock_path: Path, timeout: float = 10.0):
        import uuid
        self.lock_path = lock_path
        self.timeout = timeout
        self.acquired = False
        self.owner_token = uuid.uuid4().hex
        
    def __enter__(self):
        import os
        import socket
        import json
        
        lock_dir = self.lock_path.parent
        lock_dir.mkdir(parents=True, exist_ok=True)
        
        start_time = time.time()
        lease_duration = 60.0  # Safe duration tolerating clock drifts
        
        my_lock_data = {
            "pid": os.getpid(),
            "hostname": socket.gethostname(),
            "owner_token": self.owner_token,
            "acquired_at": time.time(),
            "lease_duration": lease_duration
        }
        
        while time.time() - start_time < self.timeout:
            try:
                # Attempt to create lock exclusively
                fd = os.open(self.lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                try:
                    # Write entire metadata in a single checked os.write system call (REL-15)
                    json_bytes = json.dumps(my_lock_data).encode("utf-8")
                    view = memoryview(json_bytes)
                    while view:
                        written = os.write(fd, view)
                        if written <= 0:
                            raise OSError("Failed to write lock metadata")
                        view = view[written:]
                    os.fsync(fd)
                finally:
                    os.close(fd)
                self.acquired = True
                return self
            except FileExistsError:
                # Check for stale lock
                try:
                    with open(self.lock_path, "r", encoding="utf-8") as f:
                        lock_data = json.load(f)
                    
                    pid = lock_data.get("pid", 0)
                    hostname = lock_data.get("hostname", "")
                    
                    is_same_host = (hostname == socket.gethostname())
                    if is_same_host:
                        # Same host: stale only if the PID is dead (REL-11)
                        is_stale = not is_pid_alive(pid)
                    else:
                        # Cross-host: no automatic reclamation to prevent split-brain theft (REL-16)
                        is_stale = False
                    
                    if is_stale:
                        # Safely break lock using atomic quarantine rename (REL-17)
                        if _safe_break_stale_lock(self.lock_path, lock_data.get("owner_token")):
                            time.sleep(0.02)
                            continue
                except Exception:
                    # Fail closed for corrupt/unparseable lock files. Remove automatic deletion.
                    pass
                
                time.sleep(0.05)
        raise TimeoutError(f"Failed to acquire registry write lock within {self.timeout}s")
        
    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.acquired:
            import json
            try:
                if self.lock_path.exists():
                    with open(self.lock_path, "r", encoding="utf-8") as f:
                        lock_data = json.load(f)
                    if lock_data.get("owner_token") == self.owner_token:
                        self.lock_path.unlink(missing_ok=True)
            except Exception:
                # REL-13: Do not delete lock on read/parse failure
                pass
            self.acquired = False


def get_registry_path() -> Path:
    """Locate the centralized registry file path under validated safe bases."""
    registry_file = Path(__file__).resolve().parent / "zkaedi_model_registry.json"
    return validate_safe_path(str(registry_file), must_exist=False, description="model registry database")


def _get_db_dir() -> Path:
    """Resolve directory-based database location matching registry path."""
    path = get_registry_path()
    if path.suffix == ".json":
        return path.with_name(path.stem)
    return path


def _get_signature_path(registry_path: Path) -> Path:
    """Returns the matching .sig file path for the registry."""
    return registry_path.with_suffix(registry_path.suffix + ".sig")


def load_registry(verify_signature: bool = False, public_key_path: Optional[str | bytes | Any] = None) -> Dict[str, Any]:
    """Loads the model registry database. Optionally verifies its Ed25519 signature."""
    reg_path = get_registry_path()
    db_dir = _get_db_dir()
    
    current_file = db_dir / "current"
    generations_dir = db_dir / "generations"
    
    if not current_file.exists():
        # Fallback migration path from old flat JSON file
        if reg_path.exists() and reg_path.is_file():
            # Perform atomic migration under write lock (REL-10)
            lock_path = db_dir / "write.lock"
            with RegistryLock(lock_path):
                if not current_file.exists():
                    try:
                        with open(reg_path, "r", encoding="utf-8") as f:
                            data = json.load(f)
                            
                        # SEC-23: Legacy generation type and range validation
                        was_injected = False
                        if verify_signature:
                            if not public_key_path:
                                raise ValueError("[ZKAEDI REG] public_key_path is required when verify_signature=True")
                            
                            # SEC-20: require legacy document to contain generation field for signature checks
                            if "generation" not in data:
                                raise ValueError("[ZKAEDI REG] Legacy registry document is missing 'generation' field; signature verification cannot proceed")
                            
                            legacy_gen = data["generation"]
                            if type(legacy_gen) is not int or legacy_gen < 0:
                                raise ValueError("Legacy registry generation must be a non-negative integer")
                            gen_num = legacy_gen
                        else:
                            if "generation" in data:
                                legacy_gen = data["generation"]
                                if type(legacy_gen) is not int or legacy_gen < 0:
                                    raise ValueError("Legacy registry generation must be a non-negative integer")
                                gen_num = legacy_gen
                            else:
                                gen_num = 0
                                data["generation"] = gen_num
                                was_injected = True
                                
                        # If verification requested, verify legacy signature before migration (SEC-18)
                        if verify_signature:
                            legacy_sig = _get_signature_path(reg_path)
                            if not legacy_sig.exists():
                                raise RegistrySignatureMissingError(f"[ZKAEDI REG] Legacy registry signature missing: {legacy_sig}")
                            signature = legacy_sig.read_bytes()
                            if not verify_registry_signature(data, signature, public_key_path):
                                raise RegistrySignatureValidationError("[ZKAEDI REG] Legacy registry signature verification failed!")
                        
                        db_dir.mkdir(parents=True, exist_ok=True)
                        generations_dir.mkdir(parents=True, exist_ok=True)
                        
                        # SEC-22: Validate generation value before constructing filesystem paths
                        gen_num = validate_generation_value(gen_num)
                        gen_json_path = generations_dir / f"{gen_num}.json"
                        gen_sig_path = generations_dir / f"{gen_num}.sig"
                        
                        if was_injected:
                            json_bytes = json.dumps(data, indent=2).encode("utf-8")
                        else:
                            with open(reg_path, "rb") as sf:
                                json_bytes = sf.read()
                            
                        import tempfile
                        import os
                        
                        temp_json = None
                        temp_sig = None
                        try:
                            with tempfile.NamedTemporaryFile("wb", dir=str(generations_dir), delete=False) as tf:
                                temp_json = Path(tf.name)
                                tf.write(json_bytes)
                                tf.flush()
                                os.fsync(tf.fileno())
                                
                            legacy_sig = _get_signature_path(reg_path)
                            if legacy_sig.exists() and verify_signature:
                                with tempfile.NamedTemporaryFile("wb", dir=str(generations_dir), delete=False) as tf:
                                    temp_sig = Path(tf.name)
                                    tf.write(legacy_sig.read_bytes())
                                    tf.flush()
                                    os.fsync(tf.fileno())
                                    
                            os.replace(temp_json, gen_json_path)
                            temp_json = None
                            
                            if temp_sig is not None:
                                os.replace(temp_sig, gen_sig_path)
                                temp_sig = None
                                
                            try:
                                dir_fd = os.open(str(generations_dir), os.O_RDONLY)
                                try:
                                    os.fsync(dir_fd)
                                finally:
                                    os.close(dir_fd)
                            except OSError:
                                pass
                                
                            temp_current = None
                            try:
                                with tempfile.NamedTemporaryFile("w", dir=str(db_dir), delete=False, encoding="utf-8") as tf:
                                    temp_current = Path(tf.name)
                                    tf.write(f"{gen_num}\n")
                                    tf.flush()
                                    os.fsync(tf.fileno())
                                os.replace(temp_current, current_file)
                                temp_current = None
                            finally:
                                if temp_current is not None and temp_current.exists():
                                    temp_current.unlink(missing_ok=True)
                                    
                            try:
                                dir_fd = os.open(str(db_dir), os.O_RDONLY)
                                try:
                                    os.fsync(dir_fd)
                                finally:
                                    os.close(dir_fd)
                            except OSError:
                                pass
                                
                            reg_path.unlink(missing_ok=True)
                            _get_signature_path(reg_path).unlink(missing_ok=True)
                        finally:
                            if temp_json is not None and temp_json.exists():
                                temp_json.unlink(missing_ok=True)
                            if temp_sig is not None and temp_sig.exists():
                                temp_sig.unlink(missing_ok=True)
                                
                        return data
                    except Exception as e:
                        # Fail-closed migration exception: do not swallow legacy migration failures (REL-18 / SEC-21)
                        raise e
                        
        return {"models": {}, "generation": 0}
        
    lock_path = db_dir / "write.lock"
    
    for attempt in range(5):
        try:
            gen_id_str = current_file.read_text(encoding="utf-8").strip()
            # SEC-22: Validate generation value before path construction
            generation = validate_generation_value(gen_id_str)
            gen_json_path = generations_dir / f"{generation}.json"
            gen_sig_path = generations_dir / f"{generation}.sig"
            
            if not gen_json_path.exists():
                raise FileNotFoundError(f"Generation JSON file not found: {gen_json_path}")
                
            with open(gen_json_path, "r", encoding="utf-8") as f:
                data = json.load(f)
                
            if "models" not in data:
                data["models"] = {}
                
            # SEC-19: confirm pointer/generation consistency before signature verification, reject missing generation
            if "generation" not in data:
                raise ValueError("Registry generation document is missing 'generation'")
                
            pointer_generation = generation
            document_generation = validate_generation_value(data["generation"])
            if document_generation != pointer_generation:
                raise ValueError("Registry generation does not match current pointer")
                
            if verify_signature:
                if not public_key_path:
                    raise ValueError("[ZKAEDI REG] public_key_path is required when verify_signature=True")
                if not gen_sig_path.exists():
                    raise RegistrySignatureMissingError(f"[ZKAEDI REG] Signature file not found: {gen_sig_path}")
                with open(gen_sig_path, "rb") as f:
                    signature = f.read()
                if not verify_registry_signature(data, signature, public_key_path):
                    raise RegistrySignatureValidationError("[ZKAEDI REG] Registry signature verification failed!")
                    
            return data
            
        except (json.JSONDecodeError, FileNotFoundError) as e:
            if lock_path.exists() and attempt < 4:
                time.sleep(0.05)
                continue
            raise e
        except Exception as e:
            raise e
            
    return {"models": {}, "generation": 0}


def _save_registry_unlocked(
    data: Dict[str, Any],
    sign: bool = False,
    private_key_path: Optional[str] = None,
    password: Optional[str] = None,
) -> None:
    """Saves the model registry database using immutable generations layout."""
    import tempfile
    import os
    
    db_dir = _get_db_dir()
    db_dir.mkdir(parents=True, exist_ok=True)
    
    generations_dir = db_dir / "generations"
    generations_dir.mkdir(parents=True, exist_ok=True)
    
    current_file = db_dir / "current"
    current_gen = 0
    if current_file.exists():
        try:
            # SEC-22: Validate generation value from current pointer file
            current_gen = validate_generation_value(current_file.read_text(encoding="utf-8").strip())
        except Exception:
            pass
            
    next_gen = current_gen + 1
    # SEC-22: Validate next generation value before use
    next_gen = validate_generation_value(next_gen)
    data["generation"] = next_gen
    
    gen_json_path = generations_dir / f"{next_gen}.json"
    gen_sig_path = generations_dir / f"{next_gen}.sig"
    
    temp_json_path = None
    temp_sig_path = None
    
    try:
        with tempfile.NamedTemporaryFile("w", dir=str(generations_dir), delete=False, encoding="utf-8") as f:
            temp_json_path = Path(f.name)
            json.dump(data, f, indent=2)
            f.flush()
            os.fsync(f.fileno())
            
        if sign:
            if not private_key_path:
                raise ValueError("[ZKAEDI REG] private_key_path is required when sign=True")
            signature = sign_registry(data, private_key_path, password=password)
            with tempfile.NamedTemporaryFile("wb", dir=str(generations_dir), delete=False) as f:
                temp_sig_path = Path(f.name)
                f.write(signature)
                f.flush()
                os.fsync(f.fileno())
                
        os.replace(temp_json_path, gen_json_path)
        temp_json_path = None
        
        if sign:
            os.replace(temp_sig_path, gen_sig_path)
            temp_sig_path = None
        else:
            try:
                gen_sig_path.unlink(missing_ok=True)
            except Exception:
                pass
                
        # Durability fsync of generations directory (REL-14)
        try:
            dir_fd = os.open(str(generations_dir), os.O_RDONLY)
            try:
                os.fsync(dir_fd)
            finally:
                os.close(dir_fd)
        except OSError:
            pass
            
        temp_current_path = None
        try:
            with tempfile.NamedTemporaryFile("w", dir=str(db_dir), delete=False, encoding="utf-8") as f:
                temp_current_path = Path(f.name)
                f.write(f"{next_gen}\n")
                f.flush()
                os.fsync(f.fileno())
            os.replace(temp_current_path, current_file)
            temp_current_path = None
        finally:
            if temp_current_path is not None and temp_current_path.exists():
                temp_current_path.unlink(missing_ok=True)
                
        try:
            get_registry_path().unlink(missing_ok=True)
            _get_signature_path(get_registry_path()).unlink(missing_ok=True)
        except Exception:
            pass
            
        try:
            for filepath in generations_dir.glob("*"):
                try:
                    name_stem = filepath.stem
                    # SEC-22: Validate generation value for filename cleanup safety
                    gen_num = validate_generation_value(name_stem)
                    if gen_num < next_gen - 4:
                        filepath.unlink(missing_ok=True)
                except ValueError:
                    pass
        except Exception:
            pass
            
        # Durability fsync of parent db_dir (REL-14)
        try:
            dir_fd = os.open(str(db_dir), os.O_RDONLY)
            try:
                os.fsync(dir_fd)
            finally:
                os.close(dir_fd)
        except OSError:
            pass
            
    finally:
        if temp_json_path is not None and temp_json_path.exists():
            temp_json_path.unlink(missing_ok=True)
        if temp_sig_path is not None and temp_sig_path.exists():
            temp_sig_path.unlink(missing_ok=True)


def save_registry(
    data: Dict[str, Any],
    sign: bool = False,
    private_key_path: Optional[str] = None,
    password: Optional[str] = None,
) -> None:
    """Saves the model registry database atomically with serializing write lock."""
    db_dir = _get_db_dir()
    lock_path = db_dir / "write.lock"
    with RegistryLock(lock_path):
        _save_registry_unlocked(data, sign=sign, private_key_path=private_key_path, password=password)


# =============================================================================
# CRYPTOGRAPHIC UTILITIES
# =============================================================================

def get_file_sha256(file_path: Path) -> str:
    """Computes the SHA-256 hash of a single file."""
    if hasattr(hashlib, "file_digest"):
        with open(file_path, "rb") as f:
            return hashlib.file_digest(f, "sha256").hexdigest()
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
        
    files_to_hash = []
    for f in sorted(model_path.rglob("*")):
        # Explicitly reject symlinks in release artifacts (SEC-14)
        if f.is_symlink():
            rel_path = f.relative_to(model_path).as_posix()
            raise ValueError(f"Symlink not permitted in release artifact: {rel_path}")
            
        if f.is_file():
            # Exclude provenance metadata itself to prevent circular hashing
            if f.name == "quantization_provenance.json":
                continue
            rel_path = f.relative_to(model_path).as_posix()
            files_to_hash.append((rel_path, f))

    # Sort files_to_hash by relative path to maintain determinism
    files_to_hash.sort(key=lambda x: x[0])

    files_hashes = {}
    from concurrent.futures import ThreadPoolExecutor
    import os

    def hash_single_file(item):
        rel_p, abs_p = item
        return rel_p, get_file_sha256(abs_p)

    max_workers = min(len(files_to_hash), 4)
    if hasattr(os, "cpu_count"):
        cpus = os.cpu_count()
        if cpus:
            max_workers = min(max_workers, cpus)
    max_workers = max(1, max_workers)

    if max_workers > 1:
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            results = executor.map(hash_single_file, files_to_hash)
            for rel_p, f_hash in results:
                files_hashes[rel_p] = f_hash
    else:
        for item in files_to_hash:
            rel_p, f_hash = hash_single_file(item)
            files_hashes[rel_p] = f_hash

    combined_hasher = hashlib.sha256()
    for rel_p, _ in files_to_hash:
        f_hash = files_hashes[rel_p]
        # Deterministic framed encoding including path (REL-05)
        combined_hasher.update(rel_p.encode("utf-8"))
        combined_hasher.update(b"\0")
        combined_hasher.update(f_hash.encode("ascii"))
        combined_hasher.update(b"\n")
            
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

    print("[ZKAEDI REG] Ed25519 keypair generated successfully.")
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


def verify_registry_signature(
    data: Dict[str, Any],
    signature: bytes,
    public_key_path: str | bytes | ed25519.Ed25519PublicKey,
) -> bool:
    """Verifies the registry signature using Ed25519."""
    try:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric import ed25519
        from cryptography.exceptions import InvalidSignature
    except ImportError:
        raise ImportError("[ZKAEDI REG] 'cryptography' package is required for registry verification.")

    canonical_json = json.dumps(data, sort_keys=True, separators=(",", ":")).encode("utf-8")

    if isinstance(public_key_path, ed25519.Ed25519PublicKey):
        public_key = public_key_path
    elif isinstance(public_key_path, bytes):
        public_key = serialization.load_pem_public_key(public_key_path)
    else:
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
    
    if len(files_dict) == 0:
        raise ValueError("Cannot register an empty file set")
        
    total_size = 0
    for rel_path in files_dict:
        file_p = validated_path / rel_path
        size = file_p.stat().st_size
        if size == 0:
            raise ValueError(f"Cannot register a zero-byte file: {rel_path}")
        total_size += size
        
    if total_size == 0:
        raise ValueError("Cannot register an empty file set")
    
    db_dir = _get_db_dir()
    lock_path = db_dir / "write.lock"
    with RegistryLock(lock_path):
        if sign:
            if not private_key_path:
                raise ValueError("[ZKAEDI REG] private_key_path is required when sign=True")
            current_file = db_dir / "current"
            if current_file.exists():
                from cryptography.hazmat.primitives import serialization
                
                priv_path = validate_safe_path(private_key_path, must_exist=True, description="private key path")
                password_bytes = password.encode("utf-8") if password else None
                with open(priv_path, "rb") as f:
                    private_key = serialization.load_pem_private_key(f.read(), password=password_bytes)
                public_key = private_key.public_key()
                
                try:
                    registry_data = load_registry(verify_signature=True, public_key_path=public_key)
                except Exception as e:
                    raise ValueError(f"Refusing to sign over unverified registry state: {e}")
            else:
                registry_data = load_registry()
        else:
            registry_data = load_registry()
            
        # Concurrency generation increment (lost update check)
        initial_gen = registry_data.get("generation", 0)
        registry_data["generation"] = initial_gen + 1
        
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
        _save_registry_unlocked(registry_data, sign=sign, private_key_path=private_key_path, password=password)
    
    print(f"[ZKAEDI REG] Successfully registered model '{model_name}' (Hash: {combined_sha256[:16]}...)")
    return entry


def deregister_model(
    model_name: str,
    sign: bool = False,
    private_key_path: Optional[str] = None,
    password: Optional[str] = None,
) -> bool:
    """Removes a model entry from the registry allow-list."""
    db_dir = _get_db_dir()
    lock_path = db_dir / "write.lock"
    with RegistryLock(lock_path):
        if sign:
            if not private_key_path:
                raise ValueError("[ZKAEDI REG] private_key_path is required when sign=True")
            current_file = db_dir / "current"
            if current_file.exists():
                from cryptography.hazmat.primitives import serialization
                
                priv_path = validate_safe_path(private_key_path, must_exist=True, description="private key path")
                password_bytes = password.encode("utf-8") if password else None
                with open(priv_path, "rb") as f:
                    private_key = serialization.load_pem_private_key(f.read(), password=password_bytes)
                public_key = private_key.public_key()
                
                try:
                    registry_data = load_registry(verify_signature=True, public_key_path=public_key)
                except Exception as e:
                    raise ValueError(f"Refusing to sign over unverified registry state: {e}")
            else:
                registry_data = load_registry()
        else:
            registry_data = load_registry()

        if model_name in registry_data["models"]:
            # Concurrency generation increment
            initial_gen = registry_data.get("generation", 0)
            registry_data["generation"] = initial_gen + 1
            
            del registry_data["models"][model_name]
            _save_registry_unlocked(registry_data, sign=sign, private_key_path=private_key_path, password=password)
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
