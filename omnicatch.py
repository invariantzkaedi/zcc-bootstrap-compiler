#!/usr/bin/env python3
"""
OMNICATCH — deity-tier error interception.

Design thesis: try/except catches *raised* errors. That's mortal tier.
The failures that actually burn you are the ones that never raise:

  TIER 1 (mortal)   : exceptions in the main thread            -> try/except
  TIER 2 (heroic)   : exceptions in threads, tasks, __del__,
                      GC, warnings, signals, hard crashes      -> global hooks
  TIER 3 (deity)    : failures that raise NOTHING —
                      silent no-ops, dead fallback paths,
                      NaN propagation, swallowed exceptions,
                      hung subprocesses, pipe deadlocks,
                      "PASS" claims with no exit code          -> active detection

Every event lands in an append-only JSONL ledger with timestamp,
monotonic clock, thread, and full traceback. Nothing is ever
silently dropped: OMNICATCH's own failures are written to a
last-resort stderr channel.

Usage
-----
    import omnicatch
    omnicatch.ascend()                    # install all global hooks

    @omnicatch.guarded(
        validate=lambda x: isinstance(x, (int, float)),
        post=lambda r: math.isfinite(r),
        retries=3, jitter=(0.05, 0.25),
    )
    def risky(x): ...

    with omnicatch.no_silent_noop("weight_bake", lambda: hash_mesh(m)):
        bake_weights(m)                   # raises if pre/post state identical

    res = omnicatch.run_verified(["blender", "--background", ...],
                                 timeout=120, log="/tmp/gate.log")
    # res carries .exit_code, .log_path, .verdict — or it didn't happen.

Python >= 3.8 (threading.excepthook). Stdlib only.
"""

from __future__ import annotations

import ast
import atexit
import faulthandler
import functools
import inspect
import io
import json
import logging
import math
import os
import random
import signal
import subprocess
import sys
import threading
import time
import traceback
import warnings
from contextlib import contextmanager
from dataclasses import dataclass, field, asdict
from pathlib import Path
from types import TracebackType
from typing import (Any, Callable, Iterable, Optional, Sequence, Tuple,
                    Type, Union)

__all__ = [
    "ascend", "descend", "guarded", "no_silent_noop", "finite",
    "run_verified", "Watchdog", "audit_swallowed_exceptions",
    "ledger_path", "report", "OmniEvent", "SilentNoOpError",
    "NonFiniteError", "PostconditionError", "ValidationError",
    "HangError", "FabricationError", "tolerance_check",
]

# --------------------------------------------------------------------------
# Ledger — append-only JSONL. Every event, no exceptions (pun intended).
# --------------------------------------------------------------------------

_LEDGER_LOCK = threading.Lock()
_LEDGER_PATH = Path(os.environ.get(
    "OMNICATCH_LEDGER",
    Path.home() / ".omnicatch" / "ledger.jsonl"
))
_INSTALLED = False
_PRIOR_HOOKS: dict = {}
_EVENT_COUNTS: dict = {}
_START_MONO = time.monotonic()

log = logging.getLogger("omnicatch")
if not log.handlers:
    _h = logging.StreamHandler(sys.stderr)
    _h.setFormatter(logging.Formatter(
        "%(asctime)s.%(msecs)03d OMNICATCH %(levelname)s %(message)s",
        datefmt="%H:%M:%S"))
    log.addHandler(_h)
    log.setLevel(logging.INFO)


def ledger_path() -> Path:
    return _LEDGER_PATH


@dataclass
class OmniEvent:
    kind: str                      # e.g. "exception", "silent_noop", "hang"
    severity: str                  # "INFO" | "WARN" | "ERROR" | "FATAL"
    message: str
    where: str = ""                # module:func:line or thread name
    traceback_str: str = ""
    data: dict = field(default_factory=dict)
    ts_wall: float = field(default_factory=time.time)
    ts_mono: float = field(default_factory=lambda: time.monotonic() - _START_MONO)
    thread: str = field(default_factory=lambda: threading.current_thread().name)
    pid: int = field(default_factory=os.getpid)


