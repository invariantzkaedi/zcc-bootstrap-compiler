# ZKAEDI PRIME Model Governance & Cryptographic Signing Guide

**Version**: 1.0  
**Status**: ACTIVE  
**Audience**: Sovereign Swarm Operators, ML Engineers, Security Auditors

---

## 1. Overview

To guarantee the execution of untampered and authorized model weights/adapters in the sovereign ZKAEDI PRIME environment, the system utilizes a **Model Registry & Cryptographic Allow-list**. 

This system enforces two layers of assurance:
1. **Cryptographic Directory/File Hashing**: Computes SHA-256 digests for model assets, verifying file presence and identifying any modified, deleted, or untracked files (tamper detection).
2. **Ed25519 Digital Signatures**: Optionally signs the canonical registry database using Ed25519. This guarantees the authenticity of the allow-list, preventing unauthorized additions to the registry file.

---

## 2. Key Management & Setup

Operators must generate an Ed25519 keypair to manage registry signatures. Keep the private key highly restricted and secure.

### Generate Keypair
Run the command-line utility to generate a new keypair:
```bash
python3 zkaedi_model_registry.py keygen \
  --private-key /secure/keys/registry_private.pem \
  --public-key /secure/keys/registry_public.pem
```

*Note: Ensure both paths reside inside your validated safe bases (e.g., under `/mnt/h`).*

---

## 3. Operational CLI Workflows

The `zkaedi_model_registry.py` CLI provides subcommands for managing the allow-list database.

### 3.1 Registering a Model
To add a model directory (or file) to the allow-list and sign the registry:
```bash
python3 zkaedi_model_registry.py register \
  --name llama-3-8b-instruct \
  --path /mnt/h/models/llama-3-8b-instruct \
  --author "Meta" \
  --description "Baseline Llama 3 8B Instruct model" \
  --sign \
  --private-key /secure/keys/registry_private.pem
```

This will:
1. Recursively compute SHA-256 hashes for all files under the model path.
2. Calculate a combined hash for the model directory.
3. Write the entry to `zkaedi_model_registry.json`.
4. Sign the database and save the signature to `zkaedi_model_registry.json.sig`.

---

### 3.2 Listing Registered Models
To view all registered model configurations:
```bash
python3 zkaedi_model_registry.py list
```

---

### 3.3 Verifying Model Integrity
To verify that on-disk files match their registered state and confirm registry signature authenticity:
```bash
python3 zkaedi_model_registry.py verify \
  --path /mnt/h/models/llama-3-8b-instruct \
  --verify-sig \
  --public-key /secure/keys/registry_public.pem
```

If files are modified, deleted, or untracked, this command will print details of the integrity mismatch and exit with status code `1`.

---

### 3.4 Deregistering a Model
To remove a model from the registry allow-list:
```bash
python3 zkaedi_model_registry.py deregister \
  --name llama-3-8b-instruct \
  --sign \
  --private-key /secure/keys/registry_private.pem
```

---

## 4. Loader Integration (Python API)

To enforce model verification at runtime, enable the `enforce_allow_list` flag in the hardened loaders.

### Loading standard model:
```python
from zkaedi_security_utils import load_model_hardened

model, tokenizer = load_model_hardened(
    "/mnt/h/models/llama-3-8b-instruct",
    enforce_allow_list=True
)
```

### Loading GPTQ model:
```python
from zkaedi_security_utils import load_gptq_model_hardened

model, tokenizer = load_gptq_model_hardened(
    "/mnt/h/models/llama-3-8b-instruct-gptq",
    enforce_allow_list=True
)
```

*Note: By default, the Python loaders read the registry without signature checks. To configure signature verification at runtime, you can configure your registry settings accordingly.*

---

## 5. Security & Governance Best Practices

- **Air-Gapped Ingestion**: Verify model integrity and register hashes *before* exposing the model to the swarm nodes.
- **Fail-Closed Strategy**: Loader integration will fail-closed (raising a `ValueError` and preventing load) if the registry database signature is invalid or if the model files do not exactly match their registered hashes.
- **Secure Key Storage**: Swarm nodes should only contain the public key (`registry_public.pem`) to verify registry files. The private key (`registry_private.pem`) should never reside on runner nodes; it should be kept only on authorization servers or HSMs.
