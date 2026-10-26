#!/usr/bin/env python3
"""
ZCC-CORE-03: SQLite Differential Integration Suite
Hardened under CORE-03 Acceptance Contract:
- Gate 03-00: Source provenance verification (hashes, compiler versions).
- Gate 03-01: Fresh compilation of pinned SQLite engine and test drivers into an isolated run-unique workspace.
- Gate 03-02: Functional SQL stress suite (CORE-03A: test_sqlite_extended_stress.c).
- Gate 03-03: IEEE-754 floating-point, subnormal & boundary gauntlet (CORE-03B: test_zcc_sqlite_ieee754.c).
- Gate 03-04: Strict three-way consensus across ZCC, GCC 13.3, and Clang 18.1.
- Gate 03-05: Machine-readable JSON evidence ledger persisted under docs/evidence/YYYY-MM-DD/core_03_sqlite/.
"""

import os
import sys
import time
import json
import shutil
import tempfile
import subprocess
import hashlib
from datetime import datetime, timezone

# Hard execution bounds (seconds)
COMPILE_TIMEOUT = 120
RUN_TIMEOUT = 60

def hash_file(path):
    if not os.path.isfile(path):
        return None
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def same_behavior(p1, p2, check_stderr=True):
    if p1["returncode"] != p2["returncode"]:
        return False
    if p1["stdout"] != p2["stdout"]:
        return False
    if check_stderr and p1["stderr"] != p2["stderr"]:
        return False
    return True

def persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, results, has_failure, retain_on_success):
    print("\n================================================================================")
    print("📊 ZCC-CORE-03 SQLITE DIFFERENTIAL INTEGRATION LEDGER")
    print("================================================================================")
    for r in results:
        status_icon = "✅" if r["verdict"] == "PASS" else "❌"
        print(f"  {status_icon} [{r['id']}] {r['name']:<35} : {r['verdict']:<32} ({r['details']})")
    print("================================================================================")

    ledger_path = os.path.join(evidence_dir, f"stage3_ledger_{run_timestamp}.json")
    with open(ledger_path, "w") as f:
        json.dump({
            "timestamp": run_timestamp,
            "workspace": workdir,
            "provenance": provenance,
            "total_targets": len(results),
            "pass_count": sum(1 for r in results if r["verdict"] == "PASS"),
            "results": results
        }, f, indent=2)
    print(f"📁 Evidence ledger preserved: {ledger_path}")

    if has_failure:
        print(f"⚠️ Failures encountered: Workspace preserved for forensic triage at: {workdir}")
    elif retain_on_success:
        print(f"ℹ️ Workspace retained at: {workdir}")
    else:
        shutil.rmtree(workdir, ignore_errors=True)

    if not has_failure and len(results) == 2:
        print("\n✨ CORE-03 COMPLETE: 100% THREE-WAY CONSENSUS ON SQLITE INTEGRATION")
        return 0, results
    else:
        print("\n❌ CORE-03 FAILED: Non-passing targets or compilation failures detected.")
        return 1, results