def _emit(ev: OmniEvent) -> None:
    """Write an event to the ledger. Failure here falls back to raw stderr —
    the catcher itself is not allowed to fail silently."""
    _EVENT_COUNTS[ev.kind] = _EVENT_COUNTS.get(ev.kind, 0) + 1
    line = json.dumps(asdict(ev), default=repr, ensure_ascii=False)
    try:
        with _LEDGER_LOCK:
            _LEDGER_PATH.parent.mkdir(parents=True, exist_ok=True)
            with open(_LEDGER_PATH, "a", encoding="utf-8") as f:
                f.write(line + "\n")
    except Exception:  # noqa: BLE001 — last-resort channel, deliberately broad
        sys.__stderr__.write("OMNICATCH-LEDGER-FAIL " + line + "\n")
        sys.__stderr__.write(traceback.format_exc())
    lvl = {"INFO": logging.INFO, "WARN": logging.WARNING,
           "ERROR": logging.ERROR, "FATAL": logging.CRITICAL}.get(
        ev.severity, logging.ERROR)
    log.log(lvl, "[%s] %s%s", ev.kind, ev.message,
            f" @ {ev.where}" if ev.where else "")


def _tb_str(exc: BaseException) -> str:
    return "".join(traceback.format_exception(type(exc), exc, exc.__traceback__))


# --------------------------------------------------------------------------
# Custom exception taxonomy — precise names, precise meanings.
# --------------------------------------------------------------------------

class OmniError(RuntimeError):
    """Base class for failures OMNICATCH itself raises."""


class SilentNoOpError(OmniError):
    """Code ran, raised nothing, and changed nothing. Dead fallback path."""


class NonFiniteError(OmniError):
    """A NaN or Inf escaped a numeric function."""


class PostconditionError(OmniError):
    """Function returned, but its post-condition contract is violated."""


class ValidationError(OmniError):
    """Input rejected before execution."""


class HangError(OmniError):
    """Watchdog deadline exceeded — stacks were dumped to the ledger."""


class FabricationError(OmniError):
    """A result was claimed without a logged exit code. By protocol: fabricated."""


# --------------------------------------------------------------------------
# TIER 2 — global hooks: nothing raised anywhere escapes the ledger.
# --------------------------------------------------------------------------

