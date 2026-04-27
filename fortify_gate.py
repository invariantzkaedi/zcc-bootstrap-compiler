from __future__ import annotations

import re
import json
from pathlib import Path
import logging
import os
import tempfile
from error_handling import run_bounded_subprocess

logger = logging.getLogger("fortify_gate")

def load_policy():
    with open("fortify_policy.json", "r", encoding="utf-8") as f:
        return json.load(f)

def validate_source(source_code: str, file_path: Path) -> tuple[bool, str]:
    """
    Statically analyzes the generated C source code in multiple stages.
    """
    policy = load_policy()
    
    # 1. Raw source size check
    if len(source_code.encode('utf-8')) > policy["max_source_bytes"]:
        return False, "SOURCE_TOO_LARGE"

    # 2. Raw source include allowlist check
    include_pattern = re.compile(r"#include\s+([<\"].*?[>\"])")
    for match in include_pattern.finditer(source_code):
        inc = match.group(1)
        if inc not in policy["allowed_includes"]:
            if inc.startswith('"') and inc.endswith('"'):
                return False, f"FORBIDDEN_QUOTED_INCLUDE: {inc}"
            return False, f"FORBIDDEN_INCLUDE: {inc}"

    # 3. Raw source forbidden-token scan
    # Build token boundary regex: \b(system|fork|execve|...)\b
    tokens = "|".join(policy["forbidden_tokens"])
    token_regex = re.compile(rf"\b({tokens})\b")
    
    match = token_regex.search(source_code)
    if match:
        return False, f"RAW_DENYLIST_MATCH: {match.group(1)}"
        
    # Block obvious infinite loops
    if re.search(r"while\s*\(\s*1\s*\)", source_code) or re.search(r"for\s*\(\s*;\s*;\s*\)", source_code):
        return False, "INFINITE_LOOP_DETECTED"

    # 4. Preprocess with timeout (using WSL gcc)
    # To prevent standard library inline assembly (like in stdio.h) from triggering the 
    # denylist during the preprocessed scan, we strip the verified includes before expanding.
    stripped_source = re.sub(r"^\s*#include\s+.*$", "// include stripped", source_code, flags=re.MULTILINE)
    
    scratch_pre_file = Path("sandbox_gen_pre.c")
    scratch_pre_file.write_text(stripped_source, encoding="utf-8")
        
    # Run gcc -E -P safely. We capture stdout which is the preprocessed code.
    code, stdout, stderr = run_bounded_subprocess(
        ["wsl", "gcc", "-E", "-P", "sandbox_gen_pre.c"],
        timeout_sec=policy["max_compile_seconds"],
        max_stdout_bytes=policy["max_preprocessed_bytes"],
        max_stderr_bytes=policy["max_stderr_bytes"]
    )
    if code == -3:
        return False, "PREPROCESSED_SOURCE_TOO_LARGE"
    if code != 0:
        return False, f"PREPROCESSOR_FAULT: {stderr.strip()}"
        
    preprocessed_code = stdout

    # 5. Preprocessed output size check (redundant but safe)
    if len(preprocessed_code.encode('utf-8')) > policy["max_preprocessed_bytes"]:
        return False, "PREPROCESSED_SOURCE_TOO_LARGE"

    # 6. Preprocessed forbidden-token scan
    match = token_regex.search(preprocessed_code)
    if match:
        return False, f"PREPROCESSED_DENYLIST_MATCH: {match.group(1)}"
        
    # If it passes, we write the actual full source to sandbox_gen.c for the true compiler pass
    scratch_file = Path("sandbox_gen.c")
    scratch_file.write_text(source_code, encoding="utf-8")
        
    return True, "FORTIFY_PASS"

def fortify_check_file(file_path: Path) -> tuple[bool, str]:
    try:
        content = file_path.read_text(encoding="utf-8")
        return validate_source(content, file_path)
    except Exception as e:
        return False, f"FILE_READ_ERROR: {str(e)}"
