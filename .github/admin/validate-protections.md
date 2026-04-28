# Validate GitHub Protections

## Preconditions

- Branch protection applied for `main`
- Required check includes `rust-front-smoke`
- Secret scanning + push protection enabled

## Validation 1: Failing PR is blocked

1. Create branch: `git checkout -b test/failing-rust-gate`
2. Introduce an intentional failing Rust test expectation (temporary change in `tests/rust/test_rust_frontend.py`).
3. Push and open PR to `main`.
4. Confirm:
   - `rust-front-smoke` fails
   - merge button is disabled

## Validation 2: Passing PR requires review

1. Revert failure and add a harmless docs change.
2. Push and open PR to `main`.
3. Confirm:
   - `rust-front-smoke` passes
   - merge still blocked until at least 1 approval
   - unresolved comments block merge

## Validation 3: Direct push to main blocked

1. Attempt `git push origin main`.
2. Confirm server rejects with protected-branch message.
