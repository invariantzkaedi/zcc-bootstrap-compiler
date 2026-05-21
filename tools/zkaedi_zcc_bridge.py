import sys
import os
import hashlib
import struct
import random
import json

def generate_ir_ghosts(ir_path, ghost_bin, depth=50.0):
    """
    Parses the ZCC intermediate representation snapshot and mathematically maps 
    the compiler opcodes directly into the ghost layer's parallax vector space.
    """
    with open(ir_path, 'r', encoding='utf-8') as f:
        ir_data = json.load(f)
        
    nodes = []
    for func in ir_data.get('functions', []):
        nodes.extend(func.get('nodes', []))
        
    if not nodes:
        return 0

    with open(ghost_bin, 'wb') as f:
        for node in nodes:
            # Map OP to X [-0.15, 0.15]
            op = node.get('op', 0)
            x = ((op / 40.0) * 0.3) - 0.15
            
            # Map TYPE to Y [-0.15, 0.15]
            typ = node.get('type', 0)
            y = ((typ / 10.0) * 0.3) - 0.15
            
            # Map IMM to Z space (bounded to prevent blowout)
            imm = float(node.get('imm', 0))
            imm_bound = min(max(imm / 100.0, -1.0), 1.0)
            z = (imm_bound * 0.15) * (depth * 0.05)
            
            f.write(struct.pack('<fff', x, y, z))
            
    os.chmod(ghost_bin, 0o444)
    return len(nodes)

def unknowncode_seed_native(svg_hash: str, depth: float = 50.0, ghost_bin: str = ''):
    seed = int(svg_hash, 16) % (2**32)
    random.seed(seed)
    
    with open(ghost_bin, 'wb') as f:
        for _ in range(32):
            x = random.uniform(-0.15, 0.15)
            y = random.uniform(-0.15, 0.15)
            z = random.uniform(-0.15, 0.15) * (depth * 0.05)
            f.write(struct.pack('<fff', x, y, z))
            
    os.chmod(ghost_bin, 0o444)
    return 32

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("[-] Missing ZCC telemetry hash payload")
        sys.exit(1)
        
    zcc_hash = sys.argv[1]
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    zcc_root = os.path.dirname(tools_dir)
    ghost_bin = os.path.join(tools_dir, 'proxy_ghost.bin')
    ir_path = os.path.join(zcc_root, 'ir_snapshot.json')
    
    if zcc_hash == "0000000000000000":
        print("\n[ZKAEDI KINETIC BRIDGE] BOOTSTRAP_RED DETECTED.")
        print("[ZKAEDI KINETIC BRIDGE] Shattering sentient geometry. Compiler loop fractured.")
        if os.path.exists(ghost_bin):
            os.chmod(ghost_bin, 0o666)
            os.remove(ghost_bin)
        sys.exit(1)
        
    seed_hash = hashlib.sha256(zcc_hash.encode()).hexdigest()[:16]
    
    print(f"\n[ZKAEDI KINETIC BRIDGE] Received ZCC Telemetry (SHA256): {zcc_hash[:16]}...")
    
    if os.path.exists(ghost_bin):
        os.chmod(ghost_bin, 0o666)
        
    # Attempt to ingest the real IR snapshot from the compiler
    if os.path.exists(ir_path):
        num_ghosts = generate_ir_ghosts(ir_path, ghost_bin, depth=50.0)
        print(f"[ZKAEDI KINETIC BRIDGE] IR SNAPSHOT SYNCED. Mapped {num_ghosts} opcodes to parallax nodes.")
    else:
        num_ghosts = unknowncode_seed_native(seed_hash, depth=50.0, ghost_bin=ghost_bin)
        print(f"[ZKAEDI KINETIC BRIDGE] Sentient ghost layer seeded from raw hash.")
        
    print(f"[ZKAEDI KINETIC BRIDGE] Modesto grid successfully synchronized to C compiler.")