def ascend(*,
           escalate_warnings: bool = True,
           catch_signals: Sequence[int] = (signal.SIGTERM,),
           faulthandler_log: Optional[Union[str, Path]] = None) -> None:
    """Install every global hook. Idempotent. Call once at program start.

    escalate_warnings : record every warnings.warn() in the ledger.
    catch_signals     : signals to intercept, ledger, then re-raise default for.
    faulthandler_log  : file for segfault/deadlock C-level tracebacks
                        (default: <ledger dir>/faulthandler.log).
    """
    global _INSTALLED
    if _INSTALLED:
        return
    _INSTALLED = True

    # 1. Main-thread uncaught exceptions
    _PRIOR_HOOKS["excepthook"] = sys.excepthook

    def _sys_hook(etype: Type[BaseException], exc: BaseException,
                  tb: Optional[TracebackType]) -> None:
        _emit(OmniEvent("exception", "FATAL", f"{etype.__name__}: {exc}",
                        traceback_str="".join(
                            traceback.format_exception(etype, exc, tb))))
        _PRIOR_HOOKS["excepthook"](etype, exc, tb)

    sys.excepthook = _sys_hook

    # 2. Thread exceptions (silently killed the thread before 3.8; still
    #    invisible to sys.excepthook today)
    _PRIOR_HOOKS["threading"] = threading.excepthook

    def _thread_hook(args) -> None:
        _emit(OmniEvent("thread_exception", "ERROR",
                        f"{args.exc_type.__name__}: {args.exc_value}",
                        where=getattr(args.thread, "name", "?"),
                        traceback_str="".join(traceback.format_exception(
                            args.exc_type, args.exc_value, args.exc_traceback))))
        _PRIOR_HOOKS["threading"](args)

    threading.excepthook = _thread_hook

    # 3. Unraisable exceptions — __del__, GC callbacks, weakref callbacks.
    #    These are printed to stderr and DISCARDED by default. Not here.
    _PRIOR_HOOKS["unraisable"] = sys.unraisablehook

    def _unraisable_hook(args) -> None:
        _emit(OmniEvent("unraisable", "ERROR",
                        f"{type(args.exc_value).__name__}: {args.exc_value} "
                        f"(in {args.object!r})",
                        traceback_str=_tb_str(args.exc_value)
                        if args.exc_value else ""))
        _PRIOR_HOOKS["unraisable"](args)

    sys.unraisablehook = _unraisable_hook

    # 4. asyncio — exceptions in never-awaited / detached tasks vanish
    #    unless the loop's handler is set. Patch loop creation so every
    #    loop made after ascend() reports here.
    try:
        import asyncio

        def _async_handler(loop, context) -> None:
            exc = context.get("exception")
            _emit(OmniEvent("asyncio_exception", "ERROR",
                            context.get("message", "asyncio error"),
                            traceback_str=_tb_str(exc) if exc else "",
                            data={k: repr(v) for k, v in context.items()
                                  if k != "exception"}))

        _orig_new_loop = asyncio.new_event_loop

        def _patched_new_loop(*a, **kw):
            loop = _orig_new_loop(*a, **kw)
            loop.set_exception_handler(_async_handler)
            return loop

        asyncio.new_event_loop = _patched_new_loop
        _PRIOR_HOOKS["asyncio_new_loop"] = _orig_new_loop
        try:  # patch a currently-running loop too, if any
            asyncio.get_running_loop().set_exception_handler(_async_handler)
        except RuntimeError:
            pass
    except ImportError:
        pass

    # 5. Warnings — a DeprecationWarning today is a crash next release.
    if escalate_warnings:
        _PRIOR_HOOKS["showwarning"] = warnings.showwarning

        def _show_warning(message, category, filename, lineno,
                          file=None, line=None) -> None:
            _emit(OmniEvent("warning", "WARN",
                            f"{category.__name__}: {message}",
                            where=f"{filename}:{lineno}"))
            _PRIOR_HOOKS["showwarning"](message, category, filename,
                                        lineno, file, line)

        warnings.showwarning = _show_warning
        warnings.simplefilter("default")   # un-suppress duplicates

    # 6. Signals — record why the process died.
    for sig in catch_signals:
        try:
            prior = signal.getsignal(sig)

            def _sig_handler(signum, frame, _prior=prior):
                _emit(OmniEvent("signal", "FATAL",
                                f"received {signal.Signals(signum).name}",
                                traceback_str="".join(
                                    traceback.format_stack(frame))))
                signal.signal(signum, _prior if callable(_prior)
                              else signal.SIG_DFL)
                os.kill(os.getpid(), signum)

            signal.signal(sig, _sig_handler)
        except (ValueError, OSError):
            pass  # not main thread / unsupported platform: skip, don't die

    # 7. faulthandler — the only thing that speaks after a segfault.
    fh_path = Path(faulthandler_log) if faulthandler_log else \
        _LEDGER_PATH.parent / "faulthandler.log"
    fh_path.parent.mkdir(parents=True, exist_ok=True)
    fh_file = open(fh_path, "a")          # kept open for process lifetime
    faulthandler.enable(file=fh_file, all_threads=True)
    _PRIOR_HOOKS["faulthandler_file"] = fh_file

    # 8. Exit summary
    atexit.register(_exit_report)

    _emit(OmniEvent("ascend", "INFO",
                    f"all hooks installed; ledger={_LEDGER_PATH}"))


