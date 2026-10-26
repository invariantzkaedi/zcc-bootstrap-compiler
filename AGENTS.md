# Agent instructions for ZCC

## ZCC-EF-0 — Independent Verification Authority

**Classification:** Mandatory engineering invariant\
**Priority:** Release-blocking\
**Applies to:** All ZCC agents, compilers, build scripts, bootstrap gates, test harnesses, provenance ledgers, dashboards, CI pipelines, and verification tools.

### The absolute rule

**NO COMPONENT MAY CERTIFY ITS OWN CORRECTNESS USING EVIDENCE OR VERDICTS THAT IT CAN FABRICATE, MODIFY, REPLACE, OR CONTROL WITHOUT INDEPENDENT VERIFICATION.**

The system under test may produce evidence. It must never be the sole authority deciding whether that evidence proves success.

### Mandatory enforcement requirements

1. **Separate evidence production from verification authority.** A trusted verifier must operate outside the monitored process's write authority. A child process must not control its authoritative verdict, receipt, or signing credentials.
2. **Never trust success messages.** Printed markers such as `PASS`, `VERIFIED`, `BYTE-IDENTICAL`, or `BOOTSTRAP_GREEN` are telemetry, not proof. Exit code `0` alone is also insufficient to establish correctness.
3. **Verify actual artifacts and execution.** A trusted runner must invoke the approved executable, independently capture its termination status, inspect its generated outputs, validate required invariants, and produce the final verdict. Command-name matching, timestamps, hashes, and child-written ledgers alone cannot establish authentic execution.
4. **Fail closed by default.** Missing evidence, skipped tests, unexpected exit codes, timeouts, malformed receipts, interrupted runs, stale artifacts, inconsistent hashes, and verifier exceptions must produce a nonzero gate result. Unknown is never PASS.
5. **Prove the verifier can detect failure.** Every critical gate must include adversarial negative controls, at least one valid positive control, and mutation tests. A verifier that passes when its checks are disabled, bypassed, or replaced is defective.
6. **Preserve independent evidence lineage.** Every release verdict must link the source revision, exact command, environment, toolchain, inputs, execution record, artifact hashes, test results, and verifier version. The authoritative receipt must be created and protected by the trusted verification process.
7. **Treat dashboards as observers, never authorities.** ZCC Quest and other interfaces may display a trusted verification result but must not invent certification from progress, stdout, file existence, or inferred success.
8. **Require independent review of the verification infrastructure.** No verification system may achieve release approval solely through its own tests or self-reported results.

### Mandatory release decision

**PASS** is permitted only when the trusted verification system has independently executed the required checks and validated the complete evidence.

**FAIL** is required when any mandatory invariant is violated.

**UNVERIFIED** is required when sufficient independent evidence is unavailable.

Neither FAIL nor UNVERIFIED may be represented as success.

### Violation policy

Any false-positive certification, vacuous test pass, fabricated provenance acceptance, silent skip, or fail-open condition is a **release-blocking verification defect**.

The affected gate must be repaired, regression-tested, mutation-tested, and independently revalidated before release approval.

**Foundational invariant: Evidence producers do not grant themselves verification authority.**

### Permanent Operating Instruction for Every Agent

> Before reporting PASS, attempt to make the verifier report PASS without the required work actually succeeding. If the attempt works, the verifier is defective and certification is prohibited.

---

## ZCC-EF-7 — Evidence-Calibrated Verification, Bounded Closure & Reopening Authority

**Version:** 1.0.0\
**Effective date:** 2026-10-26\
**Status:** Normative\
**Authority:** All ZCC engineering agents, specialist reviewers, verification harnesses, CI gates, release authorities, and milestone adjudicators.

### Governing Principle

**Be difficult to convince, but willing to be convinced.**

Verification SHALL resist premature acceptance and unjustified perpetual rejection.

A demonstrated defect SHALL be acknowledged. A falsified suspicion SHALL be retired within the tested scope. A successful experiment SHALL receive the verdict its evidence warrants.

### §1 — Evidence Classification and Provenance

Every material technical proposition SHALL carry an evidence classification:

- **OBSERVED:** Directly inspected source, artifacts, or execution by the reporting agent.
- **REPORTED:** Evidence supplied by a user, external system, or another agent but not independently reproduced.
- **INFERRED:** A conclusion supported by evidence but not directly established.
- **UNVERIFIED:** A proposition lacking sufficient supporting evidence.
- **PROPOSED:** A future experiment, remediation, or engineering action.

An agent MAY independently inspect a reported artifact without claiming to have reproduced the underlying execution.

Evidence provenance SHALL remain attached to findings throughout analysis, synthesis, publication, and release adjudication.

No agent SHALL silently promote REPORTED or INFERRED evidence into OBSERVED evidence.

### §2 — Correctness Oracles and Decisive Experiments

Before attributing a suspected failure to ZCC, the investigating agent SHALL identify:

1. The exact proposition or violated invariant.
2. The applicable language standard, ABI specification, API contract, or project requirement.
3. The smallest decisive experiment.
4. The independent oracles available and their limitations.
5. The inputs, configuration, environment, and evidence necessary to discriminate competing explanations.

Where a normative specification exists, it SHALL govern the correctness claim.

Agreement between GCC, Clang, and ZCC establishes implementation consensus for the tested behavior. It does not automatically establish standards conformance.

Undefined, unspecified, implementation-defined, and platform-dependent behavior SHALL be classified before differential results are interpreted.

When reference implementations cannot serve as trustworthy correctness oracles, agents SHALL use specification-derived invariants, independent algorithms, property-based tests, or appropriately qualified manual analysis.

#### Historical SQLite `Parse` investigation

User-provided execution transcripts reported identical `Parse` layout measurements under ZCC, GCC, and Clang:

| Measurement            | Result | Evidence class |
| ---------------------- | ------ | -------------- |
| `sizeof(Parse)`        | 424    | REPORTED       |
| `offsetof(zErrMsg)`    | 8      | REPORTED       |
| `offsetof(aTempReg)`   | 232    | REPORTED       |
| `offsetof(sLastToken)` | 288    | REPORTED       |
| Tail size              | 136    | INFERRED (424 − 288, from reported measurements) |

**Evidence class:** REPORTED EXECUTION.

**Disposition:** The suspected layout divergence is CLOSED for the tested definition and configuration.

The origin of the historical 312-byte measurement remains unestablished but is classified as **HISTORICAL EXPLANATION — OUTSIDE ACCEPTED SCOPE**, not an active compiler defect.

Recovering its original configuration is OPTIONAL forensic work unless new evidence makes it material to an accepted claim.

### §3 — Meta-Verification and Non-Vacuous Gates

Every verification harness, oracle, benchmark, CI gate, and evidence pipeline SHALL be treated as fallible production software.

Each gate SHALL document:

- The proposition it establishes.
- Its inputs and environment.
- Its PASS, FAIL, INCONCLUSIVE, and NOT RUN conditions.
- Its required evidence artifacts.
- Its potential silent-failure and fail-open paths.

Every executable acceptance gate SHALL have at least one retained **known-good control** and one retained **known-bad control**, unless its owner documents why such controls are inapplicable.

The known-good control demonstrates that the gate accepts a valid fixture.

The known-bad control demonstrates that the gate rejects a deliberately invalid fixture.

Where relevant, additional controls SHALL exercise compilation failure, linker failure, stale artifacts, output divergence, oracle disagreement, timeouts, and missing inputs.

Controls MUST reach the mechanism they claim to test.

**A passing test suite does not prove that its own verification infrastructure is universally correct.**

### §4 — Source Equivalence and Differential Testing

Every compiler comparison SHALL declare its source-equivalence class.

**Class I — Identical Source**

The candidate and reference compilers receive byte-identical source artifacts, subject to explicitly documented compiler options and environmental differences.

**Class II — Preprocessed Equivalent**

The compared translation units derive from a common source through documented preprocessing, with relevant configuration and preprocessing differences disclosed.

**Class III — Adapted Source**

One implementation uses compatibility transformations, alternate declarations, rewritten constructs, or source-level shims.

A Class III result SHALL NOT be presented as Class I compiler equivalence.

Successful adapted-source integration remains valid evidence of the capability actually tested.

Compiler agreement SHALL NOT supersede a normative specification when standards conformance is the claim.

### §5 — Evidence Tiers and Binding Claim Requirements

