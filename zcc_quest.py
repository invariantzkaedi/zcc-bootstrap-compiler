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
    python3 zcc_quest.py --cmd "make selfhost" --mode prime
    python3 zcc_quest.py --demo --mode garden           # Zen Garden Mode
    python3 zcc_quest.py --demo --mode prime            # ZKAEDI PRIME Mode
    python3 zcc_quest.py --demo --mode frogger          # Frogger Mode
"""

from __future__ import annotations

import argparse
import hashlib
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
    files_seen: List[str] = field(default_factory=list)   # filenames, in order
    started: float = field(default_factory=time.monotonic)

    def snapshot(self) -> dict:
        with self.lock:
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
                "files_seen": list(self.files_seen),
                "elapsed": time.monotonic() - self.started,
            }


STAGE_RE = re.compile(r"(?:===+\s*)?STAGE\s*([123])", re.I)
BOSS_RE = re.compile(r"BYTE.?IDENTICAL|IDENTICAL\s+OUTPUT|SHA256\s+MATCH|BOOTSTRAP_GREEN|DETERMINISM LOCK SECURED", re.I)
FILE_RE = re.compile(r"(?:^(?:CC|AS|LD|CCLD)\s)|(?:\.(?:c|s|o)\b.*(?:->|→))")
FNAME_RE = re.compile(r"([\w./-]+\.(?:c|s|o))\b")
WARN_RE = re.compile(r"\bwarning\b", re.I)
ERR_RE = re.compile(r"\berror\b|\bfatal\b|segmentation fault", re.I)


def watch_process(cmd: str, state: BuildState, log_path: str) -> None:
    """Run `cmd`, stream its output into `state`, mirror verbatim to log."""
    try:
        proc = subprocess.Popen(
            cmd, shell=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, errors="replace", bufsize=1,
            env=os.environ.copy(),
        )
    except OSError as exc:
        LOG.error("failed to spawn %r: %s", cmd, exc)
        with state.lock:
            state.finished, state.exit_code = True, 127
            state.tail.append(f"spawn failed: {exc}")
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
                    elif BOSS_RE.search(line):
                        state.boss_seen = True
                        LOG.info("boss marker seen: %r", line)
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
        LOG.info("child exited rc=%d", rc)


def demo_process(state: BuildState) -> None:
    """Fake triple-stage build for --demo mode."""
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
        state.tail.append("sha256: zcc2 == zcc3  BYTE-IDENTICAL ✔")
        state.finished, state.exit_code = True, 0


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
                A(f" {BOLD}{fg(46)}★ SELF-HOST COMPLETE — {BOSS_NAME} — exit 0 "
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
                A(f" {BOLD}{fg(46)}★ FROG HOME — {BOSS_NAME} — exit 0 "
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
                A(f" {BOLD}{fg(213)}❀ FULL BLOOM — {BOSS_NAME} — exit 0 — "
                  f"garden {garden_id} will grow the same way next run ❀{RESET}")
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

        A(f" {fg(240)}┈┈ build output ┈┈{RESET}")
        for ln in snap["tail"]:
            color = 196 if ERR_RE.search(ln) else 178 if WARN_RE.search(ln) else 245
            A(f" {fg(color)}{ln[:field_w]}{RESET}")

        if snap["finished"]:
            if snap["exit_code"] == 0:
                A(f" {BOLD}{fg(46)}★ ZKAEDI PRIME FIXED POINT REPO - H0 CONVERGED - exit 0 "
                  f"(scars: {self.scars}, {BOSS_NAME}) ★{RESET}")
            else:
                A(f" {BOLD}{fg(196)}✖ BIFURCATION DRIFT - exit {snap['exit_code']} - "
                  f"Check the .build log.{RESET}")
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
    ap.add_argument("--mode", choices=("runner", "frogger", "garden", "prime"), default="runner",
                    help="runner: side-scroller · frogger: traffic lanes · garden: zen mode · prime: ZKAEDI PRIME Hamiltonian energy plot")
    ap.add_argument("--lanes", type=int, default=9,
                    help="frogger traffic lanes (multiple of 3, 3..15)")
    ap.add_argument("--frames", type=int, default=0,
                    help="render at most N frames then exit (testing/CI)")
    args = ap.parse_args(argv)

    if args.files <= 0 or args.lines_per_stage <= 0 or args.fps <= 0:
        ap.error("--files, --lines-per-stage and --fps must be positive")

    if args.low_power:
        args.fps = 2.0

    _setup_logging(args.log)
    LOG.info("start cmd=%r demo=%s", args.cmd, args.demo)

    state = BuildState()
    target = demo_process if args.demo else watch_process
    t_args = (state,) if args.demo else (args.cmd, state, args.log)
    worker = threading.Thread(target=target, args=t_args, daemon=True)
    worker.start()

    if args.mode == "garden":
        game = GardenGame(args.files, args.lines_per_stage)
    elif args.mode == "frogger":
        if args.lanes % 3 or not (3 <= args.lanes <= 15):
            ap.error("--lanes must be a multiple of 3 in [3, 15]")
        game = FroggerGame(args.files, args.lines_per_stage, args.lanes)
    elif args.mode == "prime":
        game = PrimeEnergyGame(args.files, args.lines_per_stage)
    else:
        game = Game(args.files, args.lines_per_stage)
    interactive = sys.stdout.isatty()
    if interactive:
        sys.stdout.write(ALT_ON + HIDE_CUR)

    stop = {"flag": False}

    def _sigint(_sig, _frm):
        stop["flag"] = True

    signal.signal(signal.SIGINT, _sigint)

    frames = 0
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
    finally:
        if interactive:
            sys.stdout.write(SHOW_CUR + ALT_OFF)
            sys.stdout.flush()

    snap = state.snapshot()
    rc = snap["exit_code"] if snap["exit_code"] is not None else 130
    print(f"zcc_quest: child exit={snap['exit_code']} warnings={snap['warnings']} "
          f"errors={snap['errors']} elapsed={snap['elapsed']:.1f}s "
          f"(raw output: {args.log}.build)")
    return rc


if __name__ == "__main__":
    sys.exit(main())
