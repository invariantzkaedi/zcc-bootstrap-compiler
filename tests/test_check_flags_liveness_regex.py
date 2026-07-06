import sys
from pathlib import Path

# Add tools to sys.path so we can import from check_flags_liveness
sys.path.append(str(Path(__file__).resolve().parent.parent / 'tools'))
from check_flags_liveness import FLAG_CONSUMERS_RE

def test_jmp_not_consumer():
    assert FLAG_CONSUMERS_RE.match("jmp") is None
    assert FLAG_CONSUMERS_RE.match("ljmp") is None

def test_conditional_jumps_are_consumers():
    for op in ["je", "jne", "jg", "jl", "jae", "jbe", "jnz", "jge"]:
        assert FLAG_CONSUMERS_RE.match(op), f"{op} should be a consumer"

def test_setcc_and_cmovcc_are_consumers():
    for op in ["setne", "setl", "cmovg", "cmovle"]:
        assert FLAG_CONSUMERS_RE.match(op), f"{op} should be a consumer"

def test_other_consumers_match():
    for op in ["adc", "sbb", "pushf", "pushfq", "lahf"]:
        assert FLAG_CONSUMERS_RE.match(op), f"{op} should be a consumer"