Evidence requirements SHALL be assigned according to the claim's scope and consequence **before the result is known**. The assignment timestamp SHALL be recorded as `evidence_tier_assigned_at` and SHALL predate test execution.

#### E1 — Executed Test or Scoped Defect Closure

Applies to individual test outcomes, targeted regressions, and bug closures within a specified configuration.

Requires:

- Identified source and test inputs.
- Execution command or equivalent procedure.
- Explicit expected and observed outcomes.
- Sufficient evidence to support the scoped verdict.
- Accurate provenance classification.

User-reported execution MAY satisfy E1 if explicitly labeled REPORTED.

#### E2 — Engineering Milestone Closure

Applies to milestones whose acceptance will be relied upon by subsequent engineering work, including bootstrap convergence, compiler compatibility campaigns, and application integration milestones.

Requires E1 plus:

- Predeclared acceptance contract.
- Inspectable test specifications or harness source.
- Recorded exit statuses and relevant outputs.
- Source revision and toolchain identity.
- Relevant artifact hashes.
- Defined exclusions and limitations.
- Verification Architect adjudication.

E2 MAY incorporate user-reported execution evidence, but its provenance SHALL remain explicit.

#### E3 — Independent Release or External Compatibility Certification

Applies to:

- Release-gating decisions.
- Published compatibility certifications offered for external reliance.
- Artifact acceptance by an independent release authority.
- Other claims explicitly designated as requiring independent certification.

Requires E2 plus:

- Independent reproduction or independently operated verification of the properties being certified.
- Authenticated artifact identity and traceable toolchain provenance.
- Retained machine-readable results.
- Relevant positive and negative controls.
- Enforcement of the declared release acceptance policy.

**Publication of an ordinary engineering status report does not automatically turn every reported test result into E3. An externally relied-upon certification claim does.**

#### §5.1 — Dependency Evidence Rule

A claim SHALL NOT receive a stronger effective evidence tier than its weakest material, unverified dependency.

The effective tier is the minimum supported tier among the material evidence dependencies needed to establish the claim.

A dependency MAY be upgraded only when its required proposition is independently re-established at the higher tier.

An E3 release authority MAY rely on E2 milestones as historical engineering context, but SHALL independently verify every property required by its E3 release policy. E3 applies to the exact certified proposition, not to every historical dependency in the project's development.

Evidence provenance and tier SHALL remain separately recorded.

**Aggregation, repetition, majority agreement, or institutional authority SHALL NOT silently upgrade an evidence tier.**

### §6 — Bounded Closure and Follow-Up Discipline

Every verification milestone SHALL define its acceptance contract before execution.

The contract SHALL include:

1. Target capability or artifact.
2. Acceptance conditions.
3. Correctness oracles.
4. Required evidence tier.
5. Observable PASS and FAIL conditions.
6. Exclusions and deferred capabilities.

Once the prescribed conditions are satisfied at the assigned tier, the Verification Architect SHALL close the milestone with an explicitly scoped verdict.

New desirable tests SHALL NOT become retroactive acceptance conditions.

Every proposed follow-up SHALL identify:

- The proposition under investigation.
- The existing acceptance condition it could falsify, if any.
- The smallest discriminating experiment.
- The disposition if the experiment succeeds.
- The disposition if the experiment fails.

**A proposal that does not specify what evidence would retire its suspicion is not a decisive verification experiment.**

A follow-up incapable of invalidating an existing acceptance condition SHALL default to FUTURE CAPABILITY, SEPARATE DEFECT, or HARNESS MAINTENANCE.

### §7 — Independent Specialist Analysis and Synthesis

Skeptic, Believer, Master Coder, and Bug Hunter SHALL investigate bounded questions independently.

Each finding SHALL include its claim, evidence class, source evidence, inference, uncertainty, severity, reproducer, proposed remediation, and confidence.

The Verification Architect SHALL reconcile findings while preserving their evidence lineage and classifications under §1.

Correlated assumptions SHALL be identified rather than counted as independent corroboration.

Disagreements SHALL remain explicit until resolved by evidence.

### §8 — Controlled Milestone Reopening

A closed milestone MAY be reopened only through a documented invalidation proposal.

The proposal SHALL identify:

