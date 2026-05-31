#!/usr/bin/env python3
"""
🔱 ZCC MANIFEST SIGNER — sign_manifest.py
Signs the canonical hash manifest using the ZCC Sovereign Authority Ed25519 Private Key.
"""
import json
import sys
from pathlib import Path
from cryptography.hazmat.primitives.asymmetric import ed25519
from cryptography.hazmat.primitives import serialization

def sign_manifest():
    manifest_path = Path("governance/canonical_hash_manifest.json")
    private_key_path = Path("governance/authority_private_key.pem")
    
    if not manifest_path.exists():
        print(f"[ERROR] Manifest not found at {manifest_path}", file=sys.stderr)
        sys.exit(1)
    if not private_key_path.exists():
        print(f"[ERROR] Private key not found at {private_key_path}", file=sys.stderr)
        sys.exit(1)
        
    try:
        # Read manifest dict
        data = json.loads(manifest_path.read_text(encoding="utf-8"))
        
        # Parse / restructure to nested layout if flat
        if "manifest" not in data:
            manifest_dict = data
        else:
            manifest_dict = data["manifest"]
            
        # Clean any old signatures/keys out of the dict we are signing
        manifest_dict = {k: v for k, v in manifest_dict.items() if k not in ("signature", "public_key")}
        
        # Load private key
        with open(private_key_path, "rb") as f:
            private_key = serialization.load_pem_private_key(f.read(), password=None)
            
        # Canonicalize and sign
        canonical_bytes = json.dumps(manifest_dict, sort_keys=True, separators=(',', ':')).encode('utf-8')
        signature = private_key.sign(canonical_bytes)
        
        # Save nested structure
        output_data = {
            "manifest": manifest_dict,
            "signature": signature.hex()
        }
        
        manifest_path.write_text(json.dumps(output_data, indent=2), encoding="utf-8")
        print(f"🔱 Manifest successfully signed with Ed25519!")
        print(f"   Signature: {signature.hex()}")
        sys.exit(0)
    except Exception as e:
        print(f"[ERROR] Signing failed: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    sign_manifest()
