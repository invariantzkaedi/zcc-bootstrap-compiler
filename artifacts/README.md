# Artifacts Directory Guide

This directory stores machine-generated verification outputs from local runs and CI.

## Common files

- `failure_<seed>.json`  
  Structured failure record (schema: `schemas/qec_failure_schema.json`).

- `failure_<seed>.min.json`  
  Minimized witness preserving failure signature.

- `repro_<seed>.sh`  
  One-command reproduction script for corresponding failure.

- `index.json`  
  Run-level index with signature hashes and metadata.

- `summary.md`  
  Human-readable run summary (also posted to GitHub Actions summary UI).

- `determinism_hashes.json`  
  Hash comparison output for repeated deterministic runs.

- `golden_semantic_diff.json`  
  Semantic explanation of expected vs actual golden mismatches.

- `mutation_report.json`  
  Mutation outcomes and kill-rate metrics.

- `perf_timeseries.json`  
  Performance timing trends and regression flags (if enabled).

## Notes
- Artifacts are uploaded by CI with `if: always()`.
- Artifact JSON files must validate against schemas before upload.
- Do not hand-edit generated artifacts unless explicitly debugging tooling.
