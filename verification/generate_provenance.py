import hashlib
import json
import subprocess
import datetime
import os

def get_sha256(filepath):
    h = hashlib.sha256()
    with open(filepath, 'rb') as f:
        while chunk := f.read(8192):
            h.update(chunk)
    return h.hexdigest()

def run_cmd(cmd_list):
    return subprocess.check_output(cmd_list, shell=False).decode('utf-8').strip()

def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo_root)

    try:
        commit = run_cmd(["git", "rev-parse", "HEAD"])
        tree = run_cmd(["git", "rev-parse", "HEAD^{tree}"])
        
        dirty_status = run_cmd(["git", "status", "--porcelain"])
        dirty = bool(dirty_status)
    except Exception as e:
        print(f"Error getting git info: {e}")
        commit = "unknown"
        tree = "unknown"
        dirty = True

    artifacts_to_hash = [
        "verification/status.json",
        "verification/status.schema.json",
        "verification/rule_inventory.md",
        "verification/acceptance_tests.json"
    ]
    
    artifacts = []
    for path in artifacts_to_hash:
        if os.path.exists(path):
            artifacts.append({
                "path": path,
                "sha256": get_sha256(path)
            })

    now = datetime.datetime.now(datetime.timezone.utc)
    manifest = {
        "experiment_id": "EXP-2026-07-20-0001",
        "verification_run_id": f"VRF-{now.strftime('%Y%m%d%H%M%S')}",
        "timestamp": now.isoformat(),
        "builder": {
            "id": "https://github.com/zkaedi/zcc/actions/workflows/quantum-verification.yml"
        },
        "git": {
            "commit": commit,
            "tree": tree,
            "dirty": dirty
        },
        "artifacts": artifacts
    }

    manifest_path = "verification/provenance_manifest.json"
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    print(f"Generated {manifest_path} successfully.")

if __name__ == "__main__":
    main()
