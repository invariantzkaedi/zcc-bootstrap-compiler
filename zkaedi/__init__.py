"""
🧿 ZKAEDI import-time patcher.

Demonstrates the three mechanisms behind Unsloth-style "import magic":

  1. BANNER      — __init__.py executes on first import; print there.
  2. EAGER PATCH — monkey-patch already-importable stdlib/third-party
                   attributes at import time (here: subprocess.run and Popen gain
                   automatic exit-code evidence logging with tamper-evident chain).
  3. LAZY PATCH  — a sys.meta_path import hook that patches modules the
                   *moment they are imported later*, even if the user
                   imports them after `import zkaedi`. This is how Unsloth
                   patches transformers/peft without requiring import order.

Usage:
    import zkaedi          # banner prints, patches install
    import subprocess
    subprocess.run(["true"])   # exit code auto-logged to the evidence ledger

Idempotent: importing twice does not double-patch.
"""

from __future__ import annotations

import functools
import hashlib
import importlib.abc
import importlib.machinery
import importlib.util
import json
import logging
import os
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path
from typing import Any

__version__ = "0.2.0"

# --------------------------------------------------------------------------
# Logging: real logger, not print, so downstream code can silence/redirect.
# --------------------------------------------------------------------------
_LOG = logging.getLogger("zkaedi")
if not _LOG.handlers:
    _h = logging.StreamHandler(sys.stderr)
    _h.setFormatter(logging.Formatter("🧿 %(name)s: %(message)s"))
    _LOG.addHandler(_h)
    _LOG.setLevel(os.environ.get("ZKAEDI_LOG_LEVEL", "INFO").upper())

# Default ledger path (portably resolves to the OS temp directory).
_default_ledger = Path(tempfile.gettempdir()) / "zkaedi_ledger.jsonl"
_LEDGER = Path(os.environ.get("ZKAEDI_LEDGER", str(_default_ledger)))

# Idempotency guard — module-level sentinel survives re-import via sys.modules,
# and we also tag patched functions so a forced reload can't double-wrap.
_PATCH_SENTINEL = "_zkaedi_patched"


# --------------------------------------------------------------------------
# Mechanism 1: the banner. Nothing magic — __init__.py IS the import hook.
# --------------------------------------------------------------------------
def _banner() -> None:
    _LOG.info(
        "ZKAEDI %s: Will patch subprocess for exit-code evidence capture. "
        "Ledger: %s",
        __version__,
        _LEDGER,
    )


