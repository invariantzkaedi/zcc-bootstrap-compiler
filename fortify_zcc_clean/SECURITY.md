# Security Policy

## Reporting

Report compiler correctness, CI integrity, or attestation bypass issues privately to the maintainers.

## Relevant security-sensitive areas

- layout correctness
- `_Alignof`
- `_Static_assert`
- target ABI handling
- artifact hashing
- signature verification
- production trust policy
- CI placeholder gates

## Examples of security-relevant bugs

- `_Static_assert` silently ignored
- `_Alignof` returns size
- unknown target silently falls back
- production signing skipped
- attestation accepts wrong git commit
- attestation accepts unallowed signer
- bundle verifier trusts bundle policy instead of external policy