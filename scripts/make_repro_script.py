#!/usr/bin/env python3
import json
import os
from pathlib import Path

ART_DIR = Path("artifacts")
ART_DIR.mkdir(parents=True, exist_ok=True)

for fp in sorted(ART_DIR.glob("failure_*.json")):
    try:
        obj = json.loads(fp.read_text(encoding='utf-8'))
        seed = obj.get("seed", "unknown")
        repro = obj.get("repro", {})
        cmd = repro.get("command", "pytest -q")
        env = repro.get("env", {})
        lines = ["#!/usr/bin/env bash", "set -euo pipefail"]
        for k, v in env.items():
            if v is None:
                v = ""
            lines.append(f'export {k}="{v}"')
        lines.append(cmd)
        out = ART_DIR / f"repro_{seed}.sh"
        out.write_text("\n".join(lines) + "\n", encoding='utf-8')
        os.chmod(out, 0o755)
        print(f"Wrote {out}")
    except Exception:
        pass
