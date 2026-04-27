# ZKAEDI Mutagenesis Sandbox (v7) - Independent Audit Package

## 1. Architecture Summary
The ZKAEDI Mutagenesis Sandbox is a continuous adversarial execution engine. It accepts hostile generative code (written in C) mutated by an AI orchestrator and attempts to compile and execute it using a mathematically limited ZCC compiler. The entire system operates under an absolute "Fail Closed" philosophy.

## 2. Threat Model
- **Primary Adversary:** The AI Mutagenesis generator attempting to execute network egress, fork-bomb the kernel, leak environment secrets, or rewrite the orchestrator policy.
- **Secondary Adversary:** Supply chain compromises of the ZCC compiler or the Docker base image.
- **Containment Strategy:** Network nullification, mathematical generation bounds, and cryptographically sealed evidence pipelines.

## 3. Sandbox Boundary
Execution occurs strictly within an isolated Docker container adhering to the following profile:
- Network disabled (`--network=none`)
- Read-only filesystem (`--read-only`)
- Process isolation (`--pids-limit=128`)
- Memory ceiling (`--memory=512m`)
- Privilege drop (`--cap-drop=ALL` & `--security-opt no-new-privileges`)

## 4. Execution Command
```bash
docker run --rm \
  --network=none \
  --read-only \
  --cap-drop=ALL \
  --security-opt no-new-privileges \
  --pids-limit=128 \
  --memory=512m \
  --cpus=1 \
  --tmpfs /tmp:rw,nosuid,size=64m \
  -e CHIMERA_SANDBOX_ONLY=1 \
  -v "$PWD/pilot_artifacts:/workspace/release_artifacts" \
  chimera-sandbox:latest \
  python scheduled_sandbox_pilot.py --once
```

## 5. System Policies & Governance
Refer to the following attached governance files for operational constraints:
- `SECURITY_BOUNDARY.md`
- `OPERATOR_RUNBOOK.md`
- `ROLLBACK.md`
- `EVIDENCE_RETENTION.md`
- `CAPABILITY_EXPANSION_REQUEST.md`

## 6. Cryptographic Baseline
The latest execution parameters are permanently locked to:
- **fortify_policy.json**
- **policy_approvals.json**
- **baseline_snapshot.json**

*(For live hashes and latest manifest validation, inspect the `pilot_artifacts/manifest.json` from the latest CI run).*
