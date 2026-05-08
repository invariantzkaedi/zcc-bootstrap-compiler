#!/usr/bin/env python3
"""
training_pipeline.py — Mega-Swarm Ingestion Loop
================================================
Automates the creation of a fine-tuning dataset mapping vulnerable
C pseudo-code (from mega_decomp/) to optimal ZCC .ir attack vectors.
"""

import os
import json
import glob
from tqdm import tqdm

DECOMP_DIR = "../../mega_decomp/"
OUTPUT_DATASET = "neural_warden_finetune.jsonl"

# Cyber-noir aesthetic colors
C_NAVY = "\033[38;5;17m"
C_CYAN = "\033[36m"
C_MAGENTA = "\033[35m"
C_BOLD = "\033[1m"
C_RESET = "\033[0m"

def print_telemetry(msg):
    print(f"{C_BOLD}{C_NAVY}[SWARM]{C_RESET} {C_CYAN}{msg}{C_RESET}")

def extract_vulnerability_type(code):
    # Simulated heuristic for dataset generation
    code_lower = code.lower()
    if "call" in code_lower and "gas" in code_lower and "balance" in code_lower:
        return "reentrancy"
    elif "price" in code_lower and "reserve" in code_lower:
        return "oracle_manipulation"
    elif "div" in code_lower and "mul" in code_lower:
        return "precision_loss"
    return "access_control"

def synthesize_ir_attack_vector(vuln_type):
    if vuln_type == "reentrancy":
        return "IR_FUNC exploit_reentrancy\n  IR_CALL t0 fallback\n  IR_RET t0\n"
    elif vuln_type == "oracle_manipulation":
        return "IR_FUNC exploit_oracle\n  IR_STORE t0 0x9999\n  IR_RET t0\n"
    elif vuln_type == "precision_loss":
        return "IR_FUNC exploit_precision\n  IR_DIV t0 t1 t2\n  IR_RET t0\n"
    return "IR_FUNC exploit_generic\n  IR_STORE t0 0x1\n  IR_RET t0\n"

def build_dataset():
    print_telemetry(f"Scanning directory: {DECOMP_DIR}")
    
    files = glob.glob(os.path.join(DECOMP_DIR, "*.c"))
    if not files:
        print_telemetry(f"Warning: No .c files found in {DECOMP_DIR}. Using mock files for test.")
        files = ["mock1.c", "mock2.c"]
    
    dataset = []
    
    print_telemetry("Ingesting and mapping vulnerable structures...")
    for f_path in tqdm(files, desc=f"{C_MAGENTA}Ingesting{C_RESET}"):
        if f_path.startswith("mock"):
            c_code = "function test() { call(msg.sender, gas(), balance(address())); }"
        else:
            with open(f_path, 'r', errors='ignore') as f:
                c_code = f.read()
        
        vuln_type = extract_vulnerability_type(c_code)
        ir_payload = synthesize_ir_attack_vector(vuln_type)
        
        prompt = f"[INST] You are an expert smart contract auditor. Find a vulnerability in this decompiled EVM C pseudo-code and output ONLY a valid ZCC .ir script to exploit it:\n\n{c_code}\n[/INST]"
        
        dataset.append({
            "text": f"{prompt}\n{ir_payload}</s>"
        })
        
    print_telemetry(f"Writing {len(dataset)} entries to {OUTPUT_DATASET}")
    with open(OUTPUT_DATASET, 'w') as out_f:
        for entry in dataset:
            out_f.write(json.dumps(entry) + '\n')
            
    print_telemetry("Mega-Swarm ingestion complete. Fine-tuning dataset ready.")

if __name__ == "__main__":
    build_dataset()
