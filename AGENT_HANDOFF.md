# AGENT_HANDOFF.md — Closure Discipline for Agent-Delivered Work

This document defines how work delivered by AI coding agents (Cursor, Antigravity, GitHub Copilot, Claude Code, etc.) is validated before it enters `main`. It is the companion to `AGENTS.md`, which defines the **Read-Before-Touch (RBT)** protocol for what agents do *during* a session. AGENT_HANDOFF.md defines what humans (or other agents) do *after* a session, before merging.

It exists because of a specific failure mode that has bitten this repo and will keep biting it: **the working-tree closure**.

---

## The Failure Mode

An agent finishes work, runs tests locally, sees green, declares the work shipped, opens a PR. The PR's `commit` contains *some* of the work. The rest sits in the agent's working tree as files that are untracked, modified-but-not-staged, or in a directory the agent didn't think to `git add`.

The agent's tests passed because the on-disk state had everything needed. The committed state — what anyone else fetches when they clone the branch — does not.

**Anyone cloning the branch fresh hits errors immediately:**

- Linker errors on functions that were defined in an uncommitted source file
- `FileNotFoundError` on test fixtures that were generated in working-tree but never `git add`-ed
- Build steps that fail because a `Makefile` change was uncommitted
- Tests that fail because `.gitignore` was edited but not committed, leaving runtime artifacts untracked but expected

**It worked on the agent's machine. The pushed branch is broken. CI passes if and only if the workflow paths-filter doesn't trigger on the affected files.**

---

## Why Local Tests Aren't Enough

The agent's working tree silently *adds to* the committed state when tests run:

- `make` reads from disk, not from `git ls-tree HEAD`
- Python test harnesses read from `os.listdir`, not from the index
- Shell scripts find files via `*.txt` globs, not via `git ls-files`

There is no automatic mechanism that says "this file isn't in the commit, so the test should pretend it doesn't exist." Tests pass. PRs ship. Branches break.

The only check that catches this reliably is: **temporarily remove the working tree and re-run the build/tests against committed state alone.**

---

## The Three Mandatory Pre-Merge Checks

Any agent-delivered PR — defined as any PR opened by a bot account or any PR where the commit author is `copilot-swe-agent[bot]`, `cursor[bot]`, or similar — requires these three checks before merge.

### Check 1: Authorship Audit

```bash
git log --format='%h | %an <%ae> | %s' <main>..<branch>
```

Confirm authorship matches expectation. Bot accounts on commits that should be human-authored are a smell. Mixed authorship (some commits by bot, some by human) on a single PR is fine — that's the rescue pattern below — but pure-bot PRs need extra scrutiny.

### Check 2: Diff-vs-Message Audit

For each commit on the branch:

```bash
git show --stat <commit>      # how many lines does it actually touch?
git log -1 --format='%B' <commit>   # how does the message describe it?
```

