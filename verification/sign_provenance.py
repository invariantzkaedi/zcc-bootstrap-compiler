import os
import json
import base64
try:
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import padding, rsa
    from cryptography.hazmat.primitives import serialization
except ImportError:
    print("cryptography module not found. Please install it using 'python -m pip install cryptography'")
    import sys
    sys.exit(1)

def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo_root)
    
    private_key = rsa.generate_private_key(
        public_exponent=65537,
        key_size=2048,
    )
    
    public_key = private_key.public_key()
    
    with open("verification/public_key.pem", "wb") as f:
        f.write(public_key.public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        ))
        
    manifest_path = "verification/provenance_manifest.json"
    if not os.path.exists(manifest_path):
        print(f"Error: Manifest not found at {manifest_path}")
        import sys
        sys.exit(1)
        
    with open(manifest_path, "rb") as f:
        manifest_data = f.read()
        
    signature = private_key.sign(
        manifest_data,
        padding.PSS(
            mgf=padding.MGF1(hashes.SHA256()),
            salt_length=padding.PSS.MAX_LENGTH
        ),
        hashes.SHA256()
    )
    
    with open("verification/provenance_manifest.sig", "wb") as f:
        f.write(signature)
        
    print("Signed provenance_manifest.json with new local ephemeral key.")
    print("Created verification/provenance_manifest.sig")
    print("Created verification/public_key.pem")

if __name__ == "__main__":
    main()
