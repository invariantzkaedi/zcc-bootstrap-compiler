#!/usr/bin/env python3

import argparse
import json
import re
from pathlib import Path

RULES = [
    {"id": "placeholder.todo", "pattern": r"\bTODO\b"},
    {"id": "placeholder.fixme", "pattern": r"\bFIXME\b"},
    {"id": "placeholder.stub", "pattern": r"\bSTUB\b|\bstubbed\b|\bstubbing\b"},
    {"id": "placeholder.placeholder", "pattern": r"\bPLACEHOLDER\b|\bplaceholder\b"},
    {"id": "placeholder.omitted_for_brevity", "pattern": r"omitted\s+for\s+brevity"},
    {"id": "placeholder.exact_production_logic", "pattern": r"exact\s+production\s+logic"},
    {"id": "placeholder.as_provided", "pattern": r"as\s+provided"},
    {"id": "placeholder.same_as", "pattern": r"same\s+.*\s+as"},
    {"id": "placeholder.ellipsis_comment", "pattern": r"/\*[^*]*\.\.\.[^*]*\*/|//.*\.\.\."},
]

DEFAULT_INCLUDE_SUFFIXES = {".c", ".h", ".py", ".sh"}
DEFAULT_EXCLUDE_DIRS = {".git", "artifacts", "build", "dist", "__pycache__"}
ALLOW_MARKER = "zcc-no-placeholder-allow"

def load_policy(path):
    policy_path = Path(path)
    if not policy_path.exists():
        return {
            "exclude_paths": [],
            "allow_paths": [],
        }

    data = json.loads(policy_path.read_text(encoding="utf-8"))
    section = data.get("no_placeholders", {})

    return {
        "exclude_paths": section.get("exclude_paths", []),
        "allow_paths": section.get("allow_paths", []),
    }

def rel_string(path, root):
    return str(path.relative_to(root)).replace("\\", "/")

def allow_reason(rel, policy):
    for entry in policy["allow_paths"]:
        if entry.get("path") == rel:
            return entry.get("reason", "allowed by policy")
    return None

def should_skip(path, root, exclude_dirs, policy):
    rel = rel_string(path, root)

    if any(part in exclude_dirs for part in Path(rel).parts):
        return True, "excluded directory"

    if rel in policy["exclude_paths"]:
        return True, "excluded by policy"

    return False, None

def scan_file(path, rules):
    findings = []

    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return findings

    for line_no, line in enumerate(lines, start=1):
        if ALLOW_MARKER in line:
            continue

        for rule in rules:
            if re.search(rule["pattern"], line, flags=re.IGNORECASE):
                findings.append({
                    "rule": rule["id"],
                    "path": str(path),
                    "line": line_no,
                    "text": line.rstrip(),
                })

    return findings

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--policy", default=".zcc-fortify-policy.json")
    parser.add_argument("--report", default="artifacts/no-placeholders.json")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    report = Path(args.report)
    policy = load_policy(args.policy)

    findings = []
    allowed_files = []
    skipped_files = []

    for path in root.rglob("*"):
        if not path.is_file():
            continue

        if path.suffix not in DEFAULT_INCLUDE_SUFFIXES:
            continue

        rel = rel_string(path, root)

        skip, reason = should_skip(path, root, DEFAULT_EXCLUDE_DIRS, policy)
        if skip:
            skipped_files.append({
                "path": rel,
                "reason": reason,
            })
            continue

        reason = allow_reason(rel, policy)
        if reason:
            allowed_files.append({
                "path": rel,
                "reason": reason,
            })
            continue

        findings.extend(scan_file(path, RULES))

    result = {
        "status": "fail" if findings else "pass",
        "count": len(findings),
        "findings": findings,
        "allowed_files": allowed_files,
        "skipped_files": skipped_files,
    }

    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    if findings:
        for finding in findings:
            print(
                f"{finding['path']}:{finding['line']}: "
                f"{finding['rule']}: {finding['text']}"
            )
        print(f"[NO-PLACEHOLDERS] failed: report={report}")
        raise SystemExit(1)

    print(f"[NO-PLACEHOLDERS] clean: report={report}")

if __name__ == "__main__":
    main()