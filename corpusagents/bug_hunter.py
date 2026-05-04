#!/usr/bin/env python3
"""
ZKAEDI Bug Hunter — Supercharged Error Management Squad runner.   v2.0.0

Operationalizes the doctrine:
  - Every bug is an invariant violation until proven otherwise.
  - Every error path is part of the product.
  - Every recovery path must be deterministic, observable, safe.
  - Every production push requires evidence.

v2.0.0 adds:
  - Parallel execution matrix via concurrent.futures.ThreadPoolExecutor.
  - Forensic semantic diffing via difflib.unified_diff (line-level deltas).
  - Execution sandboxing: process-group kill on timeout (POSIX) + Windows
    job-kill fallback. SEC-01/T6 hardened.
  - Segmentation fault harvesting: exit 139 / 0xC0000005 auto-promoted to P0.
  - Agentic LLM hand-off hooks: --auto-triage generates CausalTracer /
    ReferenceJudge system prompt payloads for every divergent finding.

Backward compatible with v1.0/v1.1 corpus entries.
No third-party deps. Stdlib only.
"""
from __future__ import annotations

import argparse
import base64
import concurrent.futures
import difflib
import functools
import hashlib
import json
import os
import shlex
import signal
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, asdict, field
from pathlib import Path
from typing import Any, Optional


__version__ = "2.0.0"

# ──────────────────────────────────────────────────────────────────────────────
# Segfault exit codes (Linux SIGSEGV=139, Windows access-violation=0xC0000005)
# ──────────────────────────────────────────────────────────────────────────────
_SEGFAULT_CODES: frozenset[int] = frozenset([139, -11, 0xC0000005])
# SIGABRT=134/-6, SIGBUS=135/-7 — also lethal crash families worth flagging
_CRASH_CODES: frozenset[int] = frozenset([139, -11, 0xC0000005, 134, -6, 135, -7])
_PRINT_LOCK = threading.Lock()   # guards stderr console output in parallel runs
_CATALOG_LOCK = threading.Lock()  # guards lazy catalog() init under ThreadPoolExecutor


def _is_segfault(exit_code: int) -> bool:
    """Detect SIGSEGV, SIGABRT, SIGBUS — any lethal crash."""
    return exit_code in _CRASH_CODES or (exit_code < 0 and -exit_code in (signal.SIGSEGV, signal.SIGABRT))


# ──────────────────────────────────────────────────────────────────────────────
# Catalog — loaded lazily from invariants.json
# ──────────────────────────────────────────────────────────────────────────────

_HERE = Path(__file__).resolve().parent
_CATALOG: Optional[dict] = None


def catalog() -> dict:
    global _CATALOG
    with _CATALOG_LOCK:  # prevents double-init race under ThreadPoolExecutor
        if _CATALOG is None:
            try:
                _CATALOG = json.loads((_HERE / "invariants.json").read_text(encoding="utf-8"))
            except FileNotFoundError:
                print(f"FATAL: Doctrine missing. Could not find invariants.json at {_HERE}", file=sys.stderr)
                sys.exit(1)
            except json.JSONDecodeError as e:
                print(f"FATAL: Doctrine corrupted. invariants.json is malformed: {e}", file=sys.stderr)
                sys.exit(1)
    return _CATALOG


@functools.lru_cache(maxsize=None)
def invariant(inv_id: str) -> dict:
    """Look up a domain or tier invariant by id. Returns {} if unknown."""
    cat = catalog()
    for inv in cat["invariants"]:
        if inv["id"] == inv_id:
            return inv
    # Search tier invariants too (T0-01 etc.)
    for tier_body in cat.get("tiers", {}).values():
        for inv in tier_body.get("invariants", []):
            if inv["id"] == inv_id:
                # Annotate so callers know it's a tier invariant
                return {**inv, "category": f"tier_{tier_body.get('label', '')}"}
    return {}


def all_categories() -> list[str]:
    return list(catalog().get("categories", {}).keys())


def normalize_violated(entry: dict) -> list[str]:
    """Accept v1.0 (singular) or v1.1 (plural) corpus shapes."""
    if "violated_invariants" in entry:
        v = entry["violated_invariants"]
        return list(v) if isinstance(v, list) else [v]
    if "violated_invariant" in entry:
        return [entry["violated_invariant"]]
    return ["G-01"]  # safe default — Evidence


# ──────────────────────────────────────────────────────────────────────────────
# Subprocess wrapper — every run is timed, captured, never silently swallowed
# (Error Visibility Invariant G-05 / EM-01 applied to ourselves)
# ──────────────────────────────────────────────────────────────────────────────


@dataclass
class RunResult:
    cmd: list[str]
    stdout: str
    stderr: str
    exit_code: int
    duration_ms: float
    timed_out: bool = False

    @property
    def is_segfault(self) -> bool:
        return _is_segfault(self.exit_code)

    @property
    def segfault_label(self) -> str:
        if self.is_segfault:
            return f"SEGFAULT(exit={self.exit_code})"
        return ""

    def summary(self) -> str:
        crash = f"  {self.segfault_label}" if self.is_segfault else ""
        return (
            f"$ {' '.join(shlex.quote(c) for c in self.cmd)}\n"
            f"  exit={self.exit_code}  time={self.duration_ms:.1f}ms"
            f"{'  TIMEOUT' if self.timed_out else ''}{crash}"
        )


