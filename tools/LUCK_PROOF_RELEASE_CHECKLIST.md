# ZCC Visualizer — Luck-Proof Release Checklist (Legendary)

This checklist is the final pre-release hardening gate for the Cosmic Elephant / audio-reactive visualizer stack.  
Use it after static + behavioral QA pass, before tagging a production release.

---

## 0) Release Identity (must be filled)
- [ ] **Release Name**: __________________________
- [ ] **Target Version**: _________________________
- [ ] **Commit SHA**: ____________________________
- [ ] **Build Timestamp (UTC)**: _________________
- [ ] **QA Owner**: ______________________________
- [ ] **Secondary Verifier**: _____________________

---

## 1) Freeze Discipline (48h before release)
- [ ] Freeze runtime code changes (no feature additions).
- [ ] Freeze manifest schema and threshold constants.
- [ ] Freeze test corpus and baseline metrics.
- [ ] Record dependency versions (browser/runtime/toolchain).
- [ ] Enable “only bugfix commits” policy.

**Notes:**  
__________________________________________________  
__________________________________________________

---

## 2) Determinism & Reproducibility
Run the full behavioral suite for each seed:
- [ ] Seed `7`
- [ ] Seed `42`
- [ ] Seed `1337`
- [ ] Seed `9001`

For each run, capture:
- [ ] commit SHA
- [ ] OS version
- [ ] browser version
- [ ] GPU + driver
- [ ] display refresh rate
- [ ] audio source hash
- [ ] active calibration profile
- [ ] active accessibility profile

**Pass criteria:** identical pass/fail status and bounded metric variance across seeds.

---

## 3) Environment Matrix (must pass)
### Browser matrix
- [ ] Chrome (pinned stable)
- [ ] Edge (pinned stable)
- [ ] Firefox (pinned stable)
- [ ] Latest available stable browser versions

### Refresh-rate matrix
- [ ] 60 Hz
- [ ] 90 Hz
- [ ] 120 Hz
- [ ] 144 Hz (if available)

### Device tiers
- [ ] Low-end mobile / integrated GPU equivalent
- [ ] Mid-tier device
- [ ] High-end desktop / discrete GPU

---

## 4) Audio Corpus Matrix (must pass)
- [ ] Calm ambient track
- [ ] Festival/high-energy transient-heavy track
- [ ] Boss/heavy low-end cinematic track
- [ ] Lo-fi chill track
- [ ] Quiet near-silence track
- [ ] Clipped/distorted track
- [ ] Noisy microphone feed
- [ ] Pure silence / no input

**Pass criteria:** no crashes, no NaN/Inf, graceful fallback behavior in low-confidence/silence states.

---

## 5) Core Behavioral Stress Tests
### Interaction stress
- [ ] Rapid mood switching (20 switches / 10s) without instability.
- [ ] Macro slider fuzzing (random values @ 30 Hz for 60s).
- [ ] Resize stress (window resize + fullscreen toggle repeatedly).
- [ ] Background/foreground tab transitions recover sync correctly.
- [ ] Hot reload of manifest (valid) with no pop/jump explosion.

### Failure stress
- [ ] Corrupt manifest fails safely with readable error.
- [ ] Missing optional fields fallback correctly.
- [ ] Audio device hot-swap mid-session without crash.
- [ ] Simulated dropped audio frames handled with smoothing.
- [ ] Simulated network jitter/clock drift (if sync enabled).

---

## 6) Performance & Stability Tiers
### Short smoke (5 minutes)
- [ ] Stable FPS target met.
- [ ] CPU frame time within threshold.
- [ ] No constraint violations.

### Soak (30 minutes)
- [ ] No memory growth trend beyond acceptable slope.
- [ ] No accumulating sync drift.
- [ ] No escalating jitter score.

### Endurance (2 hours)
- [ ] No leak symptoms.
- [ ] No degradation ladder lock-in unless expected.
- [ ] No visual solver divergence.

**Record actuals:**  
- Avg FPS: __________  
- p95 frame time (ms): __________  
- Max memory (MB): __________  
- Sync error p95 (ms): __________

---

