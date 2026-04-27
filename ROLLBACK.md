# Rollback Plan

This document details the exact rollback procedure if the Mutagenesis Daemon escapes containment or the sandbox attestation fails during a scheduled run.

## Rollback Triggers
Execute a rollback immediately if ANY of the following occur:
- Network is unexpectedly reachable during a run.
- Secret tokens/variables are detected in the sandbox.
- Policy hash becomes unapproved or baseline is violated.
- Compiler hash changes unexpectedly.
- A red-team fixture bypasses the fuzzer.
- Output cap exceeds without early-termination kill.
- Provenance/Manifest mismatch.
- Canary run fails.

## Rollback Action
1. **Stop Scheduled Sandbox Execution:** Immediately kill the `scheduled_sandbox_pilot.py` process.
2. **Global Disable:** Set `CHIMERA_GLOBAL_DISABLE=1` across the deployment fleet.
3. **Quarantine:** Move all artifacts from the latest run to the `quarantine/` directory.
4. **Restore Baseline:** Roll back `fortify_policy.json` and `Dockerfile.sandbox` to the last known-good commits matching the `policy_approvals.json` hashes.
5. **Rerun Drills:** Execute `incident_drills.py` and `chaos_drills.py` manually. Ensure all fail closed.
6. **Resume:** Require explicit human PR approval to unset `CHIMERA_GLOBAL_DISABLE` and resume pilot.
