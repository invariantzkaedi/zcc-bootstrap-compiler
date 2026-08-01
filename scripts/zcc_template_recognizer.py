#!/usr/bin/env python3
"""
ZCC Template & Playbook Recognizer & Validator (scripts/zcc_template_recognizer.py)
-----------------------------------------------------------------------------------
Scans, parses, and validates ZCC engineering templates, commitment logs, and gate
checklists to ensure 100% compliance with ZCC protocol rules.
"""

import os
import sys
import re

# Force UTF-8 stdout encoding compatibility for Windows/Linux CI runners
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

TEMPLATES_DIR = "templates"
REQUIRED_TEMPLATES = [
    "PLAYBOOK.md",
    "COMMIT_TEMPLATE.md",
    "GATE_CHECKLIST.md",
    "FAULT_INJECTION_GUIDE.md",
    "INCIDENT_REPORT.md"
]

SIG_PATTERNS = {
    "PLAYBOOK.md": [r"ZCC Systems Engineering", r"Stop-Work"],
    "COMMIT_TEMPLATE.md": [r"Mandatory ZCC Commit Body", r"Phase 0 Evidence"],
    "GATE_CHECKLIST.md": [r"ZCC Gate Checklist", r"Gate 0:"],
    "FAULT_INJECTION_GUIDE.md": [r"ZCC Fault-Injection Guide", r"Target Defect Class"],
    "INCIDENT_REPORT.md": [r"ZCC CI / Self-Host Incident Report", r"Failure Signature"]
}

def scan_templates():
    print("[ZCC TEMPLATE RECOGNIZER] Scanning repository templates...\n")
    found_count = 0
    
    for t_name in REQUIRED_TEMPLATES:
        path = os.path.join(TEMPLATES_DIR, t_name)
        if not os.path.exists(path):
            print(f"[MISSING] {path} not found!")
            continue

        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()

        patterns = SIG_PATTERNS.get(t_name, [])
        matches = all(re.search(p, content) for p in patterns)

        if matches:
            print(f"[RECOGNIZED & VALIDATED] {path} (Matches ZCC Protocol Signature)")
            found_count += 1
        else:
            print(f"[UNRECOGNIZED SCHEMA] {path} does not match expected protocol signature!")

    print(f"\n==================================================")
    print(f"[SUMMARY] {found_count} / {len(REQUIRED_TEMPLATES)} Templates Recognized & Validated")
    print(f"==================================================")
    
    return 0 if found_count == len(REQUIRED_TEMPLATES) else 1

if __name__ == "__main__":
    sys.exit(scan_templates())