def descend() -> None:
    """Uninstall hooks (mainly for tests)."""
    global _INSTALLED
    if not _INSTALLED:
        return
    sys.excepthook = _PRIOR_HOOKS.get("excepthook", sys.__excepthook__)
    threading.excepthook = _PRIOR_HOOKS.get(
        "threading", threading.__excepthook__)
    sys.unraisablehook = _PRIOR_HOOKS.get(
        "unraisable", sys.__unraisablehook__)
    if "showwarning" in _PRIOR_HOOKS:
        warnings.showwarning = _PRIOR_HOOKS["showwarning"]
    if "asyncio_new_loop" in _PRIOR_HOOKS:
        import asyncio
        asyncio.new_event_loop = _PRIOR_HOOKS["asyncio_new_loop"]
    _INSTALLED = False


def _exit_report() -> None:
    _emit(OmniEvent("exit_report", "INFO",
                    "process exiting", data=dict(_EVENT_COUNTS)))


def report() -> dict:
    """Current event counts by kind."""
    return dict(_EVENT_COUNTS)


# --------------------------------------------------------------------------
# TIER 3a — the guarded decorator: validation, retries+jitter, finiteness,
# post-conditions, timing. One decorator, full contract enforcement.
# --------------------------------------------------------------------------

def guarded(*,
            validate: Optional[Callable[..., bool]] = None,
            post: Optional[Callable[[Any], bool]] = None,
            retries: int = 0,
            jitter: Tuple[float, float] = (0.05, 0.5),
            backoff: float = 2.0,
            retry_on: Tuple[Type[BaseException], ...] = (Exception,),
            check_finite: bool = False,
            reraise: bool = True) -> Callable:
    """Contract-enforcing decorator.

    validate     : called with the function's args; False -> ValidationError
                   BEFORE execution. Its exact failure is ledgered.
    post         : called with the return value; False -> PostconditionError.
    retries      : attempts beyond the first, exponential backoff with
                   uniform jitter drawn from `jitter` (thundering-herd safe).
    check_finite : recursively assert no NaN/Inf in numeric returns.
    reraise      : if False, ledger the terminal failure and return None
                   (opt-in ONLY — swallowing is normally the enemy).
    """
    def deco(fn: Callable) -> Callable:
        qual = f"{fn.__module__}:{fn.__qualname__}"

        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            if validate is not None:
                try:
                    ok = bool(validate(*args, **kwargs))
                except Exception as ve:
                    ok = False
                    _emit(OmniEvent("validation", "ERROR",
                                    f"validator itself raised: {ve!r}",
                                    where=qual, traceback_str=_tb_str(ve)))
                if not ok:
                    err = ValidationError(
                        f"{qual}: input validation failed for "
                        f"args={_short(args)} kwargs={_short(kwargs)}")
                    _emit(OmniEvent("validation", "ERROR", str(err),
                                    where=qual))
                    raise err

            attempt, delay = 0, 0.0
            while True:
                t0 = time.perf_counter()
                try:
                    result = fn(*args, **kwargs)
                except retry_on as exc:
                    attempt += 1
                    _emit(OmniEvent(
                        "guarded_failure", "WARN" if attempt <= retries
                        else "ERROR",
                        f"attempt {attempt}/{retries + 1} failed: {exc!r}",
                        where=qual, traceback_str=_tb_str(exc)))
                    if attempt > retries:
                        if reraise:
                            raise
                        return None
                    delay = (delay or 1.0) * backoff
                    time.sleep(delay * random.uniform(*jitter))
                    continue

                dt = time.perf_counter() - t0
                if check_finite:
                    bad = _find_nonfinite(result)
                    if bad is not None:
                        err = NonFiniteError(
                            f"{qual}: non-finite value {bad!r} in return")
                        _emit(OmniEvent("nonfinite", "ERROR", str(err),
                                        where=qual))
                        raise err
                if post is not None and not post(result):
                    err = PostconditionError(
                        f"{qual}: post-condition failed for "
                        f"result={_short(result)}")
                    _emit(OmniEvent("postcondition", "ERROR", str(err),
                                    where=qual))
                    raise err
                _emit(OmniEvent("guarded_ok", "INFO",
                                f"ok in {dt * 1e3:.2f} ms"
                                + (f" after {attempt} retries" if attempt
                                   else ""),
                                where=qual, data={"ms": round(dt * 1e3, 3)}))
                return result
        return wrapper
    return deco


