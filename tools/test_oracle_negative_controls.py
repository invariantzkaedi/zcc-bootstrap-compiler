#!/usr/bin/env python3
"""
ZCC_ORACLE_NEGATIVE_CONTROLS.py
Automated negative controls and adversarial regression harness for ZCC Quest oracle.
Verifies resolution of ZCC-VER-005.

Fail-closed semantics:
- Exits 0 if and only if ALL negative controls are correctly rejected.
- Exits 1 if ANY negative control falsely certifies.
"""

import os
import shutil
import subprocess
import sys
import tempfile
import time


def run_probe_a(quest_path: str) -> bool:
    """Probe A: counterfeited zero-byte files written during command without compiler."""
    print("--- [CONTROL 1 / PROBE A] Counterfeit zero-byte files with marker ---")
    tmpdir = tempfile.mkdtemp(prefix="zcc-neg-a-")
    try:
        cmd = [
            sys.executable, quest_path,
            "--cmd", (
                "dd if=/dev/zero of=zcc2.s bs=2048 count=1 status=none && "
                "cp zcc2.s zcc3.s && "
                "printf '=== VERDICT: BOOTSTRAP DETERMINISM LOCK SECURED ===\\n'"
            ),
            "--mode", "runner",
            "--frames", "30",
            "--fps", "30",
        ]
        res = subprocess.run(cmd, cwd=tmpdir, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        out = res.stdout + res.stderr
        # Must NOT certify self-host
        certified = "SELF-HOST VERIFIED" in out or "BYTE-IDENTICAL SEAL" in out
        if certified:
            print("[FAIL] Probe A falsely certified counterfeit zero-byte files!")
            return False
        print("[PASS] Probe A correctly rejected counterfeit files (no false certification).")
        return True
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def run_probe_b(quest_path: str) -> bool:
    """Probe B: pre-existing identical files with marker-only command."""
    print("--- [CONTROL 2 / PROBE B] Pre-existing identical files with marker ---")
    tmpdir = tempfile.mkdtemp(prefix="zcc-neg-b-")
    try:
        # Pre-create identical files
        s2 = os.path.join(tmpdir, "zcc2.s")
        s3 = os.path.join(tmpdir, "zcc3.s")
        data = b"pre_existing_data" * 150
        with open(s2, "wb") as f:
            f.write(data)
        shutil.copyfile(s2, s3)
        time.sleep(0.5)

        cmd = [
            sys.executable, quest_path,
            "--cmd", "printf '=== VERDICT: BOOTSTRAP DETERMINISM LOCK SECURED ===\\n'",
            "--mode", "runner",
            "--frames", "30",
            "--fps", "30",
        ]
        res = subprocess.run(cmd, cwd=tmpdir, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        out = res.stdout + res.stderr
        certified = "SELF-HOST VERIFIED" in out or "BYTE-IDENTICAL SEAL" in out
        if certified:
            print("[FAIL] Probe B falsely certified pre-existing files!")
            return False
        print("[PASS] Probe B correctly rejected pre-existing files.")
        return True
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def main():
    quest_path = sys.argv[1] if len(sys.argv) > 1 else os.path.abspath("zcc_quest.py")
    if not os.path.isfile(quest_path):
        print(f"Error: zcc_quest.py not found at {quest_path}", file=sys.stderr)
        sys.exit(2)

    print(f"Executing ZCC Oracle Negative Controls against: {quest_path}")
    pass_a = run_probe_a(quest_path)
    pass_b = run_probe_b(quest_path)

    if pass_a and pass_b:
        print("\n========================================================")
        print("★ ALL NEGATIVE CONTROLS PASSED (0 false certifications) ★")
        print("========================================================")
        sys.exit(0)
    else:
        print("\n========================================================")
        print("✖ NEGATIVE CONTROLS FAILED (counterfeit accepted) ✖")
        print("========================================================")
        sys.exit(1)


if __name__ == "__main__":
    main()
