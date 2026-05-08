#!/usr/bin/env python3
"""
evm_256bit_fuzzer.py — ZKAEDI PRIME
Courtroom Verification for EVM 256-bit Constant Folding (EQ, LT, GT, ISZERO).

Generates synthetic EVM bytecode utilizing PUSH32 to push wide constants,
then applies comparison operators. Validates that ZCC folds these ops
into a single boolean CONST (0 or 1) in the emitted Yul, ensuring that
the comparison does not appear in the final generated code.
"""

import os
import subprocess
import random

# Constants for 256-bit integers
MAX_UINT256 = (1 << 256) - 1
HALF_UINT256 = 1 << 127

def to_push32_hex(val: int) -> str:
    # returns hex string without '0x', zero-padded to 64 chars
    return f"{val:064x}"

import os
import subprocess
import random

# Constants for 256-bit integers
MAX_UINT256 = (1 << 256) - 1
HALF_UINT256 = 1 << 127

def to_push32_hex(val: int) -> str:
    # returns hex string without '0x', zero-padded to 64 chars
    return f"{val:064x}"

def generate_evm_hex(op: str, val1: int, val2: int) -> str:
    """
    Generate EVM bytecode:
    PUSH32 val1
    PUSH32 val2
    OP
    """
    bytecode = "7f" + to_push32_hex(val1) # PUSH32 val1
    bytecode += "7f" + to_push32_hex(val2) # PUSH32 val2
    
    if op == "EQ":
        bytecode += "14" # EQ
    elif op == "LT":
        bytecode += "10" # LT
    elif op == "GT":
        bytecode += "11" # GT
    elif op == "ISZERO":
        # ISZERO only takes 1 argument
        bytecode = "7f" + to_push32_hex(val1) # PUSH32 val1
        bytecode += "15" # ISZERO
    else:
        raise ValueError(f"Unknown op: {op}")
        
    # Add PUSH1 00; SSTORE to consume the result and prevent DCE
    bytecode += "600055"
    return bytecode

def expected_result(op: str, val1: int, val2: int) -> int:
    if op == "EQ":
        return 1 if val1 == val2 else 0
    elif op == "LT":
        return 1 if val1 < val2 else 0
    elif op == "GT":
        return 1 if val1 > val2 else 0
    elif op == "ISZERO":
        return 1 if val1 == 0 else 0
    return -1

def run_zcc_on_hex(hex_str: str) -> str:
    cmd = ["./tools/battle/fuzz_fold", hex_str]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print("fuzz_fold Failed:")
        print(res.stderr)
        return ""
    return res.stdout

def main():
    print("╔══════════════════════════════════════════════════════════════╗")
    print("║  ▸ ZKAEDI EVM 256-BIT CONSTANT FOLDING FUZZER                ║")
    print("╚══════════════════════════════════════════════════════════════╝")
    
    ops = ["EQ", "LT", "GT", "ISZERO"]
    
    # 1. Edge Cases (must be covered for 95%+ limb logic)
    edge_cases = [
        (0, 0),
        (0, 1),
        (1, 0),
        (MAX_UINT256, MAX_UINT256),
        (MAX_UINT256 - 1, MAX_UINT256),
        (MAX_UINT256, MAX_UINT256 - 1),
        (HALF_UINT256, HALF_UINT256),
        (HALF_UINT256 - 1, HALF_UINT256),
        (HALF_UINT256, HALF_UINT256 - 1),
        (1 << 64, 1 << 64),
        ((1 << 64) - 1, 1 << 64),
        (1 << 64, (1 << 64) - 1),
        (1 << 192, 1 << 192),
        ((1 << 192) - 1, 1 << 192),
        (1 << 192, (1 << 192) - 1),
    ]
    
    tests = []
    # Add edge cases for all ops
    for op in ops:
        for v1, v2 in edge_cases:
            tests.append((op, v1, v2))
            
    # 2. Random fuzzing up to 500 tests
    random.seed(42)
    while len(tests) < 500:
        op = random.choice(ops)
        v1 = random.randint(0, MAX_UINT256)
        v2 = random.randint(0, MAX_UINT256)
        if random.random() < 0.2:
            v2 = v1 # Force some equalities
        tests.append((op, v1, v2))
        
    passed = 0
    failed = 0
    
    # Build the C executable
    subprocess.run(["gcc", "-O0", "-g0", "-w", "-fno-asynchronous-unwind-tables", "-o", "tools/battle/fuzz_fold", 
                    "tools/battle/fuzz_fold.c", "ir_pass_manager.c", "ir_pass_warden.c", "ir_symbolic_cfg.c", "ir_dominance.c", "evm_lifter.c", "ir.c", "ir_vuln_tag.c", "compiler_passes_ir.c", 
                    "src/evm/memory_v2.c", "src/evm/symbolic.c", "src/evm/abi_extractor.c", "-lm"], capture_output=True)
    
    for i, (op, v1, v2) in enumerate(tests):
        bytecode = generate_evm_hex(op, v1, v2)
        ir_out = run_zcc_on_hex(bytecode)
        
        expected = expected_result(op, v1, v2)
        
        ir_lower = ir_out.lower()
        has_eq = " eq " in ir_lower
        has_lt = " lt " in ir_lower
        has_gt = " gt " in ir_lower
        has_iszero = " iszero " in ir_lower
        
        failed_fold = False
        if op == "EQ" and has_eq: failed_fold = True
        if op == "LT" and has_lt: failed_fold = True
        if op == "GT" and has_gt: failed_fold = True
        if op == "ISZERO" and has_iszero: failed_fold = True
        if op == "ISZERO" and has_eq: failed_fold = True # ISZERO emits EQ
        
        expected_str = f"imm={expected}"
        has_expected_const = expected_str in ir_lower
        
        if failed_fold or not has_expected_const:
            print(f"❌ FAIL [{i+1}/500]: {op} 0x{v1:x} 0x{v2:x}")
            print(f"   Expected folded {expected_str}, but got raw op or missing const.")
            print(ir_out)
            failed += 1
        else:
            passed += 1
            
    print(f"\n  Results: {passed} PASSED, {failed} FAILED")
    
    if os.path.exists("tools/battle/fuzz_fold"): os.remove("tools/battle/fuzz_fold")
    
    if failed > 0:
        exit(1)
    else:
        print("  ✓ 500/500 GREEN")
        exit(0)

if __name__ == "__main__":
    main()
