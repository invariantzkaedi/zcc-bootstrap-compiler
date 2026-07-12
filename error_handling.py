from __future__ import annotations

import enum
import logging
import math
import os
import subprocess
import tempfile
from pathlib import Path
import omnicatch

logging.basicConfig(level=logging.INFO, format='[%(levelname)s] %(message)s')
logger = logging.getLogger("bonus_error_handler")

class ErrorTier(enum.Enum):
    WARNING = 1
    COMPILER_REJECT = 2
    RUNTIME_FAULT = 3
    CRITICAL_BREACH = 4

import threading


def run_bounded_subprocess(cmd: list[str], timeout_sec: int = 5, max_stdout_bytes: int = 65536, max_stderr_bytes: int = 65536) -> tuple[int, str, str]:
    """
    Delegates to omnicatch.run_verified to execute the subprocess under evidence-discipline.
    """
    with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as log_f:
        log_path = log_f.name

    try:
        res = omnicatch.run_verified(cmd, timeout=timeout_sec, log=log_path)
        
        # Read the captured stdout+stderr log
        output = Path(log_path).read_text(encoding="utf-8", errors="replace")
        
        # Enforce size capping post-execution (containment check)
        size = Path(log_path).stat().st_size
        if size > (max_stdout_bytes + max_stderr_bytes):
            logger.error(f"PROCESS OUTPUT LIMIT EXCEEDED: size={size}")
            return -3, "", "output_limit_exceeded"

        if res.timed_out:
            logger.error(f"PROCESS TIMEOUT EXCEEDED: {cmd}")
            return -1, "", "TIMEOUT_EXCEEDED"
            
        return res.exit_code, output, ""

    except Exception as e:
        logger.error(f"SUBPROCESS FAULT: {e!s}")
        return -2, "", str(e)
    finally:
        try:
            os.unlink(log_path)
        except OSError:
            pass


@omnicatch.finite(check_args=False)
def validate_energy_output(stdout: str) -> float | None:
    """
    Treats mathematical results as hostile. Ensures the returned energy is finite and bounded.
    """
    try:
        lines = stdout.split("\n")
        energy_val = None
        for line in lines:
            if "ENERGY=" in line:
                val_str = line.split("ENERGY=")[1].strip()
                if val_str.lower() == "nan":
                    logger.error("NON-FINITE ENERGY DETECTED (NaN string). FAILING CLOSED.")
                    return None
                energy_val = float(val_str)
                break
            elif "ENERGY:" in line:
                val_str = line.split("ENERGY:")[1].strip()
                if val_str.lower() == "nan":
                    logger.error("NON-FINITE ENERGY DETECTED (NaN string). FAILING CLOSED.")
                    return None
                energy_val = float(val_str)
                break
                
        if energy_val is None:
            logger.error("NO ENERGY METRIC RETURNED. FAILING CLOSED.")
            return None
            
        if math.isnan(energy_val) or math.isinf(energy_val):
            logger.error("NON-FINITE ENERGY DETECTED. FAILING CLOSED.")
            return None
            
        return energy_val
    except ValueError:
        logger.error("MALFORMED ENERGY OUTPUT. FAILING CLOSED.")
        return None
