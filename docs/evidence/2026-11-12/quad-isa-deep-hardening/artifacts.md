# Artifacts & Checksums — quad-isa-deep-hardening (2026-11-12)

## Modified / New Source & Infrastructure Files
| File | Scope (`git diff --numstat` vs HEAD) | SHA-256 |
| :--- | :--- | :--- |
| `part4.c` | `+34 / -14` (48 total changed lines, `< 50` cap) | `7df82284e3c5f629a7180b2c61421e99c2257facb4a2efccf2bf7a079c84f749` |
| `part6_arm.c` | `+20 / -2` (22 total changed lines, `< 50` cap) | `49557a8fcc408506bce6aee88b94e1a84105c9278731cddc00e685bf9b631b9a` |
| `part6_riscv.c` | `+24 / -4` (28 total changed lines, `< 50` cap; unchanged this batch) | `b9d29a088b393eb517ecbc8dbeeace62eb446dd34754a6b22ca59e3f67e21d29` |
| `Makefile` | `+8 / -1` (`test-quad-isa` promoted to `--strict-tier 3 --verify-ui` + deep regression) | `b73b19fd479654d19c09bc724bd519adc84b00fb461d391e1bdd59d79e97d9f8` |
| `tools/zcc_quad_isa_gate.py` | untracked (default `--strict-tier 3`; fixed `h.gateReceipt` DOM check + empty-ledger assert) | `c1a803fc052fb88098e31ea98d9a47a82a556f891f590475a9f2342f040f6dce` |
| `tools/zcc_hyperdim_ui_parts.py` | untracked (Tier 1-3 badge + `data-empty-ledger="1"` ALL CLEAR row) | `9cf9d715b2601c867cceea6d042fa2ecd4158e21da4da919bda6f4794f245730` |
| `tests/test_quad_isa_deep_regression.py` | untracked (new standalone differential regression suite vs GCC/Clang `-O2`) | `79ee39f1c90d2992f72a03073a109ae3deddd3779d8567dd550647c397efe3f4` |

Note: `part0_pp.c`, `part3.c`, and `part5.c` were not modified (`0` diff lines).

## Built Compiler, Self-Host Stages & Gate Receipts
| Artifact | Path | SHA-256 |
| :--- | :--- | :--- |
| Host compiler binary | `zcc` | `a754e4c9b19cbd5f69c50b98dd9f8447ee10091d50e1535a2b6d809d642860e3` |
| Stage 2 self-host asm | `zcc2.s` (`evidence/zcc-omega-1794538154-323-1a1518db8e32dba5/stage2.s`) | `c38fc91a16e3836cc1a214636e51de7dd443841520c3aadb600980890171e357` |
| Stage 3 self-host asm | `zcc3.s` (`evidence/zcc-omega-1794538154-323-1a1518db8e32dba5/stage3.s`) | `c38fc91a16e3836cc1a214636e51de7dd443841520c3aadb600980890171e357` |
| Stage 2/3 binary | `zcc2` (`evidence/zcc-omega-1794538154-323-1a1518db8e32dba5/stage{2,3}.bin`) | `c7d5516ad8447fe01640c830d36f5fb81b7eabf1cdb7d7e0871e2cc93d3f36b8` |
| Quad-ISA Gate receipt | `artifacts/ZCC_QUAD_ISA_GATE_RECEIPT.json` | `ec90cd4969985ab8cd913a74adff38bcde677b3517e487c952be66fb5088a590` |
| Quad-ISA HTML harness | `zcc_hyperdim_test_harness.html` | `ec1a91f3660e280cb807b51c708bbdd79f93e5843100ae469f24e66cc54f0470` |

## Emitted Target Assembly Verification (EF-2)
| Assembly Path | SHA-256 | Verified Instructions |
| :--- | :--- | :--- |
| `/tmp/zcc_ef2_verify/rv64.s` | `37a723ade9dd1a47a4e33275d7e7a2632169f4cef2cef4d2b92057cd5b57199f` | `divu`/`remu` for runtime unsigned pow2 `/`,`%`; `div` for signed pow2 `/` (`L260..L430`) |
| `/tmp/zcc_ef2_verify/arm64.s` | `21d90a3fcb314518c8407d3311787cbf89eb8cebec9df5d9d5463dadfaf5d8a7` | `ldr x16, =68160` + `ldr x16, =-68096` (`L475..L565`) replacing unencodable `mov x16, #-68096` |

