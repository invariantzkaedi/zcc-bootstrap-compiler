import tempfile
import sys
import os
from pathlib import Path

# Add tools to sys.path so we can import from check_flags_liveness
sys.path.append(str(Path(__file__).resolve().parent.parent / 'tools'))
from check_flags_liveness import analyze_asm

def _run(asm_lines, mode="mov-zero-only"):
    with tempfile.NamedTemporaryFile("w", suffix=".s", delete=False, encoding="utf-8") as f:
        f.write("\n".join(asm_lines) + "\n")
        path = f.name
    try:
        res = analyze_asm(path, mode=mode, debug=False)
    finally:
        os.unlink(path)
    return res

def test_no_false_positive_on_unconditional_jmp():
    asm = [
        "cmpq $0, %rax",
        "movq $0, %r10",
        "jmp .L1",   # must NOT consume flags
        ".L1:",
        "ret",
    ]
    violations = _run(asm, mode="mov-zero-only")
    assert len(violations) == 0

def test_true_positive_on_conditional_jump():
    asm = [
        "cmpq $0, %rax",
        "movq $0, %r10",
        "je .L1",    # consumes flags -> should report
    ]
    violations = _run(asm, mode="mov-zero-only")
    assert len(violations) == 1