def run(cmd: list[str], timeout: int = 30, cwd: Optional[Path] = None) -> RunResult:
    """
    Sandboxed subprocess runner (SEC-01 / T6-Must-Resist-Adversaries).
    POSIX: spawns in a new process group so os.killpg() cleanly terminates
           the entire process tree on timeout. No orphan zombie compilers.
    Windows: falls back to taskkill /T on timeout to kill child tree.
    """
    t0 = time.time()
    _use_pgroup = hasattr(os, "setsid")  # POSIX only

    popen_kwargs: dict = dict(
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=str(cwd) if cwd else None,
    )
    if _use_pgroup:
        popen_kwargs["start_new_session"] = True  # equivalent to setsid

    try:
        proc = subprocess.Popen(cmd, **popen_kwargs)
        try:
            stdout_b, stderr_b = proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            # Kill the whole process group / job tree
            if _use_pgroup:
                try:
                    os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                except (ProcessLookupError, PermissionError):
                    pass
            else:
                # Windows: taskkill /F /T kills child tree
                try:
                    subprocess.run(
                        ["taskkill", "/F", "/T", "/PID", str(proc.pid)],
                        capture_output=True,
                    )
                except FileNotFoundError:
                    proc.kill()  # fallback if taskkill binary missing (MinGW, etc.)
            proc.wait()
            return RunResult(
                cmd=cmd,
                stdout="",
                stderr=f"TIMEOUT after {timeout}s — process group killed",
                exit_code=124,
                duration_ms=(time.time() - t0) * 1000.0,
                timed_out=True,
            )

        stdout_s = stdout_b.decode("utf-8", errors="replace")
        stderr_s = stderr_b.decode("utf-8", errors="replace")
        return RunResult(
            cmd=cmd,
            stdout=stdout_s,
            stderr=stderr_s,
            exit_code=proc.returncode,
            duration_ms=(time.time() - t0) * 1000.0,
        )

    except FileNotFoundError as e:
        return RunResult(
            cmd=cmd,
            stdout="",
            stderr=f"BINARY_NOT_FOUND: {e}",
            exit_code=127,
            duration_ms=(time.time() - t0) * 1000.0,
        )
    except OSError as e:
        # Catches PermissionError, WinError, or any OS-level exec failure
        return RunResult(
            cmd=cmd,
            stdout="",
            stderr=f"OS_ERROR: {e}",
            exit_code=126,
            duration_ms=(time.time() - t0) * 1000.0,
        )


# ──────────────────────────────────────────────────────────────────────────────
# Finding Report — supercharged template, structured, multi-invariant aware
# ──────────────────────────────────────────────────────────────────────────────


@dataclass
class Finding:
    name: str
    category: str
    violated_invariant_ids: list[str]
    primary_invariant_name: str
    status: str               # suspected | confirmed | fixed_verified | doc_drift | ...
    severity: str             # P0 | P1 | P2 | P3
    evidence_strength: str    # strong | moderate | uncertain
    trigger: str
    expected: dict
    actual: dict
    target_run: dict
    reference_run: dict
    diffs: list[str]
    causal_chain: str
    suggested_fix: str
    regression_test: str
    ship_decision: str        # ship | hold | needs_evidence | baseline_red
    ship_rationale: list[str]
    repro_sha256: str
    schema_version: str = __version__
    notes: str = ""
    base64_payload: str = ""
    counterexamples_tried: list[str] = field(default_factory=list)
    remaining_uncertainty: str = ""

    def to_markdown(self) -> str:
        # Render the violated-invariants block, listing each rule + smell
        inv_lines = []
        for inv_id in self.violated_invariant_ids:
            inv = invariant(inv_id)
            rule = inv.get("rule", "(unknown invariant id)")
            smell = inv.get("violation_smell", "")
            line = f"- **`{inv_id}` — {inv.get('name', '?')}**: {rule}"
            if smell:
                line += f"\n  _Smell:_ {smell}"
            inv_lines.append(line)
        inv_block = "\n".join(inv_lines)

        diff_block = "\n".join(f"  - {d}" for d in self.diffs) or "  - (none — outputs matched)"
        rationale_block = "\n".join(f"  - {r}" for r in self.ship_rationale) or "  - (no rationale recorded)"
        cex_block = "\n".join(f"  - {c}" for c in self.counterexamples_tried) or "  - (none tried — Gate 11 unfilled)"

        return f"""# FINDING: {self.name}

**Status:** {self.status}
**Evidence strength:** {self.evidence_strength}
**Severity:** {self.severity}
**Repro sha256:** `{self.repro_sha256}`
**Schema:** v{self.schema_version}

## Violated invariants
{inv_block}

## Trigger
{self.trigger}

## Expected vs Actual
- expected: exit={self.expected.get('exit')!r}  stdout={self.expected.get('stdout')!r}  stderr={self.expected.get('stderr')!r}
- actual:   exit={self.actual.get('exit')!r}  stdout={self.actual.get('stdout')!r}  stderr={self.actual.get('stderr')!r}

## Diffs (target vs reference)
{diff_block}

## Target run
```
{self.target_run.get('summary', '')}
--- stdout ---
{self.target_run.get('stdout', '')}
--- stderr ---
{self.target_run.get('stderr', '')}
```

## Reference run
```
{self.reference_run.get('summary', '')}
--- stdout ---
{self.reference_run.get('stdout', '')}
--- stderr ---
{self.reference_run.get('stderr', '')}
```

## Causal chain
{self.causal_chain or '(unfilled — Causal Tracer agent must complete before ship)'}

## Suggested fix
{self.suggested_fix or '(unfilled)'}

## Regression test
{self.regression_test or '(required by Invariant G-10 / T3-02 before ship)'}

## Counterexamples tried (Gate 11)
{cex_block}

## Remaining uncertainty
{self.remaining_uncertainty or '(none stated)'}

## Base64 repro payload
```
{self.base64_payload}
```

## Ship decision: **{self.ship_decision.upper()}**
Rationale:
{rationale_block}

{self.notes}
"""


