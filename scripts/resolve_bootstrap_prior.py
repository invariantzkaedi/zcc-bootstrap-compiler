#!/usr/bin/env python3
"""
Robust Bootstrap Baseline Hash Resolver for ZCC
Extracts the latest matching bootstrap hash for the target host environment
(Azure CI vs WSL2 vs Generic Linux) from BOOTSTRAP_BASELINES.tsv.
"""

import sys
import os

def main():
    if len(sys.argv) < 3:
        sys.exit(0)
    ledger_path = sys.argv[1]
    uname_str = sys.argv[2].lower()
    
    if not os.path.exists(ledger_path):
        sys.exit(0)
        
    is_azure = "azure" in uname_str
    is_wsl = "wsl" in uname_str or "microsoft" in uname_str
    
    prior_hash = ""
    
    with open(ledger_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            parts = line.rstrip("\r\n").split("\t")
            if len(parts) >= 6:
                row_hash = parts[2].strip()
                row_uname = parts[5].strip().lower()
                
                if is_azure:
                    if "azure" in row_uname:
                        prior_hash = row_hash
                elif is_wsl:
                    if "wsl" in row_uname or "microsoft" in row_uname:
                        prior_hash = row_hash
                else:
                    if "azure" not in row_uname and "wsl" not in row_uname and "microsoft" not in row_uname:
                        prior_hash = row_hash
                        
    print(prior_hash)

if __name__ == "__main__":
    main()