1. The original acceptance condition allegedly violated.
2. The accepted evidence and its tier.
3. New contradictory evidence.
4. The reproducible defect or failure of the original acceptance mechanism.
5. The minimum affected scope.

#### Reopening authority

A specialist other than the original closure adjudicator SHALL review the invalidation proposal.

If such a reviewer is unavailable, the original Verification Architect MAY adjudicate only with a recorded, independent Skeptic review that explicitly addresses the contradictory evidence.

The independent reviewer SHALL not rely solely on the original adjudicator's summary.

An adequately supported contradiction SHALL place the affected condition into **REOPENED — UNDER INVESTIGATION**.

Unaffected acceptance conditions SHALL remain closed.

If the contradiction is not substantiated, the original closure SHALL stand, and the invalidation attempt SHALL be recorded with its evidence and reasoning.

Neither reopening nor refusal to reopen SHALL be based solely on institutional authority or reviewer preference.

### §9 — Verdicts and Evidence Closure

Every final adjudication SHALL report:

**Claim → Evidence Class → Evidence Tier → Evidence → Scope → Limitations → Verdict**

Permitted test verdicts:

- **PASS:** The predefined acceptance condition is satisfied at the required evidence tier.
- **FAIL:** A predefined acceptance condition is demonstrably violated.
- **INCONCLUSIVE:** Evidence is insufficient or conflicting.
- **NOT RUN:** The required test has not executed.

Every INCONCLUSIVE verdict SHALL name the missing or conflicting evidence and the smallest practical experiment capable of resolving it.

REOPENED — UNDER INVESTIGATION is a milestone lifecycle state, not a test verdict.

Every verdict SHALL record `governing_rule_version` (the ZCC-EF-7 version in force) and `evidence_tier_assigned_at` (per §5), with the latter predating the test execution.

### Final Invariant

**Verification authority is earned through evidence, not assertion, repetition, agreement, or position.**

The burden of proof SHALL apply symmetrically to claims of success and claims of failure.

A demonstrated success must be recognized.

A demonstrated defect must be repaired or explicitly accepted as a limitation.

A falsified suspicion must be retired.

A closed milestone must remain closed unless materially contradictory evidence justifies reopening.

**ZCC agents exist to establish engineering truth—not to manufacture confidence, manufacture doubt, or move the finish line.**

---

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
       make selfhost | tail -5
     Or equivalent:
       make selfhost
     Expect to see "SELF-HOST VERIFIED (assembly identical)" indicating BASELINE_GREEN.

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

* DO NOT reference or push to `izkaedi-ui` or `izkaedi-ui/ZCC`. The authoritative repository is `https://github.com/invariantzkaedi/zcc-bootstrap-compiler` under user `invariantzkaedi`. All git remotes, Colab links, workflows, documentation, and push targets MUST use `invariantzkaedi/zcc-bootstrap-compiler`.

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
- **Self-host**: In WSL: `make selfhost`. From PowerShell:
  `wsl -e bash -c "make selfhost"`.

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
  AGENTS.md § ZCC-EF-7                    evidence tiers E1/E2/E3; verdict schema; reopening authority