# ──────────────────────────────────────────────────────────────────────────────
# Corpus entry shape (v1.1, backward compatible with v1.0)
#
# Required:
#   name, category
#   source_base64
#   target_compile_argv  : list[str], may use {SRC} {OUT}
#   reference_compile_argv: list[str], may use {SRC} {OUT}
#   target_run_argv      : list[str], may use {OUT}
#   reference_run_argv   : list[str], may use {OUT}
# Recommended:
#   violated_invariants  : list[str]   (v1.1; v1.0 'violated_invariant' also accepted)
#   status               : finding_status (default: 'suspected' if no fix, else 'confirmed')
# Optional:
#   expected_stdout, expected_stderr, expected_exit
#   trigger, decode_command, severity (auto if omitted), evidence_strength (auto)
#   counterexamples_tried, remaining_uncertainty
# ──────────────────────────────────────────────────────────────────────────────


def _expand(argv: list[str], mapping: dict[str, str]) -> list[str]:
    out: list[str] = []
    for a in argv:
        for k, v in mapping.items():
            a = a.replace("{" + k + "}", v)
        out.append(a)
    return out


def _decide_ship(
    *,
    runtime_divergent: bool,
    timed_out: bool,
    compile_asymmetric: bool,
    both_rejected: bool = False,
    severity: str,
    has_validation: bool,
    has_regression: bool,
) -> tuple[str, list[str]]:
    """Apply ship_decision_policy rules. Returns (verdict, rationale-list)."""
    rationale: list[str] = []

    if both_rejected:
        rationale.append("both compilers rejected the source (illegal C or diagnostic check)")
        return "needs_evidence", rationale

    if compile_asymmetric:
        rationale.append("compile-side asymmetry: target/reference disagreed at compile-time")
        return "needs_evidence", rationale

    if runtime_divergent:
        rationale.append("runtime divergence: target stdout or exit differs from reference")
        if severity in ("P0", "P1"):
            rationale.append(f"P-tier {severity}: runtime divergence at high severity → hold (ship_decision_policy.hold)")
        return "hold", rationale

    if timed_out:
        rationale.append("target timed out — possible nontermination")
        return "hold", rationale

    # No divergence — but check that we have what 'ship' requires
    if severity in ("P0", "P1") and not has_validation:
        rationale.append(f"{severity} requires targeted validation evidence; none captured")
        return "needs_evidence", rationale

    rationale.append("baseline understood")
    rationale.append("targeted validation passed with raw output")
    if has_regression:
        rationale.append("regression coverage exists (this corpus entry)")
    rationale.append("blast radius: scoped (single corpus entry)")
    return "ship", rationale


