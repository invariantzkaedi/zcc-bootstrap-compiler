#!/usr/bin/env python3
"""
ZCC Forensic CFG Discrepancy Tool
Compares two assembly files (parent vs mutant, stage2 vs stage3) to detect control flow graph
discrepancies, label node shifts, edge changes, and isolated label prunings.
"""

import os
import sys
import hashlib
import re
import argparse

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, REPO_ROOT)
from zcc_cfg_extract import extract_cfg, cfg_stats

def analyze(path: str):
    if not os.path.exists(path):
        print(f"[ERROR] File non-existent: {path}")
        return None

    with open(path, 'rb') as f:
        data = f.read()

    file_hash = hashlib.sha256(data).hexdigest()[:16]
    lines = data.decode('utf-8', errors='ignore').splitlines()

    label_re = re.compile(r'^(\.[A-Za-z_]\w*|[A-Za-z_]\w*):')
    raw_labels = [label_re.match(l.strip()).group(1) for l in lines if label_re.match(l.strip())]

    cfg = extract_cfg(lines)
    stats = cfg_stats(cfg)

    return {
        'path': path,
        'hash': file_hash,
        'line_count': len(lines),
        'raw_label_count': len(raw_labels),
        'unique_label_count': len(set(raw_labels)),
        'cfg': cfg,
        'stats': stats,
    }

def main():
    parser = argparse.ArgumentParser(description="Forensic CFG discrepancy audit between two assembly files")
    parser.add_argument("file_a", nargs="?", default="zcc2.s", help="Base assembly file (default: zcc2.s)")
    parser.add_argument("file_b", nargs="?", default="dreams/island_0_parent.s", help="Target assembly file")
    args = parser.parse_args()

    file_a = os.path.join(REPO_ROOT, args.file_a) if not os.path.isabs(args.file_a) else args.file_a
    file_b = os.path.join(REPO_ROOT, args.file_b) if not os.path.isabs(args.file_b) else args.file_b

    res_a = analyze(file_a)
    res_b = analyze(file_b)

    if not res_a:
        return

    print("=== FORENSIC CFG DISCREPANCY AUDIT ===")
    print(f"File A: {res_a['path']} (hash={res_a['hash']}, lines={res_a['line_count']:,}, nodes={res_a['stats']['nodes']:,}, edges={res_a['stats']['edges']:,})")

    if res_b:
        print(f"File B: {res_b['path']} (hash={res_b['hash']}, lines={res_b['line_count']:,}, nodes={res_b['stats']['nodes']:,}, edges={res_b['stats']['edges']:,})")
        print("\n--- Differences ---")
        print(f"Line Count Delta:  {res_b['line_count'] - res_a['line_count']:+d}")
        print(f"Node Delta:        {res_b['stats']['nodes'] - res_a['stats']['nodes']:+d}")
        print(f"Edge Delta:        {res_b['stats']['edges'] - res_a['stats']['edges']:+d}")

if __name__ == "__main__":
    main()
