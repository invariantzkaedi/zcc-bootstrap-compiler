import json
import hashlib
import os
from jsonschema import Draft7Validator

repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
status_path = os.path.join(repo_root, "verification", "status.json")
schema_path = os.path.join(repo_root, "verification", "status.schema.json")
manifest_path = os.path.join(repo_root, "verification", "provenance_manifest.json")

# 1. Read files
with open(status_path, encoding='utf-8') as f:
    status = json.load(f)

with open(schema_path, encoding='utf-8') as f:
    schema = json.load(f)

# 2. Conformance validation
Draft7Validator.check_schema(schema)
errors = sorted(
    Draft7Validator(schema).iter_errors(status),
    key=lambda error: list(error.absolute_path)
)

if errors:
    for error in errors:
        location = '.'.join(str(part) for part in error.absolute_path) or '<root>'
        print(f'FAIL [{location}]: {error.message}')
    raise SystemExit(1)

print('Verification: STATUS.JSON CONFORMS TO STATUS.SCHEMA.JSON')

# 3. Compute SHA256 hashes
def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        while chunk := f.read(8192):
            h.update(chunk)
    return h.hexdigest()

status_sha = sha256_file(status_path)
schema_sha = sha256_file(schema_path)

print(f"status.json SHA256: {status_sha}")
print(f"status.schema.json SHA256: {schema_sha}")

# 4. Generate provenance manifest
manifest = {
  "experiment_id": status["experiment_id"],
  "verification_run_id": status["verification_run_id"],
  "json_syntax": "VERIFIED",
  "schema_conformance": "VERIFIED",
  "artifacts": [
    {
      "path": "verification/status.json",
      "sha256": status_sha
    },
    {
      "path": "verification/status.schema.json",
      "sha256": schema_sha
    }
  ]
}

with open(manifest_path, 'w', encoding='utf-8') as f:
    json.dump(manifest, f, indent=2)

print(f"Provenance manifest written to: {manifest_path}")
