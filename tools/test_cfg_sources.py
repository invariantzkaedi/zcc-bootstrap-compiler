#!/usr/bin/env python3
"""
ZCC Test CFG Sources Tool
Inspects assembly files (e.g., zcc2.s, benchmark assembly) and reports file hash,
line count, parsed labels, and extracted control flow graph (CFG) node/edge statistics.
"""

import os
import sys
import hashlib
import argparse

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, REPO_ROOT)
from zcc_cfg_extract import extract_cfg, cfg_stats

def audit_file(path: str):
    if not os.path.exists(path):
        print(f"[MISSING] {path}")
        return None

    with open(path, 'rb') as f:
        content = f.read()

    file_hash = hashlib.sha256(content).hexdigest()[:16]
    lines = content.decode('utf-8', errors='ignore').splitlines()

    cfg = extract_cfg(lines)
    stats = cfg_stats(cfg)

    print(f"File:       {path}")
    print(f"Hash:       {file_hash}")
    print(f"Lines:      {len(lines):,}")
    print(f"CFG Nodes:  {stats['nodes']:,}")
    print(f"CFG Edges:  {stats['edges']:,}")
    print(f"Avg Degree: {stats['avg_degree']}")
    print(f"Max Degree: {stats['max_degree']}")
    return stats

def main():
    parser = argparse.ArgumentParser(description="Audit assembly CFG stats across files")
    parser.add_argument("files", nargs="*", default=["zcc2.s"], help="Assembly files to audit")
    args = parser.parse_args()

    for path in args.files:
        full_path = os.path.join(REPO_ROOT, path) if not os.path.isabs(path) else path
        print(f"\n--- Auditing: {path} ---")
        audit_file(full_path)

if __name__ == "__main__":
    main()
