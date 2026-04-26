# Fortify Failure Playbook

## `_Alignof` mismatch

Likely cause:

- `_Alignof` still routed through `sizeof`
- target ABI alignment table is wrong
- struct max alignment is wrong

Check:

```bash
grep -R "_Alignof\|alignof\|sizeof" -n src include
```

## Static assert unexpectedly passes

Likely cause:

- `_Static_assert` is still macro-erased
- const-eval treats unknown expressions as true
- diagnostic does not mark compile failure

Check:

```bash
grep -R "_Static_assert" -n .
```

## Array layout mismatch

Likely cause:

- array size uses alignment instead of element size
- multiplication overflow not checked

Expected invariant:

```text
array.size = element.size * count
array.align = element.align
```

## Fuzz mismatch

Artifacts should exist:

```text
artifacts/layout-fuzz-failures/*.c
artifacts/layout-fuzz-failures/*.reduced.c
artifacts/layout-fuzz-failures/*.oracle.txt
artifacts/layout-fuzz-failures/*.zcc.txt
artifacts/layout-fuzz-failures/*.manifest.json
```

Promote the reduced case:

```bash
python3 tools/promote_reduced_case.py \
  artifacts/layout-fuzz-failures/random_layout_0042.reduced.c \
  --out tests/layout/regression_random_layout_0042.c
```

## Placeholder gate fails

Run:

```bash
python3 tools/check_no_placeholders.py \
  --root . \
  --policy .zcc-fortify-policy.json \
  --report artifacts/no-placeholders.json
```

Do not bypass unless the file is an intentional fixture and policy includes a reason plus `expected_rules`.

## Signature verification fails

Check:

- signer matches `fortify-verify-policy.json`
- public key/fingerprint/identity is correct
- subject file was not mutated after signing
- signature path in bundle exists
- cosign certificate is hash-bound