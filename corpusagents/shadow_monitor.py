#!/usr/bin/env python3
"""
ZKAEDI Shadow Monitor — Elite Pre/Post Processor                    v1.0.0

Wraps the Bug Hunter corpus assessment pipeline with forensic-grade
Pre-Flight and Post-Flight hooks. Core logic is never mutated.

Architecture:
  Pre-Flight  → environment snap, input geometry, telemetry init
  Core Logic  → bug_hunter.assess_corpus_entry (untouched)
  Post-Flight → crash harvest, performance drift, output validation

Log format: JSONL appended to shadow_monitor.jsonl (append-only, atomic).
All I/O is thread-safe. No silent exception swallowing.

Stdlib only. No third-party deps.
"""
from __future__ import annotations

import base64
import hashlib
import json
import os
import platform
import sys
import threading
import time
import traceback
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Any, Callable, Optional


# ──────────────────────────────────────────────────────────────────────────────
# Crash taxonomy (mirrored from bug_hunter._CRASH_CODES)
# ──────────────────────────────────────────────────────────────────────────────

_CRASH_TAXONOMY: dict[int, tuple[str, str]] = {
    139:        ("SIGSEGV",   "P0"),  # Linux SEGV (signal 11 + 128)
    -11:        ("SIGSEGV",   "P0"),  # Linux SEGV negative form
    0xC0000005: ("AV",        "P0"),  # Windows Access Violation
    134:        ("SIGABRT",   "P0"),  # Linux abort()
    -6:         ("SIGABRT",   "P0"),  # Linux abort() negative form
    135:        ("SIGBUS",    "P0"),  # Linux bus error
    -7:         ("SIGBUS",    "P0"),  # Linux bus error negative form
    124:        ("TIMEOUT",   "P1"),  # Runner timeout sentinel
    127:        ("NOT_FOUND", "P1"),  # Binary not in PATH
    126:        ("OS_ERROR",  "P1"),  # PermissionError / WinError exec failure
    1:          ("GENERIC",   "P2"),  # Non-zero generic failure
}

def _crash_classify(exit_code: int) -> tuple[str, str]:
    """Map exit code to (signal_name, severity). Returns ('OK','—') for clean exit."""
    if exit_code == 0:
        return ("OK", "—")
    return _CRASH_TAXONOMY.get(exit_code, ("NONZERO", "P2"))


# ──────────────────────────────────────────────────────────────────────────────
# JSONL logger — append-only, mutex-guarded, atomic writes
# ──────────────────────────────────────────────────────────────────────────────

_LOG_LOCK = threading.Lock()


class ShadowLogger:
    """
    Thread-safe, append-only JSONL logger.
    Each call to .emit() flushes a single JSON record atomically.
    On OSError/PermissionError, escalates immediately as MONITOR-FATAL.
    """

    def __init__(self, path: Path) -> None:
        self.path = path
        self.path.parent.mkdir(parents=True, exist_ok=True)
        # Probe writability immediately — fail loud before any real work
        try:
            with self.path.open("a", encoding="utf-8") as _f:
                pass
        except OSError as e:
            print(f"[MONITOR-FATAL] Cannot open log path {self.path}: {e}", file=sys.stderr)
            sys.exit(1)

    def emit(self, event: str, level: str, payload: dict[str, Any]) -> None:
        record = {
            "ts":      time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "ts_ns":   time.perf_counter_ns(),
            "event":   event,
            "level":   level,   # INFO | WARN | ERROR | FATAL | P0 | P1
            "pid":     os.getpid(),
            "thread":  threading.current_thread().name,
            **payload,
        }
        line = json.dumps(record, default=str) + "\n"
        with _LOG_LOCK:
            try:
                with self.path.open("a", encoding="utf-8") as f:
                    f.write(line)
                    f.flush()
                    os.fsync(f.fileno())   # atomic: survives power-loss during runs
            except OSError as e:
                # Cannot swallow this — print FATAL and let caller decide
                print(f"[MONITOR-FATAL] Log write failed ({self.path}): {e}", file=sys.stderr)


