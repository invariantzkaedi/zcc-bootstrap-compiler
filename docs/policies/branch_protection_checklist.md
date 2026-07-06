# Branch Protection Checklist (main)

- [ ] Require pull request before merging
- [ ] Require approvals: 1 (or 2 for Class A via CODEOWNERS/review policy)
- [ ] Dismiss stale approvals on new commits
- [ ] Require review from Code Owners
- [ ] Require conversation resolution
- [ ] Require status checks:
  - [ ] IR Opt Quality Gate (Correctness + Perf) / correctness
  - [ ] IR Opt Quality Gate (Correctness + Perf) / perf
- [ ] Require branches up to date before merge
- [ ] Restrict pushes to matching branches
- [ ] Do not allow force pushes
- [ ] Do not allow deletions
- [ ] Disable admin bypass (preferred)