def _short(obj: Any, limit: int = 120) -> str:
    s = repr(obj)
    return s if len(s) <= limit else s[:limit] + "…"


def _find_nonfinite(obj: Any, _depth: int = 0) -> Optional[Any]:
    """Recursively locate the first NaN/Inf in nested containers."""
    if _depth > 6:
        return None
    if isinstance(obj, bool):
        return None
    if isinstance(obj, float):
        return obj if not math.isfinite(obj) else None
    if isinstance(obj, complex):
        return obj if not (math.isfinite(obj.real)
                           and math.isfinite(obj.imag)) else None
    if isinstance(obj, dict):
        obj = obj.values()
    if isinstance(obj, (list, tuple, set, frozenset)) or (
            hasattr(obj, "__iter__") and not isinstance(obj, (str, bytes))):
        try:
            for item in obj:
                bad = _find_nonfinite(item, _depth + 1)
                if bad is not None:
                    return bad
        except TypeError:
            pass
    return None


def finite(check_args: bool = True) -> Callable:
    """Shorthand: @finite() — NaN/Inf in args or return raises immediately."""
    def deco(fn):
        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            if check_args:
                bad = _find_nonfinite((args, kwargs))
                if bad is not None:
                    raise NonFiniteError(
                        f"{fn.__qualname__}: non-finite input {bad!r}")
            result = fn(*args, **kwargs)
            bad = _find_nonfinite(result)
            if bad is not None:
                raise NonFiniteError(
                    f"{fn.__qualname__}: non-finite return {bad!r}")
            return result
        return wrapper
    return deco


def tolerance_check(value: float, expected: float, *,
                    atol: float = 1e-12, rtol: float = 1e-9,
                    label: str = "tolerance") -> float:
    """Precise tolerance gate. Returns |value - expected| on PASS;
    raises PostconditionError with full diagnostics on FAIL. Never
    silently normalizes."""
    if not (math.isfinite(value) and math.isfinite(expected)):
        raise NonFiniteError(f"{label}: non-finite operand "
                             f"value={value!r} expected={expected!r}")
    diff = abs(value - expected)
    bound = atol + rtol * abs(expected)
    if diff > bound:
        err = PostconditionError(
            f"{label}: |{value!r} - {expected!r}| = {diff:.3e} "
            f"> bound {bound:.3e} (atol={atol:.1e}, rtol={rtol:.1e})")
        _emit(OmniEvent("tolerance", "ERROR", str(err)))
        raise err
    _emit(OmniEvent("tolerance", "INFO",
                    f"{label}: diff={diff:.3e} <= bound={bound:.3e}"))
    return diff


# --------------------------------------------------------------------------
# TIER 3b — silent no-op detection. The dead-fallback-path killer.
# A function that runs clean and changes nothing is a red event, not a pass.
# --------------------------------------------------------------------------

