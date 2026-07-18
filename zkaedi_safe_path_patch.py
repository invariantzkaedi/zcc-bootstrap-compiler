import os
from pathlib import Path
from typing import Optional, List

DEFAULT_SAFE_BASES = [Path("/mnt/h")]

def get_safe_bases() -> List[Path]:
    env_base = os.environ.get("ZKAEDI_SAFE_BASE")
    if env_base:
        return [Path(env_base).resolve()]
    return [p.resolve() for p in DEFAULT_SAFE_BASES]

def _resolve_authoritative_bases(bases) -> List[Path]:
    if not isinstance(bases, (list, tuple)):
        raise TypeError("authoritative_safe_bases must be a list or tuple")
    if len(bases) == 0:
        raise ValueError("authoritative_safe_bases must not be empty")
    out = []
    for b in bases:
        p = Path(b)
        if not p.is_absolute():
            raise ValueError(f"Authoritative safe base must be absolute: {p}")
        rp = p.resolve(strict=False)
        if not rp.exists():
            raise ValueError(f"Authoritative safe base does not exist: {rp}")
        if not rp.is_dir():
            raise ValueError(f"Authoritative safe base is not a directory: {rp}")
        out.append(rp)
    return out

def _contained(resolved: Path, bases) -> bool:
    for base in bases:
        try:
            resolved.relative_to(base)   # no startswith; component-wise only
            return True
        except ValueError:
            continue
    return False

def validate_safe_path(
    user_path: str,
    must_exist: bool = True,
    allow_symlinks: bool = False,
    description: str = "path",
    extra_safe_bases: Optional[list] = None,
    authoritative_safe_bases: Optional[list] = None,
) -> Path:
    if extra_safe_bases is not None and authoritative_safe_bases is not None:
        raise ValueError("extra_safe_bases and authoritative_safe_bases are mutually exclusive")
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
    if authoritative_safe_bases is not None:
        safe_bases = _resolve_authoritative_bases(authoritative_safe_bases)
    else:
        safe_bases = get_safe_bases()
        if extra_safe_bases:
            safe_bases.extend([Path(p).resolve() for p in extra_safe_bases])
    if not _contained(resolved, safe_bases):
        raise ValueError(f"[ZKAEDI SEC] Path traversal blocked for {description}.")
    return resolved
