# Formal Verification Verdict: Designated Union Initializer Stride (Vault 04)

**Target Dimension**: `04_Designated_Union_Initializer_Stride`  
**Primary Reproducer**: `APEX_BUG_03a6564c0351d740`  
**Total Dimension Scope**: 33 confirmed bugs in `docs/evidence/proof_of_bug_vault/`  
**Status**: VERIFIED PASS (33 / 33 resolved, 100.0%)  

---

## 1. Violated Invariant & Root Cause
- **Grammar & AST Desynchronization (`part3.c:3352`)**:
  `parse_initializer_list` consumed chained designator tokens (`.data.pt`) in a while-loop that overwrote `desig_name` with the terminal identifier (`"pt"`), completely dropping outer parent struct/union fields (`"data"`).
- **Positional Member Binding Defect (`part3.c:3528`)**:
  `emit_local_initializer` matched initializer expressions to `curr_type->fields` strictly by sequential position rather than searching for `sf->name == item->member_name`.
- **Consequence**:
  When scalar initializers were matched against aggregate struct fields without intermediate wrapper lists, the codegen lowerer in `part4.c:1233` treated the scalar literal as an aggregate address, issuing `rep movsb` with an immediate register operand as source pointer `%rsi`, causing runtime SIGSEGV page fault.

---

## 2. Minimal Surgical Patch Summary
- **File Modified**: `part3.c` (25 net changed lines; diff strictly bounded < 50 lines).
- **Core Changes**:
  1. `parse_initializer_list`: Collects chained dot designators into `desig_names[]` and dynamically wraps nested initializers in hierarchical `ND_INIT_LIST` containers with outer `member_name` tags.
  2. `validate_initializer_dimensions`: Matches designated member names to `StructField` definitions to validate child type dimensions accurately.
  3. `emit_local_initializer`: Looks up `curr_type->fields` by `item->member_name` for designated elements while preserving sequential traversal for positional initializers, correctly dispatching union member offsets and nested struct assignments.
- **Foundational Part Diff**:
  - `part0_pp.c`: 0 lines
  - `part3.c`: +37 / -12 lines
  - `part4.c`: 0 lines

---

## 3. Verification Gates

### Gate 1 — Self-Host Identity (Mandatory)
```text
$ cmp zcc2.s zcc3.s && sha256sum zcc2.s zcc3.s
ec2ccf1d9abf219509768316fdf54b190accbb1af8110f2f21516dbf3dd469fe  zcc2.s
ec2ccf1d9abf219509768316fdf54b190accbb1af8110f2f21516dbf3dd469fe  zcc3.s
$ md5sum zcc2.s zcc3.s
4f3dc63784c27ac868c2d87c10ea029a  zcc2.s
4f3dc63784c27ac868c2d87c10ea029a  zcc3.s
```
**Verdict**: PASS (Byte-identical self-host fixed point confirmed, MD5 audited and appended to `BOOTSTRAP_BASELINES.tsv`).

### Gate 2 — Cross-Toolchain Bidirectional Interoperability (Mandatory)
- Test 1 (`zcc-lib` + `gcc-main`): Exit 42 (PASS)
- Test 2 (`gcc-lib` + `zcc-main`): Exit 42 (PASS)  
**Verdict**: PASS

### Gate 3 — Multi-Suite Production Optimization Gauntlet
`python3 tools/test_zcc_opt_passes.py`:
- Suites Evaluated: 19
- Bit-Exact Passes: 19 / 19
- Semantic Divergences: 0
- Net Instructions Elided: 8,891 lines (9.93%)  
**Verdict**: PASS

### Gate 4 — Bug Vault Target Harness (33 / 33 Repros)
`python3 tools/auto_patch_bug_vault.py --dimension 04_Designated_Union_Initializer_Stride`:
- Total Dimension Bugs: 33
- Resolved & Verified: 33 / 33 (100.0%)
- Pre-Patch: 33 / 33 SIGSEGV (exit 139)
- Post-Patch: 33 / 33 Bit-Exact Exit Match vs GCC 13.3 & Clang 18.1  
**Verdict**: PASS

### Gate 5 — Evidence Freshness (Mandatory)
All gate measurements captured live in the current turn. Preserved under `docs/evidence/2026-10-10/designated_union_initializer_stride/`.  
**Verdict**: PASS

---

## 4. Overall Closure Verdict
STATUS: VERIFIED & HARDENED PASS
PROCEED WITH CLOSURE COMMIT
