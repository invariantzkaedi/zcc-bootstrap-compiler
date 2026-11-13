#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Drop-in verifier and installer for the ZCC BUG-DEEP-01 / 03 / 05 + 32-bit extension patch.

Modes:
  --check           Verify git apply cleanliness and <50-line part4.c diff budget.
  --apply           Apply deep_01_03_05.patch, rebuild ./zcc, and run regression gates.
  --revert          Reverse deep_01_03_05.patch and rebuild ./zcc.
  --verify-shadow   Test the patch end-to-end inside an ephemeral /tmp shadow repo.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from typing import List, Optional, Tuple

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")

BUNDLE_DIR = Path(__file__).resolve().parent
REPO_ROOT = BUNDLE_DIR.parent.parent
PATCH_PATH = BUNDLE_DIR / "deep_01_03_05.patch"
DIFF_TEST = REPO_ROOT / "tests" / "test_quad_isa_deep_01_03_05.py"
DEEP_REG_TEST = REPO_ROOT / "tests" / "test_quad_isa_deep_regression.py"
QUAD_ISA_GATE = REPO_ROOT / "tools" / "zcc_quad_isa_gate.py"

SHADOW_ROOT_EXTS = {".c", ".h", ".inc", ".py", ".sh"}
SHADOW_ROOT_EXACT = {
    "Makefile",
    "BOOTSTRAP_BASELINES.tsv",
    "corpus_baseline.json",
    "zcc_hyperdim_test_harness.html",
    "build_policy.json",
    "fortify_policy.json",
}
SHADOW_SUBDIRS = [
    "src",
    "include",
    "scripts",
    "tools",
    "tests",
    "courtroom",
    "contracts",
    "tickets",
    "templates",
    "docs/lexicon",
    "docs/evidence/2026-10-28",
    "experiments/zcc_eai_006_g3_m5",
    "experiments/zcc_eai_006_g3_m3_readout",
    ".github",
    "zcc-libc",
    "zcc_sys_includes",
]


def run_cmd(cmd: List[str], cwd: Path = REPO_ROOT, timeout: int = 600) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(cwd), capture_output=True, text=True, errors="replace", timeout=timeout)


def get_numstat_total(rel_file: str, cwd: Path = REPO_ROOT) -> Tuple[int, int]:
    proc = run_cmd(["git", "diff", "--numstat", "HEAD", "--", rel_file], cwd=cwd, timeout=30)
    if proc.returncode != 0 or not proc.stdout.strip():
        return 0, 0
    parts = proc.stdout.strip().split("\t")
    return int(parts[0]), int(parts[1])


def count_patch_changes(patch_file: Path) -> Tuple[int, int]:
    ins = 0
    dels = 0
    for line in patch_file.read_text(encoding="utf-8").splitlines():
        if line.startswith("+++ ") or line.startswith("--- ") or line.startswith("@@"):
            continue
        if line.startswith("+"):
            ins += 1
        elif line.startswith("-"):
            dels += 1
    return ins, dels


def detect_patch_state(cwd: Path = REPO_ROOT) -> str:
    fwd = run_cmd(["git", "apply", "--check", str(PATCH_PATH)], cwd=cwd, timeout=30)
    if fwd.returncode == 0:
        return "APPLICABLE"
    rev = run_cmd(["git", "apply", "--reverse", "--check", str(PATCH_PATH)], cwd=cwd, timeout=30)
    if rev.returncode == 0:
        return "APPLIED"
    return f"CONFLICT: {fwd.stderr.strip() or rev.stderr.strip()}"


def run_post_build_gates(work_dir: Path, zcc_bin: Path, include_selfhost: bool) -> None:
    for label, cmd in [
        ("test_quad_isa_deep_01_03_05", ["python3", str(work_dir / "tests" / "test_quad_isa_deep_01_03_05.py"), str(zcc_bin)]),
        ("test_quad_isa_deep_regression", ["python3", str(work_dir / "tests" / "test_quad_isa_deep_regression.py")]),
    ]:
        print(f"=== Running {label} ===", flush=True)
        res = run_cmd(cmd, cwd=work_dir, timeout=300)
        if res.stdout.strip():
            print(res.stdout.strip(), flush=True)
        if res.returncode != 0:
            raise RuntimeError(f"{label} FAILED (rc={res.returncode}):\n{res.stderr}")

    if include_selfhost:
        for label, cmd in [
            ("Quad-ISA Tier 1-3 + EF-7 UI", ["python3", str(work_dir / "tools" / "zcc_quad_isa_gate.py"), "--strict-tier", "3", "--verify-ui"]),
            ("Quad-ISA Self-Test", ["python3", str(work_dir / "tools" / "zcc_quad_isa_gate.py"), "--self-test"]),
            ("Full Self-Host & ARGON 4 (gate.sh)", ["bash", "gate.sh"]),
        ]:
            print(f"=== Running {label} ===", flush=True)
            res = run_cmd(cmd, cwd=work_dir, timeout=600)
            if res.stdout.strip():
                print(res.stdout.strip()[-2000:], flush=True)
            if res.returncode != 0:
                raise RuntimeError(f"{label} FAILED (rc={res.returncode}):\n{res.stderr[-1500:]}")