def assess_corpus_entry(
    entry: dict,
    workdir: Path,
    timeout: int = 30,
    env: Optional[dict] = None,
) -> Finding:
    # Guard: corpus entry must have required keys (fail loud, not silent)
    for required_key in ("name", "source_base64", "target_compile_argv",
                         "reference_compile_argv", "target_run_argv", "reference_run_argv"):
        if required_key not in entry:
            raise KeyError(f"Corpus entry missing required key: {required_key!r} (entry keys: {list(entry.keys())})")

    workdir.mkdir(parents=True, exist_ok=True)

    raw = base64.b64decode(entry["source_base64"])
    repro_sha = hashlib.sha256(raw).hexdigest()
    src = workdir / f"{entry['name']}.c"
    src.write_bytes(raw)

    target_bin = workdir / f"{entry['name']}.target"
    reference_bin = workdir / f"{entry['name']}.reference"

    target_compile    = _apply_env_overrides(_expand(entry["target_compile_argv"],    {"SRC": str(src), "OUT": str(target_bin)}),    env or {})
    reference_compile = _apply_env_overrides(_expand(entry["reference_compile_argv"], {"SRC": str(src), "OUT": str(reference_bin)}), env or {})
    target_run        = _apply_env_overrides(_expand(entry["target_run_argv"],         {"OUT": str(target_bin)}),                       env or {}, is_run_argv=True)
    reference_run     = _apply_env_overrides(_expand(entry["reference_run_argv"],      {"OUT": str(reference_bin)}),                    env or {}, is_run_argv=True)

    tgt_c = run(target_compile, timeout=timeout)
    ref_c = run(reference_compile, timeout=timeout)

    diffs: list[str] = []
    notes_lines: list[str] = []
    compile_asymmetric = False

    if tgt_c.exit_code != 0 and ref_c.exit_code == 0:
        diffs.append(f"compile: target FAILED (exit={tgt_c.exit_code}); reference OK")
        compile_asymmetric = True
        return _build_finding(
            entry, repro_sha, raw,
            target_run_result=tgt_c, reference_run_result=ref_c,
            diffs=diffs, runtime_divergent=False, compile_asymmetric=True,
        )
    if tgt_c.exit_code == 0 and ref_c.exit_code != 0:
        diffs.append(f"compile: target OK; reference FAILED (exit={ref_c.exit_code})")
        notes_lines.append(
            "BASELINE: target accepts what reference rejects. Promote to a "
            "diagnostic-quality finding before shipping a 'fix'."
        )
        return _build_finding(
            entry, repro_sha, raw,
            target_run_result=tgt_c, reference_run_result=ref_c,
            diffs=diffs, runtime_divergent=False, compile_asymmetric=True,
            extra_notes="\n".join(notes_lines),
        )
    if tgt_c.exit_code != 0 and ref_c.exit_code != 0:
        diffs.append("compile: both compilers rejected the source")
        notes_lines.append(
            "Both compilers reject. Either an illegal-C corpus entry or a "
            "parallel diagnostic check. Verify expected behavior with "
            "expected_exit and category."
        )
        return _build_finding(
            entry, repro_sha, raw,
            target_run_result=tgt_c, reference_run_result=ref_c,
            diffs=diffs, runtime_divergent=False, compile_asymmetric=False,
            both_rejected=True,
            extra_notes="\n".join(notes_lines),
        )

    # Both compiled. Run both.
    tgt_r = run(target_run, timeout=timeout)
    ref_r = run(reference_run, timeout=timeout)

    if tgt_r.exit_code != ref_r.exit_code:
        diffs.append(f"exit: target={tgt_r.exit_code} reference={ref_r.exit_code}")
    if tgt_r.is_segfault:
        diffs.append(f"CRASH: target produced {tgt_r.segfault_label} — automatic P0 escalation")
    if ref_r.is_segfault and not tgt_r.is_segfault:
        # Reference crashed but target didn't — suspicious baseline. Surface it.
        diffs.append(f"BASELINE-CRASH: reference produced {ref_r.segfault_label} — investigate reference environment")

    if tgt_r.stdout != ref_r.stdout:
        delta = list(difflib.unified_diff(
            ref_r.stdout.splitlines(keepends=True),
            tgt_r.stdout.splitlines(keepends=True),
            fromfile="reference/stdout",
            tofile="target/stdout",
            lineterm="",
        ))
        if delta:
            diffs.append("stdout delta (unified diff):")
            diffs.extend(delta[:40])  # cap at 40 lines to avoid report bloat
            if len(delta) > 40:
                diffs.append(f"  ... ({len(delta) - 40} more diff lines truncated)")
        else:
            diffs.append(f"stdout: divergent (target={len(tgt_r.stdout)}B, reference={len(ref_r.stdout)}B — whitespace only?)")

    if tgt_r.stderr != ref_r.stderr:
        diffs.append("stderr: divergent (informational — diagnostic text may legitimately differ)")

    runtime_divergent = (tgt_r.exit_code != ref_r.exit_code) or (tgt_r.stdout != ref_r.stdout)

    if "expected_exit" in entry and tgt_r.exit_code != entry["expected_exit"]:
        diffs.append(f"exit: target={tgt_r.exit_code} expected={entry['expected_exit']}")
        runtime_divergent = True
    if "expected_stdout" in entry and tgt_r.stdout != entry["expected_stdout"]:
        diffs.append("stdout: target differs from corpus expected_stdout")
        runtime_divergent = True
    if "expected_stderr" in entry and tgt_r.stderr != entry["expected_stderr"]:
        diffs.append("stderr: target differs from corpus expected_stderr")
        runtime_divergent = True

    return _build_finding(
        entry, repro_sha, raw,
        target_run_result=tgt_r, reference_run_result=ref_r,
        diffs=diffs,
        runtime_divergent=runtime_divergent,
        compile_asymmetric=False,
        extra_notes="\n".join(notes_lines),
    )


