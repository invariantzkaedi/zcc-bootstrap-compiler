from __future__ import annotations
from kill_switch import assert_not_globally_disabled
assert_not_globally_disabled()

import os
import tempfile
from pathlib import Path
import logging
import hashlib
import sys
import shutil
import json
import time

from error_handling import run_bounded_subprocess, validate_energy_output, ErrorTier
from fortify_gate import fortify_check_file
from production_guard import assert_sandbox_mode
from sandbox_attestation import get_compiler_provenance

logging.basicConfig(level=logging.INFO, format='[%(levelname)s] %(message)s')
logger = logging.getLogger("chimera_daemon")

COMPILER_CMD = ["wsl", "./zcc", "sandbox_gen.c"]
EXEC_CMD = ["wsl", "./a.out"]

def load_policy():
    with open("fortify_policy.json", "r", encoding="utf-8") as f:
        return json.load(f)

def get_policy_hash():
    with open("fortify_policy.json", "r", encoding="utf-8") as f:
        return hashlib.sha256(f.read().encode()).hexdigest()

def generate_bounded_c_template(generation: int, seed: str) -> str:
    # A bounded template replacing the unbounded mutagenesis.
    return f"""// CHIMERA MUTAGEN DAEMON - Bounded Generation {generation}
// SEED: {seed}
#include <stdio.h>

int main(void) {{
    double energy = 0.0;
    for(int i = 0; i < 100 * {generation}; i++) {{
        energy += (double)i / 255.0;
    }}
    
    // Simulate mapping the energy state
    double baseline = {generation * 0.1234};
    energy = baseline + (energy > 0 ? energy : -energy);
    
    printf("ENERGY: %f\\n", energy);
    return 0;
}}
"""

