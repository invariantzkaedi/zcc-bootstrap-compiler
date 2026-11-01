# Artifact Manifest & SHA-256 Checksums (Rule MV-9 — Hardened v1.1)

| Artifact Path | SHA-256 Checksum | Role |
| :--- | :--- | :--- |
| `zcc2.s` | `d3d5fcf2043ff16cf17dee6641dfa216de2c6047fb0c20a0c4e78c78f770420c` | Stage 2 Bootstrap Assembly |
| `zcc3.s` | `d3d5fcf2043ff16cf17dee6641dfa216de2c6047fb0c20a0c4e78c78f770420c` | Stage 3 Bootstrap Assembly |
| `zkaedi_prime/squozen_engine.py` | `cb9e638f2ee15707d77f27a6379d6c217809260fa7f091c6afad966f9522d541` | Hardened String-Aware & Depth-Aware BPE-Delta PDA (`SquozenLogitsEngine`) |
| `zkaedi_prime/code_grammar_engine.py` | `3580f8e80312977f093d64976270507f1eb63081196f051c82a1c6ff2123ea63` | Universal Multi-Language Code Grammar PDA (`SovereignControlFlowLogitsProcessor`) |
| `tools/prime/symplectic_annealing_sampler.py` | `03eafe29babaceeb49b9af1e38dd764860a3d12a2f65f86485cbc6db9b1f0f2b` | Canonical ZKAEDI PRIME Coupled Hamiltonian + Benettin FTLE Sampler (`SymplecticAnnealingSampler`) |
| `tools/prime/zero_drop_gateway.py` | `299dfc7b9851e66b7da743ff454738580b64d9a2a890027571c3e655c5108ed9` | Hardened Zero-Drop Structured LLM Gateway & 4-Arm Paired Causal Harness |
| `tools/prime/test_zero_drop_gateway.py` | `68ee379cbe432a8ab06c3553966247b4a69c13b0398edc4a7ead924a4a338a20` | 10-Stage Deterministic Hardened Regression Suite |
| `docs/evidence/2026-11-01/zero-drop-gateway/gauntlet_report.json` | `a9b1021921b9909e9731c8ee5ce3d2494cc9595a12fd8c4aec8a9389c4c0c0e1` | Full 4-Arm Paired Causal Harness + HTTP Smoke Test Raw Telemetry |
