# Fortify ZCC Pack — Clean Edition

This is the audited, repaired version of the original `fortify_zcc_pack`.
The original shipped a mix of substantive infrastructure and phantom
closures: a no-placeholders scanner that fired on the pack itself, a
`compat-stubs.h` that defined `_Static_assert(x,y)` as nothing, three
orchestrator shells that didn't run the pipeline they claimed to run,
and four Python "tools" that were 1–8-line orphan fragments.

## What this pack contains

```
.zcc-fortify-policy.json      # patched: allowlists the rule-mirror file
fortify-verify-policy.example.json
Makefile.fortify
INTEGRATION.md                # how to wire compiler/ into your ZCC tree
README.md                     # this file

ci/                           # 13 PASS scripts + 3 rewritten orchestrators
tools/                        # 17 PASS tools + 3 rewritten ones
tests/                        # 13 real C test cases for layout & static_assert
schemas/                      # 3 real JSON Schemas (draft 2020-12)
docs/                         # threat model, rollout checklist, layout doc
compiler/                     # 4 well-formed C/H files for ZCC integration
.github/                      # workflow + PR & issue templates
artifacts/layout-fuzz/        # example fuzz manifest (reference)
```

The agent-instruction docs (`AGENTS.md`, `AGENT_TASK.md`,
`AGENT_DISCOVERY_TEMPLATE.md`, `AGENT_REVIEW_CHECKLIST.md`,
`ULTRA_AGENT_PROMPT.md`, `REVIEW_AGENT_PROMPT.md`,
`compact-agent-prompt.txt`, `FORTIFY_QUICKSTART.md`,
`FORTIFY_FAILURE_PLAYBOOK.md`, `FORTIFY_RUNBOOK.md`,
`SECURITY.md`, `CODEOWNERS`) are kept verbatim from the original.

## What was removed

| Path | Reason |
|---|---|
| `parse-static-assert-pseudocode.c` | Filename literally "pseudocode" — function-body fragment |
| `compat-stubs.h` | `#define _Static_assert(x,y)` — exact pattern AGENTS.md forbids as macro erasure |
| `oracle-bug.c` | 16-byte fragment, not compilable C |
| `diagnostics-example.c`, `alignof-valid.c`, `static-assert-real.c`, `alignof-sizeof-semantics.c` | All <200-byte expression/statement fragments |
| `zcc_layout.c` (4 lines) | Fragment of a function body referencing undefined helpers; declared API not defined |
| `diagnostic_codes.h` | Competing enum with names that don't match AGENTS.md spec |
| `layout.h` (257 bytes) | Stub header superseded by `zcc_layout.h` |

## What was rewritten

Three orchestrator shells and three Python tools were rewritten from
scratch because the originals were fragments or stubs:

| Path | Original | Now |
|---|---|---|
| `ci/fortify-layout.sh` | 3 lines, only ran a broken Python | Full 14-step dev pipeline |
| `ci/fortify-layout-production.sh` | 6 lines | Strict-gate production pipeline with policy enforcement |
| `ci/sign-fortify-manifest.sh` | Bare `cosign)` case body, no `case ... in`, no `esac` | Complete cosign/minisign/gpg signer with status writer |
| `tools/verify_attestation_bundle.py` | 8 lines, `NameError` on first call | Full verifier: structure, hashes, signature, policy gates |
| `tools/validate_attestation_bundle.py` | 8 lines, no main, no imports | Full structural validator |
| `tools/write_fortify_manifest.py` | 1 line of dict literal | Full immutable subject-manifest writer |

## What was patched

| Path | Patch |
|---|---|
| `.zcc-fortify-policy.json` | Added `tools/validate_fortify_policy.py` to `allow_paths` so the pack's own no-placeholders scanner stops failing on its own legitimate rule registry |

## Quickstart

```bash
chmod +x ci/*.sh
ci/fortify-layout.sh
```

This runs the full development-mode pipeline against itself:
preflight → no-placeholders → policy validate → core freeze
→ host-compiler oracle → hash → write subject manifest → validate
→ sign (skipped without `FORTIFY_SIGNER`) → write attestation bundle
→ validate bundle → re-verify evidence on disk → end-to-end verify.

The final artifact is `artifacts/fortify-attestation.bundle.json`.

## Production mode

```bash
export GITHUB_SHA="$(git rev-parse HEAD)"
export TARGET="x86_64-linux-gnu"
export FORTIFY_SIGNER="cosign"        # or minisign | gpg
export FORTIFY_REQUIRE_SIGNATURE=1
# plus signer-specific creds, e.g. FORTIFY_SECRET_KEY for minisign

ci/fortify-layout-production.sh
```

Production mode rejects:

- missing `GITHUB_SHA`
- missing `TARGET`
- missing `FORTIFY_SIGNER`
- `FORTIFY_REQUIRE_SIGNATURE != 1`
- missing `fortify-verify-policy.json`
- bundles whose `signing.status != signed`
- bundles whose `signer` is not in the verify-policy's `allowed_signers`
- bundles whose `subject.git.commit` doesn't match `GITHUB_SHA`
- bundles whose `subject.target` doesn't match `TARGET`

## Compiler integration

The C files under `compiler/` are well-formed but reference your ZCC tree's
`Type`, `Expr`, `SourceLoc`, and `zcc_const_eval`/`zcc_diag` APIs.
See `INTEGRATION.md` for what to do with each file and what the missing
piece (the real `zcc_layout.c`) needs to provide.
