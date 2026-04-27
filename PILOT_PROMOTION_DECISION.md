# Pilot Promotion Decision

## Pilot Window
**Start Date:** 2026-04-27
**Duration:** 7 Days
**Frequency:** Every 12 Hours

## Evidence Reviewed
- `manifest.json` (SHA256 verified)
- `sandbox_attestation.json` (Network: None, Secrets: 0)
- `pilot_run_report.json`
- `chaos_drills.json` (5/5 successfully trapped)
- `containment_regression_check` (Capability Freeze enforced)

## Incidents Observed
None. Chaos drills proved the system fails closed gracefully.

## SLO Results
- [x] 20 consecutive successful sandbox pilot runs
- [x] 0 secret detections
- [x] 0 network reachable events
- [x] 0 unapproved policy drifts
- [x] 0 manifest verification failures
- [x] 0 accepted hostile fixtures
- [x] 0 quarantine events
- [x] 100% artifact bundles verified
- [x] Kill switch drill passed
- [x] Rollback drill passed

## Residual Risks
- The `zcc` compiler cache could be poisoned if the upstream Github repository is compromised before the Docker container is rebuilt.
- Zero-day container escape vulnerabilities in `docker run`.

## Approved Operating Mode
**Approved for recurring sandbox-only operation.**

## Explicitly Forbidden Modes
- Not approved for production-native generated-code execution.
- Not approved for deployment outside the hardened `--network=none` Docker profile.

## Approvers
- [Security Architect Signature]
- [Operations Lead Signature]

## Next Review Date
2026-05-27
