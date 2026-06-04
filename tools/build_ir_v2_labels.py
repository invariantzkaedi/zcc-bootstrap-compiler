#!/usr/bin/env python3
"""
ZCC IR V2 Label Extraction Pipeline
===================================
Parses ZCC compiler pass logs to extract compiler-verified DCE labels,
matches nodes by index, and joins them with the Hugging Face dataset
'zkaedi/zcc-ir-prime-v1' to create 'v2'.
"""

import os
import sys
import re
import argparse
from datasets import load_dataset, Features, Value

# Cyber-noir console coloring
C_NAVY = "\033[38;5;17m"
C_CYAN = "\033[36m"
C_MAGENTA = "\033[35m"
C_GREEN = "\033[32m"
C_RED = "\033[31m"
C_YELLOW = "\033[33m"
C_BOLD = "\033[1m"
C_RESET = "\033[0m"

def log_info(msg):
    print(f"{C_BOLD}{C_NAVY}[ZCC-IR-V2]{C_RESET} {C_CYAN}{msg}{C_RESET}")

def log_success(msg):
    print(f"{C_BOLD}{C_GREEN}[SUCCESS]{C_RESET} {msg}")

def log_warn(msg):
    print(f"{C_BOLD}{C_YELLOW}[WARNING]{C_RESET} {msg}")

def log_error(msg):
    print(f"{C_BOLD}{C_RED}[ERROR]{C_RESET} {msg}")

def parse_dce_logs(log_path):
    """
    Parses interleaved logs to track functions and their deleted register destinations.
    Returns: dict mapping func_name -> set of (op, dst)
    """
    dce_deleted = {}
    current_func = None

    func_pattern = re.compile(r'\[ZCC-SNAPSHOT\] Pass (dce\d?): Func=([a-zA-Z0-9_]+)')
    dce_pattern = re.compile(r'\[dce\] deleted:\s*([a-zA-Z0-9_]+)\s*(.*?)\s*->\s*(\S+)')

    log_info(f"Parsing DCE logs: {log_path}...")
    if not os.path.exists(log_path):
        log_error(f"DCE logs file not found: {log_path}")
        sys.exit(1)

    total_lines = 0
    match_count = 0
    with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            total_lines += 1
            # Check for function context switch in DCE pass
            func_match = func_pattern.search(line)
            if func_match:
                current_func = func_match.group(2)
                if current_func not in dce_deleted:
                    dce_deleted[current_func] = set()
                continue

            # Check for deleted instruction
            dce_match = dce_pattern.search(line)
            if dce_match:
                op = dce_match.group(1)
                dst = dce_match.group(3).strip()
                if current_func:
                    dce_deleted[current_func].add((op, dst))
                    match_count += 1

    log_info(f"Processed {total_lines} log lines. Found {len(dce_deleted)} functions with deletes and {match_count} deleted register instructions.")
    return dce_deleted