@contextmanager
def no_silent_noop(label: str,
                   fingerprint: Callable[[], Any],
                   *, must_change: bool = True):
    """Assert that the wrapped block actually mutated observable state.

    fingerprint : zero-arg callable capturing the relevant state (hash,
                  tuple of values, serialized snapshot...). Called before
                  and after the block.
    must_change : True  -> identical fingerprints raise SilentNoOpError
                  False -> inverted: the block must NOT change state.

    Born from the audit finding of fallback paths producing identical
    pre/post values while reporting success.
    """
    before = fingerprint()
    yield
    after = fingerprint()
    changed = (before != after)
    if must_change and not changed:
        err = SilentNoOpError(
            f"'{label}' ran without error but state fingerprint is "
            f"IDENTICAL pre/post ({_short(before)}). Dead path / silent "
            f"fallback suspected.")
        _emit(OmniEvent("silent_noop", "ERROR", str(err), where=label,
                        data={"fingerprint": _short(before)}))
        raise err
    if not must_change and changed:
        err = SilentNoOpError(
            f"'{label}' was declared side-effect-free but state changed: "
            f"{_short(before)} -> {_short(after)}")
        _emit(OmniEvent("unexpected_mutation", "ERROR", str(err),
                        where=label))
        raise err
    _emit(OmniEvent("noop_check", "INFO",
                    f"'{label}' state-change contract satisfied "
                    f"(changed={changed})", where=label))


# --------------------------------------------------------------------------
# TIER 3c — swallowed-exception audit. Finds the `except: pass` that will
# eat your error six months from now. Static AST scan, zero execution risk.
# --------------------------------------------------------------------------

def audit_swallowed_exceptions(root: Union[str, Path],
                               *, ledger: bool = True) -> list:
    """Scan a source tree for exception-swallowing patterns:

      1. bare `except:` / `except BaseException:` whose body is only
         pass / continue / ... / return-None
      2. `except Exception:` with no raise, no logging call, and no
         name binding used in the body

    Returns a list of {file, line, pattern, snippet} dicts.
    """
    findings = []
    root = Path(root)
    files = [root] if root.is_file() else sorted(root.rglob("*.py"))
    for path in files:
        try:
            tree = ast.parse(path.read_text(encoding="utf-8",
                                            errors="replace"))
        except SyntaxError as se:
            findings.append({"file": str(path), "line": se.lineno or 0,
                             "pattern": "unparseable", "snippet": str(se)})
            continue
        for node in ast.walk(tree):
            if not isinstance(node, ast.ExceptHandler):
                continue
            body_dump = ast.dump(ast.Module(body=node.body,
                                            type_ignores=[]))
            trivial = all(isinstance(s, (ast.Pass, ast.Continue))
                          or (isinstance(s, ast.Return)
                              and (s.value is None
                                   or isinstance(s.value, ast.Constant)
                                   and s.value.value is None))
                          or (isinstance(s, ast.Expr)
                              and isinstance(s.value, ast.Constant))
                          for s in node.body)
            broad = (node.type is None
                     or (isinstance(node.type, ast.Name)
                         and node.type.id in ("BaseException", "Exception")))
            reraises = "Raise(" in body_dump
            logs = any(tok in body_dump for tok in
                       ("'error'", "'warning'", "'exception'", "'critical'",
                        "'log'", "'warn'", "'print'", "'_emit'", "'emit'",
                        "'debug'", "'info'", "'write'"))
            if broad and trivial:
                findings.append({"file": str(path), "line": node.lineno,
                                 "pattern": "broad-except-trivial-body",
                                 "snippet": ast.get_source_segment(
                                     path.read_text(errors="replace"), node,
                                     padded=False)[:160] if hasattr(
                                     ast, "get_source_segment") else ""})
            elif broad and not reraises and not logs:
                findings.append({"file": str(path), "line": node.lineno,
                                 "pattern": "broad-except-no-log-no-raise",
                                 "snippet": ""})
    if ledger:
        _emit(OmniEvent(
            "swallow_audit", "WARN" if findings else "INFO",
            f"{len(findings)} exception-swallowing site(s) under {root}",
            data={"findings": findings[:50]}))
    return findings


# --------------------------------------------------------------------------
# TIER 3d — hang watchdog. If the deadline passes, dump EVERY thread's
# stack to the ledger and (optionally) raise in the owning thread.
# The tool you want pointed at a wedged Blender subprocess driver.
# --------------------------------------------------------------------------