def _build_finding(
    entry: dict,
    repro_sha: str,
    raw: bytes,
    target_run_result: RunResult,
    reference_run_result: RunResult,
    diffs: list[str],
    runtime_divergent: bool,
    compile_asymmetric: bool,
    both_rejected: bool = False,
    extra_notes: str = "",
) -> Finding:
    inv_ids = normalize_violated(entry)
    primary_inv = invariant(inv_ids[0]) if inv_ids else {}
    primary_name = primary_inv.get("name", "(unknown)")

    # Segfault detection: auto-escalate to P0 regardless of corpus annotation
    target_segfaulted = target_run_result.is_segfault

    # Severity defaulting
    if target_segfaulted:
        severity = "P0"  # SEC-01: memory corruption cannot be downgraded
    elif "severity" in entry:
        severity = entry["severity"]
    elif runtime_divergent or compile_asymmetric:
        severity = "P1"
    else:
        severity = "P2"

    # Evidence strength defaulting
    if "evidence_strength" in entry:
        evidence = entry["evidence_strength"]
    elif runtime_divergent:
        evidence = "strong"
    elif diffs:
        evidence = "moderate"
    else:
        evidence = "strong" if not compile_asymmetric else "moderate"

    # Status defaulting
    if target_segfaulted:
        status = "confirmed"  # crash is always confirmed evidence
    elif "status" in entry:
        status = entry["status"]
    elif compile_asymmetric or both_rejected:
        status = "suspected"
    elif runtime_divergent:
        status = "confirmed"
    else:
        status = "fixed_verified" if entry.get("suggested_fix") else "not_a_bug"

    ship, rationale = _decide_ship(
        runtime_divergent=runtime_divergent,
        timed_out=target_run_result.timed_out,
        compile_asymmetric=compile_asymmetric,
        both_rejected=both_rejected,
        severity=severity,
        has_validation=True,  # we just ran target + reference
        has_regression=True,  # this corpus entry IS the regression
    )

    return Finding(
        name=entry["name"],
        category=entry.get("category", "uncategorized"),
        violated_invariant_ids=inv_ids,
        primary_invariant_name=primary_name,
        status=status,
        severity=severity,
        evidence_strength=evidence,
        trigger=entry.get("trigger", "(unfilled)"),
        expected={"exit": entry.get("expected_exit"), "stdout": entry.get("expected_stdout"), "stderr": entry.get("expected_stderr")},
        actual={"exit": target_run_result.exit_code, "stdout": target_run_result.stdout, "stderr": target_run_result.stderr},
        target_run={"summary": target_run_result.summary(), "stdout": target_run_result.stdout, "stderr": target_run_result.stderr, "exit_code": target_run_result.exit_code, "duration_ms": target_run_result.duration_ms},
        reference_run={"summary": reference_run_result.summary(), "stdout": reference_run_result.stdout, "stderr": reference_run_result.stderr, "exit_code": reference_run_result.exit_code, "duration_ms": reference_run_result.duration_ms},
        diffs=diffs,
        causal_chain=entry.get("causal_chain", ""),
        suggested_fix=entry.get("suggested_fix", ""),
        regression_test=entry.get("regression_test", ""),
        ship_decision=ship,
        ship_rationale=rationale,
        repro_sha256=repro_sha,
        notes=extra_notes,
        base64_payload=base64.b64encode(raw).decode("ascii"),
        counterexamples_tried=entry.get("counterexamples_tried", []),
        remaining_uncertainty=entry.get("remaining_uncertainty", ""),
    )


# ──────────────────────────────────────────────────────────────────────────────
# Environment override support  (--env env_windows_wsl.json)
# Lets the runner transparently remap bare compiler names (zcc, cc, gcc) to
# WSL-bridged invocations and rewrite Windows paths to /mnt/ equivalents.
# ──────────────────────────────────────────────────────────────────────────────


def _load_env_overrides(path: Optional[str]) -> dict:
    if not path:
        return {}
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError) as e:
        print(f"FATAL: --env file invalid: {e}", file=sys.stderr)
        sys.exit(1)


def _win_to_wsl(p: str) -> str:
    """Convert a Windows absolute path to its WSL /mnt/<drive>/... equivalent."""
    if len(p) >= 2 and p[1] == ":":
        drive = p[0].lower()
        rest = p[2:].replace("\\", "/")
        return f"/mnt/{drive}{rest}"
    # Relative path with backslashes — convert separators only
    return p.replace("\\", "/")


def _resolve_to_wsl(p: str) -> str:
    """Resolve any path (relative or absolute) to its WSL /mnt/... equivalent."""
    resolved = str(Path(p).resolve())
    return _win_to_wsl(resolved)


def _apply_env_overrides(argv: list[str], env: dict, is_run_argv: bool = False) -> list[str]:
    """
    Given a raw argv from a corpus entry, apply env overrides:
    1. Replace the first element (compiler binary) via compiler_map.
    2. If path_rewrite.win_to_wsl, resolve+convert all path arguments to WSL /mnt/ paths.
    3. For run_argv (compiled ELF execution), prepend 'wsl --' if not already a WSL command.
    Returns the final argv ready for subprocess.
    """
    if not env:
        return argv

    result = list(argv)
    compiler_map: dict = env.get("compiler_map", {})
    do_wsl_rewrite: bool = env.get("path_rewrite", {}).get("win_to_wsl", False)

    # Replace compiler binary prefix (compile commands)
    compiler_replaced = False
    if result and result[0] in compiler_map:
        replacement: list[str] = compiler_map[result[0]]
        result = replacement + result[1:]
        compiler_replaced = True

    # Rewrite ALL path-like arguments to WSL paths (resolve relative paths)
    if do_wsl_rewrite:
        def _maybe_rewrite(a: str) -> str:
            # Rewrite if it looks like a file path (has sep, starts with drive, or is an existing path candidate)
            if len(a) >= 2 and a[1] == ":":
                return _resolve_to_wsl(a)
            if "\\" in a or (len(a) > 1 and a.startswith(".")):
                return _resolve_to_wsl(a)
            return a
        # Skip the WSL prefix tokens themselves (wsl, --, etc.)
        skip_prefix = len(compiler_map.get(result[0] if result else "", [])) if compiler_replaced else 0
        result = result[:skip_prefix] + [_maybe_rewrite(a) for a in result[skip_prefix:]]

    # For run argv (compiled ELF binaries), prepend wsl -- if not already a wsl command
    if is_run_argv and do_wsl_rewrite and result and result[0] not in ("wsl",):
        result = ["wsl", "--"] + result

    return result