## 7) Safety & Constraint Guards
- [ ] Clamp logic verified post-blend.
- [ ] Blend precedence verified: `ovr > mul > add`.
- [ ] Joint/transform hard limits never exceeded.
- [ ] Emissive caps enforced in all modes.
- [ ] No generated `NaN`, `Infinity`, or `undefined` in animation outputs.
- [ ] Panic fallback verified (last valid pose + damped return).

---

## 8) Accessibility Compliance
Run independent QA passes for:
- [ ] Default profile
- [ ] Reduced motion profile
- [ ] Photosensitive-safe profile
- [ ] Calm-focus profile

For each:
- [ ] flash frequency cap respected
- [ ] emissive cap respected
- [ ] motion scale respected
- [ ] no inaccessible rapid flicker artifacts

---

## 9) Certification Evidence Artifacts (required)
Ensure these are produced and archived:

- [ ] `artifacts/qa/<timestamp>/results.json`
- [ ] `artifacts/qa/<timestamp>/summary.md`
- [ ] `artifacts/qa/<timestamp>/plots/*.png`
- [ ] `artifacts/qa/<timestamp>/environment.json`
- [ ] `artifacts/qa/<timestamp>/logs/*.log`
- [ ] `artifacts/qa/<timestamp>/manifest.snapshot.json`

Artifact integrity:
- [ ] SHA256 checksums generated
- [ ] Artifact hashes stored in release notes
- [ ] Artifacts uploaded to CI storage and retained

---

## 10) CI/CD Gate Conditions
- [ ] Behavioral QA workflow is required status check.
- [ ] Certification gate fails PR on threshold breach.
- [ ] Threshold config is locked on release branch.
- [ ] “No-verify” commits forbidden on release branch.
- [ ] Manual override requires two approvers + written reason.

---

## 11) Rollback Readiness
- [ ] Previous certified tag identified.
- [ ] Rollback manifest available at repo root.
- [ ] Rollback instructions tested within last 7 days.
- [ ] Rollback smoke test passes.

Rollback target tag: __________________________  
Rollback owner: _______________________________

---

## 12) Final Ritual Sequence (recommended)
Run sequence:
1. [ ] silence (20s)
2. [ ] calm ambient (30s)
3. [ ] festival drop burst (30s)
4. [ ] boss heavy section (30s)
5. [ ] return to silence (20s)

**Must remain true throughout:**
- [ ] No NaN/Inf
- [ ] No solver divergence
- [ ] No hard-pop transitions
- [ ] FPS within release threshold
- [ ] Sync error within threshold

---

## 13) Release Decision
- [ ] **CERTIFIED PASS** — all required sections passed.
- [ ] **CONDITIONAL PASS** — exceptions documented and approved.
- [ ] **FAIL** — release blocked.

Decision: __________________________  
Date (UTC): ________________________  
QA Owner Signature: ________________  
Secondary Verifier Signature: _______

---

## 14) Exception Log (if any)
| ID | Section | Description | Risk | Mitigation | Approved By |
|----|---------|-------------|------|------------|-------------|
|    |         |             |      |            |             |

---

## 15) Post-Release Monitoring (first 72h)
- [ ] Live telemetry dashboard watched.
- [ ] Error spike alerts configured.
- [ ] Sync drift alerts configured.
- [ ] Accessibility complaints triage path active.
- [ ] Hotfix owner on-call assigned.

On-call owner: ______________________  
Escalation channel: __________________

---

## Appendix A — Quick Command Hooks (customize)
```bash
# Static checks
python tools/verify_audio_reactive_creature.py

# Behavioral suite
python tools/run_visualizer_behavioral_qa.py --full --seeds 7,42,1337,9001

# Certification gate
python tools/certify_visualizer_release.py --artifacts artifacts/qa/latest

# Generate checksums
python tools/generate_artifact_checksums.py --dir artifacts/qa/latest
```

## Appendix B — Hard Stop Conditions (instant FAIL)
- Any NaN/Inf in runtime outputs
- Constraint hard-limit breach without safe recovery
- Missing certification artifacts
- CI gate bypass without dual approval
- Accessibility profile failure in photosensitive-safe mode