def orchestrate():
    # Production Execution Guard
    assert_sandbox_mode()

    logger.info("Initializing Bounded Singularity Sandbox v2...")
    
    policy = load_policy()
    max_generations = policy["max_generations"]
    seed = os.environ.get("CHIMERA_SEED", "default-seed")
    report_path = os.environ.get("CHIMERA_REPORT_PATH", "chimera_run_report.json")
    
    prov = get_compiler_provenance()
    
    report = {
        "status": "ok",
        "sandbox_mode": True,
        "seed": seed,
        "policy_hash": f"sha256:{get_policy_hash()}",
        "compiler_version": prov.get("compiler_version", "unknown"),
        "compiler_provenance": prov,
        "python_version": sys.version.split(" ")[0],
        "generations_requested": max_generations,
        "generations_completed": 0,
        "source_hashes": [],
        "errors": [],
        "artifacts_cleaned": False,
        "duration_ms": 0,
        "compiler": "wsl gcc",
        "runtime": "sandbox",
        "native_execution_attempted": False,
        "native_execution_blocked": False
    }
    
    start_time = time.time()
    
    scratch_c = Path("sandbox_gen.c")
    scratch_out = Path("a.out")
    scratch_asm = Path("a.out.s")
    
    try:
        for gen in range(1, max_generations + 1):
            logger.info(f"--- GENERATION {gen} ---")
            
            c_code = generate_bounded_c_template(gen, seed)
            scratch_c.write_text(c_code, encoding="utf-8")
            
            source_hash = hashlib.sha256(c_code.encode()).hexdigest()
            logger.info(f"Source Hash: {source_hash}")
            report["source_hashes"].append(f"sha256:{source_hash}")
            
            is_valid, reason = fortify_check_file(scratch_c)
            if not is_valid:
                logger.error(f"FORTIFY REJECTED Gen {gen}: {reason}. FAILING CLOSED.")
                report["status"] = "failed"
                report["native_execution_blocked"] = True
                report["errors"].append({"code": "fortify_rejected", "message": reason, "generation": gen})
                break
            logger.info("Fortify Gate: PASSED")
            
            code, stdout, stderr = run_bounded_subprocess(
                COMPILER_CMD, 
                timeout_sec=policy["max_compile_seconds"],
                max_stdout_bytes=policy["max_stdout_bytes"],
                max_stderr_bytes=policy["max_stderr_bytes"]
            )
            if code != 0:
                logger.error(f"COMPILER REJECTED Gen {gen}. Exit code: {code}. FAILING CLOSED.")
                logger.error(f"Stderr: {stderr}")
                report["status"] = "failed"
                report["native_execution_blocked"] = True
                report["errors"].append({"code": "compiler_error", "message": stderr.strip() if code != -3 else "compiler_output_limit_exceeded", "generation": gen})
                break
            logger.info("Compilation: PASSED")
            
            if not scratch_out.exists():
                logger.error(f"EXECUTION FAILED: Binary not emitted. FAILING CLOSED.")
                report["status"] = "failed"
                report["native_execution_blocked"] = True
                report["errors"].append({"code": "binary_missing", "message": "Binary not emitted", "generation": gen})
                break
                
            report["native_execution_attempted"] = True
            code, stdout, stderr = run_bounded_subprocess(
                EXEC_CMD, 
                timeout_sec=policy["max_runtime_seconds"],
                max_stdout_bytes=policy["max_stdout_bytes"],
                max_stderr_bytes=policy["max_stderr_bytes"]
            )
            if code != 0:
                logger.error(f"RUNTIME FAULT Gen {gen}. Exit code: {code}. FAILING CLOSED.")
                report["status"] = "failed"
                report["errors"].append({"code": "runtime_fault", "message": f"Exit code {code}" if code != -3 else "runtime_output_limit_exceeded", "generation": gen})
                break
                
            energy = validate_energy_output(stdout)
            if energy is None:
                logger.error(f"VALIDATION FAILED Gen {gen}. FAILING CLOSED.")
                report["status"] = "failed"
                report["errors"].append({"code": "energy_validation_failed", "message": "Missing or malformed energy output", "generation": gen})
                break
                
            logger.info(f"Execution: PASSED. ENERGY emitted: {energy}")
            report["generations_completed"] += 1
            
        if report["status"] == "ok" and report["generations_completed"] == max_generations:
            logger.info("Bounded Sandbox Execution Completed Cleanly.")

    except Exception as e:
        report["status"] = "failed"
        report["errors"].append({"code": "unhandled_exception", "message": str(e), "generation": report["generations_completed"] + 1})

    finally:
        artifacts_cleaned = True
        quarantine_path = None
        for f in [scratch_c, scratch_out, scratch_asm, Path("sandbox_gen_pre.c")]:
            if f.exists():
                try:
                    f.unlink()
                except Exception as e:
                    logger.error(f"Failed to clean artifact {f.name}: {e}")
                    artifacts_cleaned = False
                    
        if not artifacts_cleaned:
            import uuid
            import shutil
            from alerting import emit_alert
            quarantine_dir = Path("quarantine") / f"run-{uuid.uuid4().hex[:8]}"
            quarantine_dir.mkdir(parents=True, exist_ok=True)
            
            for f in [scratch_c, scratch_out, scratch_asm, Path("sandbox_gen_pre.c")]:
                if f.exists():
                    try:
                        shutil.move(str(f), str(quarantine_dir / f.name))
                    except: pass
                    
            quarantine_path = str(quarantine_dir)
            logger.error(f"FATAL: Cleanup failed. Scratch files moved to {quarantine_path}")
            emit_alert("critical", "cleanup_failure", "chimera_daemon", "scratch_quarantined")
            report["status"] = "failed"
            report["quarantine_path"] = quarantine_path
            report["release_blocked"] = True
        else:
            logger.info("Scratch environment cleaned.")
            
        report["artifacts_cleaned"] = artifacts_cleaned
        report["duration_ms"] = int((time.time() - start_time) * 1000)
        
        with open(report_path, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2)

if __name__ == "__main__":
    orchestrate()