def cmd_run(args: argparse.Namespace) -> int:
    entries: list[dict] = []
    for p in args.corpus:
        path = Path(p)
        if path.is_dir():
            for f in sorted(path.glob("*.json")):
                try:
                    entries.append(json.loads(f.read_text(encoding="utf-8")))
                except json.JSONDecodeError as e:
                    print(f"WARN: skipping malformed corpus file {f}: {e}", file=sys.stderr)
        else:
            try:
                entries.append(json.loads(path.read_text(encoding="utf-8")))
            except json.JSONDecodeError as e:
                print(f"WARN: skipping malformed corpus file {path}: {e}", file=sys.stderr)

    if not entries:
        print("no corpus entries found", file=sys.stderr)
        return 2

    env_overrides = _load_env_overrides(getattr(args, "env", None))
    workroot = Path(args.workdir)
    workroot.mkdir(parents=True, exist_ok=True)

    workers = getattr(args, "workers", None) or min(32, len(entries))
    results: list[tuple[int, Finding]] = []  # (original index, finding)
    counts: dict[str, int] = {"ship": 0, "hold": 0, "needs_evidence": 0}

    def _assess(idx_entry: tuple[int, dict]) -> tuple[int, Finding]:
        idx, entry = idx_entry
        wd = workroot / entry["name"]
        return idx, assess_corpus_entry(entry, wd, timeout=args.timeout, env=env_overrides)

    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = {pool.submit(_assess, (i, e)): i for i, e in enumerate(entries)}
        for fut in concurrent.futures.as_completed(futures):
            try:
                idx, finding = fut.result()
            except Exception as exc:  # noqa: BLE001
                # Surface worker exceptions as non-fatal; don't silently absorb.
                with _PRINT_LOCK:
                    print(f"[WORKER-ERROR] entry #{futures[fut]}: {exc}", file=sys.stderr)
                continue
            results.append((idx, finding))
            counts[finding.ship_decision] = counts.get(finding.ship_decision, 0) + 1
            invs = ",".join(finding.violated_invariant_ids[:3])
            if len(finding.violated_invariant_ids) > 3:
                invs += "+" + str(len(finding.violated_invariant_ids) - 3)
            crash_flag = "  \u2620 SEGFAULT" if finding.target_run.get("exit_code", 0) in _SEGFAULT_CODES else ""
            with _PRINT_LOCK:
                print(
                    f"[{finding.ship_decision.upper():>14}] {finding.name:42}"
                    f"  inv={invs:14} sev={finding.severity}"
                    f"  status={finding.status:18} diffs={len(finding.diffs)}{crash_flag}",
                    file=sys.stderr,
                )

    # Restore original corpus order for deterministic reports
    results.sort(key=lambda t: t[0])
    out_md = [f.to_markdown() for _, f in results]
    out_json = [asdict(f) for _, f in results]

    # --auto-triage: write agent prompt payloads for every divergent finding
    if getattr(args, "auto_triage", False):
        triage_dir = workroot / "triage"
        triage_dir.mkdir(parents=True, exist_ok=True)
        for _, f in results:
            if f.ship_decision in ("hold", "needs_evidence"):
                payload = _auto_triage_prompt(f)
                out_path = triage_dir / f"{f.name}.triage.md"
                out_path.write_text(payload, encoding="utf-8")
                with _PRINT_LOCK:
                    print(f"  [TRIAGE] {out_path}", file=sys.stderr)

    if args.report:
        Path(args.report).write_text("\n\n---\n\n".join(out_md), encoding="utf-8")
    if args.json:
        Path(args.json).write_text(json.dumps(out_json, indent=2), encoding="utf-8")

    print(
        f"\n{counts.get('ship', 0)} ship  /  "
        f"{counts.get('hold', 0)} hold  /  "
        f"{counts.get('needs_evidence', 0)} needs-evidence"
        f"  [workers={workers}]",
        file=sys.stderr,
    )

    return 0 if counts.get("hold", 0) == 0 and counts.get("needs_evidence", 0) == 0 else 1


def _auto_triage_prompt(f: Finding) -> str:
    """Generate a dense CausalTracer / ReferenceJudge agent hand-off prompt."""
    inv_block = "\n".join(
        f"- {inv_id}: {invariant(inv_id).get('rule', '(unknown)')}"
        for inv_id in f.violated_invariant_ids
    )
    diff_block = "\n".join(f.diffs) or "(no diffs captured)"
    return f"""# ZKAEDI AUTO-TRIAGE — Agent Hand-off Payload
## Corpus entry: `{f.name}`
**Verdict:** {f.ship_decision.upper()}  |  **Severity:** {f.severity}  |  **Status:** {f.status}

## Agent Dispatch
You are operating as **CausalTracer** and **ReferenceJudge** for the ZKAEDI
Supercharged Error Management Squad. Your mandate is to determine the exact
root cause of the compiler divergence documented below and propose a
surgical fix aligned with the violated invariants.

## Violated invariants
{inv_block}

## Trigger source (C program)
```c
{f.trigger}
```

## Base64 repro (sha256={f.repro_sha256})
```
{f.base64_payload}
```

## Target compiler run
```
{f.target_run.get('summary', '')}
--- stdout ---
{f.target_run.get('stdout', '').strip()}
--- stderr ---
{f.target_run.get('stderr', '').strip()}
```

## Reference compiler run
```
{f.reference_run.get('summary', '')}
--- stdout ---
{f.reference_run.get('stdout', '').strip()}
--- stderr ---
{f.reference_run.get('stderr', '').strip()}
```

## Forensic diff
```diff
{diff_block}
```

## Task for CausalTracer
1. Trace the exact source-to-AST-to-IR-to-codegen path that causes the divergence.
2. Identify whether the root cause is: type decay, register allocation, stack alignment,
   sign extension, PHI liveness, or another category.
3. Fill in the `causal_chain` field: source/input → internal state → broken assumption → bad operation → observable failure.

## Task for ReferenceJudge
1. Confirm the reference compiler's behavior is spec-correct (cite the C standard section).
2. Classify whether this is a codegen bug, ABI violation, or an undefined-behavior exploitation.
3. Provide the `suggested_fix` — a minimal surgical patch, not a sweeping refactor.

## Deliverable
Return a v1.1 Finding JSON with `causal_chain`, `suggested_fix`, `regression_test`,
`counterexamples_tried`, and `status` set to `confirmed`.
"""


