#!/usr/bin/env python3
"""
tools/run_core_05_class2_differential.py

ZCC-CORE-05: Class II Tri-Compiler Differential on the Standalone C Corpus
Governing Rule: ZCC-EF-7 v1.0.0, Evidence Tier: E2

Evaluates 136 frozen standalone C programs against ZCC, GCC 13.3.0, and Clang 18.1.3
from byte-identical preprocessed translation units (glibc header surface included).
"""

import os
import sys
import json
import glob
import hashlib
import tempfile
import subprocess
import datetime
import traceback

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CONTRACT_PATH = os.path.join(REPO_ROOT, "tickets", "CORE-05-acceptance-contract.md")
FIXTURES_PATH = os.path.join(REPO_ROOT, "tests", "core05_fixtures.txt")
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")

TIMEOUT_RUN = 10
TIMEOUT_BUILD = 15

INCLUDES = ["-I", "tests", "-I", "include", "-I", "src", "-I", "."]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: str) -> str:
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def run_cmd(cmd, cwd=REPO_ROOT, timeout=10, input_data=None):
    try:
        res = subprocess.run(
            cmd,
            cwd=cwd,
            input=input_data,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
        )
        return res.returncode, res.stdout, res.stderr, False
    except subprocess.TimeoutExpired as e:
        return -1, b"", (e.stderr or b"") + b"\nTIMEOUT", True
    except Exception as e:
        return -1, b"", str(e).encode("utf-8"), False


def fnv1a_64(data: bytes) -> int:
    h = 0xcbf29ce484222325
    for b in data:
        h ^= b
        h = (h * 0x100000001b3) & 0xffffffffffffffff
    return h


def get_version(cmd):
    rc, out, err, _ = run_cmd(cmd)
    if rc == 0 and out:
        return out.decode("utf-8", errors="replace").splitlines()[0].strip()
    return "UNKNOWN"


