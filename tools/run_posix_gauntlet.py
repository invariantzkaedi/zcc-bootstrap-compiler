# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // POSIX SYSTEMS BRIDGE GAUNTLET 🔱
=======================================================================================================
 Automated test suite verifying native POSIX system call compilation, execution, and file I/O
 under the ZCC (Zkaedi C Compiler) pipeline.
=======================================================================================================
"""

import sys
import time
import subprocess
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

def run_posix_gauntlet() -> bool:
    print("\n" + "=" * 80)
    print(" 🔱 ZKAEDI PRIME // POSIX SYSTEMS BRIDGE GAUNTLET 🔱")
    print("=" * 80)

    # Suite 1: File I/O and Process Interrogation
    print("\n--- [SUITE 1: FILE I/O & PROCESS INTERROGATION] ---")
    cmd1 = [
        "wsl", "-e", "bash", "-c",
        "cd /mnt/h/__DOWNLOADS/zcc_github_upload && "
        "./zcc -I zcc_sys_includes tests/test_posix_bridge_native.c -o /tmp/posix_test.s && "
        "gcc -o /tmp/posix_test /tmp/posix_test.s && "
        "/tmp/posix_test"
    ]
    t0 = time.time()
    p1 = subprocess.run(cmd1, capture_output=True, text=True)
    dur1 = time.time() - t0
    print(p1.stdout)
    if p1.stderr:
        print("[STDERR]:", p1.stderr)
    ok1 = (p1.returncode == 0) and ("ALL POSIX BRIDGE TESTS PASSED CLEANLY" in p1.stdout)

    # Suite 2: Advanced Memory (mmap/munmap), IPC (pipe), and Multiprocessing (fork/waitpid)
    print("\n--- [SUITE 2: VIRTUAL MEMORY MMAP, IPC PIPE, FORK & WAITPID] ---")
    cmd2 = [
        "wsl", "-e", "bash", "-c",
        "cd /mnt/h/__DOWNLOADS/zcc_github_upload && "
        "./zcc -I zcc_sys_includes tests/test_posix_advanced_native.c -o /tmp/posix_adv.s && "
        "gcc -o /tmp/posix_adv /tmp/posix_adv.s && "
        "/tmp/posix_adv"
    ]
    t1 = time.time()
    p2 = subprocess.run(cmd2, capture_output=True, text=True)
    dur2 = time.time() - t1
    print(p2.stdout)
    if p2.stderr:
        print("[STDERR]:", p2.stderr)
    ok2 = (p2.returncode == 0) and ("ALL ADVANCED POSIX EXPANSION TESTS PASSED CLEANLY" in p2.stdout)

    passed = ok1 and ok2
    verdict = "PASS" if passed else "FAIL"

    print("-" * 80)
    print(f"VERDICT      : {verdict} (Suite 1: {'PASS' if ok1 else 'FAIL'}, Suite 2: {'PASS' if ok2 else 'FAIL'})")
    print(f"Total Time   : {(dur1 + dur2):.3f}s")
    print("=" * 80 + "\n")

    return passed

if __name__ == "__main__":
    ok = run_posix_gauntlet()
    sys.exit(0 if ok else 1)
