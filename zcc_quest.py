#!/usr/bin/env python3
r"""
ZCC QUEST v8.0 — Sovereign Quantum & Arcade Telemetry HUD for Self-Hosting Builds.
==================================================================================

Wraps your build command and renders a rich, full-color interactive terminal HUD:
  - prime     : ZKAEDI PRIME v8.0 Dual-Pane Coupled Hamiltonian Phase-Space & Tumbler HUD
  - sovereign : 7-System Sovereign Matrix & 16-Node CTQW Quantum Probability Cockpit
  - runner    : Cyberpunk Parallax Side-Scroller Arcade Runner
  - frogger   : Multi-Lane Highway & River Crossing Arcade Game
  - garden    : Deterministic Botanical Zen Garden with Celestial Arc & Koi Pond
  - tablet    : Sumerian Cuneiform Clay Tablet & Reed Stylus Scribe HUD
  - enki      : Sacred Abzu Water-Clock & Me-Decree Wisdom Observatory

Usage:
    python3 zcc_quest.py --cmd "bash gate.sh" --mode prime       # ZKAEDI PRIME v8.0 HUD
    python3 zcc_quest.py --cmd "make selfhost" --mode sovereign  # Sovereign Quantum HUD
    python3 zcc_quest.py --demo --mode prime                     # Demo ZKAEDI PRIME Mode
    python3 zcc_quest.py --demo --mode sovereign                 # Demo Sovereign Mode
    python3 zcc_quest.py --demo --mode garden                    # Demo Zen Garden Mode
    python3 zcc_quest.py --demo --mode runner                    # Demo Cyber Runner Mode
    python3 zcc_quest.py --demo --mode frogger                   # Demo Frogger Mode
"""

from __future__ import annotations

import argparse
import hashlib
import json
import logging
import math
import os
import random
import re
import secrets
import shlex
import shutil
import signal
import subprocess
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Deque, List, Optional, Sequence, Tuple

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
# ANSI & Unicode Visual Helpers
# ---------------------------------------------------------------------------
CSI = "\x1b["
RESET = CSI + "0m"
BOLD = CSI + "1m"
DIM = CSI + "2m"
HIDE_CUR = CSI + "?25l"
SHOW_CUR = CSI + "?25h"
ALT_ON = CSI + "?1049h"
ALT_OFF = CSI + "?1049l"

ANSI_STRIP_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")


def fg(n: int) -> str:
    """256-color foreground escape."""
    assert 0 <= n <= 255, f"color index out of range: {n}"
    return f"{CSI}38;5;{n}m"


def bg(n: int) -> str:
    """256-color background escape."""
    assert 0 <= n <= 255, f"color index out of range: {n}"
    return f"{CSI}48;5;{n}m"


def vis_len(s: str) -> int:
    """Approximate terminal cell width after stripping ANSI escapes."""
    clean = ANSI_STRIP_RE.sub("", s)
    w = 0
    for ch in clean:
        cp = ord(ch)
        if cp in (0xFE0F, 0x200D):
            continue
        if (
            0x1F300 <= cp <= 0x1FAFF
            or 0x2600 <= cp <= 0x27BF and ch in ("⚡", "✨", "⛩", "🐸", "👾", "🏎", "🚌", "🚛", "☄", "🛡")
        ):
            w += 2
        else:
            w += 1
    return w


def pad_ansi(s: str, width: int, fill: str = " ") -> str:
    """Pad an ANSI-colored string to an exact visible cell width."""
    vl = vis_len(s)
    if vl >= width:
        return s
    return s + fill * (width - vl)


def clip_ansi(s: str, max_vis: int) -> str:
    """Clip a plain or lightly formatted string safely to max_vis columns."""
    clean = ANSI_STRIP_RE.sub("", s)
    if len(clean) <= max_vis:
        return clean
    return clean[: max(0, max_vis - 1)] + "…"


# World palettes: (sky, ground, accent, name)
WORLDS = [
    (117, 28, 51, "STAGE 1 · cc0 → zcc1 (Lex/AST/IR)"),
    (214, 94, 226, "STAGE 2 · zcc1 → zcc2 (Self-Compile)"),
    (141, 240, 46, "STAGE 3 · zcc2 → zcc3 (Fixed-Point & Oracles)"),
]
BOSS_NAME = "⟐ BYTE-IDENTICAL SEAL ⟐"

SPINNERS = ("⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏")
SUBBLOCKS = (" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█")
SPARK_CHARS = ("▁", "▂", "▃", "▄", "▅", "▆", "▇", "█")
GRADIENT_CYAN_EMERALD = (39, 45, 51, 50, 49, 48, 47, 46)
GRADIENT_PRIME = (57, 63, 39, 51, 49, 46, 226, 201)


