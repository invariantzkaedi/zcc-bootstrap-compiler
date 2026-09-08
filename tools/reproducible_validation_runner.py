#!/usr/bin/env python3
"""
================================================================================
  ZCC INDEPENDENT REPRODUCIBLE VALIDATION RUNNER
================================================================================
A single-command, un-simulated verification engine that:
  1. Inspects host toolchains, git commit, and environment metadata
  2. Verifies ZCC binary or executes verified build (make zcc) with platform checks
  3. Verifies Gate 1: Self-Host Identity (make selfhost && cmp zcc2.s zcc3.s)
  4. Verifies Gate 4: Cloud Edge API Suite (dynamically parsed, no hardcoded strings)
  5. Executes Tri-Compiler Differential Gauntlet (ZCC vs GCC vs Clang)
  6. Emits dated evidence bundle strictly conforming to repository standards:
     - commands.txt
     - stdout.log / stderr.log
     - artifacts.md (paths + SHA-256 checksums)
     - verdict.md (4-state verdicts per Rule EF-6)
     - validation_report.json
================================================================================
"""

from __future__ import annotations

import os
import re
import sys
import json
import time
import shutil
import hashlib
import platform
import argparse
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Any, List, Tuple

# Reconfigure stdout for UTF-8
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

# Import differential gauntlet
from differential_compiler_gauntlet import run_gauntlet