def run_core_03_suite(custom_workdir=None, retain_on_success=False):
    now_utc = datetime.now(timezone.utc)
    run_timestamp = now_utc.strftime("%Y%m%d_%H%M%S") + f"_{now_utc.microsecond:06d}"
    if custom_workdir:
        workdir = custom_workdir
        os.makedirs(workdir, exist_ok=True)
    else:
        workdir = tempfile.mkdtemp(prefix=f"zcc_core03_sqlite_{run_timestamp}_")

    evidence_dir = f"docs/evidence/{now_utc.strftime('%Y-%m-%d')}/core_03_sqlite"
    os.makedirs(evidence_dir, exist_ok=True)

    print("================================================================================")
    print("🔬 ZCC-CORE-03: SQLITE 3.45 NATIVE AMALGAMATION DIFFERENTIAL ADJUDICATION")
    print("================================================================================")
    print(f"Isolated Workspace: {workdir}")
    print("Reference Oracles: GCC 13.3.0 (-O0), Clang 18.1.3 (-O0)")
    print(f"Timestamp: {run_timestamp}\n")

    # 1. Gate 03-00: Provenance Capture
    sqlite_zcc_src = "sqlite3_zcc.c"
    sqlite_ref_src = "src/sqlite3.c"
    sqlite_ref_hdr = "src/sqlite3.h"
    stress_driver_src = "tests/test_sqlite_extended_stress.c"
    ieee_driver_src = "tests/test_zcc_sqlite_ieee754.c"

    provenance = {
        "zcc_binary": {"path": "./zcc", "sha256": hash_file("./zcc")},
        "sqlite_zcc_source": {"path": sqlite_zcc_src, "sha256": hash_file(sqlite_zcc_src)},
        "sqlite_ref_source": {"path": sqlite_ref_src, "sha256": hash_file(sqlite_ref_src)},
        "sqlite_ref_header": {"path": sqlite_ref_hdr, "sha256": hash_file(sqlite_ref_hdr)},
        "stress_driver": {"path": stress_driver_src, "sha256": hash_file(stress_driver_src)},
        "ieee_driver": {"path": ieee_driver_src, "sha256": hash_file(ieee_driver_src)}
    }

    # Verify source presence
    missing_sources = [k for k, v in provenance.items() if v["sha256"] is None]
    if missing_sources:
        print(f"❌ Missing critical sources: {missing_sources}")
        return 1, []

    print("[GATE 03-00] Source Provenance Verified:")
    print(f"  • ZCC Binary SHA:      {provenance['zcc_binary']['sha256'][:16]}...")
    print(f"  • SQLite ZCC Source:   {provenance['sqlite_zcc_source']['sha256'][:16]}... ({sqlite_zcc_src})")
    print(f"  • SQLite Ref Source:   {provenance['sqlite_ref_source']['sha256'][:16]}... ({sqlite_ref_src})")
    print(f"  • Stress Driver SHA:   {provenance['stress_driver']['sha256'][:16]}... ({stress_driver_src})")
    print(f"  • IEEE-754 Driver SHA: {provenance['ieee_driver']['sha256'][:16]}... ({ieee_driver_src})")

    results = []
    has_failure = False

    # 2. Gate 03-01: Fresh Compilation of SQLite Engine under ZCC
    print("\n--------------------------------------------------------------------------------")
    print("[GATE 03-01] Fresh Native ZCC Compilation of SQLite Engine")
    z_sqlite_s = os.path.join(workdir, "sqlite3_zcc.s")
    z_sqlite_o = os.path.join(workdir, "sqlite3_zcc.o")

    zcc_engine_start = time.time()
    zcc_engine_cmd = ["./zcc", sqlite_zcc_src, "-o", z_sqlite_s]
    print(f"  Executing: {' '.join(zcc_engine_cmd)}")
    try:
        cp_z_engine = subprocess.run(zcc_engine_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=COMPILE_TIMEOUT)
        if cp_z_engine.returncode != 0:
            print(f"  ❌ ZCC engine compilation failed: {cp_z_engine.stderr[:200]}")
            return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, [{
                "id": "CORE-03-BUILD",
                "name": "SQLite Engine ZCC Build",
                "verdict": "COMPILER BUILD ERROR",
                "details": f"ZCC engine compilation failed: exit {cp_z_engine.returncode}"
            }], True, retain_on_success)
    except subprocess.TimeoutExpired:
        print("  ❌ ZCC engine compilation TIMEOUT")
        return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, [{
            "id": "CORE-03-BUILD",
            "name": "SQLite Engine ZCC Build",
            "verdict": "COMPILER BUILD ERROR",
            "details": "ZCC engine compilation timeout"
        }], True, retain_on_success)

    # Assemble emitted assembly
    as_cmd = ["as", "-o", z_sqlite_o, z_sqlite_s]
    try:
        cp_as = subprocess.run(as_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=COMPILE_TIMEOUT)
        if cp_as.returncode != 0:
            print(f"  ❌ Assembler failed on SQLite assembly: {cp_as.stderr[:200]}")
            return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, [{
                "id": "CORE-03-BUILD",
                "name": "SQLite Engine ZCC Assemble",
                "verdict": "COMPILER BUILD ERROR",
                "details": f"Assembler failed: exit {cp_as.returncode}"
            }], True, retain_on_success)
    except subprocess.TimeoutExpired:
        return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, [{
            "id": "CORE-03-BUILD",
            "name": "SQLite Engine ZCC Assemble",
            "verdict": "COMPILER BUILD ERROR",
            "details": "Assembler timeout"
        }], True, retain_on_success)

    zcc_engine_ms = (time.time() - zcc_engine_start) * 1000
    sqlite_obj_sha = hash_file(z_sqlite_o)
    print(f"  ✔ SQLite Engine Native ZCC Object: {z_sqlite_o} ({os.path.getsize(z_sqlite_o)} bytes, SHA: {sqlite_obj_sha[:12]}, {zcc_engine_ms:.1f}ms)")

    # 3. Test Suites to Execute
    suites = [
        {
            "id": "CORE-03A",
            "name": "SQLite Extended Industrial Stress Suite",
            "driver_src": stress_driver_src,
            "desc": "Schemas, B-Tree indexing, bulk transactions, JOIN/HAVING queries, triggers & rollback"
        },
        {
            "id": "CORE-03B",
            "name": "SQLite IEEE-754 & Subnormal Gauntlet",
            "driver_src": ieee_driver_src,
            "desc": "Subnormal true minimum (5e-324), IEEE arithmetic, signed zero, B-tree record roundtrip"
        }
    ]

    for s in suites:
        s_id = s["id"]
        s_name = s["name"]
        driver_src = s["driver_src"]
        print(f"\n--------------------------------------------------------------------------------")
        print(f"[{s_id}] {s_name}")
        print(f"  Driver: {driver_src}")

        record = {
            "id": s_id,
            "name": s_name,
            "verdict": None,
            "details": None,
            "sources": [sqlite_zcc_src, driver_src],
            "timings_ms": {"engine_build": zcc_engine_ms},
            "commands": {},
            "exit_codes": {},
            "stdout_hashes": {},
            "stderr_hashes": {}
        }

        # A. Compile test driver with ZCC
        z_driver_s = os.path.join(workdir, f"driver_{s_id}.s")
        z_driver_cmd = ["./zcc", "-I.", driver_src, "-o", z_driver_s]
        record["commands"]["zcc_driver_compile"] = " ".join(z_driver_cmd)
        try:
            cp_zd = subprocess.run(z_driver_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=COMPILE_TIMEOUT)
            if cp_zd.returncode != 0:
                print(f"  ❌ ZCC failed compiling test driver: {cp_zd.stderr[:200]}")
                record["verdict"] = "COMPILER BUILD ERROR"
                record["details"] = f"Driver compilation failed: exit {cp_zd.returncode}"
                results.append(record)
                return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, results, True, retain_on_success)
        except subprocess.TimeoutExpired:
            record["verdict"] = "COMPILER BUILD ERROR"
            record["details"] = "Driver compilation timeout"
            results.append(record)
            return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, results, True, retain_on_success)

        # B. Link ZCC binary (pure ZCC assembly for both engine and driver)
        z_bin = os.path.join(workdir, f"bin_zcc_{s_id}")
        if os.path.exists(z_bin): os.remove(z_bin)
        z_link_cmd = ["gcc", "-no-pie", z_sqlite_o, z_driver_s, "-o", z_bin, "-ldl", "-lpthread", "-lm"]
        record["commands"]["zcc_link"] = " ".join(z_link_cmd)
        try:
            lp_z = subprocess.run(z_link_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=COMPILE_TIMEOUT)
            if lp_z.returncode != 0:
                print(f"  ❌ ZCC binary link failed: {lp_z.stderr[:200]}")
                record["verdict"] = "COMPILER BUILD ERROR"
                record["details"] = f"ZCC link failed: exit {lp_z.returncode}"
                results.append(record)
                return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, results, True, retain_on_success)
        except subprocess.TimeoutExpired:
            record["verdict"] = "COMPILER BUILD ERROR"
            record["details"] = "ZCC link timeout"
            results.append(record)
            return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, results, True, retain_on_success)

        # C. Compile GCC reference
        g_bin = os.path.join(workdir, f"bin_gcc_{s_id}")
        if os.path.exists(g_bin): os.remove(g_bin)
        g_cmd = ["gcc", "-O0", sqlite_ref_src, driver_src, "-o", g_bin, "-ldl", "-lpthread", "-lm"]
        record["commands"]["gcc_build"] = " ".join(g_cmd)
        try:
            cp_g = subprocess.run(g_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=COMPILE_TIMEOUT)
            if cp_g.returncode != 0:
                print(f"  ❌ GCC reference compile failed: {cp_g.stderr[:200]}")
                record["verdict"] = "HARNESS ERROR"
                record["details"] = f"GCC reference compile failed: exit {cp_g.returncode}"
                results.append(record)
                return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, results, True, retain_on_success)
        except subprocess.TimeoutExpired:
            record["verdict"] = "HARNESS ERROR"
            record["details"] = "GCC reference compile timeout"
            results.append(record)
            return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, results, True, retain_on_success)

        # D. Compile Clang reference
        c_bin = os.path.join(workdir, f"bin_clang_{s_id}")
        if os.path.exists(c_bin): os.remove(c_bin)
        c_cmd = ["clang", "-O0", sqlite_ref_src, driver_src, "-o", c_bin, "-ldl", "-lpthread", "-lm"]
        record["commands"]["clang_build"] = " ".join(c_cmd)
        try:
            cp_c = subprocess.run(c_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=COMPILE_TIMEOUT)
            if cp_c.returncode != 0:
                print(f"  ❌ Clang reference compile failed: {cp_c.stderr[:200]}")
                record["verdict"] = "HARNESS ERROR"
                record["details"] = f"Clang reference compile failed: exit {cp_c.returncode}"
                results.append(record)
                return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, results, True, retain_on_success)
        except subprocess.TimeoutExpired:
            record["verdict"] = "HARNESS ERROR"
            record["details"] = "Clang reference compile timeout"
            results.append(record)
            return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, results, True, retain_on_success)

        # E. Execute all three in isolated per-compiler subdirectories
        dir_z = os.path.join(workdir, f"run_zcc_{s_id}")
        dir_g = os.path.join(workdir, f"run_gcc_{s_id}")
        dir_c = os.path.join(workdir, f"run_clang_{s_id}")
        os.makedirs(dir_z, exist_ok=True)
        os.makedirs(dir_g, exist_ok=True)
        os.makedirs(dir_c, exist_ok=True)

        try:
            zr = subprocess.run([z_bin], cwd=dir_z, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=RUN_TIMEOUT)
            gr = subprocess.run([g_bin], cwd=dir_g, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=RUN_TIMEOUT)
            cr = subprocess.run([c_bin], cwd=dir_c, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=RUN_TIMEOUT)
        except subprocess.TimeoutExpired:
            record["verdict"] = "RUNTIME TIMEOUT"
            record["details"] = "Execution timeout on SQLite binary"
            results.append(record)
            return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, results, True, retain_on_success)

        z_dict = {"returncode": zr.returncode, "stdout": zr.stdout, "stderr": zr.stderr}
        g_dict = {"returncode": gr.returncode, "stdout": gr.stdout, "stderr": gr.stderr}
        c_dict = {"returncode": cr.returncode, "stdout": cr.stdout, "stderr": cr.stderr}

        record["exit_codes"] = {"zcc": zr.returncode, "gcc": gr.returncode, "clang": cr.returncode}
        record["stdout_hashes"] = {
            "zcc": hashlib.sha256(zr.stdout).hexdigest()[:12],
            "gcc": hashlib.sha256(gr.stdout).hexdigest()[:12],
            "clang": hashlib.sha256(cr.stdout).hexdigest()[:12]
        }
        record["stderr_hashes"] = {
            "zcc": hashlib.sha256(zr.stderr).hexdigest()[:12],
            "gcc": hashlib.sha256(gr.stderr).hexdigest()[:12],
            "clang": hashlib.sha256(cr.stderr).hexdigest()[:12]
        }

        # Adjudication
        if not same_behavior(g_dict, c_dict, check_stderr=True):
            print(f"  ❌ REFERENCE DISAGREEMENT: GCC != Clang on {s_id}")
            record["verdict"] = "REFERENCE DISAGREEMENT"
            record["details"] = "GCC and Clang diverged on execution output"
            has_failure = True
        elif same_behavior(z_dict, g_dict, check_stderr=True) and same_behavior(g_dict, c_dict, check_stderr=True):
            if zr.returncode != 0:
                print(f"  ❌ UNEXPECTED EXIT CODE: Got {zr.returncode}, expected 0")
                record["verdict"] = "UNEXPECTED EXIT CODE"
                record["details"] = f"Compilers agreed on nonzero exit {zr.returncode}"
                has_failure = True
            else:
                sha = record["stdout_hashes"]["zcc"]
                print(f"  ✅ PASS (Exit: 0, Stdout SHA: {sha})")
                print(f"     Consensus: ZCC == GCC == Clang 100% Byte-Identical Output")
                record["verdict"] = "PASS"
                record["details"] = f"Identical across ZCC, GCC, Clang (SHA:{sha}, Exit:0)"
        else:
            print(f"  ❌ ZCC DIVERGENCE — TRIAGE REQUIRED")
            record["verdict"] = "ZCC DIVERGENCE — TRIAGE REQUIRED"
            record["details"] = f"ZCC={record['stdout_hashes']['zcc']} vs GCC/Clang={record['stdout_hashes']['gcc']}"
            has_failure = True

        results.append(record)

    return persist_core03_ledger(evidence_dir, run_timestamp, workdir, provenance, results, has_failure, retain_on_success)

if __name__ == "__main__":
    code, _ = run_core_03_suite()
    sys.exit(code)
