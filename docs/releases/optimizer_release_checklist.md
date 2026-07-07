# 🔱 ZCC Optimizer — Release Checklist

This checklist defines the pre-release hardening process and release gating criteria for the ZCC optimizer pipelines (including InstCombine, SCCP, CFG Simplify, Loop Unrolling, and Inlining).

---

## 0) Release Identity
- [ ] **Release Name**: __________________________
- [ ] **Target Version**: _________________________
- [ ] **Commit SHA**: ____________________________
- [ ] **Build Timestamp (UTC)**: _________________
- [ ] **QA/Release Owner**: ______________________

---

## 1) Freeze Discipline (48h before tag)
- [ ] **Code Freeze**: No functional changes to optimizer passes or code generation patterns.
- [ ] **Rule Freeze**: InstCombine and SCCP rule sets are locked; no new patterns.
- [ ] **Test Freeze**: Benchmark suites, negative tests, and verification suites frozen.
- [ ] **Flag Verification**: Ensure newly added optimization options default to **OFF** (e.g., `--enable-unroll-mvp` and `--enable-inline-mvp`).

---

## 2) Core Correctness Gates (Required)
- [ ] **Selfhost Identity Gate**: Stage 2 and Stage 3 assembly outputs must be byte-identical:
  ```bash
  make selfhost
  cmp zcc2.s zcc3.s
  ```
- [ ] **Positive Verification Suite**: All 33 test-suite categories pass cleanly with no false failures:
  ```bash
  ./zcc_test_suite.sh --quick
  ```
- [ ] **Negative Verification Suite**: All negative test fixtures compile and trigger verifier checks as expected.
- [ ] **Fuzzing Integrity**: Verification pass over 100+ random transform fuzz runs runs successfully with zero warnings/errors.

---

## 3) Performance Gates
- [ ] **Compile-Time Overhead**: Geomean compilation time overhead across the benchmark suite must not exceed **2.0%** against the pinned main baseline.
- [ ] **Runtime Improvement**: Benchmark runs indicate neutral or positive geomean execution runtime change on active workloads.
- [ ] **No Flake Breaches**: Noise control benchmarks run with a measured flake rate under **1.0%**.

---

## 4) Documented Exceptions & Known Limitations
- [ ] **IR Hang Guards**: Functions containing massive switch-case tables (>16 arms) or exceeding register pressure constraints (such as `next_token` and `node_name`) are skipped under IR backend paths and marked as deferred.
- [ ] **Flag Gating**: All unrolling and inlining transformations must remain inactive unless CLI feature flags are explicitly set to **ON**.

---

## 5) Operational Sign-Off
- **Compiler Lead Signature**: ________________________  Date: ______________
- **Release Manager Signature**: ______________________  Date: ______________
