import os
import sys
import json
import hashlib
from pathlib import Path
from alerting import emit_alert

def verify_manifest():
    manifest_path = Path("release_artifacts/manifest.json")
    if not manifest_path.exists():
        emit_alert("critical", "manifest_missing", "verify_artifacts", "verification_blocked")
        sys.exit(1)
        
    with open(manifest_path, "r") as f:
        manifest = json.load(f)
        
    # Verify bundle hash integrity
    manifest_bytes = json.dumps(manifest["artifacts"], sort_keys=True).encode("utf-8")
    expected_bundle_hash = f"sha256:{hashlib.sha256(manifest_bytes).hexdigest()}"
    
    if manifest["bundle_hash"] != expected_bundle_hash:
        emit_alert("critical", "manifest_tampered", "verify_artifacts", "verification_blocked")
        sys.exit(1)
        
    # Verify all artifacts
    for item in manifest["artifacts"]:
        p = Path(item["path"])
        if not p.exists():
            emit_alert("critical", "artifact_missing", "verify_artifacts", f"missing_{p.name}")
            sys.exit(1)
            
        with open(p, "rb") as f:
            file_hash = f"sha256:{hashlib.sha256(f.read()).hexdigest()}"
            
        if file_hash != item["sha256"]:
            emit_alert("critical", "artifact_tampered", "verify_artifacts", f"tampered_{p.name}")
            sys.exit(1)
            
    print(f"[MANIFEST] Verified sealed hash manifest. Bundle hash: {manifest['bundle_hash']}")

if __name__ == "__main__":
    verify_manifest()