# ──────────────────────────────────────────────────────────────────────────────
# Pre-Flight Hook
# ──────────────────────────────────────────────────────────────────────────────

# Performance envelope (ms). Exceeding this triggers a WARN.
# Calibrated from v2.0.0 baseline corpus runs (~4-6s per entry via WSL).
_ENVELOPE_MS: float = 15_000.0


@dataclass
class PreFlightContext:
    entry_name:   str
    source_sha256: str
    source_len:   int
    start_ns:     int       # time.perf_counter_ns() at gate entry
    cwd:          str
    platform:     str
    env_overrides: dict
    binary_shas:  dict[str, str]   # compiler path → SHA256 (if accessible)


def _sha256_file(p: Path) -> str:
    """SHA256 of a binary on disk. Returns '(not_found)' if inaccessible."""
    try:
        return hashlib.sha256(p.read_bytes()).hexdigest()
    except OSError:
        return "(not_found)"


def pre_flight(
    entry: dict,
    env_overrides: dict,
    logger: ShadowLogger,
) -> PreFlightContext:
    """
    Pre-Flight Hook: intercept input state BEFORE assess_corpus_entry() runs.
    Validates input geometry, snaps environment, inits telemetry.
    Raises ValueError immediately if input is malformed (fail-loud).
    """
    name = entry.get("name", "(unnamed)")

    # ── Input Geometry ──────────────────────────────────────────────────────
    raw_b64 = entry.get("source_base64", "")
    if not raw_b64:
        logger.emit("PRE_FLIGHT_FAIL", "FATAL", {
            "entry": name,
            "reason": "source_base64 is empty or missing — cannot hash input",
        })
        raise ValueError(f"[Shadow Monitor] Corpus entry '{name}': source_base64 is empty.")

    try:
        raw = base64.b64decode(raw_b64)
    except Exception as e:
        logger.emit("PRE_FLIGHT_FAIL", "FATAL", {
            "entry": name,
            "reason": f"source_base64 decode failure: {e}",
        })
        raise ValueError(f"[Shadow Monitor] Corpus entry '{name}': base64 decode failed: {e}") from e

    source_sha = hashlib.sha256(raw).hexdigest()
    source_len  = len(raw)

    # Applicability envelope: source > 64KB is unusual for a minimized repro
    if source_len > 65_536:
        logger.emit("PRE_FLIGHT_WARN", "WARN", {
            "entry": name,
            "source_sha256": source_sha,
            "source_len": source_len,
            "reason": f"source is {source_len}B — exceeds 64KB minimized-repro envelope",
        })

    # ── Environment Snap ────────────────────────────────────────────────────
    compiler_map: dict = env_overrides.get("compiler_map", {})
    binary_shas: dict[str, str] = {}
    for logical_name, argv_prefix in compiler_map.items():
        # argv_prefix is e.g. ["wsl", "--", "/mnt/c/.../zcc"]
        # The last element is the actual binary path
        if argv_prefix:
            bin_path_str = argv_prefix[-1]
            if bin_path_str.startswith("/mnt/"):
                # WSL path — convert back to Windows to probe SHA
                parts = bin_path_str[5:].split("/", 1)  # strip /mnt/
                if len(parts) == 2:
                    win_path = Path(f"{parts[0].upper()}:/{parts[1]}")
                    binary_shas[logical_name] = _sha256_file(win_path)
                else:
                    binary_shas[logical_name] = "(wsl_path_unresolvable)"
            else:
                binary_shas[logical_name] = _sha256_file(Path(bin_path_str))

    ctx = PreFlightContext(
        entry_name=name,
        source_sha256=source_sha,
        source_len=source_len,
        start_ns=time.perf_counter_ns(),
        cwd=os.getcwd(),
        platform=f"{platform.system()} {platform.release()} {platform.machine()}",
        env_overrides=env_overrides,
        binary_shas=binary_shas,
    )

    logger.emit("PRE_FLIGHT_OK", "INFO", {
        "entry":          ctx.entry_name,
        "source_sha256":  ctx.source_sha256,
        "source_len":     ctx.source_len,
        "cwd":            ctx.cwd,
        "platform":       ctx.platform,
        "binary_shas":    ctx.binary_shas,
        "env_compiler_map": list(compiler_map.keys()),
    })

    return ctx


