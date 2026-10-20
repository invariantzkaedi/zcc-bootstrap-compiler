#!/usr/bin/env python3
"""
ZCC_ORACLE_NEGATIVE_CONTROLS.py
Automated negative controls and adversarial regression harness for ZCC Quest oracle.
Verifies resolution of ZCC-VER-005, ZCC-VER-006, and ZCC-VER-007.

Fail-closed semantics:
- Exits 0 if and only if:
  1. ALL negative control probes execute to ordinary command success (exit 0).
  2. Ordinary success banner ("COMMAND SUCCEEDED") is explicitly rendered.
  3. Self-host certification banner is STRICTLY absent.
  4. Sidecar log explicitly records oracle_verified=False and NOT oracle_verified=True.
  5. Built-in mutation self-test verifies that broken or non-executing stubs are rejected.
- Exits 1 if ANY negative control falsely certifies or if ANY probe vacously passes.
"""

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")


def _run_wrapper_probe(quest_path: str, cmd_str: str, cwd: str, probe_name: str) -> bool:
    """Execute wrapper and perform strict, non-vacuous validation of ordinary success
    and absence of self-host certification."""
    log_path = os.path.join(cwd, f"{probe_name}.log")
    run_cmd = [
        sys.executable, quest_path,
        "--cmd", cmd_str,
        "--mode", "runner",
        "--fps", "30",
        "--log", log_path,
    ]
    try:
        res = subprocess.run(
            run_cmd,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=30.0,
        )
    except Exception as exc:
        print(f"[{probe_name}] Subprocess execution exception: {exc}")
        return False

    out = (res.stdout or "") + (res.stderr or "")

    # Invariant 1: Inner command succeeded, so wrapper MUST exit 0
    if res.returncode != 0:
        print(f"[{probe_name}] FAIL: Expected wrapper exit 0, got {res.returncode}")
        return False

    # Invariant 2: Wrapper MUST render ordinary command success banner
    has_ordinary = ("COMMAND SUCCEEDED" in out) or ("SOVEREIGN COMMAND SUCCEEDED" in out)
    if not has_ordinary:
        print(f"[{probe_name}] FAIL: Wrapper did not render ordinary command success banner")
        return False

    # Invariant 3: Wrapper MUST NOT certify self-host
    certified = ("★ SELF-HOST VERIFIED" in out) or ("★ SOVEREIGN VICTORY" in out) or ("BYTE-IDENTICAL VERIFIED" in out)
    if certified:
        print(f"[{probe_name}] FAIL: Wrapper falsely certified self-host!")
        return False

    # Invariant 4: Sidecar log MUST exist and record oracle_verified=False
    if not os.path.isfile(log_path):
        print(f"[{probe_name}] FAIL: Sidecar log {log_path} was not created")
        return False

    try:
        with open(log_path, "r", encoding="utf-8", errors="replace") as f:
            log_data = f.read()
        if "oracle_verified=False" not in log_data:
            print(f"[{probe_name}] FAIL: Sidecar log missing oracle_verified=False")
            return False
        if "oracle_verified=True" in log_data:
            print(f"[{probe_name}] FAIL: Sidecar log contains oracle_verified=True")
            return False
    except OSError as exc:
        print(f"[{probe_name}] FAIL: Error reading sidecar log: {exc}")
        return False

    return True


