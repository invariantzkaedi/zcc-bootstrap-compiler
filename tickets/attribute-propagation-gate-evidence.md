# ZCC Gate Evidence: GCC Attribute Aligned and Packed Propagation

## Goal
Establish a robust, end-to-end propagation pipeline for GCC `__attribute__((aligned(N)))` and `__attribute__((packed))` layouts through both the preprocessor and the compiler parser layers to validate exact AMD64 System V ABI alignment, padding, and struct sizes for the covered golden cases.

## Outcome
Implemented the surgical attribute preprocessor bridge emitter `__zcc_attr_aligned_N__` and `__zcc_attr_packed__` in `part0_pp.c` and integrated namespace swallowing and bounds validation in `part2.c` and `part3.c`. Successfully achieved Stage 2 and Stage 3 byte-identical assembly output, executed the 29-test quick regression suite with 100% passes, and verified all golden System V ABI structure layouts.

---

## Phase 0 Verdict
```text
BASELINE:              GREEN
SYMPTOM-IN-HISTORY:    NO
FORENSIC-LATEST-SHA:   d0ede4d3
PROCEED:               YES
```

---

## Gate 1: Self-Host Byte-Identical Parity
The Stage 2 compiler (`zcc2.s`) and Stage 3 compiler (`zcc3.s`) generated bit-for-bit identical assembly:

```text
$ make selfhost
=== Stage 1: zcc compiles itself -> zcc2 ===
...
=== Stage 2: zcc2 compiles itself -> zcc3 ===
...
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)" || (echo "SELF-HOST FAILED (assembly diverged)"; diff zcc2.s zcc3.s | head -20; exit 1)
SELF-HOST VERIFIED (assembly identical)
```

---

## Gate 2: System V ABI Layout Verification (`test_attributes.c`)
Compiled and executed structure layout sizes, confirming 100% agreement with standard GCC layout expectations:

```text
Packed size: 6 (expected 6)
Aligned size: 8 (expected 8)
BigAlign size: 32 (expected 32)
NestedPacked size: 7 (expected 7)
PackedInsideAligned size: 32 (expected 32)
BigAlign alignment check: PASS
```

---

## Gate 3: Corpus Regression (Quick Suite)
All 29 regression tests passed perfectly:

```text
══════════════════════════════════════════════════
  PASS: 29  FAIL: 0  SKIP: 3
══════════════════════════════════════════════════
ALL TESTS PASSED
```

---

## Bugs Caught Mid-Gate
- **Pre-Parser Drop**: Identified that the preprocessor originally intercepted all `__attribute__` declarations and discarded them entirely, causing structural layout declarations to miss alignments and pack declarations. Resolved via preprocessor-to-lexer bridge tokens (`__zcc_attr_*__`).
- **Format Call Type Mismatch**: Fixed compiler lexer parsing error where format specifiers were passed to a static-only message `error` routine.
- **Generic Swallow Bounds**: Added a safe range check (`1` to `8192`) on both preprocessor and lexer stages to detect invalid alignment requests.
