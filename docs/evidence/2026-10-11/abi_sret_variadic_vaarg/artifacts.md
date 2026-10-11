# Artifacts & Checksums: SystemV ABI sret, Variadic Save Base & va_arg Struct Hardening

Date: 2026-10-11
Repository: invariantzkaedi/zcc-bootstrap-compiler

## Checksums (MD5)

| Artifact | MD5 Checksum | Notes |
|---|---|---|
| `zcc` | `d8a811274d4da55b0b83186afb6ca723` | Stage 1 bootstrapped compiler binary |
| `zcc2.s` | `b20439d939d3932ad4ad89fbc86d3677` | Stage 2 compiler assembly |
| `zcc3.s` | `b20439d939d3932ad4ad89fbc86d3677` | Stage 3 compiler assembly (Fixed Point: MATCH) |
| `/tmp/probe1` | `b098cc55b23b478ec0197cc4b08edff4` | Variadic save base vs sret scratch collision probe |
| `/tmp/probe2` | `6240b06532edf4182b348d6189e12efa` | Large (>224B) sret stack buffer overflow probe |
| `/tmp/probe3` | `d9a59d00c8ea21001aa6caa75deab1b5` | 16-byte struct va_arg extraction & stream sync probe |

## Verification Status
- Stage 2 vs Stage 3 byte-identity: 100% (Byte-identical `b20439d939d3932ad4ad89fbc86d3677`)
- Probes 1, 2, 3: All Exit 0 with verified assertion semantics.
