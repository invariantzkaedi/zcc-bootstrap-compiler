# Capability Expansion Request

Before increasing any power, generation count, runtime capability, or memory limit of the Mutagenesis Sandbox, this document MUST be filled out and approved by a security owner.

## Requested Change
[Describe what limits are being raised (e.g. max_generations 3 -> 10, max_stdout 64kb -> 1mb)]

## Security Impact
[Describe what new attack surface is exposed by this capability expansion. How much more memory could an attacker consume? Can they DoS the orchestrator?]

## Policy Diff
[Paste the diff of `fortify_policy.json` and `policy_approvals.json`]

## New Fixtures Added
[List the new `.c` adversarial fixtures added to `tests/redteam/` to prove the new capability boundary holds]

## Rollback Plan
[Describe the immediate commands to revert this change if the canary crashes]

## Approval
[Sign-off by Security Owner]

## Post-Change Verification
[Provide the hash of the first successful `release_artifacts/manifest.json` after the change]
