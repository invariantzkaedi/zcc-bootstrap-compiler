# CG-IR-013 Verdict

| Gate | Status | Command | Result |
|---|---|---|---|
| Gate 1 (Self-Host Identity) | PASS | `cmp zcc2.s zcc3.s` | Byte-identical, exit 0 (`53cd1862760748f654f6caa815d7fdb7`) |
| Gate 2 (Inter-operability) | PASS | Bidirectional GCC/ZCC linking | Both directions pass assert exit 0 (`res=2110`) |
| Gate 3 (Corpus Regression) | PASS | `bash tests/test_corpus.sh` | 460/460 PASS (100.0% clean) |
| Gate 4 (Forced Spill Test) | PASS | `ZCC_DEBUG_LSCAN=1 ./zcc tests/test_ir_spill_forced.c` | Active eviction spilling verified, execution result 2110 |
| Gate 5 (Freshness) | PASS | Full rebuild & re-verification | Verified in same turn |