def cmd_check() -> int:
    if not PATCH_PATH.is_file():
        print(f"[ERROR] Missing patch file: {PATCH_PATH}")
        return 2
    p_ins, p_dels = count_patch_changes(PATCH_PATH)
    c_ins, c_dels = get_numstat_total("part4.c")
    state = detect_patch_state()
    print(f"Patch file      : {PATCH_PATH}")
    print(f"Patch delta     : +{p_ins} / -{p_dels} = {p_ins + p_dels} changed lines in part4.c (cap < 50: {'PASS' if p_ins + p_dels < 50 else 'FAIL'})")
    print(f"Working part4.c : +{c_ins} / -{c_dels} = {c_ins + c_dels} vs HEAD")
    print(f"Apply state     : {state}")
    if state in ("APPLICABLE", "APPLIED"):
        return 0
    return 1


def cmd_apply(include_selfhost: bool, force_over_50: bool) -> int:
    state = detect_patch_state()
    if state == "APPLIED":
        print("Patch is already applied. Running verification gates...")
    elif state != "APPLICABLE":
        print(f"[ERROR] Cannot apply patch cleanly: {state}")
        return 1
    else:
        p_ins, p_dels = count_patch_changes(PATCH_PATH)
        c_ins, c_dels = get_numstat_total("part4.c")
        cur_total = c_ins + c_dels
        if cur_total > 0 and (cur_total + p_ins + p_dels) >= 50 and not force_over_50:
            print(
                f"[BLOCKED] Working part4.c already has +{c_ins}/-{c_dels} = {cur_total} uncommitted lines vs HEAD. "
                f"Applying +{p_ins}/-{p_dels} would combine >= 50 lines vs HEAD. "
                "Commit prior changes first (recommended) or pass --force-over-50 if explicitly authorized."
            )
            return 2
        ap = run_cmd(["git", "apply", str(PATCH_PATH)], cwd=REPO_ROOT, timeout=30)
        if ap.returncode != 0:
            print(f"[ERROR] git apply failed: {ap.stderr}")
            return 1
        print(f"Applied {PATCH_PATH.name} cleanly.")

    print("=== Rebuilding ./zcc via make zcc ===", flush=True)
    mk = run_cmd(["make", "zcc"], cwd=REPO_ROOT, timeout=300)
    if mk.returncode != 0:
        print(mk.stdout[-1500:], mk.stderr[-1500:])
        print("[ERROR] make zcc failed; run with --revert to roll back.")
        return 1

    run_post_build_gates(REPO_ROOT, REPO_ROOT / "zcc", include_selfhost=include_selfhost)
    print("DROPIN_APPLY=PASS")
    return 0


def cmd_revert() -> int:
    state = detect_patch_state()
    if state == "APPLICABLE":
        print("Patch is not currently applied; nothing to revert.")
        return 0
    if state != "APPLIED":
        print(f"[ERROR] Cannot cleanly reverse patch: {state}")
        return 1
    rv = run_cmd(["git", "apply", "--reverse", str(PATCH_PATH)], cwd=REPO_ROOT, timeout=30)
    if rv.returncode != 0:
        print(f"[ERROR] git apply --reverse failed: {rv.stderr}")
        return 1
    mk = run_cmd(["make", "zcc"], cwd=REPO_ROOT, timeout=300)
    if mk.returncode != 0:
        print(mk.stderr[-1500:])
        return 1
    print("Reverted deep_01_03_05.patch and rebuilt ./zcc cleanly.")
    return 0


