# Evidence Artifacts: Freestanding Zero-Dependency ZSan Sanitizer

## Key Artifacts & Checksums

| Artifact Path | SHA-256 Checksum | Description |
| :--- | :--- | :--- |
| `zcc` | `a13b01e3114c233d037865c64d384d6bbc4bb7edefed17d09fa29c20c4702ef6` | Rebuilt ZCC binary with `-fzsan` support |
| `zcc2.s` | `c1efe8ebfbb11711089cf9eb047f130180c5534220eb0650611514a9f95dab57` | Stage 2 compiler bootstrap assembly |
| `zcc3.s` | `c1efe8ebfbb11711089cf9eb047f130180c5534220eb0650611514a9f95dab57` | Stage 3 compiler bootstrap assembly (byte-identical to stage 2) |
| `/tmp/probe_no_zsan.s` | `1165cd17d330ec7c095afba6c0d8682ff1c784448c0a709d0c93c6e8dd35a41e` | Baseline assembly without `-fzsan` (crashes via SIGSEGV) |
| `/tmp/probe_zsan.s` | `09c59fbe1382d033e3f644c0f237a4addde704553dd2392e35628f8640cc69a4` | Instrumented assembly with `-fzsan` (catches overflow, emits diagnostic, ud2 trap) |
| `/tmp/test_zsan_safe.s` | `618ca1ea45944bb7334b80e22bfc1a5f2eb90976ba28a9a3eac78276f9f6eb40` | Safe test assembly with `-fzsan` (exits 0 with valid execution) |

## Source Changes Inspected
- `part2.c`: Global flag `int g_zsan_mode = 0;`
- `part3.c`: Upper canopy allocation offset reservation in `parse_func_def`
- `part4.c`: Prologue canopy + XOR frame pointer binding + epilogue integrity gate with freestanding `sys_write` and `ud2`
- `part5.c`: CLI flags `-fzsan` and `--fzsan`
