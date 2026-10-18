# Artifact Manifest: Peephole Optimizer Upgrade
Date: 2026-10-17

## Modified Files
- `part5.c` (40 lines added, 0 lines deleted):
  - SHA-256: `63bc3bf49d97c7e923e7f259740523dbdb216cfdcf23927fc0ff9599fb42e535`
  - Scope: Section 4 in `peephole_optimize` (`movl $0` -> `xorl`, `shl/shr $0` elision, redundant jump elision, dead `movq %r11, %rax` elision, extended `imulq` power-of-two shift reductions).

## Intermediate & Test Artifacts
- `tests/probe_peephole_upgrade.c`:
  - Pre/post verification probe validating semantic correctness (`res == 324`).
- `zcc2.s`:
  - SHA-256: `be3f48a24a069361ace8d50ea2bb49303522bce53372ea6a52eee1b4d69a136e`
- `zcc3.s`:
  - SHA-256: `be3f48a24a069361ace8d50ea2bb49303522bce53372ea6a52eee1b4d69a136e`
- Identity: `cmp zcc2.s zcc3.s` byte-identical (0 deltas).
- Receipts:
  - `artifacts/metamorphic_gauntlet_receipt.json` (SHA-256: `098cbb8ed8b4db57c6f3baf8d0878940d2165adf5d0e591ed031f39bf213aeff`)
  - `docs/evidence/2026-10-17/multi_target_hardening/multi_target_hardening_receipt.json`