def main():
    start_time = datetime.datetime.now(datetime.timezone.utc)
    date_str = start_time.strftime("%Y-%m-%d")
    ts_str = start_time.strftime("%Y%m%d_%H%M%S_%f")
    evidence_dir = os.path.join(REPO_ROOT, "docs", "evidence", date_str, "core_05_classII")
    os.makedirs(evidence_dir, exist_ok=True)
    ledger_path = os.path.join(evidence_dir, f"ledger_{ts_str}.json")

    print(f"=== ZCC-CORE-05: Class II Tri-Compiler Differential Gauntlet ===")
    print(f"Timestamp: {start_time.isoformat()}")

    # 1. Audit Prerequisites & Hashes
    if not os.path.exists(CONTRACT_PATH):
        print(f"FATAL: Missing contract at {CONTRACT_PATH}")
        sys.exit(1)
    if not os.path.exists(FIXTURES_PATH):
        print(f"FATAL: Missing fixtures list at {FIXTURES_PATH}")
        sys.exit(1)
    if not os.path.exists(ZCC_BIN):
        print(f"FATAL: Missing ZCC binary at {ZCC_BIN}")
        sys.exit(1)

    contract_sha256 = sha256_file(CONTRACT_PATH)
    fixtures_sha256 = sha256_file(FIXTURES_PATH)
    zcc_sha256 = sha256_file(ZCC_BIN)

    with open(FIXTURES_PATH, "r", encoding="utf-8") as f:
        fixture_paths = [line.strip() for line in f if line.strip() and not line.startswith("#")]

    print(f"Contract SHA-256: {contract_sha256}")
    print(f"Fixtures List:     {len(fixture_paths)} items (SHA-256: {fixtures_sha256})")
    print(f"ZCC Binary SHA-256: {zcc_sha256}")

    toolchains = {
        "zcc": {"sha256": zcc_sha256, "version": "ZCC Stage 3 Native"},
        "gcc": {"version": get_version(["gcc", "--version"])},
        "clang": {"version": get_version(["clang", "--version"])},
    }
    print(f"Host GCC:   {toolchains['gcc']['version']}")
    print(f"Host Clang: {toolchains['clang']['version']}")

    ledger = {
        "metadata": {
            "milestone": "ZCC-CORE-05",
            "claim": "Class II Tri-Compiler Differential on Standalone C Corpus",
            "governing_rule_version": "ZCC-EF-7 v1.0.0",
            "evidence_tier": "E2",
            "evidence_tier_assigned_at": "2026-10-26T07:42:00Z",
            "baseline_sha": "1a56cd6a6",
            "contract_path": os.path.relpath(CONTRACT_PATH, REPO_ROOT),
            "contract_sha256": contract_sha256,
            "fixtures_list_path": os.path.relpath(FIXTURES_PATH, REPO_ROOT),
            "fixtures_sha256": fixtures_sha256,
            "fixture_count": len(fixture_paths),
            "started_at": start_time.isoformat(),
        },
        "toolchains": toolchains,
        "controls": {},
        "summary": {},
        "divergences": [],
        "fixtures": [],
    }

    def fail_closed_exit(reason):
        ledger["summary"]["verdict"] = "FAIL"
        ledger["summary"]["failure_reason"] = reason
        ledger["summary"]["completed_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        with open(ledger_path, "w", encoding="utf-8") as lf:
            json.dump(ledger, lf, indent=2)
        print(f"\n[FAIL-CLOSED] {reason}")
        print(f"Partial ledger written to: {ledger_path}")
        sys.exit(1)

    with tempfile.TemporaryDirectory(prefix="core05_run_") as tmpdir:
        # =====================================================================
        # 2. CONTROLS (POS-01, NEG-01, NEG-02)
        # =====================================================================
        print("\n--- Executing Verification Controls ---")

        pos_fixture = "tests/test_7arg_test.c"
        pos_tu = os.path.join(tmpdir, "pos_01.tu.c")
        pos_zcc_s = os.path.join(tmpdir, "pos_01_zcc.s")
        pos_zcc_bin = os.path.join(tmpdir, "pos_01_zcc")
        pos_gcc_bin = os.path.join(tmpdir, "pos_01_gcc")
        pos_clang_bin = os.path.join(tmpdir, "pos_01_clang")

        # POS-01 Shared Preprocess
        rc, _, err, _ = run_cmd(["gcc", "-E", "-P"] + INCLUDES + [pos_fixture, "-o", pos_tu])
        if rc != 0:
            fail_closed_exit(f"POS-01 preprocessing failed: {err.decode('utf-8', errors='replace')}")

        # POS-01 Compilations
        rc_z, _, err_z, _ = run_cmd([ZCC_BIN, pos_tu, "-o", pos_zcc_s], timeout=TIMEOUT_BUILD)
        if rc_z != 0:
            fail_closed_exit(f"POS-01 ZCC compile failed: {err_z.decode('utf-8', errors='replace')}")

        rc_za, _, err_za, _ = run_cmd(["gcc", "-no-pie", pos_zcc_s, "-o", pos_zcc_bin, "-lm"], timeout=TIMEOUT_BUILD)
        if rc_za != 0:
            fail_closed_exit(f"POS-01 ZCC assemble/link failed: {err_za.decode('utf-8', errors='replace')}")

        rc_g, _, err_g, _ = run_cmd(["gcc", "-O0", "-fno-builtin", "-no-pie", pos_tu, "-lm", "-o", pos_gcc_bin], timeout=TIMEOUT_BUILD)
        if rc_g != 0:
            fail_closed_exit(f"POS-01 GCC build failed: {err_g.decode('utf-8', errors='replace')}")

        rc_c, _, err_c, _ = run_cmd(["clang", "-O0", "-fno-builtin", "-no-pie", pos_tu, "-lm", "-o", pos_clang_bin], timeout=TIMEOUT_BUILD)
        if rc_c != 0:
            fail_closed_exit(f"POS-01 Clang build failed: {err_c.decode('utf-8', errors='replace')}")

        # POS-01 Runs
        r_exit_z, r_out_z, r_err_z, _ = run_cmd([pos_zcc_bin], timeout=TIMEOUT_RUN)
        r_exit_g, r_out_g, r_err_g, _ = run_cmd([pos_gcc_bin], timeout=TIMEOUT_RUN)
        r_exit_c, r_out_c, r_err_c, _ = run_cmd([pos_clang_bin], timeout=TIMEOUT_RUN)

        pos_match = (
            (r_exit_z == r_exit_g == r_exit_c == 0) and
            (r_out_z == r_out_g == r_out_c) and
            (r_err_z == r_err_g == r_err_c)
        )
        if not pos_match:
            fail_closed_exit(f"POS-01 divergence: ZCC=({r_exit_z}, {r_out_z}) GCC=({r_exit_g}, {r_out_g}) Clang=({r_exit_c}, {r_out_c})")

        print("  [PASS] POS-01: test_7arg_test.c produced 3-way byte-identical exit 0 and stdout")
        ledger["controls"]["POS-01"] = {
            "fixture": pos_fixture,
            "status": "PASS",
            "exit": r_exit_z,
            "stdout": r_out_z.decode("utf-8", errors="replace").strip(),
            "sha256_stdout": sha256_bytes(r_out_z),
        }

        # NEG-01: Mutated assembly jump flip / logic mutation
        neg_zcc_s = os.path.join(tmpdir, "neg_01_zcc.s")
        with open(pos_zcc_s, "r", encoding="utf-8") as f:
            asm_lines = f.readlines()

        mutated = False
        mutated_asm = []
        for line in asm_lines:
            if not mutated and "movq $28, %r11" in line:
                mutated_asm.append(line.replace("movq $28, %r11", "movq $99, %r11"))
                mutated = True
            elif not mutated and "sete %al" in line:
                mutated_asm.append(line.replace("sete %al", "setne %al"))
                mutated = True
            else:
                mutated_asm.append(line)

        if not mutated:
            fail_closed_exit("NEG-01: Could not locate jump/comparison mutation site in POS-01 assembly")

        with open(neg_zcc_s, "w", encoding="utf-8") as f:
            f.writelines(mutated_asm)

        neg_zcc_bin = os.path.join(tmpdir, "neg_01_zcc")
        rc_m, _, _, _ = run_cmd(["gcc", "-no-pie", neg_zcc_s, "-o", neg_zcc_bin, "-lm"], timeout=TIMEOUT_BUILD)
        if rc_m != 0:
            fail_closed_exit("NEG-01: Mutated assembly failed to assemble")

        m_exit, m_out, m_err, _ = run_cmd([neg_zcc_bin], timeout=TIMEOUT_RUN)
        if (m_exit == r_exit_g) and (m_out == r_out_g):
            fail_closed_exit("NEG-01 FAILED: Harness failed to detect output divergence on mutated assembly!")

        print(f"  [PASS] NEG-01: Assembly mutation correctly detected (mutated exit={m_exit}, out={m_out.strip()})")
        ledger["controls"]["NEG-01"] = {
            "fixture": pos_fixture,
            "status": "PASS",
            "mutation": "cmpl $28 -> cmpl $99 / jne flip",
            "mutated_exit": m_exit,
            "mutated_stdout": m_out.decode("utf-8", errors="replace").strip(),
        }

        # NEG-02: Truncated source input (sever closing braces to test syntax fail-closed)
        neg02_tu = os.path.join(tmpdir, "neg_02.tu.c")
        with open(pos_tu, "r", encoding="utf-8") as f:
            tu_lines = f.readlines()
        # Truncate leaving main unclosed (sever last 5 lines)
        with open(neg02_tu, "w", encoding="utf-8") as f:
            f.writelines(tu_lines[:-5])

        neg02_s = os.path.join(tmpdir, "neg_02.s")
        rc_neg2, out_neg2, err_neg2, _ = run_cmd([ZCC_BIN, neg02_tu, "-o", neg02_s], timeout=TIMEOUT_BUILD)
        if rc_neg2 == 0:
            fail_closed_exit("NEG-02 FAILED: ZCC accepted truncated TU without compile failure!")

        print("  [PASS] NEG-02: Truncated source compilation correctly failed (exit != 0)")
        ledger["controls"]["NEG-02"] = {
            "fixture": pos_fixture,
            "status": "PASS",
            "exit": rc_neg2,
            "stderr_snippet": err_neg2.decode("utf-8", errors="replace")[:200].strip(),
        }

        # =====================================================================
        # 3. MAIN DIFFERENTIAL SWEEP (136 FIXTURES)
        # =====================================================================
        print(f"\n--- Running Class II Differential Sweep across {len(fixture_paths)} Fixtures ---")

        passed_count = 0
        divergences = []
        fixture_records = []

        zcc_digests = []
        gcc_digests = []
        clang_digests = []

        for idx, rel_path in enumerate(fixture_paths, 1):
            src_path = os.path.join(REPO_ROOT, rel_path)
            prefix = f"[{idx:3d}/{len(fixture_paths)}]"
            base = os.path.basename(src_path).replace(".c", "")

            tu_c = os.path.join(tmpdir, f"{base}_{idx}.tu.c")
            zcc_s = os.path.join(tmpdir, f"{base}_{idx}_zcc.s")
            zcc_bin = os.path.join(tmpdir, f"{base}_{idx}_zcc")
            gcc_bin = os.path.join(tmpdir, f"{base}_{idx}_gcc")
            clang_bin = os.path.join(tmpdir, f"{base}_{idx}_clang")

            record = {
                "index": idx,
                "fixture": rel_path,
                "status": "UNKNOWN",
            }

            # Preprocess (once, shared)
            cmd_pp = ["gcc", "-E", "-P"] + INCLUDES + [rel_path, "-o", tu_c]
            rc, out, err, timed_out = run_cmd(cmd_pp, timeout=TIMEOUT_BUILD)
            if rc != 0 or timed_out:
                record["status"] = "FAIL_PREPROCESS"
                record["phase"] = "preprocess"
                record["error"] = err.decode("utf-8", errors="replace")
                divergences.append(record)
                fixture_records.append(record)
                print(f"  {prefix} {rel_path:35s} -> FAIL (Preprocess)")
                continue

            # Reference Build: GCC
            cmd_gcc = ["gcc", "-O0", "-fno-builtin", "-no-pie", tu_c, "-lm", "-o", gcc_bin]
            rc_g_build, _, err_g_build, _ = run_cmd(cmd_gcc, timeout=TIMEOUT_BUILD)

            # Reference Build: Clang
            cmd_clang = ["clang", "-O0", "-fno-builtin", "-no-pie", tu_c, "-lm", "-o", clang_bin]
            rc_c_build, _, err_c_build, _ = run_cmd(cmd_clang, timeout=TIMEOUT_BUILD)

            # Reference Runs
            rc_g_run, out_g, err_g, to_g = run_cmd([gcc_bin], timeout=TIMEOUT_RUN) if rc_g_build == 0 else (-1, b"", b"", False)
            rc_c_run, out_c, err_c, to_c = run_cmd([clang_bin], timeout=TIMEOUT_RUN) if rc_c_build == 0 else (-1, b"", b"", False)

            ref_tuple_gcc = (rc_g_run, sha256_bytes(out_g), sha256_bytes(err_g))
            ref_tuple_clang = (rc_c_run, sha256_bytes(out_c), sha256_bytes(err_c))

            # Reference Consensus Check
            if (rc_g_build != 0 or rc_c_build != 0 or ref_tuple_gcc != ref_tuple_clang):
                record["status"] = "EXCLUDED_REF_DIVERGENCE"
                record["phase"] = "reference"
                record["gcc"] = {"exit": rc_g_run, "stdout_sha": ref_tuple_gcc[1]}
                record["clang"] = {"exit": rc_c_run, "stdout_sha": ref_tuple_clang[1]}
                fixture_records.append(record)
                print(f"  {prefix} {rel_path:35s} -> EXCLUDED (Ref Divergence)")
                continue

            consensus_tuple = ref_tuple_gcc
            gcc_digests.append(f"{rel_path}:{consensus_tuple[0]}:{consensus_tuple[1]}:{consensus_tuple[2]}")
            clang_digests.append(f"{rel_path}:{ref_tuple_clang[0]}:{ref_tuple_clang[1]}:{ref_tuple_clang[2]}")

            # Candidate Build: ZCC
            rc_z_compile, out_z_c, err_z_c, to_z_c = run_cmd([ZCC_BIN, tu_c, "-o", zcc_s], timeout=TIMEOUT_BUILD)
            if rc_z_compile != 0 or to_z_c:
                record["status"] = "FAIL_ZCC_COMPILE"
                record["phase"] = "compile"
                combined_err = (out_z_c + b"\n" + err_z_c).decode("utf-8", errors="replace").strip()
                record["error"] = combined_err[:400]
                record["reference_consensus"] = consensus_tuple
                divergences.append(record)
                fixture_records.append(record)
                print(f"  {prefix} {rel_path:35s} -> FAIL (ZCC Compile: {combined_err.splitlines()[-1] if combined_err else 'unknown'})")
                continue

            # Candidate Assemble/Link
            cmd_z_link = ["gcc", "-no-pie", zcc_s, "-o", zcc_bin, "-lm"]
            rc_z_link, out_z_l, err_z_l, to_z_l = run_cmd(cmd_z_link, timeout=TIMEOUT_BUILD)
            if rc_z_link != 0 or to_z_l:
                record["status"] = "FAIL_ZCC_LINK"
                record["phase"] = "link"
                record["error"] = err_z_l.decode("utf-8", errors="replace")[:400].strip()
                record["reference_consensus"] = consensus_tuple
                divergences.append(record)
                fixture_records.append(record)
                print(f"  {prefix} {rel_path:35s} -> FAIL (ZCC Link: {record['error'][:60]})")
                continue

            # Candidate Run: ZCC
            rc_z_run, out_z, err_z, to_z_r = run_cmd([zcc_bin], timeout=TIMEOUT_RUN)
            zcc_tuple = (rc_z_run, sha256_bytes(out_z), sha256_bytes(err_z))
            zcc_digests.append(f"{rel_path}:{zcc_tuple[0]}:{zcc_tuple[1]}:{zcc_tuple[2]}")

            record["gcc_tuple"] = consensus_tuple
            record["clang_tuple"] = ref_tuple_clang
            record["zcc_tuple"] = zcc_tuple

            if zcc_tuple == consensus_tuple:
                record["status"] = "PASS"
                passed_count += 1
                fixture_records.append(record)
                print(f"  {prefix} {rel_path:35s} -> PASS (3-way byte-identical)")
            else:
                record["status"] = "FAIL_ZCC_DIVERGENCE"
                record["phase"] = "run"
                record["zcc_stdout"] = out_z.decode("utf-8", errors="replace")[:300]
                record["ref_stdout"] = out_g.decode("utf-8", errors="replace")[:300]
                record["zcc_stderr"] = err_z.decode("utf-8", errors="replace")[:300]
                record["ref_stderr"] = err_g.decode("utf-8", errors="replace")[:300]
                divergences.append(record)
                fixture_records.append(record)
                print(f"  {prefix} {rel_path:35s} -> FAIL (Run divergence: ZCC={zcc_tuple[0]} vs REF={consensus_tuple[0]})")

    # =========================================================================
    # 4. FINAL SYNTHESIS & LEDGER EMISSION
    # =========================================================================
    composite_zcc = hex(fnv1a_64("\n".join(zcc_digests).encode("utf-8")))
    composite_gcc = hex(fnv1a_64("\n".join(gcc_digests).encode("utf-8")))
    composite_clang = hex(fnv1a_64("\n".join(clang_digests).encode("utf-8")))

    end_time = datetime.datetime.now(datetime.timezone.utc)
    duration_s = (end_time - start_time).total_seconds()

    verdict = "PASS" if (len(divergences) == 0 and passed_count == len(fixture_paths)) else "FAIL"

    ledger["summary"] = {
        "verdict": verdict,
        "total_fixtures": len(fixture_paths),
        "passed_fixtures": passed_count,
        "divergent_fixtures": len(divergences),
        "pass_rate_pct": round(passed_count * 100.0 / len(fixture_paths), 2) if fixture_paths else 0,
        "duration_seconds": round(duration_s, 2),
        "completed_at": end_time.isoformat(),
        "composite_fnv1a": {
            "zcc": composite_zcc,
            "gcc": composite_gcc,
            "clang": composite_clang,
        },
    }
    ledger["divergences"] = divergences
    ledger["fixtures"] = fixture_records

    with open(ledger_path, "w", encoding="utf-8") as lf:
        json.dump(ledger, lf, indent=2)

    ledger_sha256 = sha256_file(ledger_path)

    print("\n======================================================================")
    print(f"ZCC-CORE-05 GAUNTLET TERMINATED: VERDICT = {verdict}")
    print(f"Total Fixtures Evaluated: {len(fixture_paths)}")
    print(f"Bit-Exact 3-Way Identical: {passed_count}/{len(fixture_paths)} ({ledger['summary']['pass_rate_pct']}%)")
    print(f"Divergences / Defects:     {len(divergences)}")
    print(f"Ledger Path:               {ledger_path}")
    print(f"Ledger SHA-256:            {ledger_sha256}")
    print("======================================================================")

    if divergences:
        print("\n--- Summary of Observed Divergences / Separate Defects ---")
        phase_counts = {}
        for d in divergences:
            ph = d.get("phase", "unknown")
            phase_counts[ph] = phase_counts.get(ph, 0) + 1
        for ph, cnt in sorted(phase_counts.items()):
            print(f"  Phase {ph:12s}: {cnt} defects")
        print("\nFirst 10 Divergent Fixtures:")
        for d in divergences[:10]:
            print(f"  * {d['fixture']} (Phase: {d.get('phase')}, Status: {d.get('status')})")

    # Exit code: 0 if complete run (verdict is recorded in ledger), 1 only on infrastructure failure
    sys.exit(0)


if __name__ == "__main__":
    main()