def sync_shadow_repo(dst: Path) -> None:
    for folder in ("artifacts", "evidence", "build", ".releases"):
        (dst / folder).mkdir(parents=True, exist_ok=True)
    for entry in os.scandir(REPO_ROOT):
        if not entry.is_file():
            continue
        name = entry.name
        if name == "zcc.c" or name.startswith("zcc_stage") or entry.stat().st_size > 15_000_000:
            continue
        ext = os.path.splitext(name)[1]
        if ext in SHADOW_ROOT_EXTS or name in SHADOW_ROOT_EXACT or name.startswith("Makefile."):
            shutil.copy2(entry.path, dst / name)
    for sub in SHADOW_SUBDIRS:
        src_sub = REPO_ROOT / sub
        if src_sub.is_dir():
            dst_sub = dst / sub
            dst_sub.parent.mkdir(parents=True, exist_ok=True)
            rc = subprocess.run(
                [
                    "rsync", "-a", "--exclude=__pycache__", "--exclude=*.o",
                    "--exclude=*.pyc", "--exclude=*.zip", "--exclude=*.bin",
                    "--max-size=15m", f"{src_sub}/", f"{dst_sub}/",
                ],
                capture_output=True,
                text=True,
            )
            if rc.returncode != 0:
                raise RuntimeError(f"rsync failed for {sub}: {rc.stderr}")


def cmd_verify_shadow(include_selfhost: bool) -> int:
    p_ins, p_dels = count_patch_changes(PATCH_PATH)
    if (p_ins + p_dels) >= 50:
        print(f"[ERROR] Patch exceeds <50 changed-line cap: +{p_ins}/-{p_dels} = {p_ins + p_dels}")
        return 2

    with tempfile.TemporaryDirectory(prefix="zcc_deep_shadow_") as td:
        shadow = Path(td)
        print(f"=== Syncing ephemeral shadow repo at {shadow} ===", flush=True)
        sync_shadow_repo(shadow)

        state = detect_patch_state()
        if state == "APPLICABLE":
            ap = run_cmd(["patch", "-p1", "-i", str(PATCH_PATH)], cwd=shadow, timeout=30)
            if ap.returncode != 0:
                print(f"[ERROR] Shadow patch -p1 failed: {ap.stdout}\n{ap.stderr}")
                return 1
            print(f"Shadow applied {PATCH_PATH.name} (+{p_ins}/-{p_dels} = {p_ins + p_dels} lines < 50).", flush=True)
        elif state == "APPLIED":
            print("Working tree already has patch applied; verifying directly in shadow.", flush=True)
        else:
            print(f"[ERROR] Patch conflict against active part4.c: {state}")
            return 1

        print("=== Building shadow ./zcc ===", flush=True)
        mk = run_cmd(["make", "zcc"], cwd=shadow, timeout=300)
        if mk.returncode != 0:
            print(mk.stdout[-1500:], mk.stderr[-1500:])
            return 1

        run_post_build_gates(shadow, shadow / "zcc", include_selfhost=include_selfhost)
        print("SHADOW_DROPIN_VERIFY=PASS")
        return 0


def to_wsl_path(win_path: Path) -> str:
    resolved = str(win_path.resolve()).replace("\\", "/")
    if len(resolved) >= 2 and resolved[1] == ":":
        return f"/mnt/{resolved[0].lower()}{resolved[2:]}"
    return resolved


def maybe_delegate_to_wsl(args: argparse.Namespace, raw_argv: List[str]) -> Optional[int]:
    if os.name != "nt" or args.check:
        return None
    if not shutil.which("wsl"):
        print("[ERROR] Windows execution of --apply/--revert/--verify-shadow requires WSL Ubuntu.")
        return 2
    wsl_script = to_wsl_path(Path(__file__))
    cmd = ["wsl", "--", "python3", "-u", wsl_script, *raw_argv]
    return subprocess.run(cmd, check=False).returncode


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Drop-in manager for ZCC BUG-DEEP-01/03/05 fixes.")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--check", action="store_true", help="Check clean applicability and diff budget.")
    group.add_argument("--apply", action="store_true", help="Apply patch, rebuild ./zcc, and run regression tests.")
    group.add_argument("--revert", action="store_true", help="Reverse patch cleanly and rebuild ./zcc.")
    group.add_argument("--verify-shadow", action="store_true", help="Verify patch end-to-end inside an ephemeral /tmp repo.")
    parser.add_argument("--selfhost", action="store_true", help="Also run Quad-ISA Tier 1-3 + full bash gate.sh.")
    parser.add_argument("--force-over-50", action="store_true", help="Allow applying onto an already-dirty part4.c.")
    raw_argv = list(sys.argv[1:] if argv is None else argv)
    args = parser.parse_args(raw_argv)

    delegated_rc = maybe_delegate_to_wsl(args, raw_argv)
    if delegated_rc is not None:
        return delegated_rc

    if args.check:
        return cmd_check()
    if args.apply:
        return cmd_apply(include_selfhost=args.selfhost, force_over_50=args.force_over_50)
    if args.revert:
        return cmd_revert()
    return cmd_verify_shadow(include_selfhost=args.selfhost)


if __name__ == "__main__":
    sys.exit(main())

