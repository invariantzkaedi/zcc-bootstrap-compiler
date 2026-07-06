# QEC Merge Policy (Zero-Flake)

**QEC-VOP (QEC Verification Operations Platform): a deterministic, policy-enforced verification operations platform.**

Version: 1.0.0  
Applies to: `main` and protected release branches

## Required checks (blocking)
- `qec-fast-required`
- `qec-determinism-required`
- `preflight-fast-required`
- `preflight-determinism-required`

## Monitored checks (non-blocking initially)
- `qec-mutation-required`
- `preflight-mutation-required`

Promotion rule: monitored checks become required after 14 consecutive days stable and threshold-compliant.

## Pull request requirements
- At least 1 approval (2 recommended for high-risk changes).
- Branch up to date with base before merge.
- Stale approvals dismissed on new commits.
- Force-push and branch deletion disabled on protected branches.

## Determinism policy
Any required determinism failure blocks merge until:
1. reproducible artifact is attached,
2. failure classification is recorded,
3. fix is merged or explicit waiver is approved by code owners.

## Golden update policy
Any PR changing `tests/golden/**/*.json` must:
- set `ALLOW_GOLDEN_UPDATE=1`,
- include semantic diff artifact,
- include a “Golden Update Justification” section in PR description,
- receive QEC owner sign-off.

## Artifact policy
- Artifact schema validation must pass before upload.
- Artifact upload must run with `if: always()`.
- All failure artifacts must include repro command and seed.

## Mutation policy
- PR smoke mutation threshold target: `kill_rate >= 0.90`.
- Nightly target: `kill_rate >= 0.95`.
- Survivors require follow-up issues and test hardening.

## Waiver process
Waivers are rare and time-boxed (max 7 days), require:
- linked issue,
- owner approval,
- rollback/repair plan.
