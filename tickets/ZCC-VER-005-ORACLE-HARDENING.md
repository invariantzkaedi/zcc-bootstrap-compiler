# TICKET: ZCC-VER-005-ORACLE-HARDENING
**Subsystem:** Verification Architecture & Self-Host Oracle Lineage  
**Target:** ZCC Quest & Cryptographic Provenance Ledger  
**Author:** Antigravity / ZCC Systems Engineering  
**Date:** 2026-10-19/20  
**Status:** **RESOLVED & VERIFIED (Gate 1 Byte-Identical, Negative Controls Exit 0, 0 False Certifications)**  
**Target Commit:** `52f650c00885cedf4a8e02ed567655a2d4003d6d`  

---

## 1. Divergence Manifest & Forensic Root Cause (ZCC-VER-005)

### Reported Finding (Audit Round 3)
In Round 3 Audit (`ZCC_AUDIT_ROUND3_2026-10-20.md`), independent auditor discovered:
- **Probe A:** An arbitrary command producing two 2,048-byte zero-filled files (`dd if=/dev/zero of=zcc2.s bs=2048 count=1`) and printing the positive marker received a false `SELF-HOST VERIFIED — BYTE-IDENTICAL SEAL — exit 0`.
- **Probe B:** Pre-created identical files in the working directory before command start were accepted due to a `-3.0s` clock-skew tolerance in `verify_selfhost_oracle()`, receiving a false verification banner without compiling anything.
- **Correlated Evidence/Oracle Defect:** Monitored commands could write both child stdout and candidate files, allowing unprovenanced counterfeit artifacts to masquerade as authentic compiler outputs.

---

## 2. Architectural Remediation

1. **Monitored Command Intent Gate (`is_bootstrap_command`)**:
   - `verify_selfhost_oracle()` rejects arbitrary commands (`dd`, `printf`, `echo`, shell tricks).
   - Only recognized bootstrap targets (`gate.sh`, `make selfhost`, etc.) are permitted to trigger self-host verification.

2. **Strict Creation Monotonicity (Zero Negative Allowance)**:
   - Child process spawn timestamp (`spawn_wall = time.time()`) is captured immediately before `subprocess.Popen`.
   - All candidate files and evidence directories must have `mtime >= spawn_wall`. Pre-existing files are rejected.

3. **x86-64 Assembly Content & Structural Validation (`validate_assembly_file`)**:
   - Files containing NUL bytes (`\x00`) are immediately rejected.
   - Files below 100,000 bytes are rejected (ZCC compiler assembly is ~9.6 MB; counterfeit 2 KB payloads fail).
   - Enforces decodable UTF-8 text containing genuine x86-64 assembly directives: `.text`, `%rbp` / `%rsp`, `ret` / `retq`, `.globl`, `.cfi_startproc`.

4. **Cryptographic Provenance Ledger Receipt Verification**:
   - Inspects `evidence/zcc-omega-*/provenance.jsonl` (or `evidence/zcc-run-*/provenance.jsonl`) generated during the current run.
   - Verifies stage 2 and stage 3 records exist with `exit_code == 0`.
   - Verifies `stage2["assembly_sha256"] == stage3["assembly_sha256"]`.
   - Computes live SHA-256 of `zcc2.s` and asserts exact byte match against the cryptographic ledger hash.
   - Performs byte-by-byte comparison (`filecmp.cmp(s2, s3, shallow=False)`).

---

## 3. Verification Gates (Rule EF-1, EF-2, EF-3, EF-6)

### Gate 1 — Self-Host Identity (Mandatory)
- **Command:** `cmp zcc2.s zcc3.s`
- **Status:** **PASS** (Byte-identical, 9,612,815 bytes)
- **Hash:** MD5 `954f3bc2cde5d2f1470fa49d551ea4d0`

### Gate 2 — Negative Controls Regression Harness (Mandatory)
- **Harness:** `python3 ZCC_ORACLE_NEGATIVE_CONTROLS.py zcc_quest.py`
- **Pre-Patch Baseline (Rule EF-3):** **Exit 1 (FAIL)**
  ```text
  Executing ZCC Oracle Negative Controls against: /mnt/h/__DOWNLOADS/zcc_github_upload/zcc_quest.py
  --- [CONTROL 1 / PROBE A] Counterfeit zero-byte files with marker ---
  [FAIL] Probe A falsely certified counterfeit zero-byte files!
  --- [CONTROL 2 / PROBE B] Pre-existing identical files with marker ---
  [FAIL] Probe B falsely certified pre-existing files!

  ========================================================
  ✖ NEGATIVE CONTROLS FAILED (counterfeit accepted) ✖
  ========================================================
  ```
- **Post-Patch Verification:** **Exit 0 (PASS)**
  ```text
  Executing ZCC Oracle Negative Controls against: /mnt/h/__DOWNLOADS/zcc_github_upload/zcc_quest.py
  --- [CONTROL 1 / PROBE A] Counterfeit zero-byte files with marker ---
  [PASS] Probe A correctly rejected counterfeit files (no false certification).
  --- [CONTROL 2 / PROBE B] Pre-existing identical files with marker ---
  [PASS] Probe B correctly rejected pre-existing files.

  ========================================================
  ★ ALL NEGATIVE CONTROLS PASSED (0 false certifications) ★
  ========================================================
  ```

### Gate 3 — Baseline Process and Control Suite
- `--cmd "true"`: Exit 0, ordinary banner `★ COMMAND SUCCEEDED — exit 0 ★`, zero false verification (`PASS`).
- `--cmd "false"`: Exit 1, error banner `✖ BUILD FAILED — exit 1 ★` (`PASS`).
- `--cmd "sleep 5" --frames 2`: Exit 124 on frame-limit (`PASS`).

### Gate 4 — Provenance Ledger Verification Unit Tests
- `validate_assembly_file("zcc2.s")`: `True`
- `validate_assembly_file("evidence/.../stage2.s")`: `True`
- Authentic run verification: `True`
- Pre-existing artifact timestamp rejection: `False`
- Counterfeit command rejection (`dd if=/dev/zero`): `False`

---

## 4. Distribution Package Manifest

- **Git Commit:** `52f650c00885cedf4a8e02ed567655a2d4003d6d`
- **Git Bundle:** `zcc_repo.bundle` (contains all 74 refs including HEAD)
- **Audit Archive:** `zcc_bootstrap_compiler_audit_package.zip`
  - Member count: 4,444
  - Archive size: 155,408,456 bytes
  - SHA-256: `8cd3196504df30931acb1e31ba2646d232bd2e9bfa395b7a8d7b5492e261603e`
  - MD5: `15dcb3a0379db5b5d144e9e122acdaba`
  - Integrity: `ZipFile.testzip() == None` (PASS)
