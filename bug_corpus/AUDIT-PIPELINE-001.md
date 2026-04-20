---
id: AUDIT-PIPELINE-001
title: "Stage 3 Pipeline KeyError: 'committed'"
date: 2026-04-19
status: OPEN
severity: LOW (No Production Impact / Local Only)
component: zkaedi_audit_pipeline.py
---

## Description
The orchestrator script `zkaedi_audit_pipeline.py` deployed in the HuggingFace `zkaedi/leviathan-v2` repository crashes on line 265 when executing Stage 3 (Leviathan Direct Inference). The script attempts to log `result['committed']`, but the upstream `Leviathan.audit()` method never emits this key.

Because this line is wrapped in a high-level `try/except Exception as e:` block, the KeyError is swallowed. The `except` handler writes a generic `Stage 3 failed` message and then synthesizes a fallback report purely from Stage 2 Swarm risk scores. This fallback silently replaces the actual CNN output, meaning local users unknowingly receive Swarm-only audits while believing Leviathan ran successfully.

## Reproduction
1. Download `zkaedi_audit_pipeline.py` and `leviathan.py` from `zkaedi/leviathan-v2`.
2. Run pipeline in local or remote mode: `python zkaedi_audit_pipeline.py --preset defi_lending_pool`
3. Observe output: 
   ```text
   Stage 3 failed: 'committed'
   ```
4. Observe the final generated Audit Report still completes, incorrectly implying full success.

## Production Impact
**0 Users Affected.**
Extensive telemetry against the production Cloudflare deployment (`selforglinux` repository, `zkaedi-site` workers, and the `zkaedi-audits` D1 database) confirmed that `zkaedi_audit_pipeline.py` was never integrated into the live $15-per-audit billing path. The live deployment uses a legacy `slither/gemma` pipeline json structure.

## Remediation (Pending)
Remove `committed={result['committed']}` from line 265 in `zkaedi_audit_pipeline.py`. To be committed concurrently with the Option 3 rebuild and retrain of the Leviathan v2 architecture.