A 3,200-line addition with a 6-line "harden strict modes" commit message is **narrative drift**. The message must honestly scope the change before merge. Reword if needed (`git commit --amend` on the agent's commit before opening the PR, or as a separate commit on top with the honest title).

**Heuristic:** if the diff stat surprises you ("wait, this commit added a *new compilation unit*?"), the message is materially shorter than the diff justifies, and the agent likely did not list everything they changed. That correlates strongly with working-tree closures.

### Check 3: Clean-Clone (or Stash) Test — MANDATORY

This is the single most important check. It costs ~60 seconds and catches the failure mode that no amount of local testing can surface.

**The fast version (in-place stash):**

```bash
# In the worktree where the agent's branch is checked out:
git stash --include-untracked         # hide everything not committed
make clean
make <whatever the build target is>   # MUST succeed against committed-only state
make <test target>                    # MUST pass against committed-only state
git stash pop                          # restore working tree
```

**The strong version (sister directory clone):**

```bash
git clone --branch <feature-branch> --single-branch <repo-url> /tmp/freshcheck
cd /tmp/freshcheck
make clean && make <build> && make <test>
# If any step fails, the agent's working tree was carrying state the commit doesn't.
```

If the build or tests fail under either version, the PR is **not mergeable as-is**. The agent's working tree was carrying state the commit doesn't have. See the rescue pattern below.

---

## The Rescue Pattern

When Check 3 reveals working-tree closure (linker errors, missing files, broken `make`), the rescue pattern is consistent:

1. **Do not amend the agent's commit.** The forensic record should preserve "the agent shipped X, the rescue added Y." Amending erases that signal.

2. **Branch off cleaned `main`** with a new name (`feature/<original-name>-merge` works well).

3. **Cherry-pick the agent's commit** onto the new branch. This will likely succeed cleanly even if the original branch had merge conflicts with main, because the cherry-pick is a fresh apply.

4. **Build and test.** The same failures from Check 3 will reproduce.

5. **Add the missing pieces as separate, named commits**, each with a forensic message:
   - `build(<area>): wire <new-file> into <build-system> (was uncommitted on agent branch)`
   - `test(<area>): add golden fixtures for <test> (were uncommitted on agent branch)`
   - `chore(repo): ignore <runtime-artifact-glob> (test outputs)`
   - Each commit message names what was missing and why.

6. **Re-run the clean-clone test** on the new branch. It must pass.

7. **Open the PR from the new branch.** The original agent branch survives as a backup and as evidence of what the agent actually shipped vs what landed on main. Do not delete the agent branch.

8. **Merge the rescue PR**, not the original agent PR.

This pattern was used to land PR #7 (Rust v1 frontend) on April 28, 2026, after Cursor's commit `0178ea6` failed Check 3. Three rescue commits followed: Makefile wiring, 17 golden test fixtures, runtime artifact gitignore. The committed state then passed all gates and merged cleanly.

---

## Required Information in Agent-Delivered PR Bodies

When an agent opens a PR, the PR body should answer (or be edited to answer) these questions before merge:

- **What was added?** Especially: any new compilation units, new build targets, new test fixtures, new Makefile rules, new gitignore rules.
- **What's the proof it works?** Specific commands and their expected output.
- **Has the clean-clone test been run?** If yes, by whom, against which commit hash, with what build/test commands. If no, the merger must run it before clicking merge.
- **What's the bootstrap baseline hash, if applicable?** For ZCC: `md5(zcc2.s)` after `make selfhost`. Compare against the dual baselines in `ZCC_STATUS.md` (C-only and C+Rust). Any drift requires investigation.

If the agent's PR body doesn't have these, the human reviewer adds them via the GitHub PR description before merging. The merge button does not get clicked until the PR body honestly describes what's landing.

---

## Workflow / CI Considerations

A path-filtered workflow (`on.pull_request.paths: [...]`) only runs when the PR's diff touches the listed paths. This means:

- A PR that introduces a NEW workflow + the files that workflow tests will NOT trigger that workflow on its own merge, because the workflow didn't exist on `main` yet.
- A PR that doesn't touch any of the workflow's filtered paths will not be checked by it, even if the workflow is "required" via branch protection.

**Implication:** branch protection rules requiring a workflow that uses `paths:` filtering can produce two failure modes:

1. **Required-but-not-triggered:** rule says "workflow X must pass" but the PR doesn't touch any path that triggers workflow X. PR is blocked forever waiting for a check that will never run.

2. **Required-but-not-yet-existent:** rule says "workflow X must pass" but workflow X doesn't exist on `main` yet. Same blocked state.

The fix in both cases is the same: temporarily remove the workflow from required checks, merge the PR that introduces or triggers the workflow, then re-add the workflow as required.

This was the pattern for PR #6 and PR #7 on April 28, 2026: `rust-front-smoke` was a required check before the workflow existed on main. Removed from required checks → merged PR #6 (cleanup) → merged PR #7 (rust frontend, which brought the workflow) → re-add `rust-front-smoke` as required for future PRs.

---

## Bootstrap Baseline Hashes — Drift Detectors

For repos that have deterministic build outputs (ZCC's `make selfhost` produces a deterministic `zcc2.s`), record baseline hashes in `ZCC_STATUS.md` (or equivalent). Any PR that affects build-relevant files MUST verify the post-merge hash matches the expected baseline, OR document why the hash legitimately changed.

Current ZCC baselines (as of April 28, 2026):

| Compiler config | md5(zcc2.s) | Locked at |
|-----------------|-------------|-----------|
| C-only (pre-rust) | `bbe72c8e677d4270bca32db48897e956` | main b69147d |
| C + Rust v1 | `a9d68cd228e2a34d508f849d7390ae9c` | main 15e2b2d |

Any future bootstrap that produces a different hash must be explained: either a new compilation unit was added (intentional, expected), or codegen drifted (regression, must be investigated). The hash is the first line of forensic defense against silent drift.

---

## When This Document Doesn't Apply

Routine commits by humans, on their own branches, that don't introduce new build/test infrastructure, are not subject to the three mandatory checks. The friction would outweigh the value.

This document applies specifically to:

- PRs opened by AI agents (Cursor, Antigravity, Copilot, Claude Code, etc.)
- PRs that introduce new compilation units or build targets
- PRs that introduce new required test fixtures or test infrastructure
- PRs from any source where the merger has not personally run the build from a fresh checkout in the current session

When in doubt, run Check 3 anyway. It's 60 seconds.

---

## Final Word

The agent's work is usually real. The agent's *delivery* is sometimes incomplete because the agent has no way to distinguish between "files I created" and "files I created and committed." The agent's tests pass because they read from disk, not from the index.

The clean-clone test is the bridge between "works for the agent" and "works for everyone." Run it. Always run it.

A merged PR that breaks `make clean && make` for the next person who clones is a far worse failure than a delayed merge. The first costs hours of rescue and shakes confidence in every prior agent-delivered PR. The second costs sixty seconds.

Pay the sixty seconds.
