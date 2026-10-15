# Preserved Artifacts: CG-FLOAT-RETURN-WIDEN-001

| Artifact Description | Path | Checksum (MD5 / SHA-256) |
|---|---|---|
| Stage 2 Self-Host Assembly | `evidence/zcc-omega-1792022505-1509/stage2.s` | `8b4b28bc4cfbd9ff867ba66f5931fb68`<br>`ba67813def6f9177fe1985a67528b601233e124c49c67ca774138e5ae0c3bce9` |
| Stage 3 Self-Host Assembly | `evidence/zcc-omega-1792022505-1509/stage3.s` | `8b4b28bc4cfbd9ff867ba66f5931fb68`<br>`ba67813def6f9177fe1985a67528b601233e124c49c67ca774138e5ae0c3bce9` |
| Regression Probe Source | `tests/probe_float_return_widen.c` | `9b533cb42f132e4d0263f3cebbd3e7a9` |
| Probe Emitted Assembly | `/tmp/probe_fround.s` | Contains `cvtss2sd %xmm0, %xmm0` (verified) |
| QuickJS Emitted Assembly | `/tmp/quickjs_objs/quickjs.s` | `js_math_fround` contains `cvtss2sd %xmm0, %xmm0` |
| QuickJS Evaluation Tool | `/tmp/zcc_qjs_eval` | Executable (verified) |
| QuickJS Native Test Runner | `/tmp/test_quickjs_runner` | 15/15 tests passing (0 failures) |
| Differential Oracle Ledger | `e:/__GROUPED_IMAGES/ABSTRACT/differential_metrics.json` | 100 tasks, 0 divergences |
| Forensic Ticket | `tickets/CG-FLOAT-RETURN-WIDEN-001.md` | Authoritative target record |
| Postmortem Analysis | `docs/postmortems/cg_float_return_widen_001.md` | Formal severe defect postmortem |