class Watchdog:
    """Deadline monitor with full stack forensics on expiry.

        with Watchdog("blender_bake", deadline=300):
            drive_blender()

    On expiry it ledgers a 'hang' event containing the stack of every
    live thread, then either raises HangError in the owner (via
    signal.SIGALRM if in main thread on POSIX) or hard-exits if
    lethal=True. Heartbeat() resets the clock for long loops.
    """

    def __init__(self, label: str, deadline: float,
                 *, lethal: bool = False, poll: float = 0.25):
        assert deadline > 0, "deadline must be positive seconds"
        self.label, self.deadline = label, float(deadline)
        self.lethal, self.poll = lethal, poll
        self._last_beat = time.monotonic()
        self._stop = threading.Event()
        self._fired = threading.Event()
        self._owner_is_main = (threading.current_thread()
                               is threading.main_thread())
        self._thread = threading.Thread(
            target=self._run, name=f"watchdog:{label}", daemon=True)

    def heartbeat(self) -> None:
        self._last_beat = time.monotonic()

    def _dump_all_stacks(self) -> str:
        buf = io.StringIO()
        frames = sys._current_frames()
        for tid, frame in frames.items():
            name = next((t.name for t in threading.enumerate()
                         if t.ident == tid), f"tid={tid}")
            buf.write(f"\n--- thread {name} ({tid}) ---\n")
            traceback.print_stack(frame, file=buf)
        return buf.getvalue()

    def _run(self) -> None:
        while not self._stop.wait(self.poll):
            if time.monotonic() - self._last_beat > self.deadline:
                stacks = self._dump_all_stacks()
                _emit(OmniEvent(
                    "hang", "FATAL",
                    f"'{self.label}' exceeded {self.deadline}s deadline",
                    where=self.label, traceback_str=stacks))
                self._fired.set()
                if self.lethal:
                    faulthandler.dump_traceback(all_threads=True)
                    os._exit(70)          # EX_SOFTWARE; do not hang forever
                if self._owner_is_main and hasattr(signal, "SIGALRM"):
                    # interrupt a blocked main thread (POSIX only)
                    signal.signal(signal.SIGALRM, self._alarm)
                    signal.setitimer(signal.ITIMER_REAL, 0.001)
                return

    @staticmethod
    def _alarm(signum, frame):
        raise HangError("watchdog deadline exceeded (see ledger for "
                        "all-thread stack dump)")

    def __enter__(self) -> "Watchdog":
        self._thread.start()
        return self

    def __exit__(self, et, ev, tb) -> bool:
        self._stop.set()
        self._thread.join(timeout=2.0)
        if self._fired.is_set() and et is None:
            raise HangError(
                f"'{self.label}' watchdog fired (deadline "
                f"{self.deadline}s); see ledger 'hang' event")
        return False


# --------------------------------------------------------------------------
# TIER 3e — run_verified: subprocess execution with the file-capture
# protocol baked in. No PIPE deadlocks (output goes to a file, never a
# pipe buffer), mandatory timeout, mandatory exit code, mandatory log.
# A result object without an exit code cannot be constructed.
# --------------------------------------------------------------------------

@dataclass(frozen=True)
class VerifiedResult:
    command: str
    exit_code: int
    log_path: str
    duration_s: float
    timed_out: bool
    verdict: str                     # "PASS" | "FAIL" | "HANG"

    @property
    def ok(self) -> bool:
        return self.verdict == "PASS"

    def tail(self, n: int = 40) -> str:
        try:
            lines = Path(self.log_path).read_text(
                errors="replace").splitlines()
            return "\n".join(lines[-n:])
        except OSError as e:
            return f"<log unreadable: {e}>"