def cmd_invariants(args: argparse.Namespace) -> int:
    cat = catalog()
    if args.id:
        inv = invariant(args.id)
        if not inv:
            print(f"no invariant with id {args.id!r}", file=sys.stderr)
            return 2
        print(json.dumps(inv, indent=2))
        return 0

    if args.search:
        needle = args.search.lower()
        hits = [
            inv for inv in cat["invariants"]
            if needle in inv["id"].lower() or needle in inv["name"].lower() or needle in inv["rule"].lower()
        ]
        if not hits:
            print(f"no invariants match {args.search!r}", file=sys.stderr)
            return 2
        for inv in hits:
            print(f"  {inv['id']:8}  {inv['name']:32}  {inv['rule']}")
        return 0

    # Group by category dynamically (handles all 8 categories)
    by_cat: dict[str, list[dict]] = {c: [] for c in cat.get("categories", {})}
    for inv in cat["invariants"]:
        by_cat.setdefault(inv["category"], []).append(inv)

    for c, items in by_cat.items():
        if args.category and args.category != c:
            continue
        if not items:
            continue
        print(f"\n=== {c.upper()} ({len(items)}) ===")
        for inv in items:
            print(f"  {inv['id']:8}  {inv['name']:32}  {inv['rule']}")
    return 0


def cmd_gates(args: argparse.Namespace) -> int:
    """Print the gate checklist as a fillable form."""
    for g in catalog()["gates"]:
        print(f"Gate {g['id']}: {g['name']}")
        print(f"  purpose: {g['purpose']}")
        print(f"  status:  [ ] pass  [ ] fail  [ ] n/a")
        print(f"  evidence:")
        print()
    return 0


def cmd_tiers(args: argparse.Namespace) -> int:
    """List the T0-T7 release-readiness ladder."""
    cat = catalog()
    tiers = cat.get("tiers", {})
    if args.id:
        # Tier id like 'T0' or 'T0-01'
        if "-" in args.id:
            inv = invariant(args.id)
            if not inv:
                print(f"no tier invariant with id {args.id!r}", file=sys.stderr)
                return 2
            print(json.dumps(inv, indent=2))
            return 0
        body = tiers.get(args.id)
        if not body:
            print(f"no tier with id {args.id!r}", file=sys.stderr)
            return 2
        print(json.dumps(body, indent=2))
        return 0

    for tier_id, body in tiers.items():
        if args.tier and args.tier != tier_id:
            continue
        print(f"\n=== {body['label']} — {body['name']} ===")
        print(f"  Q: {body['core_question']}")
        print(f"  policy: {body['policy']}")
        print(f"  ship_impact: {body['ship_impact']}")
        for inv in body["invariants"]:
            print(f"    {inv['id']:8}  {inv['name']:36}  {inv['rule']}")
            mapped = inv.get("mapped_invariants", [])
            if mapped:
                print(f"             ↳ maps to: {', '.join(mapped)}")
    return 0


def cmd_release_policy(args: argparse.Namespace) -> int:
    pol = catalog().get("release_policy", {})
    for kind, spec in pol.items():
        print(f"\n=== {kind.upper().replace('_', ' ')} ===")
        for k, v in spec.items():
            label = k.replace("_", " ")
            if isinstance(v, list):
                print(f"  {label}: {', '.join(v)}")
            else:
                print(f"  {label}: {v}")
    return 0


def cmd_workflow(args: argparse.Namespace) -> int:
    wf = catalog().get("workflows", {})
    if args.name:
        body = wf.get(args.name)
        if not body:
            print(f"no workflow named {args.name!r}. available: {', '.join(wf.keys())}", file=sys.stderr)
            return 2
        print(json.dumps(body, indent=2))
        return 0
    for name, body in wf.items():
        print(f"\n=== {name} ===")
        print(f"  mission: {body['mission']}")
        print(f"  agents:  {', '.join(body['agents'])}")
        print(f"  steps:")
        for i, step in enumerate(body["steps"], 1):
            print(f"    {i}. {step}")
        print(f"  ship_policy: {body['ship_policy']}")
    return 0


def cmd_test_matrix(args: argparse.Namespace) -> int:
    tm = catalog().get("test_matrix", {})
    for area, classes in tm.items():
        print(f"\n=== {area} ===")
        for tc in classes:
            print(f"  class:      {tc['class']}")
            print(f"  purpose:    {tc['purpose']}")
            print(f"  oracle:     {tc['oracle']}")
            print(f"  invariants: {', '.join(tc['invariants'])}")
            print()
    return 0


