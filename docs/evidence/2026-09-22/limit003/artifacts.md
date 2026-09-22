# LIMIT-003 Verified Artifacts

| Artifact Path | Format / Type | Checksum (MD5) | Role in LIMIT-003 Verification |
| :--- | :--- | :--- | :--- |
| `zcc2.s` | Assembly text | `4d414daa3c2c23bcf09fdee45c40b727` | Stage 2 bootstrap self-host compiler emission |
| `zcc3.s` | Assembly text | `4d414daa3c2c23bcf09fdee45c40b727` | Stage 3 bootstrap self-host compiler emission (byte-identical) |
| `tests/test_limit003_complex.c` | C Source | `ae230fd25e3692ea466986dfae6a3a41` | Formal C99 complex arithmetic differential test gauntlet (20/20 PASS) |
| `tests/probe_limit003.c` | C Source | `b051efefd871787d559fe3e316d2f3ff` | Minimal probe confirming symptom presence and resolution |
| `include/complex.h` | C Header | `593b45a64b971a80d463d1bc84cb6bf7` | Standard C11 / C99 complex arithmetic header |
| `include/zcc_dd_real.h` | C Header | `f599767222fc57cfa4a34b223d6a6a24` | Native 106-bit double-double extended precision library |
