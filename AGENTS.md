# Agent instructions for ZCC

## Read-Before-Touch (RBT) — FIRES ON EVERY SESSION, NON-NEGOTIABLE

Before you propose, draft, or implement ANY change to ANY file in
this repository — regardless of whether the user framed it as a
"bug fix," "feature," "hardening," "cleanup," or "quick question" —
you MUST complete Phase 0 below. No exceptions. No shortcuts. Not
even for "obvious" requests.

This rule exists because this repo has a 3-month forensic chain
that is authoritative over your training data, your intuition, and
the user's casual framing. The tree's own history is the source of
truth. See `FORENSIC_CORRECTION_2026-04-19.md` for why.

### Phase 0 — Forensic Chain Ingestion (mandatory, blocking)

Execute these in order, paste full output verbatim, then give the
verdict below before doing anything else.

0.1  List the forensic chain:
       git log --all --oneline --format='%h %ad %s' --date=short | head -30
       ls FORENSIC*.md tickets/*.md 2>/dev/null

0.2  Read IN FULL (do not summarize, do not skip):
       - The most recent FORENSIC_*.md file
       - The most recent 5 files in tickets/*.md

0.3  For every file you intend to touch, run:
       git log -p --follow <file> | head -300
     If the symptom you're about to fix has already been addressed
     in history, STOP AND REPORT. Do not proceed.

0.4  Run the bootstrap gate BEFORE any edits:
       bash ./run_selfhost.sh 2>&1 | tail -5
     Or equivalent:
       cat part1.c part0_pp.c part2.c part3.c ir.h ir_emit_dispatch.h \
           ir_bridge.h part4.c part5.c part6_arm.c ir.c ir_to_x86.c \
           ir_pass_manager.c regalloc.c ir_telemetry_stub.c > zcc.c && \
         gcc -O0 -w -fno-asynchronous-unwind-tables \
             -o zcc zcc.c compiler_passes.c compiler_passes_ir.c -lm && \
         ./zcc  -S -o zcc2.s zcc.c && \
         gcc -o zcc2 zcc2.s compiler_passes.c compiler_passes_ir.c -lm && \
         ./zcc2 -S -o zcc3.s zcc.c && \
         cmp zcc2.s zcc3.s && echo BASELINE_GREEN || echo BASELINE_RED

0.5  Report the Phase 0 verdict in this exact format:
       BASELINE:              GREEN | RED
       SYMPTOM-IN-HISTORY:    YES (commit <sha>) | NO
       FORENSIC-LATEST-SHA:   <sha of most recent FORENSIC_*.md commit>
       PROCEED:               YES | NO

     If SYMPTOM-IN-HISTORY is YES: STOP. Report only. Do not edit.
     If BASELINE is RED: STOP. Report only. Do not edit.
     If PROCEED is NO: STOP.

Only after PROCEED: YES is reported do you move to the appropriate
downstream protocol (debug, feature, hardening, etc.).

## Hard Prohibitions

* DO NOT rewrite `part0_pp.c`, `part3.c`, `part4.c`, or any PARTS
  file "wholesale." Surgical edits only; diff should be under 50
  lines unless the user explicitly authorizes more. The preprocessor
  alone has ~20 hours of landed forensic work. Rewriting it is
  almost always wrong. See `tickets/PP-REWRITE-REGRESSION-ROLLBACK.md`
  (commit 8098a94) for a case study.

* DO NOT claim a gate "passed" without pasting its raw output.
  "Self-host: VERIFIED" without a `cmp` result line is a phantom
  closure.

* DO NOT test against pre-preprocessed amalgams when the symptom
  is in from-source compilation. `selforglinux/*.c` files are
  preprocessed artifacts, not sources. Build Lua/SQLite/curl from
  their actual source trees via the project's own Makefile with
  ZCC as CC.

* DO NOT interpret "error messages changed" as "progress." A
  failure mode shifting from preprocessor-error to parser-error is
  only progress if the preprocessor output is independently valid
  (verify with `gcc -E | diff`).

* DO NOT commit with a subject line more confident than the evidence
  justifies. Per ae6b5ff precedent: every "closed" commit must carry
  raw gate output in the body.

* DO NOT `git reset --hard` or `git push --force` without explicit
  user authorization AND a reflog snapshot captured first.

## Required Gates (per ae6b5ff precedent)

Any commit claiming closure must include raw output for:

  Gate 1 — Self-host byte-identical: `cmp zcc2.s zcc3.s`
  Gate 2 — Inter-op (if codegen-touching):
            - zcc-lib + gcc-main
            - gcc-lib + zcc-main
            Both directions. Both pasted.
  Gate 3 — Corpus regression (if part0_pp.c or part3.c touched):
            Run the 797-function corpus, diff against baseline.
  Gate 4 — Target-specific (if symptom was Lua/SQLite/curl/DOOM):
            Re-run the specific harness from the tracked tree.
  Gate 5 — Prior gate re-verification: re-run the most recent
            tickets/*-gate-evidence.md's gates, confirm they still
            pass.

If any gate fails, STOP. Do not "fix" mid-gate unless that fix itself
gets documented with its own gate run (see FORENSIC_CORRECTION
Gate-2/Gate-3 mid-gate handling for the pattern).

## Stop Conditions (binary, non-negotiable)

STOP AND REPORT (do not continue, do not try harder):

* Phase 0.3 reveals the fix is already in history
* Phase 0.4 baseline is RED (tree was broken before you touched it)
* Any gate fails after your edits
* A measurement you need would require running a binary that doesn't
  exist on disk — rebuild it first, or stop and ask
* You find yourself writing "should work," "probably resolves," or
  "in theory this means" — that's a hypothesis, not evidence
* The user's memory or casual framing contradicts the tree. The tree
  wins. Report the contradiction, do not edit to match the framing.

## Commit Body Template (ae6b5ff precedent)
<scope-prefix>(<area>): <action + result>, no spin
Goal
<one sentence: what this work was supposed to produce>
Outcome
<one sentence: what is now true on disk. If outcome ≠ goal, THIS
is the commit message, not the goal.>
Gates

Gate 1: <measure> — <actual> via <command>
<paste raw output>

Gate 2: ...

Bugs caught mid-gate

<bug>: <what broke, what fix landed, file ref>
(or "None — gates ran clean on first attempt.")

Hygiene / deferred

HYGIENE-NNN: <description>

Forensic notes
<reframes, scope changes, "expected X got Y" moments worth preserving>

Show this draft to the user BEFORE running `git commit`.

---

# Downstream Protocols

## Debugging (ZCC Debug Protocol v1.0)

Fires only after Phase 0 completes with PROCEED: YES, AND the user's
request is a reported bug (crash, wrong output, self-host failure).

**Boot command** (paste into chat or use as system context):
System Command: Initialize ZCC Debug Protocol v1.0.

Current Status: [USER DESCRIBES BUG]
Constraint: Follow the 7-Phase Protocol (see docs/DEBUG_PROTOCOL.md).
Tools: Use scripts/stub_functions.py for Phase 5.5.
Invariants: Golden Rules + Semantic Truths (Phase 2 in docs).
Victory: Pass Phase 7 Squasher (CMP, 7-arg, promotion, void-check).
Action: Begin Phase 0 and Phase 1. Define bug statement and list 10+ failure modes.


**Phase 3 directive** (when adding instrumentation):
- Insert `ZCC:AST` at entry of `codegen_expr` and `codegen_stmt`.
- Insert `ZCC:EMIT` before every `emit_call()` and `emit_stack_adjust()`.
- Maintain `current_function_name` and `current_line` (or equivalent) for crumbs.

**Error-agent assist:** For ZCC bugs, the user can paste error/crumbs
or minimal repro into [ZKAEDI-CC Error Agent Team](https://hf.co/spaces/zkaedi/zkaedi-cc)
for triage ideas; then continue with the protocol below.

**Crash-path reduction (Phase 5.5):**
1. Run: `./zcc2 failing_input.c 2> debug.log`
2. Analyze: `tail -n 1 debug.log` → e.g. `ZCC:AST codegen_expr BINARY_ADD fn=my_func line=450`
3. Execute: `python scripts/stub_functions.py failing_input.c --keep main,my_func --out stubbed.c`
4. Test; recurse (if still crashes, bug in kept code; else un-stub and try another block).

## Build / run

- **Shell**: Run all terminal commands in **WSL** (default distro, e.g. Ubuntu).
  Repo root in WSL: `/mnt/d/discovered_algorithms_extracted/discovered_algorithms/zcc_repo`
  (adjust drive/path for host). Example:
  `wsl -e sh -c "cd /mnt/d/discovered_algorithms_extracted/discovered_algorithms/zcc_repo && gcc -o zcc zcc.c"`.
- **Build**: `gcc -o zcc zcc.c` (or concatenate part1.c..part5.c to zcc.c first).
  If `gcc` is missing: `sudo apt update && sudo apt install -y gcc make`.
- **Use ZCC**: In WSL: `./scripts/use_zcc.sh hello.c -o hello.s` then
  `gcc -o hello hello.s -lm && ./hello`. From PowerShell:
  `.\scripts\use_zcc.ps1 hello.c -o hello.s`. Or in WSL: `make zcc` then
  `./zcc <file.c> -o <out.s>`.
- **Self-host**: In WSL: `./run_selfhost.sh`. From PowerShell:
  `.\scripts\run_selfhost.ps1`.

---

# KB Seed List

These files are the canonical rules. Agents must read them if any
are relevant to the request, and MUST read them before forming
hypotheses about forensic work:

  FORENSIC_CORRECTION_2026-04-19.md       gate discipline; ae6b5ff precedent
  FORENSIC_023A_PARSER001.md              parser-001 scoping
  tickets/PP-REWRITE-REGRESSION-ROLLBACK.md   what NOT to do (8098a94)
  tickets/b299f43-gate-evidence.md        current evidence template
  ZCC_STATUS.md                           current milestone states
  ZCC_BATTLEPLAN_v1.0.3.md                active development plan
  BUGS.md                                 known-bug corpus
  docs/DEBUG_PROTOCOL.md                  7-phase debug protocol
