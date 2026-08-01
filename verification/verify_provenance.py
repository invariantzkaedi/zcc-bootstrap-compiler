import json
import hashlib
import os
import sys
from jsonschema import Draft7Validator

def get_sha256(filepath):
    h = hashlib.sha256()
    with open(filepath, 'rb') as f:
        while chunk := f.read(8192):
            h.update(chunk)
    return h.hexdigest()

def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo_root)

    schema_path = "verification/provenance_manifest.schema.json"
    manifest_path = "verification/provenance_manifest.json"

    if not os.path.exists(schema_path):
        print(f"Error: Schema not found at {schema_path}")
        sys.exit(1)
        
    if not os.path.exists(manifest_path):
        print(f"Error: Manifest not found at {manifest_path}")
        sys.exit(1)

    with open(schema_path, "r", encoding="utf-8") as f:
        schema = json.load(f)
    
    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    print("Checking schema...")
    Draft7Validator.check_schema(schema)
    validator = Draft7Validator(schema)
    errors = sorted(validator.iter_errors(manifest), key=lambda e: e.path)
    if errors:
        print("Schema validation failed:")
        for error in errors:
            print(f" - {error.message}")
        sys.exit(1)
    
    print("Schema OK.")
    
    print("Verifying artifacts...")
    for artifact in manifest["artifacts"]:
        path = artifact["path"]
        expected_sha = artifact["sha256"]
        if not os.path.exists(path):
            print(f"Error: Missing file: {path}")
            sys.exit(1)
        actual_sha = get_sha256(path)
        if actual_sha != expected_sha:
            print(f"Error: Hash mismatch for {path}: expected {expected_sha}, got {actual_sha}")
            sys.exit(1)
        print(f" - {path} [OK]")

    print("Provenance verification SUCCESS.")

if __name__ == "__main__":
    main()