class EvidenceLogger:
    """Captures exact commands, output logs, and generates required evidence bundle."""

    def __init__(self, evidence_dir: str):
        self.evidence_dir = evidence_dir
        os.makedirs(evidence_dir, exist_ok=True)
        self.commands: List[str] = []
        self.stdout_lines: List[str] = []
        self.stderr_lines: List[str] = []

    def record_command(self, cmd: List[str], cwd: str, rc: int, stdout: str, stderr: str):
        cmd_str = f"[{datetime.now(timezone.utc).isoformat()}] (cwd={cwd}, exit={rc}) $ " + " ".join(cmd)
        self.commands.append(cmd_str)
        if stdout:
            self.stdout_lines.append(f"--- COMMAND: {' '.join(cmd)} ---\n" + stdout)
        if stderr:
            self.stderr_lines.append(f"--- COMMAND: {' '.join(cmd)} ---\n" + stderr)

    def write_bundle(self, gates: Dict[str, Any], final_verdict: str, key_artifacts: List[str], source_artifacts: Optional[Dict[str, Dict[str, Any]]] = None):
        # 1. commands.txt
        with open(os.path.join(self.evidence_dir, "commands.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(self.commands) + "\n")

        # 2. stdout.log
        with open(os.path.join(self.evidence_dir, "stdout.log"), "w", encoding="utf-8") as f:
            f.write("\n".join(self.stdout_lines) + "\n")

        # 3. stderr.log
        with open(os.path.join(self.evidence_dir, "stderr.log"), "w", encoding="utf-8") as f:
            f.write("\n".join(self.stderr_lines) + "\n")

        # 4. artifacts.md
        with open(os.path.join(self.evidence_dir, "artifacts.md"), "w", encoding="utf-8") as f:
            f.write("# Cryptographically Sealed Evidence Bundle\n\n")
            
            if source_artifacts:
                f.write("## 1. Source Deliverables (Cryptographic Provenance)\n\n")
                f.write("| Source Deliverable | Size (Bytes) | SHA-256 Checksum |\n")
                f.write("|---|---|---|\n")
                for rel_p, p_info in source_artifacts.items():
                    f.write(f"| `{rel_p}` | {p_info['size_bytes']} | `{p_info['sha256']}` |\n")
                f.write("\n## 2. Generated Build & Test Artifacts\n\n")
            else:
                f.write("## Generated Build & Test Artifacts\n\n")
                
            f.write("| Artifact Path | Size (Bytes) | SHA-256 Checksum |\n")
            f.write("|---|---|---|\n")
            for art_path in key_artifacts:
                if os.path.isfile(art_path):
                    size = os.path.getsize(art_path)
                    with open(art_path, "rb") as bf:
                        sha = hashlib.sha256(bf.read()).hexdigest()
                    rel_p = os.path.basename(art_path)
                    f.write(f"| `{rel_p}` | {size} | `{sha}` |\n")
                else:
                    f.write(f"| `{os.path.basename(art_path)}` | MISSING | N/A |\n")

        # 5. verdict.md (Rule EF-6: Four-state verdicts)
        with open(os.path.join(self.evidence_dir, "verdict.md"), "w", encoding="utf-8") as f:
            f.write("# Independent Validation Verdicts\n\n")
            f.write(f"**Final Verdict**: `{final_verdict}`  \n")
            f.write(f"**Timestamp (UTC)**: `{datetime.now(timezone.utc).isoformat()}`  \n\n")
            f.write("| Gate | 4-State Verdict | Reason / Evidence |\n")
            f.write("|---|---|---|\n")
            for gate_name, gate_info in gates.items():
                status = gate_info.get("status", "INCONCLUSIVE")
                detail = gate_info.get("detail", "")
                f.write(f"| `{gate_name}` | **{status}** | {detail} |\n")


def resolve_git_env(cwd: str) -> Dict[str, str]:
    """Resolves worktree gitdir when crossing Windows and WSL boundaries."""
    env = os.environ.copy()
    git_file = os.path.join(cwd, ".git")
    if os.path.isfile(git_file):
        try:
            with open(git_file, "r", encoding="utf-8") as f:
                content = f.read().strip()
            if content.startswith("gitdir:"):
                raw_path = content.split("gitdir:", 1)[1].strip()
                if sys.platform.startswith("linux") and len(raw_path) >= 2 and raw_path[1] == ":":
                    drive = raw_path[0].lower()
                    wsl_path = f"/mnt/{drive}{raw_path[2:]}"
                    if os.path.exists(wsl_path):
                        env["GIT_DIR"] = wsl_path
                        env["GIT_WORK_TREE"] = cwd
        except Exception:
            pass
    return env


def run_logged_cmd(cmd: List[str], cwd: str, logger: EvidenceLogger, timeout: float = 300.0) -> Tuple[int, str, str]:
    """Runs a shell command, recording execution to evidence logger."""
    env = resolve_git_env(cwd) if cmd and cmd[0] == "git" else os.environ.copy()
    try:
        proc = subprocess.run(
            cmd, cwd=cwd, env=env, capture_output=True, text=True, timeout=timeout
        )
        rc, out, err = proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired:
        rc, out, err = -1, "", f"Command timed out after {timeout}s: {' '.join(cmd)}"
    except Exception as e:
        rc, out, err = -1, "", str(e)

    logger.record_command(cmd, cwd, rc, out, err)
    return rc, out, err


def collect_environment_metadata(repo_root: str, logger: EvidenceLogger) -> Dict[str, Any]:
    """Collects verifiable platform and toolchain versions."""
    meta = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "platform": platform.platform(),
        "system": platform.system(),
        "architecture": platform.machine(),
        "python_version": platform.python_version(),
        "tools": {}
    }

    # Git metadata
    rc, out, err = run_logged_cmd(["git", "rev-parse", "HEAD"], repo_root, logger)
    meta["git_commit_sha"] = out.strip() if rc == 0 else "UNKNOWN"

    rc, out, err = run_logged_cmd(["git", "branch", "--show-current"], repo_root, logger)
    meta["git_branch"] = out.strip() if rc == 0 else "UNKNOWN"

    rc, out, err = run_logged_cmd(["git", "status", "--porcelain"], repo_root, logger)
    meta["git_dirty"] = bool(out.strip())

    # Toolchain probes
    for tool in ["gcc", "clang", "node", "make", "as"]:
        p = shutil.which(tool)
        if p:
            rc, out, err = run_logged_cmd([tool, "--version"], repo_root, logger)
            meta["tools"][tool] = {
                "available": True,
                "path": p,
                "version": out.splitlines()[0] if out else "OK"
            }
        else:
            meta["tools"][tool] = {
                "available": False,
                "path": None,
                "version": "UNAVAILABLE"
            }

    return meta


def main():
    parser = argparse.ArgumentParser(description="ZCC Independent Reproducible Validation Runner")
    parser.add_argument("--differential-count", type=int, default=50, help="Number of programs for differential gauntlet")
    parser.add_argument("--workers", type=int, default=4, help="Worker processes for differential gauntlet")
    parser.add_argument("--skip-selfhost", action="store_true", help="Skip selfhost gate (for rapid dev only)")
    parser.add_argument("--output-dir", type=str, default="", help="Custom directory for evidence output")

    args = parser.parse_args()
    repo_root = str(Path(__file__).resolve().parent.parent)

    date_str = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    evidence_dir = args.output_dir or os.path.join(repo_root, "docs", "evidence", date_str, "independent_validation")
    logger = EvidenceLogger(evidence_dir)

    print("=" * 76)
    print("  ZCC INDEPENDENT REPRODUCIBLE VALIDATION RUNNER")
    print(f"  Target Repository: {repo_root}")
    print(f"  Evidence Target:   {evidence_dir}")
    print("=" * 76)

    # 1. Environment Attestation
    print("\n[*] Phase 1: Collecting Toolchain Provenance & Platform Integrity...")
    env_meta = collect_environment_metadata(repo_root, logger)
    print(f"    Commit:    {env_meta['git_commit_sha']}")
    print(f"    GCC:       {env_meta['tools']['gcc']['version']}")
    print(f"    Clang:     {env_meta['tools']['clang']['version']}")
    print(f"    Node:      {env_meta['tools']['node']['version']}")
    print(f"    Platform:  {env_meta['platform']}")

    report: Dict[str, Any] = {
        "metadata": env_meta,
        "gates": {},
        "verdict": "INCOMPLETE"
    }

    # Cross-Platform / Toolchain Check
    is_windows_native = sys.platform.startswith("win")
    has_make = env_meta["tools"]["make"]["available"]

    # 2. Build Verification (fail-closed, no silent unhandled failure)
    print("\n[*] Phase 2: Verifying Compiler Binary & Build Pipeline...")
    zcc_bin = os.path.join(repo_root, "zcc")
    
    if not has_make:
        if is_windows_native:
            err_msg = "GNU make is unavailable on Windows PATH. ZCC native self-host requires a POSIX environment. Please execute via WSL (wsl -e bash ./scripts/reproducible_validate.sh)."
        else:
            err_msg = "GNU make is not installed on this system."
        print(f"[-] INCONCLUSIVE: {err_msg}")
        report["gates"]["clean_build"] = {
            "status": "INCONCLUSIVE",
            "detail": err_msg
        }
    else:
        rc, out, err = run_logged_cmd(["make", "zcc"], repo_root, logger, timeout=60.0)
        if rc != 0:
            print(f"[-] FATAL: `make zcc` failed with exit code {rc}:\n{err}")
            report["gates"]["clean_build"] = {
                "status": "FAIL",
                "detail": f"make zcc failed with exit code {rc}: {err[:200]}"
            }
        else:
            print("    ✓ ZCC built successfully.")
            report["gates"]["clean_build"] = {
                "status": "PASS",
                "detail": "make zcc exited 0 and produced executable binary."
            }

    # 3. Gate 1 — Self-Host Identity
    if args.skip_selfhost:
        print("\n[!] Phase 3: Gate 1 (Self-Host Identity) SKIPPED per --skip-selfhost flag.")
        report["gates"]["gate1_selfhost"] = {
            "status": "NOT-APPLICABLE",
            "detail": "Explicitly skipped via --skip-selfhost flag."
        }
    elif not has_make:
        report["gates"]["gate1_selfhost"] = {
            "status": "INCONCLUSIVE",
            "detail": "make unavailable; selfhost gate cannot be executed."
        }
    else:
        print("\n[*] Phase 3: Gate 1 — Verifying Self-Host Identity (`make selfhost`)...")
        rc, out, err = run_logged_cmd(["make", "selfhost"], repo_root, logger, timeout=360.0)
        cmp_rc, cmp_out, cmp_err = run_logged_cmd(["cmp", "zcc2.s", "zcc3.s"], repo_root, logger, timeout=10.0)
        
        gate1_pass = (rc == 0 and cmp_rc == 0)
        print(f"    Self-host make exit: {rc}")
        print(f"    Assembly identity cmp exit: {cmp_rc} {'(BYTE-IDENTICAL)' if cmp_rc == 0 else '(DIFF DETECTED)'}")
        
        report["gates"]["gate1_selfhost"] = {
            "status": "PASS" if gate1_pass else "FAIL",
            "detail": f"cmp zcc2.s zcc3.s exit {cmp_rc} (byte-identical assembly)" if gate1_pass else f"selfhost exit {rc}, cmp exit {cmp_rc}"
        }

    # 4. Gate 4 — Cloud Edge API Test Suite (Dynamically Evaluated)
    print("\n[*] Phase 4: Gate 4 — Running Cloud Edge API Test Suite...")
    edge_test_file = os.path.join(repo_root, "apps", "zcc-cloud", "test_edge_endpoints.mjs")
    
    if not env_meta["tools"]["node"]["available"]:
        print("[-] INCONCLUSIVE: Node.js runtime not found; cannot execute edge test suite.")
        report["gates"]["gate4_edge_api"] = {
            "status": "INCONCLUSIVE",
            "detail": "Node.js unavailable on host."
        }
    else:
        rc, out, err = run_logged_cmd(["node", edge_test_file], repo_root, logger, timeout=60.0)
        
        # Dynamic verification of test results: NO HARDCODED STRINGS
        match = re.search(r"ALL RE-REVIEW VERIFICATIONS COMPLETE:\s*(\d+)\s*/\s*(\d+)\s*PASSING", out)
        has_fail = ("[FAIL]" in out) or (rc != 0)
        
        if match and not has_fail:
            passed_cnt = int(match.group(1))
            total_cnt = int(match.group(2))
            edge_pass = (passed_cnt == total_cnt and total_cnt > 0)
            detail_str = f"Dynamically verified {passed_cnt}/{total_cnt} assertions passing without failure."
        else:
            passed_cnt = 0
            total_cnt = 0
            edge_pass = False
            detail_str = f"Edge test suite failed (exit {rc}) or output contained failures."

        print(f"    Edge test suite exit: {rc} ({passed_cnt}/{total_cnt} assertions passing)")
        report["gates"]["gate4_edge_api"] = {
            "status": "PASS" if edge_pass else "FAIL",
            "detail": detail_str,
            "passed_assertions": passed_cnt,
            "total_assertions": total_cnt
        }

    # 5. Tri-Compiler Differential Gauntlet
    print(f"\n[*] Phase 5: Executing Tri-Compiler Differential Gauntlet ({args.differential_count} programs)...")
    gauntlet_bin = os.path.join(repo_root, "tools", "differential_compiler_gauntlet.py")
    json_out_file = os.path.join(evidence_dir, "differential_results.json")
    gauntlet_cmd = [
        sys.executable,
        gauntlet_bin,
        f"--count={args.differential_count}",
        f"--workers={args.workers}",
        f"--json={json_out_file}"
    ]
    rc, out, err = run_logged_cmd(gauntlet_cmd, repo_root, logger, timeout=900.0)
    print(out)
    if err:
        print(err, file=sys.stderr)

    if os.path.isfile(json_out_file):
        try:
            with open(json_out_file, "r", encoding="utf-8") as f:
                diff_results = json.load(f)
        except Exception as e:
            diff_results = {"status": "FAIL", "detail": f"Failed to parse gauntlet JSON: {e}"}
    else:
        diff_results = {"status": "FAIL", "detail": f"Gauntlet JSON output missing; process exit={rc}"}

    report["gates"]["differential_gauntlet"] = {
        "status": diff_results.get("status", "FAIL"),
        "detail": f"{diff_results.get('passed', 0)}/{diff_results.get('total', 0)} programs matched reference oracles ({diff_results.get('pass_rate', 0.0):.1f}%).",
        "results": diff_results
    }

    # 6. Final Verdict & Seal (Rule EF-6: Four-State Verdicts)
    # A closure verdict MUST NOT be GREEN if any gate is INCONCLUSIVE or FAIL
    all_gates_pass = all(g["status"] == "PASS" for g in report["gates"].values())
    any_inconclusive = any(g["status"] == "INCONCLUSIVE" for g in report["gates"].values())
    any_fail = any(g["status"] == "FAIL" for g in report["gates"].values())

    if any_fail:
        final_verdict = "FAIL"
    elif any_inconclusive:
        final_verdict = "INCONCLUSIVE"
    elif all_gates_pass:
        final_verdict = "PASS"
    else:
        final_verdict = "INCONCLUSIVE"

    report["verdict"] = final_verdict

    # 7. Cryptographically Bound Source Deliverables & Evidence Bundle
    DELIVERABLE_SOURCES = [
        "docs/SPEC_C_SUBSET_v1.md",
        "tools/differential_compiler_gauntlet.py",
        "tools/reproducible_validation_runner.py",
        "scripts/reproducible_validate.sh",
        "scripts/reproducible_validate.ps1",
        "apps/zcc-cloud/functions/api/verify.js",
        ".gitignore",
    ]
    source_artifacts: Dict[str, Dict[str, Any]] = {}
    for rel_path in DELIVERABLE_SOURCES:
        full_p = os.path.join(repo_root, rel_path)
        if os.path.isfile(full_p):
            size = os.path.getsize(full_p)
            with open(full_p, "rb") as bf:
                sha = hashlib.sha256(bf.read()).hexdigest()
            source_artifacts[rel_path] = {
                "size_bytes": size,
                "sha256": sha
            }
        else:
            source_artifacts[rel_path] = {
                "size_bytes": 0,
                "sha256": "MISSING"
            }
    report["source_deliverables"] = source_artifacts

    report_json_path = os.path.join(evidence_dir, "validation_report.json")
    with open(report_json_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    key_artifacts = [
        zcc_bin,
        os.path.join(repo_root, "zcc2.s"),
        os.path.join(repo_root, "zcc3.s"),
        json_out_file,
        report_json_path
    ]
    logger.write_bundle(report["gates"], final_verdict, key_artifacts, source_artifacts)

    print("\n" + "=" * 76)
    print(f"  FINAL VERDICT: {final_verdict}")
    print(f"  Evidence Bundle Written: {evidence_dir}")
    print("    - commands.txt")
    print("    - stdout.log")
    print("    - stderr.log")
    print("    - artifacts.md")
    print("    - verdict.md")
    print("    - validation_report.json")
    print("=" * 76)

    sys.exit(0 if final_verdict == "PASS" else 1)


def check_or_delegate_wsl() -> None:
    """If running on Windows and native POSIX tools (gcc/clang/make) are missing, delegate to WSL."""
    if sys.platform.startswith("win"):
        has_gcc = shutil.which("gcc") is not None
        has_clang = shutil.which("clang") is not None
        has_make = shutil.which("make") is not None
        if not (has_gcc and has_clang and has_make):
            wsl_bin = shutil.which("wsl")
            if wsl_bin:
                cwd = os.getcwd()
                drive = cwd[0].lower()
                wsl_cwd = f"/mnt/{drive}{cwd[2:].replace(chr(92), '/')}"
                script_path = os.path.abspath(__file__)
                sdrive = script_path[0].lower()
                wsl_script = f"/mnt/{sdrive}{script_path[2:].replace(chr(92), '/')}"
                
                forward_args = " ".join(f"'{a}'" for a in sys.argv[1:])
                wsl_cmd = ["wsl.exe", "-e", "bash", "-c", f"cd '{wsl_cwd}' && python3 '{wsl_script}' {forward_args}"]
                res = subprocess.run(wsl_cmd)
                sys.exit(res.returncode)


if __name__ == "__main__":
    check_or_delegate_wsl()
    main()
