import json
from pathlib import Path

ckpt_path = Path("tripo_lod.jsonl")
if not ckpt_path.exists():
    print("Checkpoint file not found.")
    exit(1)

lines = ckpt_path.read_text().splitlines()
new_lines = []
kept_count = 0
removed_count = 0

for line in lines:
    line = line.strip()
    if not line:
        continue
    try:
        data = json.loads(line)
        ts = data.get("ts", 0)
        # Only keep entries with timestamp from today's runs (>= 1780000000)
        if ts >= 1780000000:
            # Also filter out failed lod tasks from today if any, so they can be retried
            if data.get("kind") == "lod" and data.get("status") == "error":
                removed_count += 1
                continue
            new_lines.append(line)
            kept_count += 1
        else:
            removed_count += 1
    except Exception as e:
        new_lines.append(line)

ckpt_path.write_text("\n".join(new_lines) + "\n")
print(f"Done cleaning checkpoint! Kept {kept_count} lines, removed {removed_count} stale/failed lines.")
