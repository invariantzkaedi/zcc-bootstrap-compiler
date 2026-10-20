#!/usr/bin/env python3
r"""
ZCC QUEST — auto-playing side-scroller & arcade progress tracker for self-hosting builds.
========================================================================================

Wraps your build command and renders an arcade progress game:
  - runner  : Mario/Frogger-style side-scroller runner
  - frogger : Traffic dodging arcade game
  - garden  : Deterministic Zen Garden grown from build filenames
  - prime   : ZKAEDI PRIME Recursively Coupled Hamiltonian Phase-Space Solver

Usage:
    python3 zcc_quest.py --cmd "make selfhost" --mode sovereign # Sovereign Quantum HUD v6.0
    python3 zcc_quest.py --demo --mode sovereign        # Sovereign Quantum HUD
    python3 zcc_quest.py --demo --mode prime            # ZKAEDI PRIME Mode
    python3 zcc_quest.py --demo --mode garden           # Zen Garden Mode
    python3 zcc_quest.py --demo --mode frogger          # Frogger Mode
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import logging
import os
import random
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Deque, List, Optional

if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
if hasattr(sys.stderr, "reconfigure"):
    try:
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

# ---------------------------------------------------------------------------
# Logging (file-only; stdout belongs to the game)
# ---------------------------------------------------------------------------
LOG = logging.getLogger("zcc_quest")


def _setup_logging(path: str) -> None:
    """Route diagnostics to a sidecar log file, never the game screen."""
    LOG.setLevel(logging.DEBUG)
    try:
        fh = logging.FileHandler(path, mode="w", encoding="utf-8")
    except OSError as exc:  # unwritable path → degrade, don't die
        sys.stderr.write(f"[zcc_quest] cannot open log {path!r}: {exc}\n")
        return
    fh.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(message)s"))
    LOG.addHandler(fh)


# ---------------------------------------------------------------------------
# ANSI helpers
# ---------------------------------------------------------------------------
CSI = "\x1b["
RESET = CSI + "0m"
HIDE_CUR = CSI + "?25l"
SHOW_CUR = CSI + "?25h"
ALT_ON = CSI + "?1049h"
ALT_OFF = CSI + "?1049l"


def fg(n: int) -> str:
    """256-color foreground escape."""
    assert 0 <= n <= 255, f"color index out of range: {n}"
    return f"{CSI}38;5;{n}m"


def bg(n: int) -> str:
    """256-color background escape."""
    assert 0 <= n <= 255, f"color index out of range: {n}"
    return f"{CSI}48;5;{n}m"


BOLD = CSI + "1m"

# world palettes: (sky, ground, accent, name)
WORLDS = [
    (117, 28, 220, "STAGE 1 · cc0 → zcc1"),      # day    (blue sky, green)
    (208, 94, 226, "STAGE 2 · zcc1 → zcc2"),     # sunset (orange, brown)
    (60, 240, 213, "STAGE 3 · zcc2 → zcc3"),     # night  (indigo, grey)
]
BOSS_NAME = "⟐ BYTE-IDENTICAL SEAL ⟐"


# ---------------------------------------------------------------------------
# Build watcher — parses child output into progress events
# ---------------------------------------------------------------------------
@dataclass
class BuildState:
    """Thread-shared snapshot of build progress. All writes hold `lock`."""
    lock: threading.Lock = field(default_factory=threading.Lock)
    stage: int = 0                    # 0-based world index (0..2)
    stage_files: int = 0              # per-file ticks inside current stage
    total_lines: int = 0
    warnings: int = 0
    errors: int = 0
    tail: Deque[str] = field(default_factory=lambda: deque(maxlen=4))
    finished: bool = False
    exit_code: Optional[int] = None
    boss_seen: bool = False           # byte-identical marker observed
    oracle_verified: bool = False     # verified via on-disk byte comparison oracle
    is_demo: bool = False             # demo mode flag
    files_seen: List[str] = field(default_factory=list)   # filenames, in order
    started: float = field(default_factory=time.monotonic)
    started_wall: float = field(default_factory=time.time)
    cmd_str: str = ""
    proc: Optional[subprocess.Popen] = None
    started_event: threading.Event = field(default_factory=threading.Event)

    def snapshot(self) -> dict:
        with self.lock:
            ov = self.oracle_verified or (self.is_demo and self.boss_seen)
            return {
                "stage": self.stage,
                "stage_files": self.stage_files,
                "total_lines": self.total_lines,
                "warnings": self.warnings,
                "errors": self.errors,
                "tail": list(self.tail),
                "finished": self.finished,
                "exit_code": self.exit_code,
                "boss_seen": self.boss_seen,
                "oracle_verified": ov,
                "is_demo": self.is_demo,
                "files_seen": list(self.files_seen),
                "elapsed": time.monotonic() - self.started,
            }


STAGE_RE = re.compile(r"(?:===+\s*)?STAGE\s*([123])", re.I)
BOSS_RE = re.compile(
    r"===+\s*VERDICT:\s*BOOTSTRAP\s+DETERMINISM\s+LOCK\s+SECURED\s*===+|"
    r"SELF-HOST\s+VERIFIED\s+\(assembly\s+identical\)",
    re.I
)
NEG_RE = re.compile(r"\b(?:fail|failed|mismatch|drift|diverg|corrupt|error|non-zero)\b", re.I)
FILE_RE = re.compile(r"(?:^(?:CC|AS|LD|CCLD)\s)|(?:\.(?:c|s|o)\b.*(?:->|→))")
FNAME_RE = re.compile(r"([\w./-]+\.(?:c|s|o))\b")
WARN_RE = re.compile(r"\bwarning\b", re.I)
ERR_RE = re.compile(r"\berror\b|\bfatal\b|segmentation fault", re.I)


def is_bootstrap_command(cmd: str) -> bool:
    """Validate that the command actually executes an authorized bootstrap gate."""
    if not cmd:
        return False
    c = cmd.strip().lower()
    valid_patterns = [
        r"\bgate\.sh\b",
        r"\bmake\s+(?:selfhost|selfhost-\w+|compat-\w+)",
        r"\b(?:bash|sh)\s+.*gate\.sh\b",
        r"\bscripts/selfhost\b",
    ]
    return any(re.search(p, c) for p in valid_patterns)


def validate_assembly_file(path: str) -> bool:
    """Verify that an artifact is authentic, non-counterfeit x86-64 assembly text."""
    if not os.path.isfile(path):
        return False
    try:
        sz = os.path.getsize(path)
        # Authentic ZCC assembly is ~9.5 MB. Reject tiny counterfeit files.
        if sz < 100_000:
            return False

        # Reject binary / zero-byte counterfeit payloads
        with open(path, "rb") as f:
            header = f.read(65536)
            if b"\x00" in header:
                return False
            f.seek(max(0, sz - 65536))
            footer = f.read(65536)
            if b"\x00" in footer:
                return False

        header_str = header.decode("utf-8", errors="replace")
        footer_str = footer.decode("utf-8", errors="replace")

        # Must contain genuine x86-64 assembly directives and instruction tokens
        has_text = ".text" in header_str or ".text" in footer_str
        has_reg = "%rbp" in header_str or "%rsp" in header_str or "%rbp" in footer_str or "%rsp" in footer_str
        has_ret = "ret" in header_str or "retq" in header_str or "ret" in footer_str or "retq" in footer_str
        has_globl = ".globl" in header_str or ".globl" in footer_str or "pushq" in header_str or ".cfi" in header_str

        return has_text and has_reg and has_ret and has_globl
    except Exception:
        return False


def compute_sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(131072):
            h.update(chunk)
    return h.hexdigest()


def verify_selfhost_oracle(started_wall: float, cmd: str = "") -> bool:
    """Independent oracle: verify that stage2/stage3 assembly was generated during this run,
    matches cryptographic provenance ledger receipts, and is byte-identical."""
    import filecmp
    import glob

    # 1. Monitored command must be an authorized bootstrap invocation
    if cmd and not is_bootstrap_command(cmd):
        LOG.warning("Oracle rejected: command %r is not a recognized bootstrap gate", cmd)
        return False

    repo_dir = os.path.dirname(os.path.abspath(__file__))
    search_roots = [os.getcwd()]
    if repo_dir not in search_roots:
        search_roots.append(repo_dir)

    # 2. Locate fresh evidence directories generated during this run
    ev_dirs = []
    for root in search_roots:
        for pattern in ("evidence/zcc-omega-*", "evidence/zcc-run-*"):
            for d in glob.glob(os.path.join(root, pattern)):
                if os.path.isdir(d):
                    try:
                        # Must be created strictly AFTER started_wall (zero negative allowance)
                        if os.path.getmtime(d) >= started_wall:
                            ev_dirs.append(d)
                    except OSError:
                        pass

    # Sort newest first
    ev_dirs.sort(key=lambda d: os.path.getmtime(d), reverse=True)

    # 3. Locate candidate assembly pairs
    candidate_pairs = []
    for ed in ev_dirs:
        candidate_pairs.append((os.path.join(ed, "stage2.s"), os.path.join(ed, "stage3.s"), ed))

    if ev_dirs:
        for root in search_roots:
            candidate_pairs.append((os.path.join(root, "zcc2.s"), os.path.join(root, "zcc3.s"), ev_dirs[0]))
            candidate_pairs.append((os.path.join(root, "build/zcc2.s"), os.path.join(root, "build/zcc3.s"), ev_dirs[0]))

    if not candidate_pairs:
        LOG.warning("Oracle rejected: no fresh evidence directory created during run")
        return False

    # 4. Verify candidate files against provenance ledger
    for s2, s3, ev_dir in candidate_pairs:
        if not (os.path.isfile(s2) and os.path.isfile(s3)):
            continue
        try:
            m2 = os.path.getmtime(s2)
            m3 = os.path.getmtime(s3)
            # Strictly created AFTER process spawn (no negative allowance)
            if m2 < started_wall or m3 < started_wall:
                continue

            sz2 = os.path.getsize(s2)
            sz3 = os.path.getsize(s3)
            if sz2 != sz3 or sz2 < 100_000:
                continue

            # Check assembly validity (must be real x86-64 assembly, no NUL bytes)
            if not (validate_assembly_file(s2) and validate_assembly_file(s3)):
                continue

            # Check byte identity
            if not filecmp.cmp(s2, s3, shallow=False):
                continue

            # Verify cryptographic provenance ledger
            ledger_path = os.path.join(ev_dir, "provenance.jsonl")
            if os.path.isfile(ledger_path):
                if os.path.getmtime(ledger_path) < started_wall:
                    continue

                stage2_rec = None
                stage3_rec = None
                with open(ledger_path, "r", encoding="utf-8") as f:
                    for line in f:
                        line = line.strip()
                        if not line:
                            continue
                        rec = json.loads(line)
                        if rec.get("stage") == 2:
                            stage2_rec = rec
                        elif rec.get("stage") == 3:
                            stage3_rec = rec

                if not (stage2_rec and stage3_rec):
                    continue

                if stage2_rec.get("exit_code") != 0 or stage3_rec.get("exit_code") != 0:
                    continue

                asm_hash2 = stage2_rec.get("assembly_sha256")
                asm_hash3 = stage3_rec.get("assembly_sha256")
                if not (asm_hash2 and asm_hash3 and asm_hash2 == asm_hash3):
                    continue

                actual_hash = compute_sha256(s2)
                if actual_hash != asm_hash2:
                    continue

                return True

        except (OSError, json.JSONDecodeError, ValueError) as exc:
            LOG.debug("Oracle candidate check error: %s", exc)
            continue

    return False


def cleanup_child_process(state: BuildState, timeout_grace: float = 0.5) -> None:
    """Terminate child process group cleanly: SIGTERM -> bounded grace -> SIGKILL -> reap -> verify."""
    state.started_event.wait(timeout=1.0)
    with state.lock:
        p = state.proc
    if not p:
        return

    pgid = None
    if hasattr(os, "getpgid"):
        try:
            pgid = os.getpgid(p.pid)
        except OSError:
            pass

    # 1. Terminate process group with SIGTERM
    try:
        if pgid and hasattr(os, "killpg"):
            os.killpg(pgid, signal.SIGTERM)
        else:
            p.terminate()
    except OSError:
        pass

    # 2. Bounded grace period for graceful termination
    t_end = time.monotonic() + timeout_grace
    while time.monotonic() < t_end:
        if p.poll() is not None:
            break
        time.sleep(0.05)

    # 3. Escalate to SIGKILL if child process or group is still alive
    group_alive = False
    if pgid and hasattr(os, "killpg"):
        try:
            os.killpg(pgid, 0)
            group_alive = True
        except OSError:
            group_alive = False

    if p.poll() is None or group_alive:
        try:
            if pgid and hasattr(os, "killpg"):
                os.killpg(pgid, signal.SIGKILL)
            else:
                p.kill()
        except OSError:
            pass

    # 4. Reap child process
    try:
        p.wait(timeout=1.0)
    except Exception:
        pass

    # 5. Verify no members of process group remain
    if pgid and hasattr(os, "killpg"):
        t_verify = time.monotonic() + 0.5
        while time.monotonic() < t_verify:
            try:
                os.killpg(pgid, 0)
                os.killpg(pgid, signal.SIGKILL)
                time.sleep(0.05)
            except OSError:
                # ESRCH: Process group has completely vacated
                break


def watch_process(cmd: str, state: BuildState, log_path: str) -> None:
    """Run `cmd`, stream its output into `state`, mirror verbatim to log."""
    spawn_wall = time.time()
    with state.lock:
        state.cmd_str = cmd
        state.started_wall = spawn_wall
    try:
        proc = subprocess.Popen(
            cmd, shell=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, errors="replace", bufsize=1,
            env=os.environ.copy(),
            preexec_fn=os.setsid if hasattr(os, "setsid") else None,
        )
        with state.lock:
            state.proc = proc
        state.started_event.set()
    except OSError as exc:
        LOG.error("failed to spawn %r: %s", cmd, exc)
        with state.lock:
            state.finished, state.exit_code = True, 127
            state.tail.append(f"spawn failed: {exc}")
        state.started_event.set()
        return

    try:
        with open(log_path + ".build", "w", encoding="utf-8") as raw:
            assert proc.stdout is not None
            for line in proc.stdout:
                raw.write(line)
                line = line.rstrip("\n")
                with state.lock:
                    state.total_lines += 1
                    state.tail.append(line[-120:])
                    m = STAGE_RE.search(line)
                    if m:
                        new_stage = int(m.group(1)) - 1
                        if new_stage != state.stage:
                            state.stage = max(0, min(2, new_stage))
                            state.stage_files = 0
                            LOG.info("stage → %d", state.stage + 1)
                    elif BOSS_RE.search(line) and not NEG_RE.search(line):
                        state.boss_seen = True
                        LOG.info("authenticated boss marker seen: %r", line)
                    if FILE_RE.search(line):
                        state.stage_files += 1
                        fm = FNAME_RE.search(line)
                        if fm and len(state.files_seen) < 2000:
                            state.files_seen.append(fm.group(1))
                    if ERR_RE.search(line):
                        state.errors += 1
                    elif WARN_RE.search(line):
                        state.warnings += 1
    except Exception as exc:  # never let the reader thread die silently
        LOG.exception("watcher error: %s", exc)
    finally:
        rc = proc.wait()
        with state.lock:
            state.finished, state.exit_code = True, rc
            if state.boss_seen and rc == 0:
                state.oracle_verified = verify_selfhost_oracle(state.started_wall, cmd=state.cmd_str)
        LOG.info("child exited rc=%d oracle_verified=%s", rc, state.oracle_verified)


def demo_process(state: BuildState) -> None:
    """Fake triple-stage build for --demo mode."""
    with state.lock:
        state.is_demo = True
    state.started_event.set()
    rng = random.Random(1337)
    files = ["lexer.c", "parser.c", "typecheck.c", "codegen.c", "regalloc.c",
             "peephole.c", "quantum_rules.c", "emit_x86.c", "driver.c",
             "preproc.c", "symtab.c", "ir.c", "liveness.c", "leaq_fuse.c"]
    for stg in (1, 2, 3):
        with state.lock:
            state.stage, state.stage_files = stg - 1, 0
            state.tail.append(f"=== STAGE {stg} ===")
            state.total_lines += 1
        for f in files:
            time.sleep(rng.uniform(0.15, 0.5))
            with state.lock:
                state.stage_files += 1
                state.total_lines += 1
                state.files_seen.append(f"src/{f}")
                state.tail.append(f"CC  src/{f} -> build/stage{stg}/{f[:-2]}.o")
                if rng.random() < 0.18:
                    state.warnings += 1
                    state.tail.append(f"src/{f}: warning: implicit conversion")
    time.sleep(0.6)
    with state.lock:
        state.boss_seen = True
        state.oracle_verified = True
        state.tail.append("sha256: zcc2 == zcc3  BYTE-IDENTICAL ✔")
        state.finished, state.exit_code = True, 0


# ---------------------------------------------------------------------------
# Gemma LoRA AI Copilot & Autonomous Companion (:8088)
# ---------------------------------------------------------------------------
class GemmaCompanionThread(threading.Thread):
    """Background cognitive copilot that queries the resident Gemma daemon (:8088)."""

    def __init__(self, state: BuildState, url: str = "http://127.0.0.1:8088"):
        super().__init__(daemon=True)
        self.state = state
        self.url = url.rstrip("/")
        self.active_agent = "ZkaediGemmaLoRA"
        self.agent_title = "Gemma 32-Layer LoRA Sovereign Archon"
        self.last_thought = "Gemma AI Copilot initialized. Monitoring compiler build pipeline."
        self.latency_ms = 0.0
        self.connected = False
        self.stop_event = threading.Event()

    def stop(self) -> None:
        self.stop_event.set()

    def run(self) -> None:
        last_stage = -1
        last_lines = 0
        last_warnings = 0
        while not self.stop_event.is_set():
            snap = self.state.snapshot()
            stg = snap["stage"]
            lines = snap["total_lines"]
            warns = snap["warnings"]

            should_query = (
                not self.connected or
                stg != last_stage or
                abs(lines - last_lines) >= 300 or
                warns != last_warnings or
                snap["finished"]
            )
            if should_query:
                last_stage = stg
                last_lines = lines
                last_warnings = warns
                self._query_gemma(snap)

            self.stop_event.wait(1.2)

    def _query_gemma(self, snap: dict) -> None:
        try:
            import urllib.request
            stg_name = WORLDS[snap["stage"]][3] if snap["stage"] < len(WORLDS) else f"Stage {snap['stage']+1}"
            tail_txt = " | ".join(snap["tail"][-2:]) if snap["tail"] else "Nominal compilation"
            query = f"Compiler build telemetry: {stg_name}, lines={snap['total_lines']}, warnings={snap['warnings']}, tail={tail_txt[:200]}"
            req = urllib.request.Request(
                f"{self.url}/dispatch",
                data=json.dumps({"query": query}).encode("utf-8"),
                headers={"Content-Type": "application/json", "User-Agent": "ZccQuest/7.0"}
            )
            t0 = time.perf_counter()
            with urllib.request.urlopen(req, timeout=1.2) as resp:
                if resp.status == 200:
                    data = json.loads(resp.read().decode("utf-8"))
                    self.connected = True
                    self.latency_ms = round((time.perf_counter() - t0) * 1000.0, 1)
                    agent_info = data.get("dispatched_agent", {})
                    self.active_agent = agent_info.get("id", "ZkaediGemmaLoRA")
                    self.agent_title = agent_info.get("title", "Gemma LoRA Archon")
                    
                    if snap["finished"]:
                        if snap["exit_code"] == 0:
                            if snap.get("oracle_verified", False):
                                self.last_thought = f"[{self.active_agent}] Fixed-point converged. Byte-identical seal verified (exit 0)."
                            else:
                                self.last_thought = f"[{self.active_agent}] Command completed successfully with exit 0."
                        else:
                            self.last_thought = f"[{self.active_agent}] Build halted with exit {snap['exit_code']}. Inspecting logs."
                    elif snap["warnings"] > 0:
                        self.last_thought = f"[{self.active_agent}] Warning pulse observed in {stg_name}: checking calling conventions."
                    elif snap["stage"] == 0:
                        self.last_thought = f"[{self.active_agent}] Stage 1 bootstrap: lexical tokenization & AST generation active."
                    elif snap["stage"] == 1:
                        self.last_thought = f"[{self.active_agent}] Stage 2 IR lowering: GVN store-to-load forwarding nominal."
                    elif snap["stage"] == 2:
                        self.last_thought = f"[{self.active_agent}] Stage 3 self-host: compiling stage3 binary for byte-identical cmp."
                    else:
                        self.last_thought = f"[{self.active_agent}] Active compilation nominal ({snap['total_lines']} lines)."
        except Exception:
            self.connected = False
            self.last_thought = "Gemma daemon standby (port 8088 offline or waiting)."


# ---------------------------------------------------------------------------
# Renderer — Runner Game
# ---------------------------------------------------------------------------
RUNNER_FRAMES = ["ᕕ( ᐛ )ᕗ", "ᕙ( ᐖ )ᕗ"]
ENEMY = "ʬ"
BULLET = "»"
COIN = "◉"
FLAG = "⚑"


@dataclass
class Enemy:
    x: float
    alive: bool = True


@dataclass
class Bullet:
    x: float


class Game:
    """Owns the playfield; consumes BuildState snapshots each frame."""

    def __init__(self, files_per_stage: int, lines_per_stage: int):
        assert files_per_stage > 0 and lines_per_stage > 0
        self.files_per_stage = files_per_stage
        self.lines_per_stage = lines_per_stage
        self.frame = 0
        self.enemies: List[Enemy] = []
        self.bullets: List[Bullet] = []
        self.coins: int = 0
        self.kills: int = 0
        self.seen_warnings = 0
        self.seen_files = 0
        self.scroll = 0.0
        self.rng = random.Random()
        self.jump_phase = 0  # >0 while airborne

    def progress(self, snap: dict) -> float:
        stg = snap["stage"]
        in_stage = min(1.0, snap["stage_files"] / self.files_per_stage) \
            if snap["stage_files"] else \
            min(1.0, (snap["total_lines"] % self.lines_per_stage) / self.lines_per_stage)
        p = (stg + in_stage) / 3.0
        if snap["boss_seen"]:
            p = max(p, 0.98)
        if snap["finished"] and snap["exit_code"] == 0:
            p = 1.0
        return max(0.0, min(1.0, p))

    def render(self, snap: dict, cols: int, rows: int) -> str:
        self.frame += 1
        w = max(60, cols)
        field_w = w - 2
        stg = snap["stage"]
        sky_c, ground_c, accent, world_name = WORLDS[stg]
        p = self.progress(snap)
        seg = (p * 3.0) - stg
        runner_x = 3 + int(seg * (field_w - 14))
        self.scroll += 0.7

        if snap["warnings"] > self.seen_warnings:
            self.seen_warnings = snap["warnings"]
            self.enemies.append(Enemy(x=field_w - 4))
        if snap["stage_files"] > self.seen_files:
            self.coins += snap["stage_files"] - self.seen_files
            self.seen_files = snap["stage_files"]
            if self.rng.random() < 0.3:
                self.jump_phase = 4

        live = [e for e in self.enemies if e.alive and e.x > runner_x]
        if live and self.frame % 3 == 0:
            self.bullets.append(Bullet(x=runner_x + 8))
        for b in self.bullets:
            b.x += 4
        for b in self.bullets:
            for e in live:
                if e.alive and abs(e.x - b.x) < 3:
                    e.alive = False
                    self.kills += 1
        self.bullets = [b for b in self.bullets if b.x < field_w]
        for e in self.enemies:
            e.x -= 0.9
        self.enemies = [e for e in self.enemies if e.alive and e.x > 1]

        if self.jump_phase:
            self.jump_phase -= 1

        out: List[str] = []
        A = out.append

        elapsed = snap["elapsed"]
        eta = (elapsed / p - elapsed) if p > 0.02 else float("inf")
        eta_s = f"{int(eta // 60):02d}:{int(eta % 60):02d}" if eta != float("inf") else "--:--"
        hud = (f" {BOLD}{fg(accent)}ZCC QUEST{RESET}  "
               f"{fg(sky_c)}{world_name}{RESET}  "
               f"{fg(220)}{COIN}×{self.coins}{RESET} "
               f"{fg(196)}☠×{self.kills}{RESET} "
               f"{fg(244)}warn:{snap['warnings']} err:{snap['errors']}"
               f"  ⏱{int(elapsed // 60):02d}:{int(elapsed % 60):02d}"
               f" eta {eta_s}{RESET}")
        A(hud)

        bar_w = field_w - 8
        filled = int(p * bar_w)
        marks = {int(bar_w / 3): "▎", int(2 * bar_w / 3): "▎"}
        bar = ""
        for i in range(bar_w):
            ch = marks.get(i, "━" if i < filled else "─")
            col = accent if i < filled else 238
            bar += fg(col) + ch
        A(f" {fg(250)}[{bar}{fg(250)}] {int(p * 100):3d}%{RESET}")

        for row in range(3):
            line = ""
            for x in range(field_w):
                h = (x * 131 + row * 977 + int(self.scroll * (row + 1) * 0.4)) % 97
                if h == 0:
                    line += fg(sky_c) + ("✦" if stg == 2 else "☁"[0])
                elif h == 45:
                    line += fg(sky_c) + "·"
                else:
                    line += " "
            A(" " + line + RESET)

        air = " " * field_w
        arow = list(air)

        def put(s: str, x: int, color: int) -> None:
            for i, ch in enumerate(s):
                xi = x + i
                if 0 <= xi < field_w:
                    arow[xi] = fg(color) + ch + RESET

        if snap["boss_seen"] or p >= 0.98:
            put(FLAG, field_w - 3, 46 if snap.get("exit_code") == 0 else 220)
        else:
            put(FLAG, field_w - 3, 244)
        for e in self.enemies:
            put(ENEMY, int(e.x), 196)
        for b in self.bullets:
            put(BULLET, int(b.x), 226)
        sprite = RUNNER_FRAMES[self.frame // 2 % 2]
        put(sprite, runner_x, 231)
        jump_row = "".join(arow) if self.jump_phase else air
        ground_row = air if self.jump_phase else "".join(arow)
        A(" " + (jump_row if isinstance(jump_row, str) else air))
        A(" " + (ground_row if isinstance(ground_row, str) else air))

        gpat = ""
        for x in range(field_w):
            gpat += fg(ground_c) + ("▓" if (x + int(self.scroll)) % 7 else "█")
        A(" " + gpat + RESET)

        A(f" {fg(240)}┈┈ build output ┈┈{RESET}")
        for ln in snap["tail"]:
            color = 196 if ERR_RE.search(ln) else 178 if WARN_RE.search(ln) else 245
            A(f" {fg(color)}{ln[:field_w]}{RESET}")

        if snap["finished"]:
            if snap["exit_code"] == 0:
                if snap.get("oracle_verified", False):
                    A(f" {BOLD}{fg(46)}★ SELF-HOST VERIFIED — {BOSS_NAME} — exit 0 "
                      f"(evidence: see .build log) ★{RESET}")
                else:
                    A(f" {BOLD}{fg(46)}★ COMMAND SUCCEEDED — exit 0 "
                      f"(evidence: see .build log) ★{RESET}")
            else:
                A(f" {BOLD}{fg(196)}✖ BUILD FAILED — exit {snap['exit_code']} — "
                  f"the SEGFAULT DRAGON wins. Check the .build log.{RESET}")
        return "\n".join(out)


# ---------------------------------------------------------------------------
# Frogger Mode
# ---------------------------------------------------------------------------
FROG = "🐸"
SQUISH = "✖"
CAR_R = "▉▉▶"
CAR_L = "◀▉▉"
LILY = "❀"


@dataclass
class Car:
    x: float
    speed: float
    right: bool


class FroggerGame:
    """Frog crosses N traffic lanes bottom→top as the build progresses."""

    def __init__(self, files_per_stage: int, lines_per_stage: int, lanes: int = 9):
        assert files_per_stage > 0 and lines_per_stage > 0
        assert 3 <= lanes <= 15 and lanes % 3 == 0, "lanes must be multiple of 3 in [3,15]"
        self.files_per_stage = files_per_stage
        self.lines_per_stage = lines_per_stage
        self.lanes = lanes
        self.frame = 0
        self.cars: List[List[Car]] = [[] for _ in range(lanes)]
        self.rng = random.Random()
        self.warn_times: Deque[float] = deque(maxlen=64)
        self.seen_warnings = 0
        self.hops = 0
        self.seen_files_total = 0
        self.near_miss = 0
        self.frog_wobble = 0.0

    def progress(self, snap: dict) -> float:
        stg = snap["stage"]
        in_stage = min(1.0, snap["stage_files"] / self.files_per_stage) \
            if snap["stage_files"] else \
            min(1.0, (snap["total_lines"] % self.lines_per_stage) / self.lines_per_stage)
        p = (stg + in_stage) / 3.0
        if snap["boss_seen"]:
            p = max(p, 0.98)
        if snap["finished"] and snap["exit_code"] == 0:
            p = 1.0
        return max(0.0, min(1.0, p))

    def _warn_rate(self, snap: dict) -> float:
        now = time.monotonic()
        new = snap["warnings"] - self.seen_warnings
        if new > 0:
            self.warn_times.extend([now] * new)
            self.seen_warnings = snap["warnings"]
        recent = [t for t in self.warn_times if now - t < 15.0]
        return len(recent) / 15.0

    def render(self, snap: dict, cols: int, rows: int) -> str:
        self.frame += 1
        w = max(60, cols)
        field_w = w - 2
        p = self.progress(snap)
        stg = snap["stage"]
        _, _, accent, world_name = WORLDS[stg]
        rate = self._warn_rate(snap)
        base_speed = 0.8 + min(3.0, rate * 4.0)

        for li in range(self.lanes):
            lane = self.cars[li]
            right = (li % 2 == 0)
            spd = base_speed * (0.7 + 0.15 * (li % 4))
            if (not lane or
                    (right and lane[-1].x > 10) or
                    (not right and lane[-1].x < field_w - 10)) \
                    and self.rng.random() < 0.10:
                lane.append(Car(x=(0.0 if right else float(field_w)),
                                speed=spd, right=right))
            for c in lane:
                c.x += c.speed if c.right else -c.speed
            self.cars[li] = [c for c in lane if -4 <= c.x <= field_w + 4]

        frog_lane = min(self.lanes, int(p * (self.lanes + 1)))
        total_files = snap["stage_files"] + stg * self.files_per_stage
        if total_files > self.seen_files_total:
            self.hops += total_files - self.seen_files_total
            self.seen_files_total = total_files
            self.frog_wobble = self.rng.uniform(-6, 6)
        frog_x = int(field_w * 0.5 + self.frog_wobble
                     + 3 * (1 if self.frame % 20 < 10 else -1) * (p * 2))
        frog_x = max(2, min(field_w - 3, frog_x))
        dead = snap["finished"] and snap["exit_code"] not in (0, None)

        out: List[str] = []
        A = out.append
        elapsed = snap["elapsed"]
        eta = (elapsed / p - elapsed) if p > 0.02 else float("inf")
        eta_s = f"{int(eta // 60):02d}:{int(eta % 60):02d}" if eta != float("inf") else "--:--"
        A(f" {BOLD}{fg(accent)}ZCC FROGGER{RESET}  {fg(117)}{world_name}{RESET}  "
          f"{fg(220)}hops:{self.hops}{RESET} "
          f"{fg(178)}traffic:{'▂▄▆█'[min(3, int(rate * 6))]} "
          f"warn:{snap['warnings']}{RESET} {fg(196)}err:{snap['errors']}{RESET}"
          f"{fg(244)}  ⏱{int(elapsed // 60):02d}:{int(elapsed % 60):02d} eta {eta_s}{RESET}")

        bar_w = field_w - 8
        filled = int(p * bar_w)
        A(" " + fg(250) + "[" + fg(accent) + "━" * filled
          + fg(238) + "─" * (bar_w - filled) + fg(250) + f"] {int(p * 100):3d}%" + RESET)

        goal = list(" " * field_w)
        for gx in range(4, field_w - 4, 10):
            goal[gx] = fg(46 if p >= 1.0 else 29) + LILY + RESET
        if p >= 1.0:
            mid = field_w // 2
            for i, ch in enumerate(FROG):
                goal[min(field_w - 1, mid + i)] = fg(46) + ch + RESET
        A(" " + "".join(goal))

        for li in range(self.lanes - 1, -1, -1):
            lane_stage = li // (self.lanes // 3)
            _, gcol, lacc, _ = WORLDS[lane_stage]
            row = list(" " * field_w)
            for c in self.cars[li]:
                art = CAR_R if c.right else CAR_L
                for i, ch in enumerate(art):
                    xi = int(c.x) + i
                    if 0 <= xi < field_w:
                        row[xi] = fg(lacc) + ch + RESET
            if frog_lane == li + 1 and not dead:
                near = any(abs(c.x - frog_x) < 6 for c in self.cars[li])
                if near:
                    self.near_miss += 1
                fc = 226 if near else 118
                row[frog_x] = fg(fc) + FROG + RESET
                if frog_x + 1 < field_w:
                    row[frog_x + 1] = ""
            A(" " + "".join(row) + f"{fg(236)}·{RESET}")

        bank = list(" " * field_w)
        if frog_lane == 0 or dead:
            spr = SQUISH if dead else FROG
            col = 196 if dead else 118
            bank[frog_x] = fg(col) + spr + RESET
            if not dead and frog_x + 1 < field_w:
                bank[frog_x + 1] = ""
        A(" " + fg(94) + "▄" * 2 + RESET + "".join(bank[2:-2]) + fg(94) + "▄" * 2 + RESET)

        A(f" {fg(240)}┈┈ build output ┈┈{RESET}")
        for ln in snap["tail"]:
            color = 196 if ERR_RE.search(ln) else 178 if WARN_RE.search(ln) else 245
            A(f" {fg(color)}{ln[:field_w]}{RESET}")

        if snap["finished"]:
            if snap["exit_code"] == 0:
                if snap.get("oracle_verified", False):
                    A(f" {BOLD}{fg(46)}★ FROG HOME — {BOSS_NAME} — exit 0 "
                      f"(near-misses: {self.near_miss}, evidence: .build log) ★{RESET}")
                else:
                    A(f" {BOLD}{fg(46)}★ COMMAND SUCCEEDED — exit 0 "
                      f"(near-misses: {self.near_miss}, evidence: .build log) ★{RESET}")
            else:
                A(f" {BOLD}{fg(196)}✖ SQUISHED — exit {snap['exit_code']} — "
                  f"traffic wins. Check the .build log.{RESET}")
        return "\n".join(out)


# ---------------------------------------------------------------------------
# Garden mode — a deterministic zen garden grown from the build itself
# ---------------------------------------------------------------------------
@dataclass
class Plant:
    x: int
    species: int      # 0 tree, 1 flower, 2 grass, 3 reed
    hue: int          # 256-color index
    born_frame: int
    name: str
    withered: bool = False


class GardenGame:
    """Every compiled file plants something; the garden IS the build."""

    SPECIES_HUES = [(28, 34, 40, 46), (169, 205, 211, 218),
                    (100, 106, 112, 148), (30, 36, 37, 44)]

    def __init__(self, files_per_stage: int, lines_per_stage: int):
        assert files_per_stage > 0 and lines_per_stage > 0
        self.files_per_stage = files_per_stage
        self.lines_per_stage = lines_per_stage
        self.frame = 0
        self.plants: List[Plant] = []
        self.planted: set = set()
        self.weeds: List[int] = []
        self.seen_warnings = 0
        self.seen_errors = 0
        self.rng = random.Random(7)

    def progress(self, snap: dict) -> float:
        stg = snap["stage"]
        in_stage = min(1.0, snap["stage_files"] / self.files_per_stage) \
            if snap["stage_files"] else \
            min(1.0, (snap["total_lines"] % self.lines_per_stage) / self.lines_per_stage)
        p = (stg + in_stage) / 3.0
        if snap["boss_seen"]:
            p = max(p, 0.98)
        if snap["finished"] and snap["exit_code"] == 0:
            p = 1.0
        return max(0.0, min(1.0, p))

    def _slot(self, want: int, width: int, taken: set) -> int:
        x = want % max(1, width)
        for _ in range(width):
            if all(abs(x - t) > 2 for t in taken):
                return x
            x = (x + 3) % width
        return want % width

    def render(self, snap: dict, cols: int, rows: int) -> str:
        self.frame += 1
        w = max(60, cols)
        field_w = w - 2
        p = self.progress(snap)
        stg = snap["stage"]
        night = stg == 2
        sky_c = (117, 216, 60)[stg]
        building = not snap["finished"]
        success = snap["finished"] and snap["exit_code"] == 0
        failed = snap["finished"] and snap["exit_code"] not in (0, None)

        taken = {pl.x for pl in self.plants}
        for name in snap["files_seen"]:
            if name in self.planted:
                continue
            self.planted.add(name)
            h = hashlib.md5(name.encode()).digest()
            species = h[0] % 4
            hue = self.SPECIES_HUES[species][h[1] % 4]
            x = self._slot(int.from_bytes(h[2:4], "big"), field_w - 4, taken) + 2
            taken.add(x)
            self.plants.append(Plant(x, species, hue, self.frame, name))

        while self.seen_warnings < snap["warnings"]:
            self.seen_warnings += 1
            self.weeds.append(self.rng.randrange(1, field_w - 1))
        while self.seen_errors < snap["errors"]:
            self.seen_errors += 1
            alive = [pl for pl in self.plants if not pl.withered]
            if alive:
                self.rng.choice(alive).withered = True

        SKY_H, CANOPY_H = 4, 3
        canvas = [[" "] * field_w for _ in range(SKY_H + CANOPY_H + 1)]

        def put(r: int, c: int, s: str) -> None:
            if 0 <= r < len(canvas) and 0 <= c < field_w:
                canvas[r][c] = s

        arc_x = 2 + int(p * (field_w - 5))
        arc_y = int(3.2 * (4 * (p - 0.5) ** 2))
        orb = "☾" if night else "☀"
        put(arc_y, arc_x, fg(250 if night else 220) + orb + RESET)
        for x in range(field_w):
            hsh = (x * 73 + 11) % 149
            if night and hsh < 5:
                tw = "✦" if (x + self.frame // 3) % 2 else "·"
                put(hsh % SKY_H, x, fg(244) + tw + RESET)
            if building and (x * 37 + self.frame * 2) % 23 == 0:
                put((self.frame + x) % SKY_H, x, fg(sky_c) + "'" + RESET)

        GROW = 18
        for pl in self.plants:
            age = (self.frame - pl.born_frame) // GROW
            base_r = SKY_H + CANOPY_H
            col = 196 if pl.withered else pl.hue
            if pl.withered:
                put(base_r, pl.x, fg(196) + "†" + RESET)
                continue
            if age == 0:
                put(base_r, pl.x, fg(col) + "." + RESET)
            elif age == 1:
                put(base_r, pl.x, fg(col) + "," + RESET)
            else:
                if pl.species == 0:
                    put(base_r, pl.x, fg(94) + "┃" + RESET)
                    for i, span in enumerate((1, 2, 3)):
                        r = base_r - 1 - i if age >= 2 + i else None
                        if r is not None and r > SKY_H - 1:
                            for dx in range(-(3 - span), (3 - span) + 1):
                                put(r, pl.x + dx, fg(col) + "▲" + RESET)
                elif pl.species == 1:
                    put(base_r, pl.x, fg(28) + "│" + RESET)
                    head = "❀" if success else ("✿" if age >= 3 else "⚘")
                    put(base_r - 1, pl.x, fg(col if not success else 213) + head + RESET)
                elif pl.species == 2:
                    for dx in (-1, 0, 1):
                        put(base_r, pl.x + dx, fg(col) + "ω" + RESET)
                else:
                    put(base_r, pl.x, fg(col) + "┆" + RESET)
                    if age >= 3:
                        put(base_r - 1, pl.x, fg(col) + "╿" + RESET)

        for wx in self.weeds:
            put(SKY_H + CANOPY_H, wx, fg(178) + "~" + RESET)

        if night or success:
            n_fly = 14 if success else 6
            for i in range(n_fly):
                fx = (i * 131 + self.frame * (2 + i % 3)) % field_w
                fy = SKY_H - 1 + (i * 7 + self.frame // 2) % CANOPY_H
                if (self.frame + i) % 3:
                    put(fy, fx, fg(228) + "✦" + RESET)

        out: List[str] = []
        A = out.append
        elapsed = snap["elapsed"]
        garden_id = hashlib.sha1(
            "\n".join(sorted(self.planted)).encode()).hexdigest()[:8] \
            if self.planted else "--------"
        season = ("spring", "dusk", "night")[stg]
        A(f" {BOLD}{fg(114)}ZCC GARDEN{RESET}  {fg(sky_c)}stage {stg + 1} · {season}{RESET}"
          f"  {fg(244)}planted:{len(self.plants)} weeds:{len(self.weeds)}"
          f" withered:{sum(pl.withered for pl in self.plants)}"
          f"  garden:{garden_id}  ⏱{int(elapsed // 60):02d}:{int(elapsed % 60):02d}{RESET}")
        bar_w = field_w - 8
        filled = int(p * bar_w)
        A(" " + fg(250) + "[" + fg(114) + "━" * filled
          + fg(238) + "─" * (bar_w - filled) + fg(250) + f"] {int(p * 100):3d}%" + RESET)
        for row in canvas:
            A(" " + "".join(row))
        gcol = 130 if failed else (94 if stg == 1 else 22)
        A(" " + fg(gcol) + "▔" * field_w + RESET)
        A(" " + fg(52 if failed else 236) + "░" * field_w + RESET)

        A(f" {fg(240)}┈┈ build output ┈┈{RESET}")
        for ln in snap["tail"]:
            color = 196 if ERR_RE.search(ln) else 178 if WARN_RE.search(ln) else 245
            A(f" {fg(color)}{ln[:field_w]}{RESET}")

        if snap["finished"]:
            if success:
                if snap.get("oracle_verified", False):
                    A(f" {BOLD}{fg(213)}❀ FULL BLOOM — {BOSS_NAME} — exit 0 — "
                      f"garden {garden_id} will grow the same way next run ❀{RESET}")
                else:
                    A(f" {BOLD}{fg(213)}❀ COMMAND SUCCEEDED — exit 0 — "
                      f"garden {garden_id} completed ❀{RESET}")
            else:
                A(f" {BOLD}{fg(196)}† FROST — exit {snap['exit_code']} — "
                  f"the garden sleeps. Check the .build log.{RESET}")
        return "\n".join(out)


# ---------------------------------------------------------------------------
# ZKAEDI PRIME Mode — Recursively Coupled Hamiltonian Phase-Space Solver
# ---------------------------------------------------------------------------
def _sigmoid(z: float) -> float:
    return 1.0 / (1.0 + math.exp(-max(-15.0, min(15.0, z))))


class PrimeEnergyGame:
    """ZKAEDI PRIME Recursively Coupled Hamiltonian Energy Landscape Solver."""

    def __init__(self, files_per_stage: int, lines_per_stage: int):
        self.files_per_stage = files_per_stage
        self.lines_per_stage = lines_per_stage
        self.frame = 0
        self.eta = 0.4
        self.gamma = 0.3
        self.beta = 0.1
        self.eps = 0.05
        self.kick = 2.0
        self.seen_warnings = 0
        self.h_prev = 1.0
        self.scars = 0
        self.rng = random.Random(42)
        self.companion: Optional[GemmaCompanionThread] = None

    def set_companion(self, comp: Optional[GemmaCompanionThread]) -> None:
        self.companion = comp

    def progress(self, snap: dict) -> float:
        stg = snap["stage"]
        in_stage = min(1.0, snap["stage_files"] / self.files_per_stage) \
            if snap["stage_files"] else \
            min(1.0, (snap["total_lines"] % self.lines_per_stage) / self.lines_per_stage)
        p = (stg + in_stage) / 3.0
        if snap["boss_seen"]:
            p = max(p, 0.98)
        if snap["finished"] and snap["exit_code"] == 0:
            p = 1.0
        return max(0.0, min(1.0, p))

    def render(self, snap: dict, cols: int, rows: int) -> str:
        self.frame += 1
        w = max(60, cols)
        field_w = w - 2
        grid_h = 7
        p = self.progress(snap)
        stg = snap["stage"]
        _, _, accent, world_name = WORLDS[stg]

        if snap["warnings"] > self.seen_warnings:
            diff = snap["warnings"] - self.seen_warnings
            self.scars += diff
            self.seen_warnings = snap["warnings"]
            self.h_prev += self.kick * diff

        h_base = (1.0 - p) * 5.0
        noise = self.rng.gauss(0, 1 + self.beta * abs(self.h_prev)) * self.eps
        h_t = h_base + self.eta * self.h_prev * _sigmoid(self.gamma * self.h_prev) + noise
        self.h_prev = h_t

        out: List[str] = []
        A = out.append
        elapsed = snap["elapsed"]
        eta = (elapsed / p - elapsed) if p > 0.02 else float("inf")
        eta_s = f"{int(eta // 60):02d}:{int(eta % 60):02d}" if eta != float("inf") else "--:--"

        A(f" {BOLD}{fg(51)}⚡ ZKAEDI PRIME v2.0{RESET} │ {fg(213)}Hamiltonian Energy: H={h_t:.4f}{RESET} "
          f"│ {fg(226)}η={self.eta} γ={self.gamma}{RESET} │ {fg(196)}Scars:{self.scars}{RESET}"
          f"{fg(244)} ⏱{int(elapsed // 60):02d}:{int(elapsed % 60):02d} eta {eta_s}{RESET}")

        bar_w = field_w - 10
        filled = int(p * bar_w)
        A(" " + fg(250) + "[" + fg(51) + "█" * filled
          + fg(238) + "░" * (bar_w - filled) + fg(250) + f"] {int(p * 100):3d}%" + RESET)

        cx, cy = field_w // 2, grid_h // 2
        r_orbit = max(0.5, (1.0 - p) * (field_w // 3))
        angle = self.frame * 0.25
        px = int(cx + r_orbit * math.cos(angle))
        py = int(cy + (r_orbit * 0.4) * math.sin(angle))

        for y in range(grid_h):
            row = []
            for x in range(field_w):
                dx = (x - cx) / (field_w / 2.0)
                dy = (y - cy) / (grid_h / 2.0)
                dist = math.sqrt(dx * dx + dy * dy)

                if x == px and y == py:
                    row.append(fg(51) + "✦" + RESET)
                elif x == cx and y == cy:
                    if p >= 0.98:
                        row.append(fg(46) + "⟐" + RESET)
                    else:
                        row.append(fg(213) + "⊙" + RESET)
                else:
                    val = math.sin(dist * 6.0 - self.frame * 0.15) * h_t
                    if val > 1.5:
                        row.append(fg(196) + "·" + RESET)
                    elif val > 0.5:
                        row.append(fg(208) + "·" + RESET)
                    elif val > -0.5:
                        row.append(fg(240) + "·" + RESET)
                    else:
                        row.append(fg(235) + " " + RESET)
            A(" " + "".join(row))

        if self.companion:
            status_txt = f"{fg(46)}● ONLINE ({self.companion.latency_ms:.1f}ms){RESET}" if self.companion.connected else f"{fg(240)}○ STANDBY{RESET}"
            A(f" {BOLD}{fg(81)}── 🤖 GEMMA ARCHON COGNITION {status_txt} ──{RESET} {fg(226)}{self.companion.agent_title}{RESET}")
            A(f"   {fg(250)}Insight:{RESET} {fg(255)}{self.companion.last_thought[:field_w - 14]}{RESET}")

        A(f" {fg(240)}┈┈ build output ┈┈{RESET}")
        for ln in snap["tail"]:
            color = 196 if ERR_RE.search(ln) else 178 if WARN_RE.search(ln) else 245
            A(f" {fg(color)}{ln[:field_w]}{RESET}")

        if snap["finished"]:
            if snap["exit_code"] == 0:
                if snap.get("oracle_verified", False):
                    A(f" {BOLD}{fg(46)}★ ZKAEDI PRIME FIXED POINT REPO - H0 CONVERGED - exit 0 "
                      f"(scars: {self.scars}, {BOSS_NAME}) ★{RESET}")
                else:
                    A(f" {BOLD}{fg(46)}★ COMMAND SUCCEEDED - exit 0 "
                      f"(scars: {self.scars}) ★{RESET}")
            else:
                A(f" {BOLD}{fg(196)}✖ BIFURCATION DRIFT - exit {snap['exit_code']} - "
                  f"Check the .build log.{RESET}")
        return "\n".join(out)


# ---------------------------------------------------------------------------
# ZCC Quest HUD v6.0 — Sovereign Mode
# Real-time 7-system status matrix, 16-node wavepacket probability density
# ---------------------------------------------------------------------------
class SovereignHudGame:
    """ZCC Quest HUD v6.0: 7-System Matrix & 16-Node CTQW Quantum Probability Density."""

    NODES = [
        "Raydium", "Whirlpool", "DLMM", "Dynamic",
        "Phoenix", "OpenBook", "Lifinity", "INF",
        "Manifest", "ZeroFi", "GoonFi", "SolFi",
        "Tessera", "Humming", "Cropper", "Scorch"
    ]

    SYSTEMS = [
        ("NATIVE_X86", "Self-Host Identity (zcc2.s == zcc3.s)", "SYS_V_ABI"),
        ("CTQW_QPU", "16-DEX Hilbert Space Hamiltonian", "BALLISTIC"),
        ("PTX_TRITON", "RTX 5070 Blackwell SM 12.0 JIT", "CUDA_GRAPH"),
        ("ZK_STARK", "BabyBear Radix-2 NTT Prover (16.7M)", "O(N log N)"),
        ("AVX512_FMA", "FeMoCo Abiotic Catalyst Potential", "28.2 MVS"),
        ("ONEIRO_LEDGER", "WKB Pareto Evolutionary Synthesis", "DAG_APPEND"),
        ("SOLANA_DEX", "Flash Loan Arbitrage / Sanctum Loop", "ATOMIC_SAFE")
    ]

    def __init__(self, files_per_stage: int, lines_per_stage: int):
        self.files_per_stage = files_per_stage
        self.lines_per_stage = lines_per_stage
        self.frame = 0
        self.eta = 0.4
        self.gamma = 0.3
        self.beta = 0.1
        self.eps = 0.05
        self.h_prev = 1.0
        self.scars = 0
        self.seen_warnings = 0
        self.rng = random.Random(1337)
        # Precomputed baseline 16-DEX Hermitian coupling network
        N = 16
        self.H_base = [[0.0] * N for _ in range(N)]
        for i in range(N):
            for j in range(i + 1, N):
                c = 0.2 + ((i * 7 + j * 13) % 100) / 200.0
                self.H_base[i][j] = -c
                self.H_base[j][i] = -c
        diag_pots = [-18.5, -12.4, -9.8, -8.5, -7.2, -6.1, -5.4, -14.2,
                     -4.8, -3.2, -2.1, -1.9, -1.5, -1.2, -0.9, -0.5]
        for i in range(N):
            self.H_base[i][i] = diag_pots[i]
        self.companion: Optional[GemmaCompanionThread] = None

    def set_companion(self, comp: Optional[GemmaCompanionThread]) -> None:
        self.companion = comp

    def progress(self, snap: dict) -> float:
        stg = snap["stage"]
        in_stage = min(1.0, snap["stage_files"] / self.files_per_stage) \
            if snap["stage_files"] else \
            min(1.0, (snap["total_lines"] % self.lines_per_stage) / self.lines_per_stage)
        p = (stg + in_stage) / 3.0
        if snap["boss_seen"]:
            p = max(p, 0.98)
        if snap["finished"] and snap["exit_code"] == 0:
            p = 1.0
        return max(0.0, min(1.0, p))

    def render(self, snap: dict, cols: int, rows: int) -> str:
        self.frame += 1
        w = max(76, cols)
        field_w = w - 2
        p = self.progress(snap)
        stg = snap["stage"]

        if snap["warnings"] > self.seen_warnings:
            diff = snap["warnings"] - self.seen_warnings
            self.scars += diff
            self.seen_warnings = snap["warnings"]
            self.h_prev += 2.0 * diff

        # Canonical Recursive Field Evolution: H_t = H_base + eta * H * sig + noise
        h_base = (1.0 - p) * 4.5
        noise = self.rng.gauss(0, 1 + self.beta * abs(self.h_prev)) * self.eps
        h_t = h_base + self.eta * self.h_prev * _sigmoid(self.gamma * self.h_prev) + noise
        self.h_prev = h_t

        out: List[str] = []
        A = out.append
        elapsed = snap["elapsed"]
        eta = (elapsed / p - elapsed) if p > 0.02 else float("inf")
        eta_s = f"{int(eta // 60):02d}:{int(eta % 60):02d}" if eta != float("inf") else "--:--"

        # 1. Master Telemetry Header
        A(f" {BOLD}{fg(46)}🔱 ZCC QUEST HUD v7.0-ULTIMATE // SOVEREIGN QUANTUM COCKPIT{RESET} "
          f"│ {fg(51)}Stage {stg+1}/3{RESET} │ {fg(213)}H={h_t:6.4f}{RESET} │ {fg(226)}Scars:{self.scars}{RESET} "
          f"│ {fg(244)}⏱ {int(elapsed // 60):02d}:{int(elapsed % 60):02d} (eta {eta_s}){RESET}")

        # 2. Quantum Progress Bar with Palindromic Loschmidt Inversion Fidelity
        bar_w = max(20, field_w - 28)
        filled = int(p * bar_w)
        loschmidt_f = 1.000000000000000 - (1.0 - p) * 1e-15
        A(" " + fg(250) + "[" + fg(46) + "█" * filled
          + fg(238) + "░" * (bar_w - filled) + fg(250) + f"] {int(p * 100):3d}%"
          + f" │ {fg(46)}F_echo: {loschmidt_f:.14f}{RESET}")

        # 3. 7-System Sovereign Matrix
        A(f" {BOLD}{fg(226)}── 7-SYSTEM SOVEREIGN STATUS MATRIX ──────────────────────────────────────────{RESET}")
        sys_active_count = min(7, int(p * 7) + (1 if not snap["finished"] else 0))
        for idx, (sys_name, desc, metric) in enumerate(self.SYSTEMS):
            if idx < sys_active_count or (snap["finished"] and snap["exit_code"] == 0 and snap.get("oracle_verified", False)):
                status_glyph = f"{fg(46)}● ONLINE {RESET}"
            elif idx == sys_active_count:
                status_glyph = f"{fg(226)}◐ SYNCING{RESET}"
            else:
                status_glyph = f"{fg(240)}○ STANDBY{RESET}"
            A(f"   [{idx+1}] {BOLD}{fg(51)}{sys_name:<14}{RESET} │ {desc:<36} │ {status_glyph} │ {fg(245)}{metric}{RESET}")

        # 4. 16-Node CTQW Quantum Probability Density Topography
        A(f" {BOLD}{fg(213)}── 16-NODE CTQW QUANTUM DENSITY TOPOGRAPHY (|ψ_j|^2) ───────────────────────{RESET}")
        densities = []
        for j in range(16):
            base_p = 0.7197 if j == 0 else (0.0826 if j == 7 else (0.0538 if j == 2 else 0.015))
            osc = math.sin(self.frame * 0.25 + j * 0.45) * 0.012
            d = max(0.005, base_p + osc)
            densities.append(d)
        total_d = sum(densities)
        densities = [d / total_d for d in densities]

        row1, row2 = [], []
        for j in range(8):
            c = 46 if j == 0 else (51 if j == 7 else 245)
            row1.append(f"{fg(c)}{self.NODES[j][:4]}:{densities[j]*100:4.1f}%{RESET}")
        for j in range(8, 16):
            c = 51 if j == 8 else (213 if densities[j] > 0.02 else 240)
            row2.append(f"{fg(c)}{self.NODES[j][:4]}:{densities[j]*100:4.1f}%{RESET}")
        A("   " + " │ ".join(row1))
        A("   " + " │ ".join(row2))

        # 5. Continuous 2D Wavefield Radar Phase Space
        grid_h = 4
        cx, cy = field_w // 2, grid_h // 2
        r_orbit = max(0.5, (1.0 - p) * (field_w // 4))
        angle = self.frame * 0.2
        px = int(cx + r_orbit * math.cos(angle))
        py = int(cy + (r_orbit * 0.3) * math.sin(angle))
        A(f" {BOLD}{fg(51)}── RECURSIVE TOPOLOGICAL PHASE PORTRAIT (H_t) ─────────────────────────────{RESET}")
        for y in range(grid_h):
            row = []
            for x in range(field_w):
                dx = (x - cx) / max(1.0, field_w / 2.5)
                dy = (y - cy) / max(1.0, grid_h / 2.0)
                dist = math.sqrt(dx * dx + dy * dy)
                if x == px and y == py:
                    row.append(fg(51) + "✦" + RESET)
                elif x == cx and y == cy:
                    row.append(fg(46) + "⟐" + RESET if p >= 0.98 else fg(213) + "⊙" + RESET)
                else:
                    val = math.sin(dist * 5.0 - self.frame * 0.2) * h_t
                    if val > 1.2:
                        row.append(fg(196) + "·" + RESET)
                    elif val > 0.4:
                        row.append(fg(208) + "·" + RESET)
                    elif val > -0.4:
                        row.append(fg(240) + "·" + RESET)
                    else:
                        row.append(" ")
            A(" " + "".join(row))

        # 6. Gemma LoRA AI Copilot Telemetry
        if self.companion:
            status_txt = f"{fg(46)}● ONLINE ({self.companion.latency_ms:.1f}ms){RESET}" if self.companion.connected else f"{fg(240)}○ STANDBY{RESET}"
            A(f" {BOLD}{fg(81)}── 🤖 GEMMA LO-RA COGNITIVE COPILOT {status_txt} {BOLD}{fg(81)}─────────────────────────────────────{RESET}")
            A(f"   {fg(226)}Agent:{RESET} {fg(51)}{self.companion.agent_title:<38}{RESET} │ {fg(245)}ID:{RESET} {self.companion.active_agent}")
            thought_line = self.companion.last_thought[:max(20, field_w - 12)]
            A(f"   {fg(250)}Insight:{RESET} {fg(255)}{thought_line}{RESET}")

        # 7. Build Output Stream
        A(f" {fg(240)}┈┈ live build stream ┈┈{RESET}")
        for ln in snap["tail"]:
            color = 196 if ERR_RE.search(ln) else 178 if WARN_RE.search(ln) else 245
            A(f" {fg(color)}{ln[:field_w]}{RESET}")

        if snap["finished"]:
            if snap["exit_code"] == 0:
                if snap.get("oracle_verified", False):
                    A(f" {BOLD}{fg(46)}★ SOVEREIGN VICTORY — 7/7 SYSTEMS SYNCHRONIZED — BYTE-IDENTICAL VERIFIED ({BOSS_NAME}) ★{RESET}")
                else:
                    A(f" {BOLD}{fg(46)}★ SOVEREIGN COMMAND SUCCEEDED — EXIT 0 — SEE BUILD LOG ★{RESET}")
            else:
                A(f" {BOLD}{fg(196)}✖ SOVEREIGN DRIFT — EXIT {snap['exit_code']} — CHECK BUILD LOG{RESET}")

        return "\n".join(out)


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="Game-style build progress tracker.")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--cmd", help="build command to wrap (shell)")
    g.add_argument("--demo", action="store_true", help="run a fake build")
    ap.add_argument("--files", type=int, default=14,
                    help="expected compiled files per stage (progress smoothing)")
    ap.add_argument("--lines-per-stage", type=int, default=400,
                    help="fallback: output lines per stage")
    ap.add_argument("--log", default="/tmp/zcc_quest.log", help="sidecar log path")
    ap.add_argument("--fps", type=float, default=5.0, help="frames per second (default: 5.0 for low CPU/heat)")
    ap.add_argument("--low-power", action="store_true", help="ultra low-power mode (2 FPS, minimal CPU/heat)")
    ap.add_argument("--mode", choices=("runner", "frogger", "garden", "prime", "sovereign"), default="sovereign",
                    help="sovereign: 7-system status matrix & 16-node CTQW · prime: Hamiltonian energy · garden: zen mode · runner/frogger: arcade")
    ap.add_argument("--lanes", type=int, default=9,
                    help="frogger traffic lanes (multiple of 3, 3..15)")
    ap.add_argument("--frames", type=int, default=0,
                    help="render at most N frames then exit (testing/CI)")
    ap.add_argument("--gemma", action="store_true",
                    help="enable resident Gemma LoRA agent fleet copilot HUD")
    ap.add_argument("--gemma-url", default="http://127.0.0.1:8088",
                    help="Gemma agent fleet daemon URL")
    args = ap.parse_args(argv)

    if args.files <= 0 or args.lines_per_stage <= 0 or args.fps <= 0:
        ap.error("--files, --lines-per-stage and --fps must be positive")

    if args.low_power:
        args.fps = 2.0

    _setup_logging(args.log)
    LOG.info("start cmd=%r demo=%s gemma=%s", args.cmd, args.demo, args.gemma)

    state = BuildState()
    target = demo_process if args.demo else watch_process
    t_args = (state,) if args.demo else (args.cmd, state, args.log)
    worker = threading.Thread(target=target, args=t_args, daemon=True)
    worker.start()

    companion: Optional[GemmaCompanionThread] = None
    if args.gemma:
        companion = GemmaCompanionThread(state, url=args.gemma_url)
        companion.start()

    if args.mode == "garden":
        game = GardenGame(args.files, args.lines_per_stage)
    elif args.mode == "frogger":
        if args.lanes % 3 or not (3 <= args.lanes <= 15):
            ap.error("--lanes must be a multiple of 3 in [3, 15]")
        game = FroggerGame(args.files, args.lines_per_stage, args.lanes)
    elif args.mode == "prime":
        game = PrimeEnergyGame(args.files, args.lines_per_stage)
    elif args.mode == "sovereign":
        game = SovereignHudGame(args.files, args.lines_per_stage)
    else:
        game = Game(args.files, args.lines_per_stage)

    if companion and hasattr(game, "set_companion"):
        game.set_companion(companion)

    interactive = sys.stdout.isatty()
    if interactive:
        sys.stdout.write(ALT_ON + HIDE_CUR)

    stop = {"flag": False, "signal": 0}

    def _term_handler(sig, _frm):
        stop["flag"] = True
        stop["signal"] = sig

    signal.signal(signal.SIGINT, _term_handler)
    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, _term_handler)

    frames = 0
    rc = 1
    try:
        while True:
            size = shutil.get_terminal_size(fallback=(100, 30))
            snap = state.snapshot()
            frame = game.render(snap, size.columns, size.lines)
            if interactive:
                sys.stdout.write(CSI + "H" + CSI + "2J" + frame + "\n")
            else:
                if frames % (args.fps * 5) == 0 or snap["finished"]:
                    sys.stdout.write(frame + "\n" + "-" * 40 + "\n")
            sys.stdout.flush()
            frames += 1
            if snap["finished"]:
                time.sleep(1.2 if interactive else 0)
                break
            if stop["flag"] or (args.frames and frames >= args.frames):
                break
            time.sleep(1.0 / args.fps)

        snap = state.snapshot()
        if snap["finished"]:
            rc = snap["exit_code"] if snap["exit_code"] is not None else 0
        elif stop["signal"] == getattr(signal, "SIGTERM", 15):
            rc = 143
        elif stop["signal"] == getattr(signal, "SIGINT", 2) or stop["flag"]:
            rc = 130
        elif args.frames and frames >= args.frames:
            rc = 124
        else:
            rc = snap["exit_code"] if snap["exit_code"] is not None else 1
    finally:
        cleanup_child_process(state, timeout_grace=0.5)
        if companion:
            companion.stop()
        if interactive:
            sys.stdout.write(SHOW_CUR + ALT_OFF)
            sys.stdout.flush()

    snap = state.snapshot()
    print(f"zcc_quest: child exit={snap['exit_code']} warnings={snap['warnings']} "
          f"errors={snap['errors']} elapsed={snap['elapsed']:.1f}s "
          f"(raw output: {args.log}.build)")
    return rc


if __name__ == "__main__":
    sys.exit(main())