def render_gradient_bar(p: float, width: int, frame: int, palette: Sequence[int] = GRADIENT_CYAN_EMERALD) -> str:
    """Render a sub-pixel 8-step Unicode block progress bar with animated pulse."""
    w = max(10, width)
    p_clamped = max(0.0, min(1.0, p))
    exact = p_clamped * w
    full = int(exact)
    rem = int((exact - full) * 8)
    marks = {w // 3, (2 * w) // 3}
    pulse_idx = (frame * 2) % max(1, w)

    out: List[str] = []
    for i in range(w):
        if i < full:
            c_idx = int((i / max(1, w - 1)) * (len(palette) - 1))
            col = 231 if (i == pulse_idx and p_clamped < 1.0) else palette[c_idx]
            ch = "┃" if i in marks else "█"
            out.append(f"{fg(col)}{ch}")
        elif i == full and rem > 0 and p_clamped < 1.0:
            c_idx = int((i / max(1, w - 1)) * (len(palette) - 1))
            out.append(f"{fg(palette[c_idx])}{SUBBLOCKS[rem]}")
        else:
            ch = "┼" if i in marks else "─"
            out.append(f"{fg(237)}{ch}")
    return "".join(out) + RESET


def render_sparkline(vals: Sequence[float], width: int = 18) -> str:
    """Render a colored Unicode sparkline for a numeric time series."""
    if not vals:
        return fg(240) + "─" * width + RESET
    tail = list(vals)[-width:]
    lo = min(tail)
    hi = max(tail)
    span = max(1e-6, hi - lo)
    out: List[str] = []
    for v in tail:
        norm = max(0.0, min(1.0, (v - lo) / span))
        idx = min(7, int(norm * 7.99))
        col = 46 if norm < 0.25 else (51 if norm < 0.55 else (226 if norm < 0.80 else 201))
        out.append(f"{fg(col)}{SPARK_CHARS[idx]}")
    if len(tail) < width:
        out = [f"{fg(237)}·"] * (width - len(tail)) + out
    return "".join(out) + RESET


def render_pipeline_ribbon(snap: dict, width: int, frame: int) -> str:
    """Render a 6-stage visual pipeline tracker showing real-time bootstrap & gate progress."""
    sub = snap.get("substage", snap["stage"])
    finished = snap["finished"]
    ok = finished and snap["exit_code"] == 0
    failed = finished and snap["exit_code"] not in (0, None)
    spin = SPINNERS[frame % len(SPINNERS)]

    s2_lbl = f"S2:{snap['stage2_sha'][:6]}" if snap.get("stage2_sha") else "S2:STAGE2"
    s3_lbl = f"S3:{snap['stage3_sha'][:6]}" if snap.get("stage3_sha") else "S3:STAGE3"
    ag_step = snap.get("argon_step", 0)
    ag_lbl = f"ARGON:{ag_step}/6" if ag_step > 0 else "ARGON-4"

    steps = [
        (0, "S1:BOOT", sub > 0 or snap["stage"] > 0 or ok),
        (1, s2_lbl, bool(snap.get("stage2_sha")) or sub > 1 or snap["stage"] > 1 or ok),
        (2, s3_lbl, bool(snap.get("stage3_sha")) or sub > 2 or ok),
        (3, "GATE1:CMP", bool(snap.get("gate1_ok")) or sub > 3 or ok),
        (4, "ABI+SEM", (bool(snap.get("semantic_ok")) and bool(snap.get("abi_ok"))) or sub > 5 or ok),
        (5, ag_lbl, bool(snap.get("argon_ok")) or ok),
    ]

    active_idx = 0
    for idx, _, done in steps:
        if done:
            active_idx = min(5, idx + 1)

    pills: List[str] = []
    for idx, label, done in steps:
        if done:
            pills.append(f"{fg(46)}[✔ {label}]{RESET}")
        elif idx == active_idx and not finished:
            pills.append(f"{BOLD}{fg(51)}[{spin} {label}]{RESET}")
        elif failed and idx == active_idx:
            pills.append(f"{BOLD}{fg(196)}[✖ {label}]{RESET}")
        else:
            pills.append(f"{fg(240)}[· {label}]{RESET}")

    connector = f" {fg(239)}━▶{RESET} "
    ribbon = connector.join(pills)
    if vis_len(ribbon) > width - 4:
        connector = f"{fg(239)}▸{RESET}"
        ribbon = connector.join(pills)
    return ribbon


# ---------------------------------------------------------------------------
# Build watcher — parses child output into progress events
# ---------------------------------------------------------------------------
@dataclass
class BuildState:
    """Thread-shared snapshot of build progress. All writes hold `lock`."""
    lock: threading.Lock = field(default_factory=threading.Lock)
    stage: int = 0                    # 0-based world index (0..2)
    substage: int = 0                 # 0..6 detailed pipeline phase
    argon_step: int = 0               # 0..6 ARGON-4 sub-suite counter
    stage_files: int = 0              # per-file ticks inside current stage
    total_lines: int = 0
    warnings: int = 0
    errors: int = 0
    tail: Deque[str] = field(default_factory=lambda: deque(maxlen=6))
    finished: bool = False
    exit_code: Optional[int] = None
    boss_seen: bool = False           # byte-identical marker observed
    oracle_verified: bool = False     # verified via on-disk byte comparison oracle
    is_demo: bool = False             # demo mode flag
    stage2_sha: str = ""
    stage3_sha: str = ""
    gate1_ok: bool = False
    semantic_ok: bool = False
    abi_ok: bool = False
    argon_ok: bool = False
    files_seen: List[str] = field(default_factory=list)   # filenames, in order
    started: float = field(default_factory=time.monotonic)
    started_wall: float = field(default_factory=time.time)
    cmd_str: str = ""
    run_id: str = ""
    proc: Optional[subprocess.Popen] = None
    started_event: threading.Event = field(default_factory=threading.Event)

    def snapshot(self) -> dict:
        with self.lock:
            ov = self.oracle_verified or (self.is_demo and self.boss_seen)
            return {
                "stage": self.stage,
                "substage": self.substage,
                "argon_step": self.argon_step,
                "stage_files": self.stage_files,
                "total_lines": self.total_lines,
                "warnings": self.warnings,
                "errors": self.errors,
                "tail": list(self.tail),
                "finished": self.finished,
                "exit_code": self.exit_code,
                "boss_seen": self.boss_seen,
                "oracle_verified": ov,
                "stage2_sha": self.stage2_sha,
                "stage3_sha": self.stage3_sha,
                "gate1_ok": self.gate1_ok,
                "semantic_ok": self.semantic_ok,
                "abi_ok": self.abi_ok,
                "argon_ok": self.argon_ok,
                "run_id": self.run_id,
                "is_demo": self.is_demo,
                "files_seen": list(self.files_seen),
                "elapsed": time.monotonic() - self.started,
            }


STAGE_RE = re.compile(r"(?:===+\s*)?\[?STAGE\s*([123])(?:/3)?\]?", re.I)
ARGON_STEP_RE = re.compile(r"\[ARGON\s*4\]\s*([1-6])/6", re.I)
SHA_RE = re.compile(r"\(SHA:\s*([0-9a-fA-F]{8,64})")
BOSS_RE = re.compile(
    r"===+\s*VERDICT:\s*BOOTSTRAP\s+DETERMINISM\s+LOCK\s+SECURED\s*===+|"
    r"SELF-HOST\s+VERIFIED\s+\(assembly\s+identical\)|"
    r"Gate\s+1\s+PASS:\s+Assembly\s+byte-identical|"
    r"SELFHOST-MV1\.3b\s+PASS",
    re.I,
)
NEG_RE = re.compile(r"\b(?:fail|failed|mismatch|drift|diverg|corrupt|error|non-zero)\b", re.I)
PASS_LINE_RE = re.compile(
    r"^\s*\[PASS\]|rejected as expected|raises on missing|refuses corrupt|errors=0\b",
    re.I,
)
FILE_RE = re.compile(r"(?:^(?:CC|AS|LD|CCLD)\s)|(?:\.(?:c|s|o)\b.*(?:->|→))|(?:Compiling\s+\S+\.c)")
FNAME_RE = re.compile(r"([\w./-]+\.(?:c|s|o|py))\b")
WARN_RE = re.compile(r"\bwarning\b", re.I)
ERR_RE = re.compile(r"\berror\b|\bfatal\b|segmentation fault", re.I)


def compute_smooth_progress(snap: dict, files_per_stage: int, lines_per_stage: int) -> float:
    """Compute smooth, monotonic-feeling build progress combining stage milestones & elapsed time."""
    if snap["finished"] and snap["exit_code"] == 0:
        return 1.0

    stg = snap["stage"]
    sub = snap.get("substage", 0)
    ag = snap.get("argon_step", 0)
    elapsed = snap.get("elapsed", 0.0)

    # Milestone floor based on observed gate.sh sub-stages
    milestone_p = 0.0
    if sub == 1:
        milestone_p = 0.28
    elif sub == 2:
        milestone_p = 0.56
    elif sub == 3 or snap.get("gate1_ok"):
        milestone_p = 0.64
    elif sub == 4 or snap.get("semantic_ok"):
        milestone_p = 0.70
    elif sub == 5 or snap.get("abi_ok"):
        milestone_p = 0.76
    elif sub >= 6 or ag > 0:
        milestone_p = 0.76 + (ag / 6.0) * 0.22

    # Standard file/line stage calculation (used by demo_process and unit tests)
    in_stage = (
        min(1.0, snap["stage_files"] / max(1, files_per_stage))
        if snap["stage_files"]
        else min(1.0, (snap["total_lines"] % max(1, lines_per_stage)) / max(1, lines_per_stage))
    )
    classic_p = (stg + in_stage) / 3.0

    # Gentle asymptotic time-based creep during long silent compilation phases (e.g. Stage 2/3 zcc.c)
    time_creep = 0.0
    if not snap.get("is_demo", False) and snap["stage_files"] <= 2 and sub <= 2:
        # Smoothly glides toward ~52% over ~200s while Stage 2/3 compile
        time_creep = 0.52 * (1.0 - math.exp(-elapsed / 95.0))

    p = max(classic_p, milestone_p, time_creep)
    if snap["boss_seen"] and ag >= 6:
        p = max(p, 0.98)
    elif snap["boss_seen"] and sub == 0:
        p = max(p, 0.98)
    return max(0.0, min(0.99, p))


def is_bootstrap_command(cmd: str, repo_dir: Optional[str] = None) -> bool:
    """Validate that the command actually executes an authorized bootstrap gate,
    immune to shell comment injection and argument smuggling."""
    if not cmd or not cmd.strip():
        return False
    try:
        tokens = shlex.split(cmd, comments=True)
    except Exception:
        return False
    if not tokens:
        return False

    if repo_dir is None:
        repo_dir = os.path.dirname(os.path.abspath(__file__))
    canon_gate = os.path.realpath(os.path.join(repo_dir, "gate.sh"))

    prog = os.path.basename(tokens[0]).lower()

    # Direct gate.sh execution: ./gate.sh, /path/to/gate.sh, gate.sh
    if prog == "gate.sh":
        cand = os.path.realpath(tokens[0])
        return cand == canon_gate or os.path.basename(cand) == "gate.sh"

    # Shell-wrapped execution: bash gate.sh, sh ./gate.sh, bash -c "..."
    if prog in ("bash", "sh"):
        i = 1
        while i < len(tokens):
            tok = tokens[i]
            if tok == "-c":
                if i + 1 < len(tokens):
                    return is_bootstrap_command(tokens[i + 1], repo_dir)
                return False
            elif tok.startswith("-"):
                i += 1
                continue
            else:
                cand = os.path.realpath(tok)
                return cand == canon_gate or (os.path.basename(cand) == "gate.sh" and os.path.isfile(cand))
        return False

    # Make target execution: make selfhost, make selfhost-runner, etc.
    if prog == "make":
        valid_targets = {
            "selfhost", "selfhost-garden", "selfhost-runner",
            "selfhost-frogger", "selfhost-prime", "selfhost-sovereign",
            "selfhost-tablet", "selfhost-enki",
            "compat-quickjs", "compat-sqlite", "compat-micropython",
        }
        targets = [t for t in tokens[1:] if not t.startswith("-") and "=" not in t]
        return any(t in valid_targets for t in targets)

    return False


def validate_assembly_file(path: str) -> bool:
    """Verify that an artifact is authentic, non-counterfeit x86-64 assembly text."""
    if not os.path.isfile(path):
        return False
    try:
        sz = os.path.getsize(path)
        if sz < 100_000:
            return False

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

        has_text = ".text" in header_str or ".text" in footer_str
        has_reg = "%rbp" in header_str or "%rsp" in header_str or "%rbp" in footer_str or "%rsp" in footer_str
        has_ret = "ret" in header_str or "retq" in header_str or "ret" in footer_str or "retq" in footer_str
        has_globl = ".globl" in header_str or ".globl" in footer_str or "pushq" in header_str or ".cfi" in header_str

        return has_text and has_reg and has_ret and has_globl
    except Exception:
        return False


def validate_binary_elf(path: str) -> bool:
    """Verify that an artifact is a genuine 64-bit ELF binary executable."""
    if not os.path.isfile(path):
        return False
    try:
        sz = os.path.getsize(path)
        if sz < 10_000:
            return False
        with open(path, "rb") as f:
            hdr = f.read(16)
            if len(hdr) < 16 or hdr[:4] != b"\x7fELF" or hdr[4] != 2 or hdr[5] != 1:
                return False
        return True
    except Exception:
        return False


def compute_sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(131072):
            h.update(chunk)
    return h.hexdigest()


def verify_selfhost_oracle(started_wall: float, cmd: str = "", state: Optional[BuildState] = None) -> bool:
    """Independent oracle: verify that stage2/stage3 assembly was generated during this run,
    matches cryptographic provenance ledger receipts in the supervisor-bound run directory,
    and is byte-identical."""
    import filecmp

    repo_dir = os.path.dirname(os.path.abspath(__file__))

    if cmd and not is_bootstrap_command(cmd, repo_dir):
        LOG.warning("Oracle rejected: command %r is not an authorized bootstrap gate", cmd)
        return False

    run_id = state.run_id if state and state.run_id else os.environ.get("ZCC_VERIFIER_RUN_ID", "")
    if not run_id:
        LOG.warning("Oracle rejected: missing supervisor run_id binding")
        return False

    ev_dir = os.path.join(repo_dir, "evidence", run_id)
    if not os.path.isdir(ev_dir):
        LOG.warning("Oracle rejected: supervisor-bound evidence directory %r does not exist", ev_dir)
        return False

    try:
        ev_mtime = os.path.getmtime(ev_dir)
        if ev_mtime < started_wall:
            LOG.warning("Oracle rejected: evidence directory mtime (%.3f) predates process spawn (%.3f)",
                        ev_mtime, started_wall)
            return False
    except OSError as exc:
        LOG.warning("Oracle rejected: cannot stat evidence directory: %s", exc)
        return False

    s2 = os.path.join(ev_dir, "stage2.s")
    s3 = os.path.join(ev_dir, "stage3.s")
    b2 = os.path.join(ev_dir, "stage2.bin")
    b3 = os.path.join(ev_dir, "stage3.bin")
    ledger_path = os.path.join(ev_dir, "provenance.jsonl")

    for path, name in [(s2, "stage2.s"), (s3, "stage3.s"), (b2, "stage2.bin"), (b3, "stage3.bin"), (ledger_path, "provenance.jsonl")]:
        if not os.path.isfile(path):
            LOG.warning("Oracle rejected: missing required artifact %s in %r", name, ev_dir)
            return False
        try:
            if os.path.getmtime(path) < started_wall:
                LOG.warning("Oracle rejected: artifact %s mtime predates run spawn", name)
                return False
        except OSError:
            return False

    sz2 = os.path.getsize(s2)
    sz3 = os.path.getsize(s3)
    if sz2 != sz3 or sz2 < 100_000:
        LOG.warning("Oracle rejected: assembly size mismatch or insufficient (%d vs %d)", sz2, sz3)
        return False

    if not (validate_assembly_file(s2) and validate_assembly_file(s3)):
        LOG.warning("Oracle rejected: counterfeit or malformed assembly tokens in %s / %s", s2, s3)
        return False

    if not filecmp.cmp(s2, s3, shallow=False):
        LOG.warning("Oracle rejected: stage2.s and stage3.s are not byte-identical")
        return False

    if not (validate_binary_elf(b2) and validate_binary_elf(b3)):
        LOG.warning("Oracle rejected: stage2.bin / stage3.bin are not valid 64-bit ELF binaries")
        return False

    try:
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
            LOG.warning("Oracle rejected: provenance ledger missing stage 2 or stage 3 records")
            return False

        if stage2_rec.get("run_id") != run_id or stage3_rec.get("run_id") != run_id:
            LOG.warning("Oracle rejected: provenance ledger run_id mismatch (%r / %r vs %r)",
                        stage2_rec.get("run_id"), stage3_rec.get("run_id"), run_id)
            return False

        if stage2_rec.get("exit_code") != 0 or stage3_rec.get("exit_code") != 0:
            LOG.warning("Oracle rejected: non-zero exit code in provenance records")
            return False

        asm_hash2 = stage2_rec.get("assembly_sha256")
        asm_hash3 = stage3_rec.get("assembly_sha256")
        if not (asm_hash2 and asm_hash3 and asm_hash2 == asm_hash3):
            LOG.warning("Oracle rejected: assembly hash mismatch in provenance records")
            return False

        actual_asm_hash = compute_sha256(s2)
        if actual_asm_hash != asm_hash2:
            LOG.warning("Oracle rejected: actual assembly hash != ledger hash (%s vs %s)",
                        actual_asm_hash, asm_hash2)
            return False

        bin_hash2 = stage2_rec.get("binary_sha256")
        if bin_hash2 and compute_sha256(b2) != bin_hash2:
            LOG.warning("Oracle rejected: actual binary hash != ledger binary_sha256")
            return False

        bin_hash3 = stage3_rec.get("binary_sha256")
        if bin_hash3 and compute_sha256(b3) != bin_hash3:
            LOG.warning("Oracle rejected: actual stage3 binary hash != ledger binary_sha256")
            return False

        # Verify transactionally published root artifacts match the evidence ledger
        root_s2 = os.path.join(repo_dir, "zcc2.s")
        root_s3 = os.path.join(repo_dir, "zcc3.s")
        root_b2 = os.path.join(repo_dir, "zcc2")
        root_b3 = os.path.join(repo_dir, "zcc3")
        if not (os.path.isfile(root_s2) and os.path.isfile(root_s3)):
            LOG.warning("Oracle rejected: published root zcc2.s / zcc3.s missing")
            return False
        if compute_sha256(root_s2) != actual_asm_hash or compute_sha256(root_s3) != actual_asm_hash:
            LOG.warning("Oracle rejected: published root zcc2.s / zcc3.s hash mismatch against ledger")
            return False
        if bin_hash2 and (not os.path.isfile(root_b2) or compute_sha256(root_b2) != bin_hash2):
            LOG.warning("Oracle rejected: published root zcc2 binary hash mismatch against ledger")
            return False
        if bin_hash3 and (not os.path.isfile(root_b3) or compute_sha256(root_b3) != bin_hash3):
            LOG.warning("Oracle rejected: published root zcc3 binary hash mismatch against ledger")
            return False

        canon_gate = os.path.join(repo_dir, "gate.sh")
        if os.path.isfile(canon_gate):
            expected_verifier_sha = compute_sha256(canon_gate)
            if stage2_rec.get("verifier_sha256") != expected_verifier_sha:
                LOG.warning("Oracle rejected: verifier_sha256 mismatch against gate.sh")
                return False

        LOG.info("★ Oracle verification SUCCESS: authentic byte-identical selfhost confirmed for run %s", run_id)
        return True

    except Exception as exc:
        LOG.warning("Oracle verification exception: %s", exc)
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

    try:
        if pgid and hasattr(os, "killpg"):
            os.killpg(pgid, signal.SIGTERM)
        else:
            p.terminate()
    except OSError:
        pass

    t_end = time.monotonic() + timeout_grace
    while time.monotonic() < t_end:
        if p.poll() is not None:
            break
        time.sleep(0.05)

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

    try:
        p.wait(timeout=1.0)
    except Exception:
        pass

    if pgid and hasattr(os, "killpg"):
        t_verify = time.monotonic() + 0.5
        while time.monotonic() < t_verify:
            try:
                os.killpg(pgid, 0)
                os.killpg(pgid, signal.SIGKILL)
                time.sleep(0.05)
            except OSError:
                break


def watch_process(cmd: str, state: BuildState, log_path: str) -> None:
    """Run `cmd`, stream its output into `state`, mirror verbatim to log."""
    spawn_wall = time.time()
    run_id = f"zcc-omega-{int(spawn_wall)}-{os.getpid()}-{secrets.token_hex(8)}"
    with state.lock:
        state.cmd_str = cmd
        state.started_wall = spawn_wall
        state.run_id = run_id

    env = os.environ.copy()
    env["ZCC_VERIFIER_RUN_ID"] = run_id

    try:
        proc = subprocess.Popen(
            cmd, shell=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, errors="replace", bufsize=1,
            env=env,
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
                    if line.strip():
                        state.tail.append(line[-140:])

                    # Track Stage 1..3
                    m = STAGE_RE.search(line)
                    if m:
                        new_stage = int(m.group(1)) - 1
                        if new_stage != state.stage:
                            state.stage = max(0, min(2, new_stage))
                            state.stage_files = 0
                            LOG.info("stage → %d", state.stage + 1)
                        state.substage = max(state.substage, new_stage)

                    # Extract Stage 2 & Stage 3 SHA badges from gate.sh
                    if "[Stage 2/3]" in line:
                        state.substage = max(state.substage, 1)
                        sm = SHA_RE.search(line)
                        if sm:
                            state.stage2_sha = sm.group(1)
                    elif "[Stage 3/3]" in line:
                        state.substage = max(state.substage, 2)
                        sm = SHA_RE.search(line)
                        if sm:
                            state.stage3_sha = sm.group(1)

                    # Track Gate 1, Semantic Oracle, Cross-TU ABI Oracle, and ARGON 4 sub-steps
                    if "Gate 1 PASS" in line or "Assembly byte-identical" in line:
                        state.gate1_ok = True
                        state.substage = max(state.substage, 3)
                    if "[Semantic Oracle]" in line and "[PASS" in line:
                        state.semantic_ok = True
                        state.substage = max(state.substage, 4)
                    if "[Cross-TU ABI Oracle]" in line and "[PASS" in line:
                        state.abi_ok = True
                        state.substage = max(state.substage, 5)
                    if "[Stage 6/6]" in line:
                        state.substage = max(state.substage, 6)
                    am = ARGON_STEP_RE.search(line)
                    if am:
                        state.substage = max(state.substage, 6)
                        state.argon_step = max(state.argon_step, int(am.group(1)))
                    if "ALL 6 VERIFICATION SUITES PASSED" in line or "SELFHOST-MV1.3b PASS" in line:
                        state.argon_ok = True
                        state.argon_step = 6

                    if BOSS_RE.search(line) and not NEG_RE.search(line):
                        state.boss_seen = True
                        LOG.info("authenticated boss marker seen: %r", line)

                    if FILE_RE.search(line):
                        state.stage_files += 1
                        fm = FNAME_RE.search(line)
                        if fm and len(state.files_seen) < 2000:
                            state.files_seen.append(fm.group(1))

                    if ERR_RE.search(line) and not PASS_LINE_RE.search(line):
                        state.errors += 1
                    elif WARN_RE.search(line) and not PASS_LINE_RE.search(line):
                        state.warnings += 1
    except Exception as exc:
        LOG.exception("watcher error: %s", exc)
    finally:
        rc = proc.wait()
        with state.lock:
            state.finished, state.exit_code = True, rc
            if state.boss_seen and rc == 0:
                state.oracle_verified = verify_selfhost_oracle(state.started_wall, cmd=state.cmd_str, state=state)
        LOG.info("child exited rc=%d oracle_verified=%s", rc, state.oracle_verified)


def demo_process(state: BuildState) -> None:
    """Fake triple-stage build for --demo mode."""
    with state.lock:
        state.is_demo = True
        state.run_id = "demo-run"
    state.started_event.set()
    rng = random.Random(1337)
    files = ["lexer.c", "parser.c", "typecheck.c", "codegen.c", "regalloc.c",
             "peephole.c", "quantum_rules.c", "emit_x86.c", "driver.c",
             "preproc.c", "symtab.c", "ir.c", "liveness.c", "leaq_fuse.c"]
    for stg in (1, 2, 3):
        with state.lock:
            state.stage, state.stage_files = stg - 1, 0
            state.substage = stg - 1
            state.tail.append(f"=== STAGE {stg} ===")
            state.total_lines += 1
        for idx, f in enumerate(files):
            time.sleep(rng.uniform(0.12, 0.38))
            with state.lock:
                state.stage_files += 1
                state.total_lines += 12
                state.files_seen.append(f"src/{f}")
                state.tail.append(f"[PASS] CC  src/{f} -> build/stage{stg}/{f[:-2]}.o")
                if stg == 2 and idx == len(files) - 1:
                    state.stage2_sha = "c75b44487f704347"
                elif stg == 3 and idx == len(files) - 1:
                    state.stage3_sha = "c75b44487f704347"
                    state.gate1_ok = True
                    state.semantic_ok = True
                    state.abi_ok = True
                    state.argon_step = 6
                    state.argon_ok = True
                if rng.random() < 0.14:
                    state.warnings += 1
                    state.tail.append(f"src/{f}: warning: implicit conversion")
    time.sleep(0.5)
    with state.lock:
        state.boss_seen = True
        state.oracle_verified = True
        state.tail.append("✓ Gate 1 PASS: Assembly byte-identical (cmp stage2.s stage3.s).")
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
                headers={"Content-Type": "application/json", "User-Agent": "ZccQuest/8.0"}
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
                        self.last_thought = f"[{self.active_agent}] Stage 2 IR lowering: GVN store-to-load & ICP folding nominal."
                    elif snap["stage"] == 2:
                        self.last_thought = f"[{self.active_agent}] Stage 3 self-host: compiling stage3 binary for byte-identical cmp."
                    else:
                        self.last_thought = f"[{self.active_agent}] Active compilation nominal ({snap['total_lines']} lines)."
        except Exception:
            self.connected = False
            self.last_thought = "Gemma daemon standby (port 8088 offline or waiting)."


def _format_log_line(ln: str, max_w: int) -> str:
    """Format and syntax-highlight a build output stream line."""
    clipped = clip_ansi(ln, max_w - 4)
    if PASS_LINE_RE.search(ln) or "[PASS]" in ln or "✓" in ln or "✨" in ln:
        return f" {fg(46)}✔{RESET} {fg(120)}{clipped}{RESET}"
    if ERR_RE.search(ln):
        return f" {fg(196)}✖ {clipped}{RESET}"
    if WARN_RE.search(ln):
        return f" {fg(214)}⚠ {clipped}{RESET}"
    if "Stage" in ln or "===" in ln or "═══" in ln:
        return f" {fg(51)}◈ {clipped}{RESET}"
    return f" {fg(240)}│{RESET} {fg(250)}{clipped}{RESET}"


# ---------------------------------------------------------------------------
# Renderer — Cyber Runner Game (--mode runner)
# ---------------------------------------------------------------------------
RUNNER_FRAMES = ["ᕕ( ᐛ )ᕗ", "ᕙ( ᐖ )ᕗ"]
ENEMY = "👾"
BULLET = "═▶"
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
    """Cyberpunk Parallax Side-Scroller Runner; consumes BuildState snapshots each frame."""

    def __init__(self, files_per_stage: int, lines_per_stage: int):
        assert files_per_stage > 0 and lines_per_stage > 0
        self.files_per_stage = files_per_stage
        self.lines_per_stage = lines_per_stage
        self.frame = 0
        self.enemies: List[Enemy] = []
        self.bullets: List[Bullet] = []
        self.sparks: List[Tuple[int, int]] = []
        self.coins: int = 0
        self.kills: int = 0
        self.seen_warnings = 0
        self.seen_files = 0
        self.scroll = 0.0
        self.rng = random.Random()
        self.jump_phase = 0

    def progress(self, snap: dict) -> float:
        return compute_smooth_progress(snap, self.files_per_stage, self.lines_per_stage)

    def render(self, snap: dict, cols: int, rows: int) -> str:
        self.frame += 1
        w = max(68, cols)
        field_w = w - 4
        stg = snap["stage"]
        sky_c, ground_c, accent, world_name = WORLDS[stg]
        p = self.progress(snap)
        seg = max(0.0, min(1.0, (p * 3.0) - stg))
        runner_x = 4 + int(seg * max(1, field_w - 16))
        self.scroll += 0.85

        if snap["warnings"] > self.seen_warnings:
            self.seen_warnings = snap["warnings"]
            self.enemies.append(Enemy(x=field_w - 5))
        if snap["stage_files"] > self.seen_files:
            self.coins += snap["stage_files"] - self.seen_files
            self.seen_files = snap["stage_files"]
            if self.rng.random() < 0.35:
                self.jump_phase = 4

        live = [e for e in self.enemies if e.alive and e.x > runner_x]
        if live and self.frame % 3 == 0:
            self.bullets.append(Bullet(x=runner_x + 8))
        for b in self.bullets:
            b.x += 4
        self.sparks = [(sx, age - 1) for sx, age in self.sparks if age > 1]
        for b in self.bullets:
            for e in live:
                if e.alive and abs(e.x - b.x) < 3:
                    e.alive = False
                    self.kills += 1
                    self.sparks.append((int(e.x), 3))
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
        border_c = fg(239)

        A(f" {border_c}╔{'═' * (w - 3)}╗{RESET}")
        A(f" {border_c}║{RESET} {BOLD}{fg(accent)}🎮 ZCC QUEST v8.0 // CYBER RUNNER{RESET} │ "
          f"{fg(sky_c)}{world_name}{RESET} │ "
          f"{fg(220)}{COIN}×{self.coins}{RESET} {fg(196)}☠×{self.kills}{RESET} │ "
          f"{fg(214)}W:{snap['warnings']}{RESET} {fg(196)}E:{snap['errors']}{RESET} │ "
          f"{fg(250)}⏱ {int(elapsed // 60):02d}:{int(elapsed % 60):02d} (ETA {eta_s}){RESET}")
        bar_w = max(20, field_w - 10)
        A(f" {border_c}║{RESET} [{render_gradient_bar(p, bar_w, self.frame)}] {BOLD}{fg(51)}{int(p * 100):3d}%{RESET}")
        A(f" {border_c}║{RESET} {render_pipeline_ribbon(snap, field_w, self.frame)}")
        A(f" {border_c}╠{'═' * (w - 3)}╣{RESET}")

        # 3 Parallax sky + synthwave horizon rows
        for row in range(3):
            line = []
            for x in range(field_w):
                h = (x * 131 + row * 977 + int(self.scroll * (row + 1) * 0.4)) % 97
                if row == 2 and (x + int(self.scroll * 0.5)) % 16 in (0, 1, 2):
                    line.append(fg(238) + "▲" + RESET)
                elif h == 0:
                    line.append(fg(sky_c) + ("✦" if stg == 2 else "☁") + RESET)
                elif h == 45:
                    line.append(fg(sky_c) + "·" + RESET)
                else:
                    line.append(" ")
            A(f" {border_c}║{RESET} " + "".join(line))

        air = [" "] * field_w

        def put(s: str, x: int, color: int) -> None:
            for i, ch in enumerate(s):
                xi = x + i
                if 0 <= xi < field_w:
                    air[xi] = fg(color) + ch + RESET

        put("⛩", field_w // 3, 213 if p >= 0.33 else 239)
        put("⛩", (2 * field_w) // 3, 213 if p >= 0.66 else 239)
        if snap["boss_seen"] or p >= 0.98:
            put(FLAG, field_w - 3, 46 if snap.get("exit_code") == 0 else 220)
        else:
            put(FLAG, field_w - 3, 244)
        for sx, _ in self.sparks:
            put("✸", sx, 226)
        for e in self.enemies:
            put(ENEMY, int(e.x), 196)
        for b in self.bullets:
            put(BULLET, int(b.x), 51)
        sprite = RUNNER_FRAMES[self.frame // 2 % 2]
        if runner_x > 2:
            put("≡", runner_x - 2, 51)
        put(sprite, runner_x, 231)

        blank_row = " " * field_w
        jump_row = "".join(air) if self.jump_phase else blank_row
        ground_row = blank_row if self.jump_phase else "".join(air)
        A(f" {border_c}║{RESET} " + jump_row)
        A(f" {border_c}║{RESET} " + ground_row)

        gpat = "".join(
            fg(ground_c) + ("▓" if (x + int(self.scroll)) % 6 else "█")
            for x in range(field_w)
        ) + RESET
        A(f" {border_c}║{RESET} " + gpat)

        A(f" {border_c}╠── {fg(245)}┈┈ build output ┈┈{border_c} {'─' * max(1, w - 26)}╣{RESET}")
        for ln in list(snap["tail"])[-4:]:
            A(_format_log_line(ln, field_w))
        A(f" {border_c}╚{'═' * (w - 3)}╝{RESET}")

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
# Frogger Mode (--mode frogger)
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
    """Frog crosses N traffic & river lanes bottom→top as the build progresses."""

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
        return compute_smooth_progress(snap, self.files_per_stage, self.lines_per_stage)

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
        w = max(68, cols)
        field_w = w - 4
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
                    and self.rng.random() < 0.11:
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
        border_c = fg(239)

        A(f" {border_c}╔{'═' * (w - 3)}╗{RESET}")
        A(f" {border_c}║{RESET} {BOLD}{fg(accent)}🐸 ZCC FROGGER v8.0{RESET} │ {fg(117)}{world_name}{RESET} │ "
          f"{fg(220)}hops:{self.hops}{RESET} │ "
          f"{fg(178)}traffic:{'▂▄▆█'[min(3, int(rate * 6))]} W:{snap['warnings']}{RESET} {fg(196)}E:{snap['errors']}{RESET} │ "
          f"{fg(250)}⏱ {int(elapsed // 60):02d}:{int(elapsed % 60):02d} (ETA {eta_s}){RESET}")
        bar_w = max(20, field_w - 10)
        A(f" {border_c}║{RESET} [{render_gradient_bar(p, bar_w, self.frame)}] {BOLD}{fg(46)}{int(p * 100):3d}%{RESET}")
        A(f" {border_c}║{RESET} {render_pipeline_ribbon(snap, field_w, self.frame)}")
        A(f" {border_c}╠{'═' * (w - 3)}╣{RESET}")

        goal = [f"{fg(24)}≈{RESET}" for _ in range(field_w)]
        for gx in range(4, field_w - 4, 10):
            goal[gx] = fg(46 if p >= 1.0 else 213) + LILY + RESET
        if p >= 1.0:
            mid = field_w // 2
            goal[min(field_w - 1, mid)] = fg(46) + FROG + RESET
        A(f" {border_c}║{RESET} " + "".join(goal))

        for li in range(self.lanes - 1, -1, -1):
            lane_stage = li // (self.lanes // 3)
            _, _, lacc, _ = WORLDS[lane_stage]
            bg_ch = f"{fg(235)}┄{RESET}" if li % 2 == 0 else " "
            row = [bg_ch for _ in range(field_w)]
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
            A(f" {border_c}║{RESET} " + "".join(row))

        bank = [f"{fg(28)}▀{RESET}" for _ in range(field_w)]
        if frog_lane == 0 or dead:
            spr = SQUISH if dead else FROG
            col = 196 if dead else 118
            bank[frog_x] = fg(col) + spr + RESET
            if not dead and frog_x + 1 < field_w:
                bank[frog_x + 1] = ""
        A(f" {border_c}║{RESET} " + "".join(bank))

        A(f" {border_c}╠── {fg(245)}┈┈ build output ┈┈{border_c} {'─' * max(1, w - 26)}╣{RESET}")
        for ln in list(snap["tail"])[-4:]:
            A(_format_log_line(ln, field_w))
        A(f" {border_c}╚{'═' * (w - 3)}╝{RESET}")

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
        return compute_smooth_progress(snap, self.files_per_stage, self.lines_per_stage)

    def _slot(self, want: int, width: int, taken: set) -> int:
        x = want % max(1, width)
        for _ in range(width):
            if all(abs(x - t) > 2 for t in taken):
                return x
            x = (x + 3) % width
        return want % width

    def render(self, snap: dict, cols: int, rows: int) -> str:
        self.frame += 1
        w = max(68, cols)
        field_w = w - 4
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

        SKY_H, CANOPY_H = 4, 4
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
            if night and hsh < 6:
                tw = "✦" if (x + self.frame // 3) % 2 else "·"
                put(hsh % SKY_H, x, fg(229) + tw + RESET)
            if building and (x * 37 + self.frame * 2) % 29 == 0:
                put((self.frame + x) % SKY_H, x, fg(sky_c) + "·" + RESET)

        # Center Torii Shrine Gate
        mid_x = field_w // 2
        base_r = SKY_H + CANOPY_H
        put(base_r, mid_x - 2, fg(196) + "┃" + RESET)
        put(base_r, mid_x + 2, fg(196) + "┃" + RESET)
        put(base_r - 1, mid_x - 3, fg(196) + "┳" + RESET)
        for dx in range(-2, 3):
            put(base_r - 1, mid_x + dx, fg(196) + "━" + RESET)
        put(base_r - 1, mid_x + 3, fg(196) + "┳" + RESET)

        GROW = 14
        for pl in self.plants:
            age = (self.frame - pl.born_frame) // GROW
            col = 196 if pl.withered else pl.hue
            if pl.withered:
                put(base_r, pl.x, fg(196) + "†" + RESET)
                continue
            if age == 0:
                put(base_r, pl.x, fg(col) + "." + RESET)
            elif age == 1:
                put(base_r, pl.x, fg(col) + "🌱"[0] if False else fg(col) + "," + RESET)
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
            n_fly = 16 if success else 8
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
        border_c = fg(239)

        A(f" {border_c}╔{'═' * (w - 3)}╗{RESET}")
        A(f" {border_c}║{RESET} {BOLD}{fg(114)}🌸 ZCC ZEN GARDEN v8.0{RESET} │ {fg(sky_c)}stage {stg + 1} · {season}{RESET} │ "
          f"{fg(46)}planted:{len(self.plants)}{RESET} {fg(214)}weeds:{len(self.weeds)}{RESET} "
          f"{fg(196)}withered:{sum(pl.withered for pl in self.plants)}{RESET} │ "
          f"{fg(245)}id:{garden_id} ⏱ {int(elapsed // 60):02d}:{int(elapsed % 60):02d}{RESET}")
        bar_w = max(20, field_w - 10)
        A(f" {border_c}║{RESET} [{render_gradient_bar(p, bar_w, self.frame)}] {BOLD}{fg(114)}{int(p * 100):3d}%{RESET}")
        A(f" {border_c}║{RESET} {render_pipeline_ribbon(snap, field_w, self.frame)}")
        A(f" {border_c}╠{'═' * (w - 3)}╣{RESET}")

        for row in canvas:
            A(f" {border_c}║{RESET} " + "".join(row))
        gcol = 130 if failed else (94 if stg == 1 else 28)
        A(f" {border_c}║{RESET} " + fg(gcol) + "▔" * field_w + RESET)
        pond = "".join(
            fg(31 if (x + self.frame) % 7 else 51) + ("≈" if (x + self.frame) % 5 else "∼")
            for x in range(field_w)
        ) + RESET
        A(f" {border_c}║{RESET} " + pond)

        A(f" {border_c}╠── {fg(245)}┈┈ build output ┈┈{border_c} {'─' * max(1, w - 26)}╣{RESET}")
        for ln in list(snap["tail"])[-4:]:
            A(_format_log_line(ln, field_w))
        A(f" {border_c}╚{'═' * (w - 3)}╝{RESET}")

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
# ZKAEDI PRIME Mode v8.0 — Dual-Pane Coupled Hamiltonian Phase-Space & Tumblers
# ---------------------------------------------------------------------------
def _sigmoid(z: float) -> float:
    return 1.0 / (1.0 + math.exp(-max(-15.0, min(15.0, z))))


class PrimeEnergyGame:
    """ZKAEDI PRIME v8.0 Recursively Coupled Hamiltonian Energy Landscape & 4-Tumbler Observatory."""

    CONTOUR_GLYPHS = (
        (2.4, 201, "█"),
        (1.6, 208, "▓"),
        (0.9, 226, "▒"),
        (0.3, 46, "░"),
        (-0.2, 51, "∙"),
        (-0.8, 39, "·"),
        (-1.5, 57, "·"),
    )

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
        self.h_history: Deque[float] = deque(maxlen=24)
        self.trail: Deque[Tuple[int, int]] = deque(maxlen=8)
        self.rng = random.Random(42)
        self.companion: Optional[GemmaCompanionThread] = None

    def set_companion(self, comp: Optional[GemmaCompanionThread]) -> None:
        self.companion = comp

    def progress(self, snap: dict) -> float:
        return compute_smooth_progress(snap, self.files_per_stage, self.lines_per_stage)

    def render(self, snap: dict, cols: int, rows: int) -> str:
        self.frame += 1
        w = max(78, cols)
        inner_w = w - 4
        p = self.progress(snap)
        stg = snap["stage"]
        _, _, accent, world_name = WORLDS[stg]

        if snap["warnings"] > self.seen_warnings:
            diff = snap["warnings"] - self.seen_warnings
            self.scars += diff
            self.seen_warnings = snap["warnings"]
            self.h_prev += self.kick * diff

        # Canonical ZKAEDI PRIME Equation:
        # H_t = H_base + eta * H_(t-1) * sigmoid(gamma * H_(t-1)) + eps * N(0, 1 + beta * |H_(t-1)|)
        h_base = (1.0 - p) * 5.0
        noise = self.rng.gauss(0, 1 + self.beta * abs(self.h_prev)) * self.eps
        h_t = h_base + self.eta * self.h_prev * _sigmoid(self.gamma * self.h_prev) + noise
        self.h_prev = h_t
        self.h_history.append(h_t)

        elapsed = snap["elapsed"]
        eta_rem = (elapsed / p - elapsed) if p > 0.02 else float("inf")
        eta_s = f"{int(eta_rem // 60):02d}:{int(eta_rem % 60):02d}" if eta_rem != float("inf") else "--:--"
        lps = snap["total_lines"] / max(0.1, elapsed)
        loschmidt_f = 1.0 - (1.0 - p) * 1e-14

        if p >= 0.99 or (snap["finished"] and snap["exit_code"] == 0):
            regime_badge = f"{BOLD}{fg(46)}[⟐ H0 FIXED-POINT LOCK]{RESET}"
        elif self.scars > 0:
            regime_badge = f"{BOLD}{fg(226)}[REGIME II: SCAR+ε NAV]{RESET}"
        else:
            regime_badge = f"{BOLD}{fg(51)}[REGIME I: η-FIELD SHAPING]{RESET}"

        border_c = fg(39)
        out: List[str] = []
        A = out.append

        # ── Top Sovereign Frame & Telemetry Header ──
        A(f" {border_c}╔{'═' * (w - 3)}╗{RESET}")
        A(f" {border_c}║{RESET} {BOLD}{fg(51)}⚡ ZKAEDI PRIME v2.0 // HAMILTONIAN PHASE-SPACE OBSERVATORY{RESET} "
          f"│ {regime_badge} │ {fg(250)}⏱ {int(elapsed // 60):02d}:{int(elapsed % 60):02d} (eta {eta_s}){RESET}")
        A(f" {border_c}║{RESET} {fg(213)}Hamiltonian Energy: H={h_t:.4f}{RESET} "
          f"│ {fg(226)}η={self.eta} γ={self.gamma} β={self.beta} ε={self.eps}{RESET} "
          f"│ {fg(196)}Scars:{self.scars}{RESET} │ {fg(46)}F_echo:{loschmidt_f:.12f}{RESET} "
          f"│ {fg(245)}{lps:4.1f} ln/s{RESET}")

        bar_w = max(24, inner_w - 10)
        A(f" {border_c}║{RESET} [{render_gradient_bar(p, bar_w, self.frame, GRADIENT_PRIME)}] {BOLD}{fg(51)}{int(p * 100):3d}%{RESET}")
        A(f" {border_c}║{RESET} {render_pipeline_ribbon(snap, inner_w, self.frame)}")

        # ── Dual-Pane Split: Left = 2D Phase-Space Attractor Field, Right = Telemetry & 4 Tumblers ──
        right_w = 36
        left_w = max(28, inner_w - right_w - 3)
        grid_h = 9

        A(f" {border_c}╠{'═' * (left_w + 2)}╦{'═' * (right_w + 1)}╣{RESET}")
        left_hdr = pad_ansi(f" {BOLD}{fg(51)}◈ 2D COUPLED HAMILTONIAN FIELD H_t(x,y){RESET}", left_w + 2)
        right_hdr = pad_ansi(f" {BOLD}{fg(226)}◈ TELEMETRY & RELEASE TUMBLERS{RESET}", right_w + 1)
        A(f" {border_c}║{RESET}{left_hdr}{border_c}║{RESET}{right_hdr}{border_c}║{RESET}")
        A(f" {border_c}╟{'─' * (left_w + 2)}╫{'─' * (right_w + 1)}╢{RESET}")

        cx, cy = left_w // 2, grid_h // 2
        r_orbit = max(0.4, (1.0 - p) * (left_w * 0.40))
        angle = self.frame * 0.28
        px = int(cx + r_orbit * math.cos(angle))
        py = int(cy + (r_orbit * 0.42) * math.sin(angle))
        if not self.trail or self.trail[-1] != (px, py):
            self.trail.append((px, py))
        trail_list = list(self.trail)

        # Build right-side telemetry rows (exact 9 rows to match grid_h)
        s2_hash = snap["stage2_sha"][:14] if snap.get("stage2_sha") else "pending..."
        s3_hash = snap["stage3_sha"][:14] if snap.get("stage3_sha") else "pending..."
        ok_end = snap["finished"] and snap["exit_code"] == 0
        t0_ok = bool(snap.get("gate1_ok")) or ok_end
        t1_ok = bool(snap.get("semantic_ok")) or ok_end
        t2_ok = bool(snap.get("abi_ok")) or ok_end
        t3_ok = bool(snap.get("argon_ok")) or ok_end
        ag_n = snap.get("argon_step", 0)

        def tumbler_str(name: str, seated: bool, active: bool, extra: str = "") -> str:
            if seated:
                badge = f"{fg(46)}[● SHEAR]{RESET}"
            elif active:
                badge = f"{fg(226)}[{SPINNERS[self.frame % len(SPINNERS)]} {extra or 'RAKE '}]{RESET}"
            else:
                badge = f"{fg(240)}[○ PIN  ]{RESET}"
            return f" {fg(250)}{name:<18}{RESET} {badge}"

        # Animated Wiper-Blade Snake Rake profile under the 4 pins
        pins_seated = [t0_ok, t1_ok, t2_ok, t3_ok]
        pin_viz = " ".join(
            f"{fg(46)}▀{RESET}" if st else (f"{fg(226)}▄{RESET}" if i == sum(pins_seated) else f"{fg(196)}█{RESET}")
            for i, st in enumerate(pins_seated)
        )
        rake_offset = " " * (self.frame % 4)
        snake_viz = f"{fg(250)}══{rake_offset}{fg(51)}∿∿∿▸{RESET}"

        right_rows = [
            f" {fg(213)}H_t Spark:{RESET} {render_sparkline(self.h_history, max(8, right_w - 13))}",
            f" {fg(245)}Stage2 SHA:{RESET} {fg(120 if snap.get('stage2_sha') else 242)}{s2_hash}{RESET}",
            f" {fg(245)}Stage3 SHA:{RESET} {fg(120 if snap.get('stage3_sha') else 242)}{s3_hash}{RESET}",
            f" {BOLD}{fg(220)}── WIPER SNAKE-RAKE TUMBLERS ──{RESET}",
            f" {fg(244)}Pins:{RESET} [{pin_viz}] {snake_viz}",
            tumbler_str("T0 Stage2==Stage3", t0_ok, snap["stage"] >= 1 and not t0_ok),
            tumbler_str("T1 GCC Semantic", t1_ok, t0_ok and not t1_ok),
            tumbler_str("T2 SystemV ABI", t2_ok, t1_ok and not t2_ok),
            tumbler_str("T3 ARGON-4 Suite", t3_ok, (t2_ok or ag_n > 0) and not t3_ok, f"{ag_n}/6" if ag_n else "RAKE"),
        ]

        for y in range(grid_h):
            row_chars: List[str] = []
            for x in range(left_w):
                dx = (x - cx) / max(1.0, left_w / 2.2)
                dy = (y - cy) / max(1.0, grid_h / 2.0)
                dist = math.sqrt(dx * dx + dy * dy)
                swirl = math.atan2(dy, dx)

                if x == cx and y == cy and (p >= 0.98 or t0_ok):
                    row_chars.append(f"{BOLD}{fg(46)}⟐{RESET}")
                elif x == px and y == py:
                    row_chars.append(f"{BOLD}{fg(51)}✦{RESET}")
                elif x == cx and y == cy:
                    row_chars.append(f"{BOLD}{fg(213)}⊙{RESET}")
                elif (x, y) in trail_list[:-1]:
                    t_idx = trail_list.index((x, y))
                    t_ch = "★" if t_idx >= len(trail_list) - 3 else ("•" if t_idx >= len(trail_list) - 5 else "·")
                    row_chars.append(f"{fg(87)}{t_ch}{RESET}")
                else:
                    # Coupled wave-potential contour + harmonic spiral
                    wave = math.sin(dist * 6.5 - self.frame * 0.22 + swirl) * h_t
                    well = math.cos(dist * 3.14159) * (1.2 - 0.6 * p)
                    val = wave + well
                    placed = False
                    for thresh, col, gl in self.CONTOUR_GLYPHS:
                        if val > thresh:
                            row_chars.append(f"{fg(col)}{gl}{RESET}")
                            placed = True
                            break
                    if not placed:
                        if x == cx:
                            row_chars.append(f"{fg(236)}┊{RESET}")
                        elif y == cy:
                            row_chars.append(f"{fg(236)}┈{RESET}")
                        else:
                            row_chars.append(" ")

            left_str = " " + "".join(row_chars) + " "
            right_str = pad_ansi(right_rows[y], right_w + 1)
            A(f" {border_c}║{RESET}{left_str}{border_c}║{RESET}{right_str}{border_c}║{RESET}")

        if self.companion:
            status_txt = f"{fg(46)}● ONLINE ({self.companion.latency_ms:.1f}ms){RESET}" if self.companion.connected else f"{fg(240)}○ STANDBY{RESET}"
            A(f" {border_c}╠{'═' * (left_w + 2)}╩{'═' * (right_w + 1)}╣{RESET}")
            A(f" {border_c}║{RESET} {BOLD}{fg(81)}🤖 GEMMA ARCHON COGNITION {status_txt}{RESET} │ {fg(226)}{self.companion.agent_title}{RESET}")
            A(f" {border_c}║{RESET}   {fg(250)}Insight:{RESET} {fg(255)}{clip_ansi(self.companion.last_thought, inner_w - 12)}{RESET}")
            A(f" {border_c}╠── {fg(245)}┈┈ build output ┈┈{border_c} {'─' * max(1, w - 26)}╣{RESET}")
        else:
            A(f" {border_c}╠{'═' * (left_w + 2)}╩{'═' * (right_w + 1)}╣{RESET}")
            A(f" {border_c}║{RESET} {fg(245)}┈┈ build output ┈┈{RESET}  {fg(240)}(lines: {snap['total_lines']} │ files: {len(snap['files_seen'])} │ warnings: {snap['warnings']} │ errors: {snap['errors']}){RESET}")

        for ln in list(snap["tail"])[-5:]:
            A(_format_log_line(ln, inner_w))
        A(f" {border_c}╚{'═' * (w - 3)}╝{RESET}")

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
# ZCC Quest HUD v8.0 — Sovereign Mode (--mode sovereign / tablet / enki)
# Real-time 7-system status matrix, 16-node wavepacket probability density
# ---------------------------------------------------------------------------
class SovereignHudGame:
    """ZCC Quest HUD v8.0: 7-System Matrix & 16-Node CTQW Quantum Probability Density."""

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

    def __init__(self, files_per_stage: int, lines_per_stage: int, theme: str = "sovereign"):
        self.files_per_stage = files_per_stage
        self.lines_per_stage = lines_per_stage
        self.theme = theme
        self.frame = 0
        self.eta = 0.4
        self.gamma = 0.3
        self.beta = 0.1
        self.eps = 0.05
        self.h_prev = 1.0
        self.scars = 0
        self.seen_warnings = 0
        self.rng = random.Random(1337)
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
        return compute_smooth_progress(snap, self.files_per_stage, self.lines_per_stage)

    def render(self, snap: dict, cols: int, rows: int) -> str:
        self.frame += 1
        w = max(80, cols)
        field_w = w - 4
        p = self.progress(snap)
        stg = snap["stage"]

        if snap["warnings"] > self.seen_warnings:
            diff = snap["warnings"] - self.seen_warnings
            self.scars += diff
            self.seen_warnings = snap["warnings"]
            self.h_prev += 2.0 * diff

        h_base = (1.0 - p) * 4.5
        noise = self.rng.gauss(0, 1 + self.beta * abs(self.h_prev)) * self.eps
        h_t = h_base + self.eta * self.h_prev * _sigmoid(self.gamma * self.h_prev) + noise
        self.h_prev = h_t

        out: List[str] = []
        A = out.append
        elapsed = snap["elapsed"]
        eta = (elapsed / p - elapsed) if p > 0.02 else float("inf")
        eta_s = f"{int(eta // 60):02d}:{int(eta % 60):02d}" if eta != float("inf") else "--:--"
        border_c = fg(46 if p >= 1.0 else 39)

        if self.theme == "tablet":
            title_banner = f"{BOLD}{fg(214)}𒀭 ZCC SUMERIAN TABLET v8.0 // CUNEIFORM SCRIBE ARCHIVE{RESET}"
        elif self.theme == "enki":
            title_banner = f"{BOLD}{fg(51)}🌊 ZCC ENKI ABZU OBSERVATORY v8.0 // SACRED ME-DECREE ENGINE{RESET}"
        else:
            title_banner = f"{BOLD}{fg(46)}🔱 ZCC QUEST HUD v8.0-ULTIMATE // SOVEREIGN QUANTUM COCKPIT{RESET}"

        # 1. Master Telemetry Header
        A(f" {border_c}╔{'═' * (w - 3)}╗{RESET}")
        A(f" {border_c}║{RESET} {title_banner} "
          f"│ {fg(51)}Stage {stg+1}/3{RESET} │ {fg(213)}H={h_t:6.4f}{RESET} │ {fg(226)}Scars:{self.scars}{RESET} "
          f"│ {fg(250)}⏱ {int(elapsed // 60):02d}:{int(elapsed % 60):02d} (eta {eta_s}){RESET}")

        # 2. Quantum Progress Bar with Palindromic Loschmidt Inversion Fidelity
        bar_w = max(20, field_w - 32)
        loschmidt_f = 1.000000000000000 - (1.0 - p) * 1e-15
        A(f" {border_c}║{RESET} [{render_gradient_bar(p, bar_w, self.frame)}] {BOLD}{fg(46)}{int(p * 100):3d}%{RESET}"
          f" │ {fg(46)}F_echo: {loschmidt_f:.14f}{RESET}")
        A(f" {border_c}║{RESET} {render_pipeline_ribbon(snap, field_w, self.frame)}")

        # 3. 7-System Sovereign Matrix
        A(f" {border_c}╠── {BOLD}{fg(226)}7-SYSTEM SOVEREIGN STATUS MATRIX{RESET}{border_c} {'═' * max(1, w - 39)}╣{RESET}")
        sys_active_count = min(7, int(p * 7) + (1 if not snap["finished"] else 0))
        for idx, (sys_name, desc, metric) in enumerate(self.SYSTEMS):
            if idx < sys_active_count or (snap["finished"] and snap["exit_code"] == 0):
                status_glyph = f"{fg(46)}● ONLINE {RESET}"
                meter = f"{fg(46)}▰▰▰▰▰▰{RESET}"
            elif idx == sys_active_count:
                status_glyph = f"{fg(226)}◐ SYNCING{RESET}"
                fill_m = (self.frame % 5) + 1
                meter = f"{fg(226)}{'▰' * fill_m}{fg(238)}{'▱' * (6 - fill_m)}{RESET}"
            else:
                status_glyph = f"{fg(240)}○ STANDBY{RESET}"
                meter = f"{fg(238)}▱▱▱▱▱▱{RESET}"
            A(f" {border_c}║{RESET}  [{idx+1}] {BOLD}{fg(51)}{sys_name:<14}{RESET} │ {desc:<36} │ {status_glyph} │ {meter} │ {fg(245)}{metric}{RESET}")

        # 4. 16-Node CTQW Quantum Probability Density Topography
        A(f" {border_c}╠── {BOLD}{fg(213)}16-NODE CTQW QUANTUM DENSITY TOPOGRAPHY (|ψ_j|^2 · λ*=8.000){RESET}{border_c} {'═' * max(1, w - 67)}╣{RESET}")
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
            bar_ch = SPARK_CHARS[min(7, int(densities[j] * 14))]
            row1.append(f"{fg(c)}{bar_ch}{self.NODES[j][:4]}:{densities[j]*100:4.1f}%{RESET}")
        for j in range(8, 16):
            c = 51 if j == 8 else (213 if densities[j] > 0.02 else 240)
            bar_ch = SPARK_CHARS[min(7, int(densities[j] * 14))]
            row2.append(f"{fg(c)}{bar_ch}{self.NODES[j][:4]}:{densities[j]*100:4.1f}%{RESET}")
        A(f" {border_c}║{RESET}  " + " │ ".join(row1))
        A(f" {border_c}║{RESET}  " + " │ ".join(row2))

        # 5. Continuous 2D Wavefield Radar Phase Space
        grid_h = 4
        cx, cy = field_w // 2, grid_h // 2
        r_orbit = max(0.5, (1.0 - p) * (field_w // 4))
        angle = self.frame * 0.2
        px = int(cx + r_orbit * math.cos(angle))
        py = int(cy + (r_orbit * 0.3) * math.sin(angle))
        A(f" {border_c}╠── {BOLD}{fg(51)}RECURSIVE TOPOLOGICAL PHASE PORTRAIT (H_t){RESET}{border_c} {'═' * max(1, w - 49)}╣{RESET}")
        for y in range(grid_h):
            row = []
            for x in range(field_w):
                dx = (x - cx) / max(1.0, field_w / 2.5)
                dy = (y - cy) / max(1.0, grid_h / 2.0)
                dist = math.sqrt(dx * dx + dy * dy)
                if x == cx and y == cy and p >= 0.98:
                    row.append(fg(46) + "⟐" + RESET)
                elif x == px and y == py:
                    row.append(fg(51) + "✦" + RESET)
                elif x == cx and y == cy:
                    row.append(fg(213) + "⊙" + RESET)
                else:
                    val = math.sin(dist * 5.0 - self.frame * 0.2) * h_t
                    if val > 1.2:
                        row.append(fg(201) + "▓" + RESET)
                    elif val > 0.4:
                        row.append(fg(51) + "░" + RESET)
                    elif val > -0.4:
                        row.append(fg(39) + "·" + RESET)
                    else:
                        row.append(" ")
            A(f" {border_c}║{RESET} " + "".join(row))

        # 6. Gemma LoRA AI Copilot Telemetry
        if self.companion:
            status_txt = f"{fg(46)}● ONLINE ({self.companion.latency_ms:.1f}ms){RESET}" if self.companion.connected else f"{fg(240)}○ STANDBY{RESET}"
            A(f" {border_c}╠── {BOLD}{fg(81)}🤖 GEMMA LO-RA COGNITIVE COPILOT {status_txt}{RESET}{border_c} {'─' * max(1, w - 55)}╣{RESET}")
            A(f" {border_c}║{RESET}   {fg(226)}Agent:{RESET} {fg(51)}{self.companion.agent_title:<38}{RESET} │ {fg(245)}ID:{RESET} {self.companion.active_agent}")
            thought_line = clip_ansi(self.companion.last_thought, max(20, field_w - 12))
            A(f" {border_c}║{RESET}   {fg(250)}Insight:{RESET} {fg(255)}{thought_line}{RESET}")

        # 7. Build Output Stream
        A(f" {border_c}╠── {fg(245)}┈┈ live build stream ┈┈{border_c} {'─' * max(1, w - 30)}╣{RESET}")
        for ln in list(snap["tail"])[-4:]:
            A(_format_log_line(ln, field_w))
        A(f" {border_c}╚{'═' * (w - 3)}╝{RESET}")

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
# Lockpick / Wiper-Blade Snake Rake Mode (--mode snake / --mode lockpick)
# ---------------------------------------------------------------------------
class LockpickGame:
    """Cutaway 6-Pin Brass Cylinder & 301-Stainless Wiper-Insert Snake Rake Simulator."""

    CHAMBERS = [
        ("P1:BOOT", "Serrated", 2),
        ("P2:STG2", "Spool", 3),
        ("P3:STG3", "Spool", 2),
        ("P4:CMP",  "Mushroom", 3),
        ("P5:ABI",  "Serrated", 2),
        ("P6:ARGN", "Standard", 3),
    ]

    def __init__(self, files_per_stage: int, lines_per_stage: int):
        self.files_per_stage = files_per_stage
        self.lines_per_stage = lines_per_stage
        self.frame = 0
        self.strokes = 0
        self.false_sets = 0
        self.seen_warnings = 0
        self.companion: Optional[GemmaCompanionThread] = None

    def set_companion(self, comp: Optional[GemmaCompanionThread]) -> None:
        self.companion = comp

    def progress(self, snap: dict) -> float:
        return compute_smooth_progress(snap, self.files_per_stage, self.lines_per_stage)

    def render(self, snap: dict, cols: int, rows: int) -> str:
        self.frame += 1
        self.strokes += 1
        w = max(78, cols)
        field_w = w - 4
        p = self.progress(snap)
        stg = snap["stage"]

        if snap["warnings"] > self.seen_warnings:
            self.false_sets += snap["warnings"] - self.seen_warnings
            self.seen_warnings = snap["warnings"]

        ok_end = snap["finished"] and snap["exit_code"] == 0
        pins_set = 6 if ok_end else min(6, int(p * 6.2))
        binding_idx = min(5, pins_set)
        rake_shift = int(abs((self.frame % 12) - 6) * 2)
        torque = 4.2 + 0.6 * math.sin(self.frame * 0.3)

        out: List[str] = []
        A = out.append
        elapsed = snap["elapsed"]
        eta = (elapsed / p - elapsed) if p > 0.02 else float("inf")
        eta_s = f"{int(eta // 60):02d}:{int(eta % 60):02d}" if eta != float("inf") else "--:--"
        border_c = fg(220 if not ok_end else 46)

        A(f" {border_c}╔{'═' * (w - 3)}╗{RESET}")
        A(f" {border_c}║{RESET} {BOLD}{fg(220)}🔓 ZCC LOCKSPORT v8.0 // 301-WIPER STAINLESS SNAKE RAKE & 6-PIN CUTAWAY{RESET} "
          f"│ {fg(51)}Pins:{pins_set}/6{RESET} │ {fg(214)}Spools:{self.false_sets}{RESET} "
          f"│ {fg(250)}⏱ {int(elapsed // 60):02d}:{int(elapsed % 60):02d} (eta {eta_s}){RESET}")
        bar_w = max(20, field_w - 30)
        A(f" {border_c}║{RESET} [{render_gradient_bar(p, bar_w, self.frame)}] {BOLD}{fg(46 if ok_end else 226)}{int(p * 100):3d}%{RESET}"
          f" │ {fg(51)}TOK Torque: {torque:3.1f} N·cm{RESET}")
        A(f" {border_c}║{RESET} {render_pipeline_ribbon(snap, field_w, self.frame)}")
        A(f" {border_c}╠── {BOLD}{fg(220)}BRASS BIBLE & 6-CHAMBER SHEAR-LINE CUTAWAY{RESET}{border_c} {'═' * max(1, w - 50)}╣{RESET}")

        # Render 6 vertical pin chambers across the cylinder
        col_step = 10
        left_pad = 6
        # Chamber labels
        lbl_row = " " * left_pad + "".join(
            f"{fg(46 if i < pins_set else (226 if i == binding_idx else 245))}{name:<10}{RESET}"
            for i, (name, _, _) in enumerate(self.CHAMBERS)
        )
        type_row = " " * left_pad + "".join(
            f"{fg(240)}{ptype:<10}{RESET}"
            for _, ptype, _ in self.CHAMBERS
        )
        A(f" {border_c}║{RESET} {lbl_row}")
        A(f" {border_c}║{RESET} {type_row}")

        # 5 rows of cylinder bible (springs + driver pins above shear line) + shear line + plug (key pins + snake rake)
        for r in range(6):
            cells: List[str] = []
            for i, (_, ptype, klen) in enumerate(self.CHAMBERS):
                is_set = i < pins_set
                is_bind = (i == binding_idx) and not ok_end
                # Vertical lift offset: set pins sit cleanly with driver above shear (r=0..2) and key pin below (r=3..4)
                if r == 0:
                    ch = f"{fg(244)}║ ⌇ ║{RESET}"
                elif r == 1:
                    ch = f"{fg(244)}║{RESET} {fg(46 if is_set else 214)}▓{RESET} {fg(244)}║{RESET}" if is_set else f"{fg(244)}║ ⌇ ║{RESET}"
                elif r == 2:
                    if is_set:
                        ch = f"{fg(244)}║{RESET} {fg(46)}▓{RESET} {fg(244)}║{RESET}"
                    elif is_bind:
                        ch = f"{fg(244)}║{RESET} {fg(226)}▒{RESET} {fg(244)}║{RESET}"
                    else:
                        ch = f"{fg(244)}║{RESET} {fg(208)}▓{RESET} {fg(244)}║{RESET}"
                elif r == 3:
                    # SHEAR LINE ROW
                    if is_set:
                        ch = f"{fg(46)}══╪══{RESET}"
                    elif is_bind:
                        ch = f"{fg(226)}══▓══{RESET}"
                    else:
                        ch = f"{fg(196)}══▓══{RESET}"
                elif r == 4:
                    # Key pin upper in plug
                    kp_col = 46 if is_set else (226 if is_bind else 51)
                    spark = "✦" if (is_set and (self.frame + i) % 5 == 0) else " "
                    ch = f"{fg(240)}│{RESET}{spark}{fg(kp_col)}█{RESET} {fg(240)}│{RESET}"
                else:
                    # Key pin tip riding on the snake rake
                    kp_col = 46 if is_set else (226 if is_bind else 51)
                    ch = f"{fg(240)}│{RESET} {fg(kp_col)}▼{RESET} {fg(240)}│{RESET}"
                cells.append(ch + "     ")
            prefix = f"{fg(46 if pins_set == 6 else 226)}SHEAR▸{RESET}" if r == 3 else "      "
            suffix = f"{fg(46)}◀ PLUG UNLOCKED (+90°){RESET}" if (r == 3 and ok_end) else (f"{fg(244)}◀ SHEAR LINE{RESET}" if r == 3 else "")
            A(f" {border_c}║{RESET} {prefix}" + "".join(cells) + suffix)

        # Wiper-Blade Snake Rake + Tension Wrench Row inside Keyway
        rake_pad = " " * (2 + rake_shift)
        snake_tool = (
            f"{fg(245)}[WIPER-301 HANDLE]{RESET}"
            f"{fg(250)}══════{rake_pad}{BOLD}{fg(51)}───∿∿∿╭─╮∿∿▸{RESET}"
        )
        wrench_tool = f"  {fg(220)}┗━━━┳━ [TOK WRENCH]{RESET}"
        A(f" {border_c}║{RESET}   {snake_tool}{wrench_tool}")

        A(f" {border_c}╠── {fg(245)}┈┈ build output ┈┈{border_c} {'─' * max(1, w - 26)}╣{RESET}")
        for ln in list(snap["tail"])[-4:]:
            A(_format_log_line(ln, field_w))
        A(f" {border_c}╚{'═' * (w - 3)}╝{RESET}")

        if snap["finished"]:
            if snap["exit_code"] == 0:
                if snap.get("oracle_verified", False):
                    A(f" {BOLD}{fg(46)}★ 🔓 PLUG TURNED +90° — 6/6 PINS AT SHEAR LINE — {BOSS_NAME} — exit 0 "
                      f"(strokes: {self.strokes}) ★{RESET}")
                else:
                    A(f" {BOLD}{fg(46)}★ 🔓 CYLINDER OPEN — COMMAND SUCCEEDED — exit 0 "
                      f"(strokes: {self.strokes}) ★{RESET}")
            else:
                A(f" {BOLD}{fg(196)}✖ OVERSHOT SPOOL PIN — exit {snap['exit_code']} — "
                  f"Keyway jammed. Check the .build log.{RESET}")
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
    ap.add_argument("--mode", choices=("runner", "frogger", "garden", "prime", "sovereign", "tablet", "enki", "snake", "lockpick"), default="sovereign",
                    help="snake/lockpick: wiper-blade snake rake tumbler cutaway · prime: Hamiltonian energy · sovereign: 7-system matrix · garden/runner/frogger")
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
    elif args.mode in ("snake", "lockpick"):
        game = LockpickGame(args.files, args.lines_per_stage)
    elif args.mode in ("sovereign", "tablet", "enki"):
        game = SovereignHudGame(args.files, args.lines_per_stage, theme=args.mode)
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
    last_frame = ""
    try:
        while True:
            size = shutil.get_terminal_size(fallback=(100, 30))
            snap = state.snapshot()
            frame = game.render(snap, size.columns, size.lines)
            last_frame = frame
            if interactive:
                sys.stdout.write(CSI + "H" + CSI + "2J" + frame + "\n")
            else:
                if frames % max(1, int(args.fps * 5)) == 0 or snap["finished"]:
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
            if rc == 0 and not args.demo and is_bootstrap_command(args.cmd):
                if not snap.get("oracle_verified", False):
                    rc = 30
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
            if last_frame:
                sys.stdout.write(last_frame + "\n")
            sys.stdout.flush()

    snap = state.snapshot()
    print(f"zcc_quest: child exit={snap['exit_code']} wrapper_exit={rc} "
          f"oracle_verified={snap.get('oracle_verified', False)} "
          f"warnings={snap['warnings']} errors={snap['errors']} "
          f"elapsed={snap['elapsed']:.1f}s (raw output: {args.log}.build)")
    if not args.demo and is_bootstrap_command(args.cmd) and snap["exit_code"] == 0 and not snap.get("oracle_verified", False):
        print("[FAIL-CLOSED] zcc_quest: bootstrap command exited 0 without oracle verification; wrapper returning exit 30", file=sys.stderr)
    elif snap.get("oracle_verified", False) and snap.get("run_id"):
        repo_dir = os.path.dirname(os.path.abspath(__file__))
        ev_dir = os.path.join(repo_dir, "evidence", snap["run_id"])
        s2_p = os.path.join(ev_dir, "stage2.s")
        s3_p = os.path.join(ev_dir, "stage3.s")
        b2_p = os.path.join(ev_dir, "stage2.bin")
        b3_p = os.path.join(ev_dir, "stage3.bin")
        ledger_p = os.path.join(ev_dir, "provenance.jsonl")
        if all(os.path.isfile(p) for p in (s2_p, s3_p, b2_p, b3_p)):
            print(f"zcc_quest_provenance: run_id={snap['run_id']} ledger={ledger_p}")
            print(f"zcc_quest_sha256: stage2.s={compute_sha256(s2_p)} stage3.s={compute_sha256(s3_p)}")
            print(f"zcc_quest_sha256: stage2.bin={compute_sha256(b2_p)} stage3.bin={compute_sha256(b3_p)}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
