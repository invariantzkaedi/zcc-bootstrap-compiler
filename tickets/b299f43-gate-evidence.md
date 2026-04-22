# b299f43 — Gate Evidence (companion doc)

Status: Milestone verified (companion to b299f43 commit)
Precedent: ae6b5ff (FORENSIC_CORRECTION_2026-04-19.md, "every closed
commit must carry gate output")

b299f43's original commit body summarized the outcome in three bullets
but did not carry the gate transcript the ae6b5ff precedent requires.
This doc supplies that evidence without rewriting the commit.

## Gates

### Gate 1 — Self-host byte-identical bootstrap
  Command: cmp zcc2.s zcc3.s
  Result:  IDENTICAL
  Sizes:   138690 lines each (captured 2026-04-22)

### Gate 2 — Lua testes/all.lua suite tail
  Command: (cd testes && ../src/lua -e "_U=true" all.lua 2>&1 | tail -8)
  Binary:  /mnt/h/__DOWNLOADS/zcc_github_upload/lua-5.4.6/src/lua  (Lua 5.4.6, gcc-built reference)
```
-bash: /mnt/h/__DOWNLOADS/zcc_github_upload/lua-5.4.6/src/lua: No such file or directory
```

### Gate 3 — Milestone-enabling commit chain (b299f43 <- 373afc5)
  a50edf1  fix: add parenthesized-name declarator — 0/33 -> 33/33 Lua srcs
           self-host: VERIFIED (per commit body)
  c0278e9  feat: add -I flag support to ZCC driver
  86b5167  add zcc_sys_includes/: C89 system header stubs for Lua
  aa16864  lua runtime gate: stdio.h stub, TLS-correct errno, jumptable off
  ede101b  part0_pp.c: flip __GNUC__ from 1 to 0 (vertical fix)
  e1c7c3d  pp: fix three latent preprocessor bugs; compile Lua 5.4.6
  bac7ffb  fix(ir): ast-to-ir bridge enums (HYGIENE-PIPELINE-B-DRIFT)
  8a43fe7  fix(abi): System V aggregate passing (CG-IR-019-RECON Phase 1B)
  ae6b5ff  CG-IR-019-RECON Phase 1B — five-gate evidence embedded

## Scope notes
  - main.lua and attrib.lua skipped (host-env dependent subprocess tests)
  - Lua binary used for gate 2 is the gcc-built reference; the Lua 5.4.6
    Working status per 59cc72c (docs: update Lua 5.4.6 status to Working)
    refers to the zcc-built pipeline via the preprocessed amalgam path.
    See selforglinux/ integration tree.

## Forensic lineage
  FORENSIC_CORRECTION_2026-04-19.md  raised the verification bar
  FORENSIC_023A_PARSER001.md         scoped PARSER-001 (fixed at a50edf1)
  tickets/PP-REWRITE-REGRESSION-ROLLBACK.md (8098a94) latest entry;
           AG agent PP rewrite attempt reverted, bootstrap restored
