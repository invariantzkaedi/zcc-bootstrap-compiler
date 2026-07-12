#!/usr/bin/env python3
"""
selftest_omnicatch.py — Fault-injection and verification suite for OMNICATCH.
"""

import math
import os
import signal
import sys
import tempfile
import threading
import time
import warnings
from pathlib import Path

# Force the ledger path to a temporary file for self-test isolation
test_ledger_dir = tempfile.TemporaryDirectory()
os.environ["OMNICATCH_LEDGER"] = str(Path(test_ledger_dir.name) / "ledger.jsonl")

import omnicatch


def read_ledger_kinds() -> set[str]:
    """Read kinds of logged events in the temporary ledger."""
    p = omnicatch.ledger_path()
    if not p.exists():
        return set()
    kinds = set()
    with open(p, "r", encoding="utf-8") as f:
        for line in f:
            if line.strip():
                try:
                    ev = json.loads(line)
                    kinds.add(ev["kind"])
                except Exception:
                    pass
    return kinds


import json


def test_tier1_and_2():
    print("[*] Testing Tier 2: Global Hooks...")
    omnicatch.ascend(escalate_warnings=True, catch_signals=[signal.SIGINT])

    # 1. Thread exception
    print("    - Injecting thread exception...")
    t = threading.Thread(target=lambda: 1 / 0, name="fault_thread")
    t.start()
    t.join()

    # 2. Unraisable exception (__del__)
    print("    - Injecting unraisable exception...")
    class BadDel:
        def __del__(self):
            raise ValueError("Del error")
    
    obj = BadDel()
    del obj
    import gc
    gc.collect()

    # 3. Warnings escalation
    print("    - Injecting warning...")
    warnings.warn("Self-test warning", UserWarning)

    time.sleep(0.5)  # Let thread / GC hooks settle
    kinds = read_ledger_kinds()
    print(f"    - Current ledger kinds: {kinds}")
    assert "thread_exception" in kinds, "Missing thread_exception hook verify"
    assert "unraisable" in kinds, "Missing unraisable hook verify"
    assert "warning" in kinds, "Missing warning hook verify"
    print("[+] Tier 2 hooks verified.")


def test_tier3a_guarded():
    print("[*] Testing Tier 3a: Guarded Decorator...")

    # 1. Validation error
    @omnicatch.guarded(validate=lambda x: x > 0)
    def test_val(x):
        return x

    try:
        test_val(-5)
        raise RuntimeError("Expected ValidationError")
    except omnicatch.ValidationError:
        print("    - ValidationError caught.")

    # 2. Postcondition error
    @omnicatch.guarded(post=lambda r: r > 10)
    def test_post(x):
        return x

    try:
        test_post(5)
        raise RuntimeError("Expected PostconditionError")
    except omnicatch.PostconditionError:
        print("    - PostconditionError caught.")

    # 3. Finiteness check
    @omnicatch.guarded(check_finite=True)
    def test_finite(x):
        return [1.0, 2.0, x]

    try:
        test_finite(float("nan"))
        raise RuntimeError("Expected NonFiniteError")
    except omnicatch.NonFiniteError:
        print("    - NonFiniteError caught.")

    # 4. Retries
    retry_count = 0
    @omnicatch.guarded(retries=3, jitter=(0.01, 0.02))
    def test_retry():
        nonlocal retry_count
        retry_count += 1
        if retry_count < 3:
            raise ValueError("Transient error")
        return "success"

    res = test_retry()
    assert res == "success"
    assert retry_count == 3
    print("    - Retries verified.")

    # 5. Tolerance check
    omnicatch.tolerance_check(1.0000000000001, 1.0)
    try:
        omnicatch.tolerance_check(1.1, 1.0)
        raise RuntimeError("Expected PostconditionError on tolerance check")
    except omnicatch.PostconditionError:
        print("    - Tolerance check failure caught.")


def test_tier3b_silent_noop():
    print("[*] Testing Tier 3b: Silent No-Op Detection...")
    state = [1, 2, 3]

    def get_state():
        return tuple(state)

    try:
        with omnicatch.no_silent_noop("noop_test", get_state):
            # Code runs clean but mutates nothing
            pass
        raise RuntimeError("Expected SilentNoOpError")
    except omnicatch.SilentNoOpError:
        print("    - SilentNoOpError caught.")


def test_tier3c_swallow_audit():
    print("[*] Testing Tier 3c: Swallowed Exception Audit...")
    code = """
def bad_func():
    try:
        x = 1 / 0
    except:
        pass

def also_bad():
    try:
        y = 2
    except Exception:
        return None
"""
    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False) as f:
        f.write(code)
        f_name = f.name

    try:
        findings = omnicatch.audit_swallowed_exceptions(f_name, ledger=True)
        print(f"    - Findings: {findings}")
        assert len(findings) == 2, f"Expected 2 findings, got {len(findings)}"
    finally:
        os.unlink(f_name)
    print("[+] Swallowed exception audit verified.")


def test_tier3d_watchdog():
    print("[*] Testing Tier 3d: Watchdog...")
    
    # Run watchdog with short timeout in non-main thread
    def run_watchdog():
        try:
            with omnicatch.Watchdog("hang_test", deadline=0.1, lethal=False, poll=0.02):
                time.sleep(0.3)
        except omnicatch.HangError:
            pass

    t = threading.Thread(target=run_watchdog)
    t.start()
    t.join()

    kinds = read_ledger_kinds()
    assert "hang" in kinds, "Missing hang event in ledger"
    print("[+] Watchdog verified.")


def test_tier3e_run_verified():
    print("[*] Testing Tier 3e: Run Verified...")
    with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as log_f:
        log_path = log_f.name

    try:
        # 1. Success execution
        res = omnicatch.run_verified([sys.executable, "-c", "print('hello')"], timeout=5, log=log_path)
        assert res.ok
        assert res.exit_code == 0
        assert "hello" in Path(log_path).read_text()

        # 2. Timeout/Hang execution
        res_hang = omnicatch.run_verified([sys.executable, "-c", "import time; time.sleep(10)"], timeout=0.2, log=log_path)
        assert not res_hang.ok
        assert res_hang.verdict == "HANG"

        # 3. Demand evidence verification
        omnicatch.demand_evidence("valid_claim", res)
        try:
            omnicatch.demand_evidence("invalid_claim", None)
            raise RuntimeError("Expected FabricationError")
        except omnicatch.FabricationError:
            print("    - FabricationError caught.")
    finally:
        os.unlink(log_path)

    print("[+] Run Verified verified.")


def main():
    print("=== OMNICATCH SELF-TEST START ===")
    test_tier1_and_2()
    test_tier3a_guarded()
    test_tier3b_silent_noop()
    test_tier3c_swallow_audit()
    test_tier3d_watchdog()
    test_tier3e_run_verified()
    
    print("\n=== FINAL LEDGER KINDS ===")
    print(read_ledger_kinds())
    print("\n[SUCCESS] All OMNICATCH detectors fired and passed self-test!")


if __name__ == "__main__":
    main()
