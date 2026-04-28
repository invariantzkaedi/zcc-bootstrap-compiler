# Enable Secret Guardrails

Repository: `zkaedii/zcc`

## Required settings

1. Open `Settings -> Security -> Code security and analysis`.
2. Enable:
   - `Secret scanning`
   - `Push protection`
3. Keep both enabled for all branches.

## Verification

- Try pushing a synthetic test secret from a throwaway branch and confirm push is blocked.
- Confirm alerts are visible under `Security -> Secret scanning`.
