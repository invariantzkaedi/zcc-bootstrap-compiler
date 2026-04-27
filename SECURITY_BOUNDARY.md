# Security Boundary

## Core Rule
Generated C code is treated as strictly **hostile**. 
It is assumed to be actively attempting to escape containment, exhaust resources, and exfiltrate host data.

## What the Sandbox Guarantees
- Native C execution isolated from the host filesystem.
- Zero network egress capability during execution.
- Execution is strictly bounded by deterministic runtime, memory, and output byte limits.
- Obfuscation techniques (token pasting, recursive macros, include-next) are contained by preprocessor bounds checks.
- Artifact generation is completely reproducible and supply-chain pinned to specific compiler hashes.

## What the Sandbox Does Not Guarantee
- The sandbox does not guarantee the generated algorithm solves its mathematical objective.
- The sandbox does not protect the internal ephemeral scratch space from being temporarily overwritten by the payload (it is destroyed afterwards).
- Docker containment alone is not the sole boundary; the `wsl` path relies on WSL bounds, whereas the Docker path relies on explicit Docker capabilities dropping.

## ENERGY Is Not Cryptographic
The `ENERGY` output metric is a chaotic mathematical signal indicating optimization state. It is **not** a cryptographic signature and should never be used as a source of secure entropy or identity.

## Network Policy
- Network egress MUST be blocked.
- Any payload that attempts to open a socket or resolve DNS will fail due to environment isolation.
- The Release Gate explicitly verifies that network egress is impossible, failing closed if a route is found.

## Secret Policy
- Sensitive host environment variables (`TOKEN`, `AWS_`, `SECRET`, etc.) are actively purged and guarded against.
- The sandbox will instantly fail to initialize if a secret-like key is detected in its environment.

## Compiler and Supply Chain Policy
- The compiler binary must be pinned, hashed, and recorded on every execution.
- Every release artifact is linked to a specific software bill-of-materials (SBOM) and SLSA-style Provenance file.

## Release Gate Requirements
- The v4 Release Gate will **FAIL** if:
  - Output caps are missing.
  - The network is reachable.
  - A local network override was used.
  - Any Red-Team fixture escapes containment.
  - SBOM or Provenance hashes do not perfectly align across all reports.

## Incident Response
- If a payload bypasses the Fortify Gate but fails at runtime, this is considered a successful containment (Gate 3).
- If a payload successfully compiles, executes, and outputs a valid `ENERGY` without hitting bounds, but contains hostile logic, **this is a critical containment breach**.
- In the event of a critical breach:
  1. Revoke the Policy Hash.
  2. Isolate the offending artifact bundle.
  3. Expand the `fuzz_fortify_gate.py` payload dictionary with the new vector.
  4. Escalate the Fortify Pre-Processor Denylist.

## Known Limitations
- The `max_stdout_bytes` and `max_stderr_bytes` are enforced via streaming buffer intercepts. Exceedingly tight bounds combined with multi-threaded bursts could result in minor overshoot before the SIGKILL resolves.
- Recursive macros that exponentially expand might momentarily spike CPU usage before the preprocessor timeout bounds terminate them.
