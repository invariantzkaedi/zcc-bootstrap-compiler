# Full Sovereign Pipeline Hardening Verdict

| Gate | Status | Command | Result |
|---|---|---|---|
| Gate 1 (Self-Host Identity) | PASS | `cmp zcc2.s zcc3.s` | Byte-identical, exit 0 (`7afc1dc0bed0e049a895cf06386fcc00`) |
| Gate 2 (Inter-operability) | PASS | Bidirectional GCC/ZCC linking | Both directions pass assert exit 0 (`res=2110`) |
| Gate 3 (Corpus Regression) | PASS | `bash tests/test_corpus.sh` | 462/462 PASS (100.0% clean) |
| Gate 4 (Target Harnesses) | PASS | `verify_ir_backend.sh`, miniz, tcc, STARK | All target benchmarks and ZK STARK proofs pass exit 0 |
| Gate 5 (Evidence Freshness) | PASS | Re-run in same turn | All verification fresh and verified |
