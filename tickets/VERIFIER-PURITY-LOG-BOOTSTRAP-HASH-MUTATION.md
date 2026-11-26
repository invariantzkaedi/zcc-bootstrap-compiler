# VERIFIER-PURITY-LOG-BOOTSTRAP-HASH-MUTATION (DEFECT-T21A-VP01)

- **Status:** OPEN (Registered during Track 21A pre-commit verification; baseline untouched)
- **Invariant Violated:** `INV-T21A-013` (`INV-VERIFIER-PURITY`: $S_{\mathrm{before}} = S_{\mathrm{after}}$) & `ZCC-EF-0` (Verifier Separation)
- **Evidence Class:** `OBSERVED`
- **Target Component:** `scripts/log_bootstrap_hash.sh` (`lines 20, 59-61`)
- **Affected Reference Artifact:** `BOOTSTRAP_BASELINES.tsv`

---

## 1. Defect Description

Invoking `bash scripts/log_bootstrap_hash.sh --check` does **not** execute in a read-only verification mode. Instead, it mutates the protected reference ledger `BOOTSTRAP_BASELINES.tsv` in place by appending a new row whose `comment` column is literally `--check`.

---

## 2. Observed Evidence

During Track 21A pre-freeze verification (`task-8545`), running:
```bash
bash scripts/log_bootstrap_hash.sh --check
```
produced exit status `0` while modifying tracked `BOOTSTRAP_BASELINES.tsv`:
```diff
--- a/BOOTSTRAP_BASELINES.tsv
+++ b/BOOTSTRAP_BASELINES.tsv
@@ -282,3 +282,4 @@ REWRITE	37d34e76	...
+2026-11-26T02:34:51Z	8ff7da72	6b83f8d4c14bbdc925fc33be6af1979b	2213	gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0	Linux 6.18.58.1-microsoft-standard-WSL2 x86_64	--check
```

Root cause observed in `scripts/log_bootstrap_hash.sh`:
- Line 20 unconditionally assigns `COMMENT="${1:-}"` without parsing `--check` or read-only flags.
- Lines 60-61 unconditionally execute:
  ```python
  with open(ledger, "a", encoding="utf-8") as f:
      f.write(f"{date}\t{commit}\t{current_hash}\t{elisions}\t{cc_id}\t{uname}\t{comment}\n")
  ```
- Historical precedent in `git show 72ff668bedd6ca46329b9d79a68a8f6d0f4ad6fd` confirms this same side effect previously required manual pruning (`Transient log_bootstrap_hash.sh append row in BOOTSTRAP_BASELINES.tsv pruned after verifying EXIT=0`).

---

## 3. Remediation Scope & Track 21A Disposition

- **Immediate Track 21A Workspace Action:** Restored `BOOTSTRAP_BASELINES.tsv` (`git checkout -- BOOTSTRAP_BASELINES.tsv`, `git diff BOOTSTRAP_BASELINES.tsv` empty) without modifying `scripts/log_bootstrap_hash.sh` or altering protected compiler baselines during Track 21A freeze.
- **Standalone Verifier Guard (`INV-T21A-013`):** `tools/verify_track21a_release.py` now snapshots SHA-256 digests of `BOOTSTRAP_BASELINES.tsv`, `part0_pp.c..part6_ptx.c`, and unrelated workspace files before and after verification and fails closed if $S_{\mathrm{before}} \neq S_{\mathrm{after}}$.
- **Future Track Fix:** Add an explicit read-only `--check` mode (and/or temporary file copy comparison) to `scripts/log_bootstrap_hash.sh` in a dedicated toolchain hygiene commit outside Track 21A, with explicit `S_before == S_after` assertion.
