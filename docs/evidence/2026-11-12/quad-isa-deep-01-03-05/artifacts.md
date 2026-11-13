# Artifacts & SHA-256 Checksums (`quad-isa-deep-01-03-05`)

| Artifact Path | SHA-256 Checksum | Notes |
| :--- | :--- | :--- |
| `part4.c` | `1baba4154ce24e249402a34d7ecdc54e3c4bcaf78e0cf3d5f6e22fb6af9d933a` | `+23 / -23 = 46` changed lines vs `HEAD` (`07785e96e`, cap `< 50`) |
| `patches/quad_isa_deep_01_03_05/deep_01_03_05.patch` | `4c6a7459bd4ec8fd4c9cf0cc8f8783e9f31d2ff3ee34a54bf28413a71d5cf1c4` | Drop-in unified diff for DEEP-01, DEEP-03, DEEP-05 + compound-assign sub-word extension |
| `patches/quad_isa_deep_01_03_05/apply_deep_fixes.py` | `e533fdc7bfa10d34c4853243459c900aeaa4e6c3812b60a72719c0076021b2f2` | Cross-platform (PowerShell + WSL) drop-in installer (`pre_iterate_verify.py` PASS) |
| `tests/test_quad_isa_deep_01_03_05.py` | `457f03b14b86ee8dc37f75edac6801e5ed7c9a04689c3918dec1f95467673f7e` | Differential `gcc -O2` & `clang -O2` vs ZCC `x86_64` / `arm64` / `riscv64` + Rule EF-2 `.s` verifier |
| `zcc` | `34c19ad05ef5eaeb180e730d776598923ffc44ff69c4fc08de703452d1858215` | Stage 1 compiler binary with DEEP-01/03/05 applied |
| `zcc2` | `e1389e7eb1e83d81a7403520682c6895f80cca741f6d52b37d13eae2ad6bada5` | Stage 2 self-hosted compiler binary |
| `zcc2.s` | `862259d78e54b853a0f178406d2892815ef126847caee3edd0c7fa66b2f79a08` | Stage 2 self-hosted assembly |
| `zcc3.s` | `862259d78e54b853a0f178406d2892815ef126847caee3edd0c7fa66b2f79a08` | Stage 3 self-hosted assembly (`0`-byte delta vs `zcc2.s`) |
| `artifacts/ZCC_QUAD_ISA_GATE_RECEIPT.json` | `759bdf7042f3a76ce64fb2e415d29defefd433b615d29defefd433b615d8706d53d1db62fbb94e7a` | Cryptographic Quad-ISA receipt (`stage23_sha256`: `862259d78e54b853...`) |
| `zcc_hyperdim_test_harness.html` | `c412432cbe7eb297e36699e5dd572d313e50b1fc050048853f5ec3133cd27a22` | Synced Hyperdimensional Matrix UI (`EF-7 PASS`) |

