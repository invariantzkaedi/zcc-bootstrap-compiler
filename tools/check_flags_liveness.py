#!/usr/bin/env python3
"""
=============================================================================
ZCC Hardened Assembly Flags Liveness Checker
=============================================================================
Statically analyzes x86-64 assembly files for flag-liveness violations:
- Flags are set by FLAG_SETTERS.
- Flags are read/consumed by FLAG_CONSUMERS.
- A movq $0 occurring between a setter and a consumer is a violation (clobbers flags).
- Label definitions are treated as conservative 'unknown-live' flags state.
- Flags persist after their first consumer (e.g. je followed by setne).
- Control flow (jmp, call, ret, leave) clears the flags state.
=============================================================================
"""

import re
import sys

FLAG_SETTERS_RE = re.compile(
    r'^(cmp|test|add|sub|xor|and|or|inc|dec|neg|imul|mul|div|idiv|shl|shr|sar|sal|rol|ror|rcl|rcr|shld|shrd)[bwlqd]?$'
)

FLAG_CONSUMERS_RE = re.compile(
    r'^(j[znspcoeaglb]\w*|set[a-z]{1,2}|cmov[a-z]{1,2}|adc|sbb|pushfq?|lahf)$'
)

def analyze_asm(filepath, mode="mov-zero-only", debug=False):
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    violations = []
    
    # State tracking:
    # flags_state can be:
    # - None (dead/safe)
    # - ('live', setter_line_index, setter_instruction)
    # - ('unknown-live', label_line_index, 'label')
    flags_state = None
    active_movs = []
    
    for idx, line in enumerate(lines):
        stripped = line.strip()
        if not stripped or (stripped.startswith('.') and not stripped.endswith(':')):
            # Skip empty lines and dot-directives, keep labels like .L1:
            continue
            
        if stripped.endswith(':'):
            # Label definition. Conservatively treat as unknown-live.
            flags_state = ('unknown-live', idx, 'label')
            active_movs = []
            continue
            
        parts = stripped.split()
        op = parts[0]
        
        # Check if flags are read/consumed
        if FLAG_CONSUMERS_RE.match(op):
            if flags_state and active_movs:
                for mov_idx, mov_line in active_movs:
                    violations.append({
                        'setter_idx': flags_state[1],
                        'setter_line': lines[flags_state[1]].strip(),
                        'mov_idx': mov_idx,
                        'mov_line': mov_line,
                        'consumer_idx': idx,
                        'consumer_line': stripped
                    })
            # Flags outlive their first consumer, do not clear flags_state
            
        # Check if flags are clobbered/written
        elif FLAG_SETTERS_RE.match(op):
            flags_state = ('live', idx, op)
            active_movs = []
            
        # Check for movq $0
        elif re.match(r'^movq\s+\$0,\s*(%[a-z0-9]+)\s*$', stripped):
            if flags_state:
                active_movs.append((idx, stripped))
                
        # Control flow boundary resets flags analysis
        elif op in ('call', 'ret', 'leave') or op.startswith('jmp'):
            flags_state = None
            active_movs = []
            
    return violations

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 check_flags_liveness.py <assembly_file>")
        sys.exit(1)
        
    violations = analyze_asm(sys.argv[1])
    if violations:
        print(f"Total violations found: {len(violations)}")
        for v in violations:
            print(f"Violation:")
            print(f"  Setter   [L{v['setter_idx']+1}]: {v['setter_line']}")
            print(f"  Movq $0  [L{v['mov_idx']+1}]: {v['mov_line']}")
            print(f"  Consumer [L{v['consumer_idx']+1}]: {v['consumer_line']}")
        sys.exit(1)
    else:
        print("All flag liveness checks passed.")
        sys.exit(0)
