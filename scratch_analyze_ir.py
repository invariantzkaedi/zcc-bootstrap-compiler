import re
import sys

def analyze(filename):
    print(f"Analyzing {filename}")
    with open(filename, "r") as f:
        lines = f.readlines()
    
    current_block = -1
    first_use = {}
    first_def = {}
    
    for line in lines:
        m_block = re.match(r'^Block (\d+)', line)
        if m_block:
            current_block = int(m_block.group(1))
            continue
            
        m_ins = re.search(r'ins \d+: op=\d+ dst=(\d+)(?: src0=(\d+))?(?: src1=(\d+))?', line)
        if m_ins:
            dst = int(m_ins.group(1))
            src0 = int(m_ins.group(2)) if m_ins.group(2) else None
            src1 = int(m_ins.group(3)) if m_ins.group(3) else None
            
            if src0 is not None:
                if src0 not in first_use:
                    first_use[src0] = current_block
            if src1 is not None:
                if src1 not in first_use:
                    first_use[src1] = current_block
                    
            if dst not in first_def:
                first_def[dst] = current_block

    inverted = []
    for vreg, use_blk in first_use.items():
        if vreg in first_def:
            def_blk = first_def[vreg]
            if use_blk < def_blk:
                inverted.append((vreg, use_blk, def_blk))
    
    if inverted:
        print(f"Found {len(inverted)} inverted intervals:")
        for vreg, use_blk, def_blk in inverted:
            print(f"  vreg {vreg}: used in Block {use_blk} before defined in Block {def_blk}")
    else:
        print("  None visible at IR level.")
    print(f"Total blocks: {current_block + 1}")

if __name__ == "__main__":
    analyze("next_token_preopt.txt")
    analyze("peek_token_preopt.txt")
