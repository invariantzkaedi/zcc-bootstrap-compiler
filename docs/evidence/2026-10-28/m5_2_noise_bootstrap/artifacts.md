# Artifacts Ledger: Gate M5-2 Multi-Seed Generalization, Native 10-Seed Engine & AVX-512 Optimization

**Milestone:** `ZCC-EAI-006-G3-M5`  
**Gate:** `GATE-M5-2`  
**Standard:** `ZCC-EF-0` & `ZCC-EF-7`  
**Evidence Tier:** `Tier E3`  
**Independent Verifier:** `tools/verify_m5_2_noise_bootstrap.py`  

---

## 1. Verified Cryptographic Artifact Hashes

| Artifact Path | Size (Bytes) | SHA-256 Checksum | Classification |
| :--- | :--- | :--- | :--- |
| `experiments/zcc_eai_006_g3_m5/run_m5_2_bootstrap.py` | 43,834 | `8d12f3a4d53b4fbc5650c554f8694a3d527255df6866a759f590f27ece5bb547` | Primary Experiment Script |
| `experiments/zcc_eai_006_g3_m5/zcc_m5_2_native_multiseed.c` | 42,766 | `3d784d20f4583d6c5946fe294d524c026c81c5dbda5eda8e71c1def1dd185875` | Native 10-Seed & AVX-512 C Engine Source |
| `experiments/zcc_eai_006_g3_m5/zcc_m5_2_native_bin` | 70,744 | `45945e04fe4fef053661f924c823e59d10a323be7bbb9ea5d25ffa0fd5c55a0f` | ZCC Stage-2 Native 10-Seed & AVX-512 Binary |
| `experiments/zcc_eai_006_g3_m5/gcc_m5_2_native_bin` | 35,056 | `b154629cd38457177015bde875b93ce0ac6af8b1ae2302061596aba45b336117` | GCC Reference 10-Seed & AVX-512 Binary |
| `experiments/zcc_eai_006_g3_m5/zcc_m5_2_native_bin.s` | 313,444 | `3721dd03be2a786ae0451d18349ea2af0da604c053b5bfb891998f3b0752ff9b` | ZCC Emitted x86-64 + AVX-512 Assembly |
| `experiments/zcc_eai_006_g3_m3_readout/evidence/dataset_a/dataset_a_train_X.csv` | 12,478,525 | `3a4786e0426fabe9e4d94d12f9808cb223c1f6a277d502807cd7012c575bc114` | Control 0 Reference Train Matrix (1000x512) |
| `experiments/zcc_eai_006_g3_m3_readout/evidence/dataset_a/dataset_a_test_X.csv` | 4,991,617 | `ce0028f7bfdc522a9386ce861da9ac1a24d5102d8a0043587abfa38bbf43238a` | Control 0 Reference Test Matrix (400x512) |
| `experiments/zcc_eai_006_g3_m3_readout/zcc_m3_bin` | 57,248 | `5116b79b7d0f3c1956f362d8621c9ae69dc44a188b9a0b2975502b79f67699a7` | ZCC Native Reference Binary (Seed 42) |
| `docs/evidence/2026-10-28/m5_2_noise_bootstrap/m5_2_zcc_native_multiseed_receipt.json` | 4,612 | `5666139a3a67f6b8be96cddd9b9864104ec3526bb4cf6465df18bc8a0e733e5e` | ZCC Native 10-Seed & AVX-512 Receipt |
| `docs/evidence/2026-10-28/m5_2_noise_bootstrap/m5_2_gcc_native_multiseed_receipt.json` | 4,566 | `ca56713d05f333971028184d3c8edaa81d37470f0c95ba5bb72d7bf6964d13dd` | GCC Native 10-Seed & AVX-512 Receipt |
| `docs/evidence/2026-10-28/m5_2_noise_bootstrap/m5_2_noise_bootstrap_receipt.json` | 16,705 | `8483ae27c7ec9ef41c0181a5d2cc5479ef1df8a82ee792d9f6669b1269ec6cd8` | Machine-Readable Master Receipt |
| `docs/evidence/2026-10-28/m5_2_noise_bootstrap/m5_2_noise_bootstrap_receipt.json.sha256` | 100 | `dd4b44969ee03f3ec13c537771c7566f4f63cf90cd56a11cc94bf2dd375690f1` | Detached Checksum |
| `docs/evidence/2026-10-28/m5_2_noise_bootstrap/stdout.log` | 6,812 | `fc98afa97b12cd8bdc9803e49f7ebb07bed2294df456ace8d1b253463057072c` | Verbatim Execution Trace |
| `docs/evidence/2026-10-28/m5_2_noise_bootstrap/commands.txt` | 1,749 | `d4b1fee6208579b0f1dbdccdca4201d88d7512929d7a5efb232ca88238518c78` | Exact Command Sequence |
| `docs/evidence/2026-10-28/m5_2_noise_bootstrap/M5_2_NOISE_BOOTSTRAP_REPORT.md` | 16,454 | `63a7930956b4a38cb49a9c82e7686b8599f703f3187f8cecce160d94f2cf15a7` | Full Scientific Report |
| `docs/evidence/2026-10-28/m5_2_noise_bootstrap/verdict.md` | 2,985 | `621a15037d7c8416a9d7c7dafff5d4fa1e7e2e8487ccd88e60928b934d64a409` | Gate Verdict Summary |

---

## 2. Integrity Verification Proof

```console
$ python3 tools/verify_m5_2_noise_bootstrap.py --self-test
```
