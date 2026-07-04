# Gate Evidence — Struct Packing and Alignment Conformance

- **Target Ticket / Topic:** Struct Packing, Member Attribute Alignment, and `#pragma pack` Conformance
- **Commit SHA:** `224156a6`
- **Evidence Directory:** `docs/evidence/2026-07-04/struct-pack-matrix-2026-07-04_152221/`
- **Verification Date:** 2026-07-04

---

## Gate 1: Self-Host Identity
Byte-identical assembly produced by Stage 2 and Stage 3 compilers.
```bash
cmp zcc2.s zcc3.s
```
Status: **PASS** (Zero diff / exit code 0)

---

## Gate 2: ABI Interoperability Lanes
```bash
make abi-lanes
```
- **Argument-Passing ABI Lane:** 31/31 passed.
- **Return-Value ABI Lane:** 17/17 passed.
- **Arrays-in-Structs ABI Lane:** 20/20 passed.
- **Packed-Struct ABI Lane:** 12/12 passed.
- **Bitfield ABI Lane:** 16/16 passed.
Status: **PASS** (96/96 passed)

---

## Gate 3: Compatibility Smoke Corpus
```bash
make compat-smoke
```
Status: **PASS** (`COMPAT SMOKE COMPLETE`)

---

## Gate 4: Test Matrix Parity
```bash
/tmp/run_matrix.sh
```
Results (staged from `docs/evidence/2026-07-04/struct-pack-matrix-2026-07-04_152221/10_matrix_results.log`):
```text
=== m01_packed_basic ===
RESULT: MATCH (m01_packed_basic)
=== m02_unpacked_control ===
RESULT: MATCH (m02_unpacked_control)
=== m03_packed_nested ===
RESULT: MATCH (m03_packed_nested)
=== m04_packed_array_stride ===
RESULT: MATCH (m04_packed_array_stride)
=== m05_packed_tail_padding ===
RESULT: MATCH (m05_packed_tail_padding)
=== m06_bitfield_compare ===
RESULT: MATCH (m06_bitfield_compare)
=== m07_packed_bitfield ===
RESULT: MATCH (m07_packed_bitfield)
=== m08_zero_width_bitfield ===
RESULT: MATCH (m08_zero_width_bitfield)
=== m09_packed_with_aligned_member ===
RESULT: MATCH (m09_packed_with_aligned_member)
=== m10_pragma_pack ===
RESULT: MATCH (m10_pragma_pack)
```
Status: **PASS** (10/10 matches)

---

## Gate 5: Bootstrap Stage Adjacency
```bash
make selfhost
```
Status: **PASS** (`SELF-HOST VERIFIED (assembly identical)`)