def run_verified(cmd: Sequence[str] | str, *,
                 timeout: float,
                 log: Union[str, Path],
                 shell: bool = False,
                 env: Optional[dict] = None,
                 cwd: Optional[Union[str, Path]] = None,
                 kill_grace: float = 5.0,
                 expect_exit: int = 0) -> VerifiedResult:
    """Run a subprocess under full evidence discipline.

    - stdout+stderr -> log FILE (never subprocess.PIPE: a chatty child
      like Blender fills the 64 KiB pipe buffer and deadlocks — output
      to a file cannot deadlock).
    - hard timeout: SIGTERM, grace period, then SIGKILL of the whole
      process group.
    - the numeric exit code and log path are recorded in the ledger and
      embedded in the frozen result. There is no code path that returns
      success without them.
    """
    if timeout <= 0:
        raise ValidationError("run_verified: timeout must be > 0 "
                              "(unbounded subprocesses are how hangs are born)")
    log_p = Path(log)
    log_p.parent.mkdir(parents=True, exist_ok=True)
    cmd_str = cmd if isinstance(cmd, str) else " ".join(map(str, cmd))
    t0 = time.monotonic()
    timed_out = False
    with open(log_p, "w", encoding="utf-8", errors="replace") as lf:
        lf.write(f"# OMNICATCH run_verified\n# CMD: {cmd_str}\n"
                 f"# T0: {time.strftime('%Y-%m-%dT%H:%M:%S%z')}\n\n")
        lf.flush()
        popen_kw: dict = dict(stdout=lf, stderr=subprocess.STDOUT,
                              stdin=subprocess.DEVNULL, shell=shell,
                              env=env, cwd=str(cwd) if cwd else None)
        if os.name == "posix":
            popen_kw["start_new_session"] = True   # own process group
        proc = subprocess.Popen(cmd, **popen_kw)
        try:
            proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            _kill_tree(proc, kill_grace)
    dur = time.monotonic() - t0
    code = proc.returncode if proc.returncode is not None else -1
    with open(log_p, "a", encoding="utf-8") as lf:
        lf.write(f"\n# EXIT:{code}  DURATION:{dur:.3f}s"
                 f"  TIMED_OUT:{timed_out}\n")
    verdict = ("HANG" if timed_out
               else "PASS" if code == expect_exit else "FAIL")
    res = VerifiedResult(cmd_str, code, str(log_p), round(dur, 3),
                         timed_out, verdict)
    _emit(OmniEvent("subprocess", "INFO" if res.ok else "ERROR",
                    f"{verdict} exit={code} in {dur:.2f}s: {cmd_str[:120]}",
                    data=asdict(res)))
    return res


def _kill_tree(proc: subprocess.Popen, grace: float) -> None:
    """SIGTERM the process group, wait `grace`, then SIGKILL."""
    try:
        if os.name == "posix":
            pgid = os.getpgid(proc.pid)
            os.killpg(pgid, signal.SIGTERM)
        else:
            proc.terminate()
        proc.wait(timeout=grace)
    except (subprocess.TimeoutExpired, ProcessLookupError, OSError):
        try:
            if os.name == "posix":
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            else:
                proc.kill()
            proc.wait(timeout=grace)
        except (ProcessLookupError, OSError, subprocess.TimeoutExpired):
            pass


def demand_evidence(claim: str, result: Optional[VerifiedResult]) -> VerifiedResult:
    """Gatekeeper: converting a claim into an accepted result REQUIRES a
    VerifiedResult. None (or anything without an exit code) is FABRICATED
    by protocol and raises."""
    if not isinstance(result, VerifiedResult):
        err = FabricationError(
            f"claim '{claim}' has no VerifiedResult — no command, no exit "
            f"code, no log. Verdict: FABRICATED.")
        _emit(OmniEvent("fabrication", "FATAL", str(err)))
        raise err
    return result


# --------------------------------------------------------------------------
# Self-test note: this module ships with selftest_omnicatch.py, which
# fault-injects every tier and requires each detector to fire. A detector
# that has never been observed firing is UNVERIFIED regardless of how
# clean the code looks. Run:  python3 selftest_omnicatch.py
# --------------------------------------------------------------------------

if __name__ == "__main__":
    print(__doc__)
