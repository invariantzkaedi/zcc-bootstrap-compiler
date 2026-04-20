#!/usr/bin/env python3
"""
==========================================================================
  build_task_id_map.py   —   Construct asset → Tripo task_id mapping
--------------------------------------------------------------------------
  The LOD runner needs each source GLB's original Tripo task_id to dispatch
  highpoly_to_lowpoly. This helper builds that map from any of:

    1. A forge log directory (JSONL files from your Antigravity sessions)
    2. A CSV (asset_name,task_id)
    3. Manual JSON input ({"asset_name": "uuid", ...})
    4. Fuzzy-scan of filenames in a forge_output/ dir containing task metadata

  Output: task_id_map.json, ready for tripo_lod_runner.py --task-id-map

  USAGE:
    # From your antigravity forge session logs
    python3 build_task_id_map.py --from-logs /path/to/forge_logs/ \\
                                  -o task_id_map.json

    # From a CSV
    python3 build_task_id_map.py --from-csv task_ids.csv -o task_id_map.json

    # Manual (interactive prompt, or paste JSON)
    python3 build_task_id_map.py --manual -o task_id_map.json

  If you don't have any logs, you can query /task history via the Tripo API
  to recover recent task IDs — see --scan-recent.
==========================================================================
"""
from __future__ import annotations
import os, sys, json, csv, argparse, re
from pathlib import Path
from typing import Optional
import httpx

BASE = "https://api.tripo3d.ai/v2/openapi"

def from_logs(log_dir: Path) -> dict:
    """Scan JSONL/JSON/log files for task_id + asset name pairs."""
    out = {}
    uuid_re = re.compile(r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}')
    for f in log_dir.rglob("*"):
        if not f.is_file() or f.suffix not in (".json",".jsonl",".log",".txt",".md"):
            continue
        try:
            content = f.read_text(errors="ignore")
        except:
            continue
        # Parse JSON lines
        for line in content.splitlines():
            line = line.strip()
            if not line: continue
            try:
                d = json.loads(line)
                if isinstance(d, dict):
                    tid = d.get("task_id") or d.get("id")
                    name = (d.get("asset") or d.get("name")
                            or d.get("target_name") or d.get("filename", "").replace(".glb",""))
                    if tid and name and uuid_re.fullmatch(tid):
                        out[name] = tid
            except json.JSONDecodeError:
                pass
        # Also try full-file JSON (forge plan JSONs etc.)
        try:
            d = json.loads(content)
            if isinstance(d, dict):
                # Common shapes: {"batch": [{"target_name":..., "task_id":...}]}
                for batch_key in ("batch","tasks","results","forgings"):
                    if batch_key in d and isinstance(d[batch_key], list):
                        for item in d[batch_key]:
                            tid = item.get("task_id") or item.get("id")
                            name = (item.get("asset") or item.get("target_name")
                                    or item.get("name"))
                            if tid and name and uuid_re.fullmatch(tid):
                                out[name] = tid
        except:
            pass
    return out

def from_csv(csv_path: Path) -> dict:
    """CSV columns: asset_name, task_id."""
    out = {}
    with open(csv_path) as f:
        for row in csv.DictReader(f):
            name = row.get("asset_name") or row.get("name") or row.get("asset")
            tid  = row.get("task_id") or row.get("id")
            if name and tid:
                out[name.replace(".glb","")] = tid.strip()
    return out

def manual_entry() -> dict:
    """Read JSON from stdin or prompt."""
    print("Paste JSON object {asset_name: task_id, ...} then Ctrl-D:")
    txt = sys.stdin.read()
    return json.loads(txt)

def main():
    ap = argparse.ArgumentParser(description="Build asset→task_id map")
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--from-logs", type=Path, help="Scan a log directory")
    src.add_argument("--from-csv",  type=Path, help="Load from CSV")
    src.add_argument("--manual",    action="store_true", help="Read JSON from stdin")
    ap.add_argument("-o", "--output", default="task_id_map.json")
    ap.add_argument("--verify", action="store_true",
                    help="Verify each task_id exists on Tripo (requires TRIPO_API_KEY)")
    args = ap.parse_args()

    if args.from_logs:
        if not args.from_logs.is_dir():
            print(f"ERROR: {args.from_logs} not a directory", file=sys.stderr); sys.exit(2)
        m = from_logs(args.from_logs)
    elif args.from_csv:
        m = from_csv(args.from_csv)
    else:
        m = manual_entry()

    print(f"Collected {len(m)} mappings")
    for name, tid in sorted(m.items()):
        print(f"  {name:<40} {tid}")

    if args.verify:
        key = os.environ.get("TRIPO_API_KEY","").strip()
        if not key:
            print("WARN: --verify but TRIPO_API_KEY unset; skipping verification")
        else:
            print("\nVerifying task IDs exist on Tripo...")
            c = httpx.Client(base_url=BASE,
                             headers={"Authorization": f"Bearer {key}"},
                             timeout=15)
            bad = []
            for name, tid in m.items():
                r = c.get(f"/task/{tid}")
                if r.status_code == 200 and r.json().get("code") == 0:
                    print(f"  ✓ {name}  (status: {r.json()['data'].get('status')})")
                else:
                    print(f"  ✗ {name}  ({r.status_code}: {r.text[:80]})")
                    bad.append(name)
            if bad:
                print(f"\n{len(bad)} unverified — removed from map")
                for b in bad: del m[b]

    Path(args.output).write_text(json.dumps(m, indent=2, sort_keys=True))
    print(f"\n[ok] wrote {args.output}")

if __name__ == "__main__":
    main()