# --------------------------------------------------------------------------
# Mechanism 2: eager monkey-patch of already-imported module.
# subprocess.run / subprocess.Popen -> wrapped version that appends evidence rows.
# --------------------------------------------------------------------------
def _canon(row: dict[str, Any]) -> str:
    """Canonical serialization — the ONE serializer used by both the write
    side and the verify side. Any drift between the two silently breaks
    chain verification on valid ledgers, so there is exactly one."""
    return json.dumps(row, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


# Chain state: last hash in memory; seeded lazily from the file tail so a
# new process continues an existing chain instead of forking it.
# LIMITATION (by design, not accident): single writer per ledger. Two
# processes appending concurrently WILL fork the chain and verification
# will catch it — that is the correct failure, not something to paper over.
_CHAIN: dict[str, str | None] = {"last": None}
_CHAIN_LOCK = threading.Lock()
_GENESIS = "GENESIS"


def _seed_chain() -> str:
    if _CHAIN["last"] is not None:
        return _CHAIN["last"]
    try:
        tail = _LEDGER.read_text(encoding="utf-8").splitlines()
        _CHAIN["last"] = json.loads(tail[-1])["hash"] if tail else _GENESIS
    except (OSError, KeyError, json.JSONDecodeError, IndexError):
        _CHAIN["last"] = _GENESIS
    return _CHAIN["last"]


def _record(row: dict[str, Any]) -> None:
    """Append one hash-chained evidence row; ledger I/O failure must never
    break the user's call."""
    try:
        with _CHAIN_LOCK:
            prev = _seed_chain()
            digest = hashlib.sha256((prev + _canon(row)).encode("utf-8")).hexdigest()
            out = dict(row, prev=prev, hash=digest)
            _LEDGER.parent.mkdir(parents=True, exist_ok=True)
            with _LEDGER.open("a", encoding="utf-8") as fh:
                fh.write(json.dumps(out, ensure_ascii=False) + "\n")
            _CHAIN["last"] = digest
    except OSError as exc:  # ledger failure must not poison the patched call
        _LOG.warning("ledger write failed (%s); row dropped", exc)


def verify_ledger(path: Path | str | None = None) -> tuple[bool, int | None, str]:
    """Walk the hash chain. Returns (ok, first_bad_line_index, reason).

    Falsifiable by construction: any edited byte in any row body, any
    deleted row, or any forked chain flips ok to False at the exact line.
    """
    p = Path(path) if path is not None else _LEDGER
    try:
        lines = p.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        return (False, None, f"unreadable ledger: {exc}")
    prev = _GENESIS
    for i, line in enumerate(lines):
        try:
            row = json.loads(line)
            claimed_hash = row.pop("hash")
            claimed_prev = row.pop("prev")
        except (json.JSONDecodeError, KeyError) as exc:
            return (False, i, f"malformed row: {exc}")
        if claimed_prev != prev:
            return (False, i, "chain break: prev pointer mismatch")
        expect = hashlib.sha256((prev + _canon(row)).encode("utf-8")).hexdigest()
        if claimed_hash != expect:
            return (False, i, "tamper: row body does not match its hash")
        prev = claimed_hash
    return (True, None, f"chain intact: {len(lines)} rows")


# run() constructs Popen internally — this thread-local marks "inside a
# patched run() call" so the Popen patch skips recording and we get exactly
# one row per logical command, never two.
_IN_RUN = threading.local()


def _patch_subprocess() -> None:
    if getattr(subprocess.run, _PATCH_SENTINEL, False):
        _LOG.debug("subprocess.run already patched; skipping")
        return

    _original_run = subprocess.run

    @functools.wraps(_original_run)
    def run_with_evidence(*args: Any, **kwargs: Any) -> subprocess.CompletedProcess:
        t0 = time.time()
        cmd = args[0] if args else kwargs.get("args")
        _IN_RUN.active = True
        try:
            # Safe string mapping to prevent user-supplied sequence conversion crashes
            try:
                cmd_repr = cmd if isinstance(cmd, str) else list(map(str, cmd or []))
            except Exception:
                cmd_repr = repr(cmd)

            result = _original_run(*args, **kwargs)
            _record(
                {
                    "ts": t0,
                    "via": "run",
                    "cmd": cmd_repr,
                    "exit": result.returncode,
                    "duration_s": round(time.time() - t0, 4),
                    "verdict": "PASS" if result.returncode == 0 else "FAIL",
                }
            )
            return result
        except Exception as exc:
            try:
                cmd_repr = cmd if isinstance(cmd, str) else list(map(str, cmd or []))
            except Exception:
                cmd_repr = repr(cmd)

            _record(
                {
                    "ts": t0,
                    "via": "run",
                    "cmd": cmd_repr,
                    "exit": getattr(exc, "returncode", None),
                    "duration_s": round(time.time() - t0, 4),
                    "verdict": "UNVERIFIED",
                    "error": f"{type(exc).__name__}: {exc}",
                }
            )
            raise  # never swallow — evidence discipline, not error suppression
        finally:
            _IN_RUN.active = False

    run_with_evidence._zkaedi_patched = True  # type: ignore[attr-defined]
    subprocess.run = run_with_evidence  # the actual monkey-patch
    _LOG.info("patched subprocess.run — every exit code now hits the ledger")


def _patch_popen() -> None:
    """Close the escape hatch: check_call()/call() and direct Popen use
    NEVER route through run() (verified against stdlib source), so
    without this, those commands produce zero evidence rows."""
    if getattr(subprocess.Popen, _PATCH_SENTINEL, False):
        _LOG.debug("subprocess.Popen already patched; skipping")
        return

    _OriginalPopen = subprocess.Popen

    class PopenWithEvidence(_OriginalPopen):  # type: ignore[valid-type, misc]
        _zkaedi_patched = True

        def __init__(self, *args: Any, **kwargs: Any) -> None:
            self._zk_t0 = time.time()
            cmd = args[0] if args else kwargs.get("args")
            try:
                self._zk_cmd = cmd if isinstance(cmd, str) else list(map(str, cmd or []))
            except Exception:
                self._zk_cmd = repr(cmd)
            self._zk_recorded = False
            super().__init__(*args, **kwargs)

        def _zk_record(self, returncode: int) -> None:
            if self._zk_recorded or getattr(_IN_RUN, "active", False):
                return  # run() already records its own row
            self._zk_recorded = True
            _record(
                {
                    "ts": self._zk_t0,
                    "via": "Popen",
                    "cmd": self._zk_cmd,
                    "exit": returncode,
                    "duration_s": round(time.time() - self._zk_t0, 4),
                    "verdict": "PASS" if returncode == 0 else "FAIL",
                }
            )

        def wait(self, timeout: float | None = None) -> int:
            rc = super().wait(timeout)
            self._zk_record(rc)
            return rc

        def poll(self) -> int | None:
            rc = super().poll()
            if rc is not None:
                self._zk_record(rc)
            return rc

    PopenWithEvidence.__wrapped__ = _OriginalPopen  # type: ignore[attr-defined]
    subprocess.Popen = PopenWithEvidence  # type: ignore[misc]
    _LOG.info("patched subprocess.Popen — check_call/direct-Popen gap closed")


# --------------------------------------------------------------------------
# Mechanism 3: lazy patch via sys.meta_path — the Unsloth trick for
# patching libraries the user imports AFTER importing us.
# --------------------------------------------------------------------------
# Registry: module name -> callable(module) applied right after exec.
_LAZY_PATCHES: dict[str, Any] = {}


def _patch_random(mod: Any) -> None:
    """Demo lazy patch: force seeded determinism unless explicitly opted out.

    Mirrors Unsloth patching transformers internals on import. Here we wrap
    random.random to log a one-time warning if no seed was set — a
    reproducibility tripwire for harness code.
    """
    if getattr(mod, _PATCH_SENTINEL, False):
        return
    _orig = mod.random
    state = {"warned": False, "seeded": False}
    _orig_seed = mod.seed

    def seed(*a: Any, **k: Any) -> None:
        state["seeded"] = True
        return _orig_seed(*a, **k)

    def tracked_random() -> float:
        if not state["seeded"] and not state["warned"]:
            state["warned"] = True
            _LOG.warning(
                "random.random() called with no seed set — "
                "non-reproducible run (set a seed for harness determinism)"
            )
        return _orig()

    mod.seed = seed
    mod.random = tracked_random
    setattr(mod, _PATCH_SENTINEL, True)
    _LOG.info("lazily patched %s on import", mod.__name__)


_LAZY_PATCHES["random"] = _patch_random


def _patch_colorsys(mod: Any) -> None:
    """Minimal lazy-patch target proving the true meta_path route: colorsys
    is never imported transitively by the stdlib at startup."""
    if getattr(mod, _PATCH_SENTINEL, False):
        return
    setattr(mod, _PATCH_SENTINEL, True)
    setattr(mod, "_zkaedi_route", "meta_path")
    _LOG.info("lazily patched %s on import", mod.__name__)


_LAZY_PATCHES["colorsys"] = _patch_colorsys


class _ZkaediLoaderShim(importlib.abc.Loader):
    """Wraps the real loader; runs our patch after the module executes."""

    def __init__(self, real_loader: importlib.abc.Loader, name: str) -> None:
        self._real = real_loader
        self._name = name

    def create_module(self, spec: importlib.machinery.ModuleSpec) -> Any:
        return self._real.create_module(spec)

    def exec_module(self, module: Any) -> None:
        self._real.exec_module(module)  # real import first
        patch = _LAZY_PATCHES.get(self._name)
        if patch is not None:
            try:
                patch(module)
            except Exception as exc:
                # A broken patch must not break the user's import.
                _LOG.error("lazy patch for %s failed: %s", self._name, exc)


# Re-entry guard for sys.meta_path finder.
_local = threading.local()


class _ZkaediFinder(importlib.abc.MetaPathFinder):
    """Meta-path finder: intercepts targeted imports, defers to normal
    machinery for the actual load, then applies the registered patch."""

    _zkaedi_finder = True  # reload-proof install guard marker

    def find_spec(self, fullname: str, path: Any, target: Any = None) -> Any:
        # Check re-entry guard to prevent infinite recursion on find_spec
        if getattr(_local, "in_find_spec", False):
            return None

        if fullname not in _LAZY_PATCHES:
            return None  # not ours — let the normal finders handle it

        _local.in_find_spec = True
        try:
            spec = importlib.util.find_spec(fullname)
        finally:
            _local.in_find_spec = False

        if spec is None or spec.loader is None:
            return None
        spec.loader = _ZkaediLoaderShim(spec.loader, fullname)
        return spec


_FINDER = _ZkaediFinder()


def _install_finder() -> None:
    # isinstance fails after importlib.reload (new class object), which would
    # stack a second finder — use a class-identity-independent marker instead.
    if any(getattr(f, "_zkaedi_finder", False) for f in sys.meta_path):
        return
    sys.meta_path.insert(0, _FINDER)
    _LOG.info("import hook installed for: %s", ", ".join(_LAZY_PATCHES))
    # If a target module was ALREADY imported before us, patch it now —
    # the hook only sees future imports.
    for name, patch in _LAZY_PATCHES.items():
        if name in sys.modules:
            patch(sys.modules[name])


# --------------------------------------------------------------------------
# Execute on import — this is the entire "magic".
# --------------------------------------------------------------------------
if os.environ.get("ZKAEDI_DISABLE") != "1":
    _banner()
    _patch_subprocess()
    _patch_popen()
    _install_finder()
else:
    _LOG.info("ZKAEDI_DISABLE=1 — patches skipped")