def main():
    parser = argparse.ArgumentParser(description="ZCC IR V2 Label Extraction Pipeline")
    parser.add_argument("--dce-logs", default="/tmp/dce_logs_interleaved_new.txt", help="Path to interleaved compiler logs")
    parser.add_argument("--source-ds", default="zkaedi/zcc-ir-prime-v1", help="Source Hugging Face dataset name")
    parser.add_argument("--target-ds", default="zkaedi/zcc-ir-prime-v2", help="Target Hugging Face dataset name")
    parser.add_argument("--dry-run", action="store_true", help="Execute dry run and print sample records without uploading")
    args = parser.parse_args()

    log_info("Starting compiler-verified DCE label extraction...")

    # Hugging Face token check
    hf_token = None
    if not args.dry_run:
        hf_token = os.environ.get("HF_TOKEN")
        if not hf_token:
            for p in [os.path.expanduser("~/.cache/huggingface/token"), os.path.expanduser("~/.huggingface/token")]:
                if os.path.exists(p):
                    try:
                        with open(p, 'r') as f:
                            hf_token = f.read().strip()
                        log_info(f"Loaded cached Hugging Face token from {p}")
                        break
                    except Exception:
                        pass
        if not hf_token:
            log_error("HF_TOKEN environment variable is not defined and no cached token found. Cannot upload dataset.")
            sys.exit(1)

    # 1. Parse DCE Logs
    dce_deleted = parse_dce_logs(args.dce_logs)

    # 2. Load Source Dataset
    log_info(f"Loading source dataset: {args.source_ds}...")
    try:
        ds = load_dataset(args.source_ds)
    except Exception as e:
        log_error(f"Failed to load Hugging Face dataset '{args.source_ds}': {e}")
        sys.exit(1)

    # 3. Join Labels
    log_info("Joining labels with Hugging Face dataset records...")
    
    # Define explicit features for schema safety
    orig_features = ds['train'].features
    new_features = Features({
        **orig_features,
        'dce_labels': [{
            'node_idx': Value('int64'),
            'op': Value('string'),
            'type': Value('string'),
            'source': Value('string')
        }],
        'label_source': Value('string'),
        'dce_label_count': Value('int64')
    })

    def add_dce_labels_fn(example):
        func_name = example['name']
        deleted_set = dce_deleted.get(func_name, set())
        labels = []
        for node_idx, node in enumerate(example['nodes']):
            op = node.get('op')
            dst = node.get('dst') or ''
            if (op, dst) in deleted_set:
                labels.append({
                    "node_idx": node_idx,
                    "op": op,
                    "type": "DCE",
                    "source": "compiler_pass"
                })
        return {
            "dce_labels": labels,
            "label_source": "compiler_pass",
            "dce_label_count": len(labels)
        }

    updated_ds = ds.map(add_dce_labels_fn, features=new_features)

    # Calculate statistics
    total_records = len(updated_ds['train'])
    labeled_records = sum(1 for x in updated_ds['train'] if x['dce_label_count'] > 0)
    total_labels_inserted = sum(x['dce_label_count'] for x in updated_ds['train'])
    coverage_pct = (labeled_records / total_records) * 100 if total_records > 0 else 0.0

    log_info("=== DATASET STATISTICS ===")
    log_info(f"  Total functions in dataset    : {total_records}")
    log_info(f"  Functions with DCE labels     : {labeled_records} ({coverage_pct:.2f}%)")
    log_info(f"  Total DCE labels inserted     : {total_labels_inserted}")
    log_info("==========================")

    # 4. Verification/Output
    if args.dry_run:
        log_info("DRY-RUN mode enabled. Printing first 5 labeled records:")
        labeled_samples = [x for x in updated_ds['train'] if x['dce_label_count'] > 0]
        samples_to_print = labeled_samples[:5]
        if not samples_to_print:
            log_warn("No functions with non-zero label counts were found. Printing first 5 general records:")
            samples_to_print = list(updated_ds['train'])[:5]

        for i, s in enumerate(samples_to_print, 1):
            print(f"\nSample {i}:")
            print(f"  Name           : {s['name']}")
            print(f"  Ret Type       : {s['ret_type']}")
            print(f"  Label Source   : {s['label_source']}")
            print(f"  DCE Label Count: {s['dce_label_count']}")
            print(f"  DCE Labels     : {s['dce_labels']}")
        log_success("Dry run completed successfully.")
        return 0

    # 5. Upload to Hugging Face
    log_info(f"Uploading labeled dataset to Hugging Face Hub as '{args.target_ds}'...")
    try:
        updated_ds.push_to_hub(args.target_ds, token=hf_token)
        log_success(f"Dataset successfully pushed to Hugging Face: https://huggingface.co/datasets/{args.target_ds}")
    except Exception as e:
        log_error(f"Failed to push dataset to Hugging Face Hub: {e}")
        sys.exit(1)

    return 0

if __name__ == '__main__':
    sys.exit(main())
