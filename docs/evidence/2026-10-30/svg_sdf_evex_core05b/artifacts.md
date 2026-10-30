# Artifact Manifest — `svg_sdf_evex_core05b` (2026-10-30)

## Primary Compiler & Verification Artifacts
| Artifact | MD5 Checksum | Description |
| :--- | :--- | :--- |
| `zcc` | `bd3dc7e37f4b6d81c7f0b262c7139fdb` | Stage 1 ZCC binary with `--svg-ast/cfg/ir`, `--svg-to-sdf`, and AVX-512 EVEX ELF object encoder |
| `zcc2` | `7c392067dd7f0055b4bd27b5c54ab71d` | Stage 2 self-hosted ZCC binary |
| `zcc2.s` | `219135ff652bcfc1a3d2e26254902d99` | Stage 2 assembly (`./zcc zcc.c -S -o zcc2.s`) |
| `zcc3.s` | `219135ff652bcfc1a3d2e26254902d99` | Stage 3 assembly (`./zcc2 zcc.c -S -o zcc3.s`, byte-identical to `zcc2.s`) |
| `docs/evidence/2026-10-30/svg_sdf_evex_core05b/stdout.log` | preserved | Full `make selfhost` log |
| `docs/evidence/2026-10-30/svg_sdf_evex_core05b/gate_raw.log` | preserved | Raw unwrapped `gate.sh` log |
| `evidence/zcc-omega-1793393696-38273-af9012b456c531a0/provenance.jsonl` | preserved | Chained provenance ledger from `gate.sh` |

