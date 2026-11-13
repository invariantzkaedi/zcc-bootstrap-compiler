# Artifacts & Checksums (quad-isa-rv64-arm64-5bugs)

## Modified Compiler Source Files (`git diff --numstat`)
| File | Diff Stat | SHA-256 |
| :--- | :--- | :--- |
| `part4.c` | `+32 / -12` (44 lines, `< 50`) | `92aa43a8f46cdf9ba6c8ee6236077581cfc5b15a444c7200810764f798a63640` |
| `part6_arm.c` | `+19 / -1` (20 lines, `< 50`) | `d192ac6cc054dff1ee0a9d5c37cfcc7ad5ea1b2f4feaa3dfdf17934680210bd0` |
| `part6_riscv.c` | `+24 / -4` (28 lines, `< 50`) | `b9d29a088b393eb517ecbc8dbeeace62eb446dd34754a6b22ca59e3f67e21d29` |
| `tools/zcc_quad_isa_gate.py` (untracked test harness) | UI defect-ledger check updated (`+8 / -2`) | `64c72c038e54cbbec63f3545b6086dcf4273e6919ddaff3cc99bb5a5315e5952` |

Note: `part0_pp.c` and `part3.c` were not modified (`0` diff lines).

## Built Compiler, Self-Host Stages & Gate Receipts
| Artifact | Path | SHA-256 |
| :--- | :--- | :--- |
| Host compiler binary | `zcc` | `4218d5037d4136e8fc6267517d9ae10f7c947b5dd8fbc9db168dda2a3ae1e6d7` |
| Stage 2 self-host asm | `evidence/zcc-omega-1794536838-894-f5121d046dad6310/stage2.s` | `540d806ebf7d751bfa03a10f4223e19c592c10733d0777ba3908224cea3a5194` |
| Stage 3 self-host asm | `evidence/zcc-omega-1794536838-894-f5121d046dad6310/stage3.s` | `540d806ebf7d751bfa03a10f4223e19c592c10733d0777ba3908224cea3a5194` |
| Stage 2 binary | `evidence/zcc-omega-1794536838-894-f5121d046dad6310/stage2.bin` | `8f7b85850a6fb0d9fe149e9b1ca74dec6a108ea5968b4bb8147cd182b5fa553a` |
| Stage 3 binary | `evidence/zcc-omega-1794536838-894-f5121d046dad6310/stage3.bin` | `8f7b85850a6fb0d9fe149e9b1ca74dec6a108ea5968b4bb8147cd182b5fa553a` |
| Quad-ISA Gate receipt | `artifacts/ZCC_QUAD_ISA_GATE_RECEIPT.json` | `c00d25c55974385ff642b4f43f26b69860e5093c34e83867a62725ec055e6443` |
| Quad-ISA HTML harness | `zcc_hyperdim_test_harness.html` | `0b712f440424214fb419ad8c16e7a8cd9b37286dcb9ad88edf1720ed5616aa18` |

## Emitted Target Assembly Probes (EF-2 / EF-3)
| Probe `.s` | Digest Observed on QEMU | SHA-256 | Key Verified Sequences |
| :--- | :--- | :--- | :--- |
| `/tmp/ef3_post/rv64_t2.s` | `FNV1A64=0xfb986fd4f88819a9` | `549a2c57c261d2766920c931fb9274ff1f4199ce1c9053468353e4b349cd13f0` | `sw a1, 0(a0)` (L2218, L2298, L2379); `slli a0, a0, 48` + `srli a0, a0, 48` (L2236-2237, L2431-2432) |
| `/tmp/ef3_post/rv64_t3.s` | `FNV1A64=0x6d4f29801741710f` | `86ee422f3a3d4f56f9b45247bdb14c3f7517f323ff01d5b74873c70a191ea59e` | `ld t1, 16(s0)` + `sd t1, -72(s0)` (L2553-2554); `addi sp, sp, -16` / `sd a0, 0(sp)` / `ld a6, a7` (L2766, L2776, L2876, L2878); `sd a0, -240(s0)` / `ld t6, -240(s0)` / `jalr t6` (L3301, L3314-3315) |
| `/tmp/ef3_post/arm64_t2.s` | `FNV1A64=0xfb986fd4f88819a9` | `7dc7111a67b24d9b74a0d047be1a933de6bfb37a2ad052c995ea8b5cb474bf52` | Clean QEMU execution matching Tier 2 Python oracle |
| `/tmp/ef3_post/arm64_t3.s` | `FNV1A64=0x6d4f29801741710f` | `02daf2d7b3973df56e181f1c90afcdb18eb5cbd768a45e850c63944e3a16db2b` | `ldr x9, [x29, #16]` + `stur x9, [x29, #-72]` (L1913-1914); `sub sp, sp, #16` / `str x0, [sp, #0]` / `ldr x6, x7` (L2070, L2078, L2148-2149); `stur x0, [x29, #-240]` / `ldur x16, [x29, #-240]` / `blr x16` (L2469, L2478-2479) |