def cmd_template(args: argparse.Namespace) -> int:
    """Emit a blank Finding template."""
    print(_FINDING_TEMPLATE)
    return 0


def cmd_severity(args: argparse.Namespace) -> int:
    sm = catalog().get("severity_model", {})
    for tier_id, body in sm.items():
        print(f"\n{tier_id} — {body['name']}")
        print(f"  ship_policy: {body['ship_policy']}")
        print(f"  criteria:")
        for c in body["criteria"]:
            print(f"    - {c}")
    return 0


_FINDING_TEMPLATE = """# FINDING: <name>

**Status:** suspected | confirmed | fixed_unverified | fixed_verified | doc_drift | not_a_bug | superseded | blocked
**Evidence strength:** strong | moderate | uncertain
**Severity:** P0 | P1 | P2 | P3
**Subsystem / file / function:**

## Violated invariants
- `<id>` — <name>: <one-line rule>
- `<id>` — <name>: <one-line rule>   (multiple OK; list most-upstream first)

## Trigger
<exact input + command>

## Expected behavior
<reference compiler / spec / corpus expected_*>

## Actual behavior
<observed output, exit, side effects>

## Generated assembly symptom (codegen findings only)
```
<relevant lines from .s>
```

## Root cause
<why the symptom appears>

## Causal chain
source/input → internal state → broken assumption → bad operation → observable failure

## Impact / blast radius
<what else could be affected; similar code paths>

## Minimal repro
```c
<smallest C source that reproduces>
```

base64:
```
<base64 of the source>
```

## Reference compiler result
```
<gcc/cc behavior>
```

## Suggested fix
<surgical patch description; not a sweeping refactor>

## Regression test
<corpus entry path; assertion id; test name>

## Validation commands
```
<exact commands>
```

## Raw validation output
```
<output that proves the fix>
```

## Counterexamples tried (Gate 11 — not overfit)
- <counterexample 1>
- <counterexample 2>

## Remaining uncertainty
<honest disclosure>

## Ship decision
ship | hold | needs_evidence
Rationale:
- <criterion from ship_decision_policy>
- <criterion from ship_decision_policy>
"""


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="bug_hunter",
        description=f"ZKAEDI Supercharged Error Management Squad runner.  v{__version__}",
    )
    p.add_argument("--version", action="version", version=f"bug_hunter {__version__}")
    sub = p.add_subparsers(dest="cmd", required=True)

    pr = sub.add_parser("run", help="Run corpus entries through target+reference compilers and emit findings.")
    pr.add_argument("corpus", nargs="+", help="Corpus JSON file(s) or directory.")
    pr.add_argument("--workdir", default="./tmp_zkaedi-bh", help="Scratch directory for compiled artifacts.")
    pr.add_argument("--timeout", type=int, default=30)
    pr.add_argument("--workers", type=int, default=None, help="ThreadPoolExecutor worker count (default: min(32, corpus_size)).")
    pr.add_argument("--report", help="Write combined markdown report to this path.")
    pr.add_argument("--json", help="Write findings as JSON array to this path.")
    pr.add_argument("--auto-triage", dest="auto_triage", action="store_true",
                    help="Generate CausalTracer/ReferenceJudge agent prompt payloads for all divergent findings.")
    pr.add_argument("--env", default=None, metavar="ENV_JSON",
                    help="Environment override JSON (e.g. env_windows_wsl.json) to remap compiler names and paths.")
    pr.set_defaults(func=cmd_run)

    pi = sub.add_parser("invariants", help="List or look up domain invariants (G/CG/EM/IR/DOC/TEST/SEC/BUILD).")
    pi.add_argument("--id", help="Look up a specific invariant id (e.g. CG-09, T0-01).")
    pi.add_argument("--category", help="Filter by category (use --list-categories to see options).")
    pi.add_argument("--search", help="Substring search across id, name, rule.")
    pi.set_defaults(func=cmd_invariants)

    pg = sub.add_parser("gates", help="Print the production-push gate checklist (0-12).")
    pg.set_defaults(func=cmd_gates)

    pti = sub.add_parser("tiers", help="List the T0-T7 release-readiness ladder.")
    pti.add_argument("--id", help="Look up a specific tier (T0) or tier invariant (T0-01).")
    pti.add_argument("--tier", help="Filter to one tier (T0..T7).")
    pti.set_defaults(func=cmd_tiers)

    pp = sub.add_parser("release-policy", help="Print release-policy gates (normal_ship / high_risk_ship / emergency_hotfix).")
    pp.set_defaults(func=cmd_release_policy)

    pw = sub.add_parser("workflow", help="Print workflow definitions (doc_drift_audit, codegen_runtime_bug, ...).")
    pw.add_argument("--name", help="Specific workflow.")
    pw.set_defaults(func=cmd_workflow)

    ptm = sub.add_parser("test-matrix", help="Print the canonical test matrix (codegen / error_management / docs).")
    ptm.set_defaults(func=cmd_test_matrix)

    psv = sub.add_parser("severity", help="Print the severity model (P0..P3).")
    psv.set_defaults(func=cmd_severity)

    pt = sub.add_parser("template", help="Print a blank Finding Report template (v1.1 schema).")
    pt.set_defaults(func=cmd_template)

    return p


def main(argv: Optional[list[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