## Error-Learner Corpus Entries
* **E-LEARN-001 (Blender glTF Import/Export API):** Plausible API deprecation asserted from training data must be refuted by RNA poll (e.g. `import_scene.gltf` exists / `wm.gltf_import` is missing on Blender 4.0.2).
* **E-LEARN-002 (Blender Ops hasattr Check):** Calling `hasattr` on `bpy.ops` (or any sub-namespace like `bpy.ops.wm`) always returns `True` due to lazy attribute resolution. It is a non-discriminating probe; verify operator existence via direct polling (e.g. `hasattr(bpy.ops.wm, 'gltf_import')` is `True` even if the operator is missing).
* **E-LEARN-003 (Blender CLI Exit Code):** Blender headlessly (`blender -b`) catches Python exceptions but still exits with code `0`. Always pass `--python-exit-code 1` on the command line to ensure assertion failures or exceptions return a non-zero exit code to the shell.
* **E-LEARN-004 (Self-Swallowing Scopes):** Catching the same exception type raised to signal a test assertion failure makes the negative control self-swallowing. Scoping try-catch blocks to target only input validation and asserting exceptions outside the try block prevents this shadow pass.
* **E-LEARN-005 (All-Runs Disclosure Rule):** The agent must fully disclose all intermediate execution runs and command outputs during verification and fault-injection, including any quoting artifacts, stale binary executions, or failed attempts. Hiding or selectively omitting intermediate runs to present a cleaner narrative violates verification integrity.
* **E-LEARN-006 (D1-SYSPATH Resolution Discrepancy):** Script-file execution (`python3 path/to/script.py`) resolves `sys.path[0]` as the script's directory, whereas stdin execution (`python3 - < path/to/script.py`) resolves `sys.path[0]` as the active CWD. A validation suite mixing both styles can cause false green/red discrepancies on identical code state if relative module loading is path-dependent. Always align invocation syntax or explicitly inject the absolute root to `sys.path`.
* **E-LEARN-007 (Flipper mJS Implicit Coercion Ban):** In the Flipper Zero mJS runtime, implicit type conversion is strictly prohibited (e.g., `'Step: ' + step` or `'' + step` throws a fatal `implicit type conversion is prohibited` error). Always invoke `.toString()` explicitly on numerical operands (e.g., `'Step: ' + step.toString()`).
* **E-LEARN-008 (Flipper Math Object Scope):** The standard JavaScript `Math` global object is not injected by default in Flipper mJS scripts. Arithmetic operations requiring PRNG, trigonometry, or exponential functions must either `require('math')` or be implemented via lightweight fixed-point integer arithmetic (e.g. LCG PRNG with bitwise masks).
* **E-LEARN-009 (Flipper GUI ViewPort Constructor Limitation):** `gui.viewPort` does not export a `.make()` constructor in Unleashed mJS. Initializing custom viewports directly will trigger a runtime type error (`js 20 error`). Use dedicated high-level view models such as `let textBoxView = require('gui/text_box')` and switch views via `gui.viewDispatcher.switchTo(view)`.
* **E-LEARN-010 (Flipper Screen Lockout & CLI Collision):** If a GUI application or infinite loop is actively running on the Flipper OS display, running CLI script invocations (`js /ext/...`) will fail with `this command cannot be run while an application is open`. Always issue `loader close` or long-press the physical `Back` button before starting a new script session.
* **E-LEARN-011 (Flipper Notification Vibro Method Signature):** In Flipper mJS, `notification` does not expose a callable `.vibro(bool)` method; attempting to call `notify.vibro(true)` throws `calling non-callable` at runtime. Use `notify.blink('color', 'duration')` for haptics/LED signals, or trigger vibration via CLI or dedicated vibration sequences.
* **E-LEARN-012 (Compiler Shared Function Linkage / cc_memdup):** When introducing shared helper functions across compiler units (e.g. `cc_memdup`), declare the prototype globally in `part1.c` and define the non-static implementation in `part2.c`. Failing to do so causes isolated unit compilation checks (like `Gate IR-1` or CI part builds) to fail with undefined reference errors.
* **E-LEARN-013 (Designated Union Initializers Stride Integrity):** In `parse_initializer_list` (`part3.c`), when parsing designated initializers for union members (`.member = val`), the parser must capture `member_name` and resolve the specific field member offset and stride in `part4.c`. Dropping member names causes unions to default to the stride of their largest element, leading to memory misalignment and SIGSEGV.
* **E-LEARN-014 (Embedded Null Preservation in String Tokens):** String literal tokenization in `part1.c`/`part2.c`/`part3.c` must use length-aware allocation (`cc_memdup`) and copy operations (`memcpy`) rather than standard null-terminated C string functions (`strdup`/`strlen`). Bytecode compilers like QuickJS embed `\0` bytes inside string constant tables (`js_atom_init[]`), and null-truncation silently corrupts runtime atom tables.
* **E-LEARN-015 (Enum Bitfield Unsigned Shift Extent):** In SystemV ABI / C99, `TY_ENUM` bitfield extractions must be treated as unsigned (`shrq`) rather than signed arithmetic shifts (`sarq`). Performing signed shifts on enum bitfields causes values with the high bit set in their bitfield window to sign-extend to negative integers, triggering assertion aborts in runtime discriminators (such as QuickJS `js_closure2`).
* **E-LEARN-016 (IR Whitelist Function Name Collision):** Target applications frequently contain generic function names like `next_token()`. Never include unqualified common function names in `ir_whitelisted()` in `part4.c`. Doing so intercepts target functions and subjects them to IR emission rules, which drops `%rdx` for 16-byte SystemV struct return values (`JSValue`) and breaks floating-point and BigInt parsing.
* **E-LEARN-017 (CI Bootstrap Ledger Update Discipline):** When verified codegen changes alter the self-host binary assembly output (`zcc2.s`), the new MD5 hash must be audited across runners and explicitly appended to `BOOTSTRAP_BASELINES.tsv` for both WSL2 and Azure runner environments. Omitting this update causes `scripts/log_bootstrap_hash.sh` to fail with exit code 1 (`DRIFT DETECTED`) on GitHub Actions.
* **E-LEARN-018 (Heterogeneous Packed Bitfield Boundary Coalescing):** In `part3.c` / `part4.c`, when packing mixed-width adjacent bitfields (e.g. `int32_t` adjacent to `uint16_t` in `__attribute__((packed))`), do NOT prematurely close the container if the cumulative bit-span fits within the aligned word. Forcing boundary splits on type change inflates packed struct sizes (e.g. 9B vs GCC's 8B). Furthermore, bitfield loads/stores must span the full container word width rather than truncating to `member_size`, and signed bitfields in IR must be sign-extended via shift pairs (`shl`/`sar`) rather than zero-extended with `OP_BAND`.
* **E-LEARN-019 (GVN Union Punning Bitstream vs Numeric Cast):** In GVN Store-to-Load Forwarding (`compiler_passes.c`), when forwarding a floating-point store (e.g. `double`) to an integer load (e.g. `uint64_t`), the forwarded value must preserve the raw IEEE-754 bitstream (`movq %xmm, %rax`), NEVER lower to a numeric conversion instruction (`cvttsd2si %xmm, %rax`). Additionally, store invalidation must check bounding box intervals rather than exact offset equality (`store_offset == load_offset`) to properly invalidate overlapping sub-byte mutations.
* **E-LEARN-020 (Variable Length Array Stack Dynamic Reservation):** In `part3.c`, local variable frame allocation must not default to scalar sizes (8 bytes) for VLAs with runtime dimensions (`vtype->array_len == 0`). Allocating fixed small slots causes writes to indices $> 0$ to clobber adjacent stack parameters (such as `n` at `-8(%rbp)`) and corrupt the base pointer `%rbp`.
* **E-LEARN-021 (HTML Canvas Zero-Dimension Gradient Kill-Switch & Chunk Truncation):** In HTML5/WebGL dashboards with tabbed or collapsible containers (`display: none`), `getBoundingClientRect()` returns $0 \times 0$. Calculating radial gradients or canvas arcs with unconstrained radii (e.g. $\min(cx, cy) - 24 = -24$) throws an unhandled `IndexSizeError` `DOMException` that silently terminates `requestAnimationFrame(animationLoop)` on frame 0, permanently freezing all canvases into blank rectangles. Furthermore, editing large JS/HTML amalgamation files via chunk replacement without re-auditing neighboring symbol references risks wiping critical functions (`renderGridCanvas`, `initEvmMemoryGrid`) and triggering fatal `ReferenceError` crashes during DOM initialization.
* **E-LEARN-022 (Flipper mJS Exception Handling Prohibition):** The Cesanta mJS engine utilized in Flipper Zero firmware does NOT support JavaScript exception handling (`try`, `catch`, `throw`, `finally`). Including `try { ... } catch (e) { ... }` causes an immediate, fatal syntax parser error at compile time (`parse error at line NN: [try {`). All code paths in Flipper mJS scripts must use procedural state checks (`if / else`) and guard flags rather than exception blocks.
* **E-LEARN-023 (GNU Archive Non-ELF Member Silent Drop & Linker Undefined Reference Trap):** When testing corrupted or non-ELF object files bundled inside a static archive (`.a`) via `ar rcs`, GNU `ar` copies raw file bytes without ELF header inspection. During subsequent `ld` linking, GNU `ld` silently ignores non-ELF archive members rather than throwing an invalid file format error, leading to downstream undefined reference errors for symbols provided by that unit. Negative controls targeting archive corruption must be classified as missing-member / symbol resolution controls rather than object format rejection controls.
