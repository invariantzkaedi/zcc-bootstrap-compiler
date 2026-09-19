# Artifacts Manifest: XMM Floating-Point Validation Harness

## Source & Runner Files
| File Path | Description | Checksum (SHA-256) |
|---|---|---|
| `tests/fractal.c` | Mandelbrot FP stress kernel with 10 live doubles | `ba3665792d77d7fa8c351f3cba800f1fa5cbaea2a71beea2bc2ba48a602cf3c9` |
| `tests/fractal_xmm_validate.sh` | Bash XMM / SSE2 opcode & execution validator | `a5f11559ee5ba2ee321689ea52bbfa7ba2aa046aeb7b24329a1b1812fe53347e` |
| `tests/fractal_xmm_validate.ps1` | PowerShell native XMM validator | `2f64d0bc91ea0a6ea12b4e8ba1c58ba0cb334e2b0ae0dfbcf1b156b8265aaeb9` |
| `docs/XMM_VALIDATION.md` | XMM validation checklist & CG-XMM bug signatures | `d1752bca7e868a83a0058b73656114a8f94354c4aa44b76a02b662a6375bcbc7` |

## Golden Output Reference
- **Expected Stdout MD5:** `9fe81c3d00c986b2882e8973bb3c15a2`
- **Output Size:** 2,178 bytes
- **ZCC Actual Codegen Features:**
  - `movsd`: 144 (target ≥ 10)
  - `addsd`: 8 (target ≥ 5)
  - `subsd`: 4 (target ≥ 2)
  - `mulsd`: 15 (target ≥ 8)
  - `divsd`: 1 (target ≥ 1)
  - `ucomisd`: 3 (target ≥ 2)
  - `cvtsi2sd`: 2 (target ≥ 2)
  - FP Spills: 66 (target ≥ 10)
  - Varargs AL sets: 36 (target ≥ 8)
  - Distinct XMM registers: 4 (`%xmm0`–`%xmm3`)
