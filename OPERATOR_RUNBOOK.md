# Operator Runbook

## Normal Scheduled Run
The `scheduled_sandbox_pilot.py` daemon executes routinely. Monitor `release_artifacts/ops_summary.json` for the `consecutive_successes` metric. No operator intervention is required as long as alerts are not firing.

## Canary Failure
If `sandbox_canary.py` fails:
1. The orchestrator will emit a critical alert.
2. The release gate halts.
3. Operator MUST investigate the failing component (typically network egress or secret leak).
4. Do NOT resume the pilot until the canary passes manually.

## Network Reachable
If `sandbox_attestation` detects a live network path:
1. Ensure `CHIMERA_ALLOW_LOCAL_NETWORK_DIAGNOSTIC` is NOT set.
2. If this happens in production, trigger **Rollback Procedure** immediately. The container isolation has failed.

## Secret Detected
If a secret is leaked into the environment:
1. Trigger **Rollback Procedure**.
2. Immediately rotate the leaked secret in the upstream provider.
3. Investigate the CI pipeline injection point.

## Compiler Hash Changed
If the `compiler_sha256` does not match the approved list:
1. Check if a recent capability expansion was approved but not seeded.
2. If unapproved, a supply-chain attack may have compromised the compiler binary.
3. Nuke the `zcc_repo/` local cache and trigger a clean clone.

## Policy Drift Detected
If `containment_regression_check.py` fails due to semantic limit expansion:
1. Check `CAPABILITY_EXPANSION_REQUEST.md` for an approved change.
2. If missing, revert `fortify_policy.json` to the baseline snapshot.

## Artifact Hash Mismatch
If `verify_release_artifacts.py` catches a tampered artifact:
1. An attacker or faulty process has modified the release bundle mid-flight.
2. Purge the `release_artifacts/` directory and re-run.

## Kill Switch Procedure
Set `CHIMERA_GLOBAL_DISABLE=1` in the master environment. All Sandbox entrypoints will immediately refuse to run and exit with `1`.

## Rollback Procedure
See `ROLLBACK.md`.

## Resume Procedure
1. Verify the root cause is patched.
2. Clear `CHIMERA_GLOBAL_DISABLE`.
3. Clear `CHIMERA_CAPABILITY_FREEZE` if resolving a freeze conflict.
4. Run `python scheduled_sandbox_pilot.py --once` to ensure clean state.