def run_probe_a(quest_path: str) -> bool:
    """Probe A: counterfeited zero-byte files written during command without compiler."""
    print("--- [CONTROL 1 / PROBE A] Counterfeit zero-byte files with marker ---")
    tmpdir = tempfile.mkdtemp(prefix="zcc-neg-a-")
    try:
        cmd_str = (
            "dd if=/dev/zero of=zcc2.s bs=2048 count=1 status=none && "
            "cp zcc2.s zcc3.s && "
            "printf '=== VERDICT: BOOTSTRAP DETERMINISM LOCK SECURED ===\\n'"
        )
        ok = _run_wrapper_probe(quest_path, cmd_str, tmpdir, "probe_a")
        if ok:
            print("[PASS] Probe A correctly executed with exit 0, rendered ordinary success, and rejected counterfeit files.")
        else:
            print("[FAIL] Probe A failed validation.")
        return ok
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def run_probe_b(quest_path: str) -> bool:
    """Probe B: pre-existing identical files with marker-only command."""
    print("--- [CONTROL 2 / PROBE B] Pre-existing identical files with marker ---")
    tmpdir = tempfile.mkdtemp(prefix="zcc-neg-b-")
    try:
        s2 = os.path.join(tmpdir, "zcc2.s")
        s3 = os.path.join(tmpdir, "zcc3.s")
        data = b"pre_existing_data" * 150
        with open(s2, "wb") as f:
            f.write(data)
        shutil.copyfile(s2, s3)
        time.sleep(0.1)

        cmd_str = "printf '=== VERDICT: BOOTSTRAP DETERMINISM LOCK SECURED ===\\n'"
        ok = _run_wrapper_probe(quest_path, cmd_str, tmpdir, "probe_b")
        if ok:
            print("[PASS] Probe B correctly executed with exit 0, rendered ordinary success, and rejected pre-existing files.")
        else:
            print("[FAIL] Probe B failed validation.")
        return ok
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def run_probe_c(quest_path: str) -> bool:
    """Probe C (ZCC-VER-006): Fabricated provenance and rogue directory with shell comment injection."""
    print("--- [CONTROL 3 / PROBE C] Fabricated provenance with shell comment injection ---")
    tmpdir = tempfile.mkdtemp(prefix="zcc-neg-c-")
    try:
        exploit_script = os.path.join(tmpdir, "fake_exploit.py")
        payload = """import os, sys, json, hashlib, shutil
ed = 'evidence/zcc-omega-fabricated'
shutil.rmtree(ed, ignore_errors=True)
os.makedirs(ed, exist_ok=True)
fake_s = '.text\\n.globl main\\nmain:\\npushq %rbp\\nmovq %rsp, %rbp\\n' + ('nop\\n' * 30000) + 'retq\\n'
with open(os.path.join(ed, 'stage2.s'), 'w') as f:
    f.write(fake_s)
with open(os.path.join(ed, 'stage3.s'), 'w') as f:
    f.write(fake_s)
sha = hashlib.sha256(fake_s.encode()).hexdigest()
with open(os.path.join(ed, 'provenance.jsonl'), 'w') as f:
    f.write(json.dumps({'stage': 2, 'exit_code': 0, 'assembly_sha256': sha}) + '\\n')
    f.write(json.dumps({'stage': 3, 'exit_code': 0, 'assembly_sha256': sha}) + '\\n')
print('=== VERDICT: BOOTSTRAP DETERMINISM LOCK SECURED ===')
print('SELF-HOST VERIFIED (assembly identical)')
"""
        with open(exploit_script, "w", encoding="utf-8") as f:
            f.write(payload)

        # Monitored command includes comment injection attempting to spoof gate.sh
        cmd_str = f"{sys.executable} {exploit_script} # gate.sh"
        ok = _run_wrapper_probe(quest_path, cmd_str, tmpdir, "probe_c")
        if ok:
            print("[PASS] Probe C correctly executed with exit 0, rendered ordinary success, and rejected rogue provenance / comment injection.")
        else:
            print("[FAIL] Probe C failed validation.")
        return ok
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def run_mutation_self_test() -> bool:
    """Self-test: mutation testing against deliberate broken stubs to prevent vacuous pass (ZCC-VER-007)."""
    print("--- [MUTATION SELF-TEST] Verifying harness rejection of non-executing / broken stubs ---")
    tmpdir = tempfile.mkdtemp(prefix="zcc-mutant-")
    try:
        # 1. Stub exiting 3 with no output
        stub_exit3 = os.path.join(tmpdir, "stub_exit3.py")
        with open(stub_exit3, "w", encoding="utf-8") as f:
            f.write("#!/usr/bin/env python3\nimport sys\nsys.exit(3)\n")

        passed_exit3 = run_probe_a(stub_exit3)
        if passed_exit3:
            print("[FAIL] Vacuous pass defect detected: harness accepted exit 3 mutant stub!")
            return False

        # 2. Stub exiting 0 but producing empty output (no success banner)
        stub_empty = os.path.join(tmpdir, "stub_empty.py")
        with open(stub_empty, "w", encoding="utf-8") as f:
            f.write("#!/usr/bin/env python3\nimport sys\nsys.exit(0)\n")

        passed_empty = run_probe_a(stub_empty)
        if passed_empty:
            print("[FAIL] Vacuous pass defect detected: harness accepted empty output mutant stub!")
            return False

        print("[PASS] Mutation self-test verified: harness rigorously rejects non-executing / broken stubs.")
        return True
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def main():
    raw_path = sys.argv[1] if len(sys.argv) > 1 else "zcc_quest.py"
    quest_path = os.path.abspath(raw_path)
    if not os.path.isfile(quest_path):
        print(f"Error: zcc_quest.py not found at {quest_path}", file=sys.stderr)
        sys.exit(2)

    print(f"Executing ZCC Oracle Negative Controls & Verification Gauntlet against: {quest_path}")

    # Step 1: Self-test mutation resilience to prevent vacuous pass (ZCC-VER-007)
    mutation_ok = run_mutation_self_test()
    if not mutation_ok:
        print("\n========================================================")
        print("✖ HARNESS INTEGRITY FAILURE: VACUOUS PASS DEFECT DETECTED ✖")
        print("========================================================")
        sys.exit(1)

    # Step 2: Execute negative control probes
    pass_a = run_probe_a(quest_path)
    pass_b = run_probe_b(quest_path)
    pass_c = run_probe_c(quest_path)

    if pass_a and pass_b and pass_c:
        print("\n========================================================")
        print("★ ALL NEGATIVE CONTROLS PASSED (0 false certifications) ★")
        print("========================================================")
        sys.exit(0)
    else:
        print("\n========================================================")
        print("✖ NEGATIVE CONTROLS FAILED (counterfeit accepted or probe failed) ✖")
        print("========================================================")
        sys.exit(1)


if __name__ == "__main__":
    main()
