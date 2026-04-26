#!/usr/bin/env python3
import argparse
import json
import re
from pathlib import Path

REQUIRED_REASON_LENGTH = 12

PLACEHOLDER_RULES = {
    "placeholder.todo": r"\bTODO\b",
    "placeholder.fixme": r"\bFIXME\b",
    "placeholder.stub": r"\bSTUB\b|\bstubbed\b|\bstubbing\b",
    "placeholder.placeholder": r"\bPLACEHOLDER\b|\bplaceholder\b",
    "placeholder.omitted_for_brevity": r"omitted\s+for\s+brevity",
    "placeholder.exact_production_logic": r"exact\s+production\s+logic",
    "placeholder.as_provided": r"as\s+provided",
    "placeholder.same_as": r"same\s+.*\s+as",
    "placeholder.ellipsis_comment": r"/\*[^*]*\.\.\.[^*]*\*/|//.*\.\.\.",
}

CANONICAL_POLICY = ".zcc-fortify-policy.json"

def file_contains_rule(path, rule_id):
    pattern = PLACEHOLDER_RULES.get(rule_id)
    if not pattern:
        return False

    text = path.read_text(encoding="utf-8", errors="replace")
    return re.search(pattern, text, flags=re.IGNORECASE) is not None

def validate_entries(root, entries, section, key, errors):
    seen = set()

    for entry in entries:
        # exclude_paths uses plain strings (scanner contract);
        # allow_paths uses {path, reason, expected_rules} dicts.
        if isinstance(entry, str):
            path = entry
            reason = ""
            entry = {"path": path}
        elif isinstance(entry, dict):
            path = entry.get("path")
            reason = entry.get("reason", "")
        else:
            errors.append(f"{section}.{key}: entry must be string or object, got {type(entry).__name__}")
            continue

        if not path:
            errors.append(f"{section}.{key}: entry missing path")
            continue

        if path in seen:
            errors.append(f"{section}.{key}: duplicate path {path}")
        seen.add(path)

        full = root / path
        if not full.exists():
            errors.append(f"{section}.{key}: path does not exist: {path}")
            continue

        if key == "allow_paths":
            if len(reason.strip()) < REQUIRED_REASON_LENGTH:
                errors.append(f"{section}.{key}: path {path} needs a specific reason")

            expected_rules = entry.get("expected_rules", [])
            if not expected_rules:
                errors.append(f"{section}.{key}: path {path} needs expected_rules")
                continue

            for rule_id in expected_rules:
                if rule_id not in PLACEHOLDER_RULES:
                    errors.append(f"{section}.{key}: path {path} has unknown expected rule {rule_id}")
                    continue

                if not file_contains_rule(full, rule_id):
                    errors.append(
                        f"{section}.{key}: path {path} no longer contains expected rule {rule_id}; remove stale allowlist entry"
                    )

def reject_case_drift(root, policy_path, errors):
    requested = policy_path.name

    if requested != CANONICAL_POLICY:
        errors.append(f"policy filename must be exactly {CANONICAL_POLICY}, got {requested}")

    for sibling in root.iterdir():
        if sibling.name.lower() == CANONICAL_POLICY.lower() and sibling.name != CANONICAL_POLICY:
            errors.append(f"non-canonical policy filename exists: {sibling.name}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--policy", default=CANONICAL_POLICY)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    policy_path = Path(args.policy)

    errors = []
    reject_case_drift(root, policy_path, errors)

    if not policy_path.exists():
        errors.append(f"policy missing: {policy_path}")
    else:
        policy = json.loads(policy_path.read_text(encoding="utf-8"))

        for section in ["no_placeholders", "layout_core_freeze"]:
            data = policy.get(section, {})
            validate_entries(root, data.get("exclude_paths", []), section, "exclude_paths", errors)
            validate_entries(root, data.get("allow_paths", []), section, "allow_paths", errors)

    if errors:
        for error in errors:
            print(f"[FORTIFY-POLICY] {error}")
        raise SystemExit(1)

    print("[FORTIFY-POLICY] valid")

if __name__ == "__main__":
    main()