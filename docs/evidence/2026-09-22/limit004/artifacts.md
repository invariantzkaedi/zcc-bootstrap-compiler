# LIMIT-004 Artifact Checksums & Provenance

| Artifact | Size (bytes) | MD5 | SHA256 | Purpose |
|---|---|---|---|---|
| `zcc2.s` | 1,263,708 | `c951344bc1dfe7b9e9395f49f22163d4` | - | Stage 2 Self-Host Emitted Assembly |
| `zcc3.s` | 1,263,708 | `c951344bc1dfe7b9e9395f49f22163d4` | - | Stage 3 Self-Host Emitted Assembly |
| `/tmp/probe_l4_exe` | 10,952 | `8f5527c771a9b59cb1d75c018a268e28` | `0c61b7e239baabc3ead67fd40e2619673ce63c3d99cc6623102c77b0bc483bc7` | Freestanding Static Linked Executable |
| `/tmp/libprobe_math.a` | 2,304 | `503eb4bf27e240c7531f830611bbdc5e` | - | Static Archive Member A (`probe_add`, `probe_sub`) |
| `/tmp/libprobe_mul.a` | 2,224 | `2296dd45471aa0534d87c603a3d8511c` | - | Static Archive Member B (`probe_mul`) |
| `/tmp/probe_l4_main.o` | 2,232 | `0d54f05d57a5c78ebd79af874a11ed82` | - | Object file referencing multi-archive symbols |
| `tests/probe_limit004.c` | 2,522 | - | - | Deterministic Regression Probe |
