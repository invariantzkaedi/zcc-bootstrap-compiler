import os
import sys
import json
import hashlib
from pathlib import Path

def generate_manifest():
    bundle_dir = Path("release_artifacts")
    if not bundle_dir.exists():
        os.makedirs(bundle_dir, exist_ok=True)
        
    artifacts = []
    
    # Hash everything in release_artifacts except manifest.json
    for p in bundle_dir.glob("*"):
        if p.name == "manifest.json":
            continue
        if p.is_file():
            with open(p, "rb") as f:
                file_hash = f"sha256:{hashlib.sha256(f.read()).hexdigest()}"
            artifacts.append({
                "path": f"release_artifacts/{p.name}",
                "sha256": file_hash
            })
            
    # Sort for deterministic bundle hashing
    artifacts.sort(key=lambda x: x["path"])
    
    manifest = {
        "version": 1,
        "bundle_hash": "pending",
        "excluded_from_bundle_hash": [
            "release_artifacts/manifest.json"
        ],
        "artifacts": artifacts
    }
    
    # Generate the bundle hash
    manifest_bytes = json.dumps(manifest["artifacts"], sort_keys=True).encode("utf-8")
    manifest["bundle_hash"] = f"sha256:{hashlib.sha256(manifest_bytes).hexdigest()}"
    
    with open(bundle_dir / "manifest.json", "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        
    print(f"[MANIFEST] Generated sealed hash manifest. Bundle hash: {manifest['bundle_hash']}")

if __name__ == "__main__":
    generate_manifest()