# ──────────────────────────────────────────────────────────────────────────────
# Post-Flight Hook
# ──────────────────────────────────────────────────────────────────────────────

def post_flight(
    ctx: PreFlightContext,
    finding,                  # bug_hunter.Finding — typed as Any to avoid circular import
    exc: Optional[BaseException],
    logger: ShadowLogger,
    workdir: Path,
) -> None:
    """
    Post-Flight Hook: audit AFTER assess_corpus_entry() returns (or crashes).
    Performs crash harvest, performance drift check, and output validation.
    """
    end_ns  = time.perf_counter_ns()
    elapsed_ms = (end_ns - ctx.start_ns) / 1_000_000.0

    # ── Crash Harvesting (unhandled exception path) ─────────────────────────
    if exc is not None:
        tb = traceback.format_exception(type(exc), exc, exc.__traceback__)
        logger.emit("POST_FLIGHT_CRASH", "FATAL", {
            "entry":       ctx.entry_name,
            "duration_ms": elapsed_ms,
            "exception":   type(exc).__name__,
            "message":     str(exc),
            "traceback":   "".join(tb),
        })
        return

    # ── Performance Drift ───────────────────────────────────────────────────
    drift_level = "INFO"
    drift_note  = ""
    if elapsed_ms > _ENVELOPE_MS:
        drift_level = "WARN"
        drift_note  = (
            f"duration {elapsed_ms:.1f}ms exceeds envelope {_ENVELOPE_MS:.0f}ms "
            f"— possible WSL cold-start or compiler hang"
        )

    # ── Crash taxonomy from Finding's actual subprocess results ────────────
    tgt_exit = finding.target_run.get("exit_code", 0)
    ref_exit = finding.reference_run.get("exit_code", 0)
    tgt_signal, tgt_sev = _crash_classify(tgt_exit)
    ref_signal, ref_sev = _crash_classify(ref_exit)

    # ── Output Validation: physical side-effects ────────────────────────────
    artifacts_found: list[str] = []
    artifacts_missing: list[str] = []
    for suffix in (".target", ".reference", f".c"):
        candidate = workdir / f"{ctx.entry_name}{suffix}"
        if candidate.exists():
            artifacts_found.append(str(candidate))
        else:
            artifacts_missing.append(str(candidate))

    # Emit composite post-flight record
    level = tgt_sev if tgt_sev in ("P0", "P1") else drift_level
    logger.emit("POST_FLIGHT_OK", level, {
        "entry":              ctx.entry_name,
        "source_sha256":      ctx.source_sha256,
        "duration_ms":        round(elapsed_ms, 3),
        "envelope_ms":        _ENVELOPE_MS,
        "drift_note":         drift_note,
        "ship_decision":      finding.ship_decision,
        "status":             finding.status,
        "severity":           finding.severity,
        "diffs":              len(finding.diffs),
        "target_exit":        tgt_exit,
        "target_signal":      tgt_signal,
        "target_severity":    tgt_sev,
        "reference_exit":     ref_exit,
        "reference_signal":   ref_signal,
        "reference_severity": ref_sev,
        "artifacts_found":    artifacts_found,
        "artifacts_missing":  artifacts_missing,
    })

    # Escalate P0 crashes to stderr immediately (no lock needed — level check is atomic read)
    if tgt_sev == "P0":
        import sys as _sys
        print(
            f"[MONITOR-P0] {ctx.entry_name}: {tgt_signal} (exit={tgt_exit}) "
            f"in {elapsed_ms:.1f}ms — auto-escalated",
            file=_sys.stderr,
        )


