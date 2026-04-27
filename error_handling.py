from __future__ import annotations

import enum
import json
import logging
import subprocess
import math

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
    Treats all subprocesses as hostile. Applies strict timeouts and incremental output capture limits.
    """
    try:
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        
        stdout_chunks = []
        stderr_chunks = []
        stdout_len = 0
        stderr_len = 0
        limit_exceeded = False
        limit_reason = ""
        
        def read_stream(stream, chunks_list, counter_ref, max_bytes, stream_name):
            nonlocal limit_exceeded, limit_reason
            while True:
                chunk = stream.read(4096)
                if not chunk:
                    break
                counter_ref[0] += len(chunk)
                if counter_ref[0] > max_bytes:
                    if not limit_exceeded:
                        limit_exceeded = True
                        limit_reason = f"{stream_name}_limit_exceeded"
                        process.kill()
                    break
                chunks_list.append(chunk)

        stdout_count = [0]
        stderr_count = [0]
        
        t_out = threading.Thread(target=read_stream, args=(process.stdout, stdout_chunks, stdout_count, max_stdout_bytes, "stdout"))
        t_err = threading.Thread(target=read_stream, args=(process.stderr, stderr_chunks, stderr_count, max_stderr_bytes, "stderr"))
        
        t_out.start()
        t_err.start()
        
        try:
            process.wait(timeout=timeout_sec)
        except subprocess.TimeoutExpired:
            process.kill()
            logger.error(f"PROCESS TIMEOUT EXCEEDED: {cmd}")
            return -1, "", "TIMEOUT_EXCEEDED"
            
        t_out.join()
        t_err.join()
        
        if limit_exceeded:
            logger.error(f"PROCESS OUTPUT LIMIT EXCEEDED: {limit_reason}")
            return -3, "", limit_reason
            
        stdout_str = b"".join(stdout_chunks).decode("utf-8", errors="replace")
        stderr_str = b"".join(stderr_chunks).decode("utf-8", errors="replace")
            
        return process.returncode, stdout_str, stderr_str

    except Exception as e:
        logger.error(f"SUBPROCESS FAULT: {str(e)}")
        return -2, "", str(e)

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
