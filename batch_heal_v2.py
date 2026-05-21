#!/usr/bin/env python3
"""
ZKAEDI OMEGA DAEMON // BATCH_HEAL.PY (MUTATOR VARIANT)
Hamiltonian State Preservation: H_t = H_0 + \eta * AST_static
"""

import os
import re
import subprocess
import sys
import shutil
import glob

HEADER_PATH = "zcc_vaccine_table.h"
BACKUP_PATH = "zcc_vaccine_table.h.bak"
COMPILER_BIN = "./zcc"
CORPUS_DIR = "mega_corpus"
BOUNDARY_MARKER = "// --- ZKAEDI_AUTO_INJECT_BOUNDARY ---"

def inject_vaccine(signature: str, action_flag: str = "0x01") -> bool:
    print(f"[*] Mutating AST Registry with Signature: {signature}")
    if not os.path.exists(HEADER_PATH):
        print(f"[!] ERR: {HEADER_PATH} not found. Operating out of bounds.")
        return False

    shutil.copyfile(HEADER_PATH, BACKUP_PATH)

    with open(HEADER_PATH, 'r') as f:
        lines = f.readlines()

    injected = False
    with open(HEADER_PATH, 'w') as f:
        for line in lines:
            if BOUNDARY_MARKER in line:
                # Precise 4-space indentation, maintaining static array alignment
                f.write(f"    {{ {signature}, {action_flag} }}, // AUTO-INJECTED PRIME\n")
                f.write(line)
                injected = True
            else:
                f.write(line)
    
    if not injected:
        print("[!] ERR: Injection boundary not found. Mutation failed.")
        rollback()
        return False

    return True

def run_selfhost_gate() -> bool:
    print("[*] Triggering Parity Verification Gate (make selfhost)...")
    try:
        # Strict stdout redirection to avoid polluting daemon logs, but stderr surfaces on fail
        result = subprocess.run(["wsl", "-e", "make", "selfhost"], check=True, capture_output=True, text=True)
        print("[+] Golden Checksum Verified. H_t state is locked and stable.")
        return True
    except subprocess.CalledProcessError as e:
        print("[-] FATAL: Compiler drift detected. Binary checksum shattered.")
        print(f"    Reason: {e.stderr.strip()}")
        return False

def rollback():
    print("[-] Executing Rollback Procedure...")
    if os.path.exists(BACKUP_PATH):
        shutil.copyfile(BACKUP_PATH, HEADER_PATH)
        os.remove(BACKUP_PATH)
        print("[+] Registry restored to H_{t-1}. Pipeline stable.")
    else:
        print("[!] CRITICAL: No backup found. Manual intervention required.")

def heal_corpus():
    print(f"[*] Initiating Mass Healing Phase on {CORPUS_DIR}/*.bin")
    binaries = glob.glob(os.path.join(CORPUS_DIR, "*.bin"))
    
    if not binaries:
        print(f"[!] No targets found in {CORPUS_DIR}.")
        return

    success_count = 0
    for target in binaries:
        base_name = os.path.splitext(os.path.basename(target))[0]
        out_file = os.path.join(CORPUS_DIR, f"{base_name}_healed.yul")
        try:
            subprocess.run([COMPILER_BIN, "-fevm-emit", target, "-o", out_file], check=True, capture_output=True)
            success_count += 1
        except subprocess.CalledProcessError:
            print(f"[-] Discarding {target}: Lifter failed.")
    
    print(f"[+] Corpus Healing Complete. {success_count}/{len(binaries)} targets neutralized.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: ./batch_heal_v2.py <0xSIGNATURE>")
        sys.exit(1)

    target_sig = sys.argv[1]
    
    if inject_vaccine(target_sig):
        if run_selfhost_gate():
            if os.path.exists(BACKUP_PATH):
                os.remove(BACKUP_PATH)
            heal_corpus()
        else:
            rollback()
            sys.exit(1)
