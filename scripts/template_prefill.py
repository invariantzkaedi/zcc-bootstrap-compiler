#!/usr/bin/env python3
import argparse
from pathlib import Path

TOKENS = {
    "INCIDENT_ID": "",
    "UTC_NOW": "",
    "BRANCH": "",
    "HEAD_SHA": "",
    "SELFHOST_CMP_EXIT": "",
    "GATE_MATRIX": "",
    "DIFFSTAT_SNIPPET": "",
    "FAULT_INJECT_EXIT": "",
    "FAULT_RESTORE_EXIT": "",
    "FAULT_INJECTION_ENABLED": "",
    "FAULT_INJECTION_VERDICT": "",
}

def prefill_file(path: Path, token_values: dict):
    text = path.read_text(errors="replace")
    for k, v in token_values.items():
        text = text.replace(f"{{{{{k}}}}}", str(v))
    path.write_text(text)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--templates-dir", required=True)
    ap.add_argument("--incident-id", required=True)
    ap.add_argument("--utc-now", required=True)
    ap.add_argument("--branch", required=True)
    ap.add_argument("--head-sha", required=True)
    ap.add_argument("--selfhost-cmp-exit", required=True)
    ap.add_argument("--gate-matrix", default="")
    ap.add_argument("--diffstat-snippet", default="")
    ap.add_argument("--fault-inject-exit", default="")
    ap.add_argument("--fault-restore-exit", default="")
    ap.add_argument("--fault-injection-enabled", default="")
    ap.add_argument("--fault-injection-verdict", default="")
    args = ap.parse_args()

    tdir = Path(args.templates_dir)
    if not tdir.exists():
        return 0

    token_values = TOKENS.copy()
    token_values.update({
        "INCIDENT_ID": args.incident_id,
        "UTC_NOW": args.utc_now,
        "BRANCH": args.branch,
        "HEAD_SHA": args.head_sha,
        "SELFHOST_CMP_EXIT": args.selfhost_cmp_exit,
        "GATE_MATRIX": args.gate_matrix,
        "DIFFSTAT_SNIPPET": args.diffstat_snippet,
        "FAULT_INJECT_EXIT": args.fault_inject_exit,
        "FAULT_RESTORE_EXIT": args.fault_restore_exit,
        "FAULT_INJECTION_ENABLED": args.fault_injection_enabled,
        "FAULT_INJECTION_VERDICT": args.fault_injection_verdict,
    })

    for md in tdir.glob("*.md"):
        prefill_file(md, token_values)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