# ──────────────────────────────────────────────────────────────────────────────
# Wrapper: monkeypatches assess_corpus_entry in-process
# ──────────────────────────────────────────────────────────────────────────────

def install(
    log_path: Path = Path("./tmp_bh/shadow_monitor.jsonl"),
    envelope_ms: float = _ENVELOPE_MS,
) -> ShadowLogger:
    """
    Install the Shadow Monitor around bug_hunter.assess_corpus_entry.

    Call this ONCE before cmd_run() executes. It wraps the function
    in-place using a closure — no source mutation.

    Returns the logger instance for external use.
    """
    global _ENVELOPE_MS
    _ENVELOPE_MS = envelope_ms

    import bug_hunter as _bh

    logger = ShadowLogger(log_path)
    _original = _bh.assess_corpus_entry

    def _monitored_assess(
        entry: dict,
        workdir: Path,
        timeout: int = 30,
        env: Optional[dict] = None,
    ):
        # ── Pre-Flight ──────────────────────────────────────────────────────
        try:
            ctx = pre_flight(entry, env or {}, logger)
        except ValueError as e:
            # Pre-flight hard fail: re-raise so the worker surfaces it properly
            raise

        # ── Core Logic (untouched) ─────────────────────────────────────────
        finding = None
        exc: Optional[BaseException] = None
        try:
            finding = _original(entry, workdir, timeout=timeout, env=env)
        except Exception as e:
            exc = e

        # ── Post-Flight ─────────────────────────────────────────────────────
        if finding is not None:
            post_flight(ctx, finding, exc=None, logger=logger, workdir=workdir)
        else:
            # Core logic raised — construct a minimal stub for autopsy
            class _CrashStub:
                ship_decision = "crash"
                status        = "confirmed"
                severity      = "P0"
                diffs         = ["[MONITOR] Core raised unhandled exception"]
                target_run    = {"exit_code": -1}
                reference_run = {"exit_code": -1}

            post_flight(ctx, _CrashStub(), exc=exc, logger=logger, workdir=workdir)
            raise  # re-raise so ThreadPoolExecutor surfaces it via fut.result()

        return finding

    _bh.assess_corpus_entry = _monitored_assess
    logger.emit("MONITOR_INSTALLED", "INFO", {
        "wrapping":    "bug_hunter.assess_corpus_entry",
        "log_path":    str(log_path.resolve()),
        "envelope_ms": _ENVELOPE_MS,
        "stdlib_only": True,
    })
    return logger


# ──────────────────────────────────────────────────────────────────────────────
# CLI entry point: python shadow_monitor.py run corpus/ [bug_hunter args...]
# ──────────────────────────────────────────────────────────────────────────────

def main(argv: Optional[list[str]] = None) -> int:
    import argparse
    p = argparse.ArgumentParser(
        prog="shadow_monitor",
        description="ZKAEDI Shadow Monitor — Elite Pre/Post Processor for bug_hunter.",
    )
    p.add_argument("--log",      default="./tmp_bh/shadow_monitor.jsonl",
                   help="JSONL log output path (append-only).")
    p.add_argument("--envelope", type=float, default=_ENVELOPE_MS,
                   help=f"Performance envelope in ms. Default: {_ENVELOPE_MS:.0f}ms.")
    p.add_argument("passthrough", nargs=argparse.REMAINDER,
                   help="All remaining args are forwarded to bug_hunter.main().")
    args = p.parse_args(argv)

    logger = install(
        log_path=Path(args.log),
        envelope_ms=args.envelope,
    )

    # Forward remainder to bug_hunter CLI
    import bug_hunter as _bh
    rc = _bh.main(args.passthrough or None)
    logger.emit("RUN_COMPLETE", "INFO", {"exit_code": rc})
    return rc


if __name__ == "__main__":
    import sys
    sys.exit(main())
