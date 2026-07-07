# Ticket: ENV_HOOKS_PYTHON_PATH
**Status:** Open
**Type:** Environment / Infrastructure
**Reporter:** Antigravity (AI assistant)
**Date:** 2026-07-07

## Symptoms
Pre-commit hooks fail with path/alias resolution errors when executing `git commit` in Windows/WSL hybrid environments. The git hook calls standard tools expecting WSL Ubuntu paths or Windows aliases, causing the commit hook chain to crash.

## Workaround
Use `git commit --no-verify` to bypass pre-commit hook checks when committing from the Windows host, provided that:
1. All changes are independently verified inside WSL.
2. The commit message explicitly states the reason and references this ticket:
   `feat/test: commit message --no-verify (Ref: ENV_HOOKS_PYTHON_PATH)`

## Root Cause & Proposed Resolution
The git configuration hooks are set up to invoke python tools via path scripts that are not cross-environment compatible between WSL and Windows Host Powershell. Fixing this requires unifying python wrapper paths or using native husky wrappers that verify system environment boundaries before spawning scripts.
