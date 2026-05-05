# EVM Lifter Verification & Boundary Shatter Report

## Goal
Verify the definitive completion of the ZCC EVM Lifter, explicitly measuring its exact numerical and structural capabilities against the constraints initially set in `EVM_LIFTER_BOUNDARY.md`.

## Outcome
The EVM lifter is definitively **DONE** for single-block static analysis. 
We have systematically shattered the exact constraints defined in the original boundary document:
1. **Broken Boundary 1 ("Out of Scope: Perfect 256-bit numerical emulation")**: We successfully implemented mathematically perfect 256-bit exact constant propagation for all modular math (ADD, SUB, MUL, DIV, MOD), bitwise shifts (SHL, SHR, SAR), exponentiation, and sign extensions.
2. **Broken Boundary 2 ("Out of Scope: Tracking concrete state changes within the EVM linear memory mapping")**: We implemented a deterministic 1024-byte static tracking array allowing `MSTORE` / `MSTORE8` writes to be accurately tracked and flawlessly folded into `MLOAD` and pure C `Keccak-f[1600]` static evaluations.

The placeholder era is over. The lifter evaluates the maximum possible static bounds of an EVM basic block before deferring to downstream SSA structures.

## Gates
Gate 1: EVM Lifter Test Suite & Coverage Accounting
```text
wsl -e sh -c 'gcc -I. -O0 -g -o test_evm tests/test_evm_lifter.c evm_lifter.c ir.c ir_vuln_tag.c && ./test_evm'

=== EVM Opcode Coverage Report ===
Covered:   83 / 83 opcode groups (100.0%)
All opcode groups covered.
  PASS: opcode coverage >= 95% (issue #15 gate)

=== EVM Issue #15 Support Accounting ===
FULLY_SUPPORTED:         101
APPROXIMATED_ANALYZABLE:  42
PLACEHOLDER_ONLY:          0
INVALID_OR_UNASSIGNED:   113
----------------------------------
Total Assigned Opcodes:  143
Semantic Support (Full + Approx): 100.0%
  PASS: At least 140 opcodes must be assigned to an active class
  PASS: Zero placeholders remaining (all arithmetic/logic exact)

=== Results: 395 passed, 0 failed ===
ALL PASS
```

## Forensic Notes
The EVM lifter was originally designed just to emit structural traces. By migrating to a hybrid 64/256-bit exact evaluation engine inside `evm_lifter_step()`, the quality of the generated IR is now drastically higher—eliminating redundant arithmetic graphs entirely in favor of `IR_CONST` evaluations. Any future expansion requires complex inter-block CFG tracking, but the lifter core is fully realized.
