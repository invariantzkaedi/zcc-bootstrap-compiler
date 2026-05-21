# b302c11 — Gate Evidence (Phase 3 Commit 1)

Status: Complete & Verified (companion to Phase 3 Commit 1)
Precedent: ae6b5ff (FORENSIC_CORRECTION_2026-04-19.md, "every closed commit must carry gate output")

This document details the gates and outputs for **Phase 3 Commit 1 — Symbolic Base Tracking & TBAA Cast Fallback**.

## Operative Assumption
Symbolic address tracing (SBT) of `(base_reg, byte_offset, has_cast)` allows the optimization pipeline to perform precise, structural struct-field TBAA redundant load elimination, while using pointer casts as a conservative invalidation fallback trigger.

## Load-Bearing Claims
- **VERIFIED**: `node_is_char_or_void_ptr_cast` correctly parses pointer casts to `char*`, `uchar*`, and `void*`.
- **VERIFIED**: Virtual register copying under `ZND_CAST` is lowered to `OP_COPY` with `sbt_has_cast = true` annotation.
- **VERIFIED**: `trace_address_root_offset` extracts base registers, constant offsets, and carries cast inheritance flags across pointer arithmetic paths (`OP_GEP`, `OP_ADD`, `OP_SUB`, `OP_COPY`).
- **VERIFIED**: GVN redundant load elimination enforces the struct field non-invalidation rule and cast fallback rule cleanly, yielding identical Stage 2 -> Stage 3 assembly.

## Gates

### Gate 1 — Self-host byte-identical bootstrap
- Command: `make selfhost`
- Result:  `SELF-HOST VERIFIED (assembly identical)`
- Output:
```text
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)" || (echo "SELF-HOST FAILED (assembly diverged)"; diff zcc2.s zcc3.s | head -20; exit 1)
SELF-HOST VERIFIED (assembly identical)
```

### Gate 2 — Struct-Field TBAA & Cast Fallback Optimization Proof
- Command: `ZCC_IR_BACKEND=1 ./zcc tests/test_gvn.c -o tests/test_gvn.s`
- Telemetry:
```text
cc_func: test_struct_tbaa
[EscapeAna] allocations promoted to stack: 2  (of 3 total)
[Mem2Reg]   single-block allocas promoted: 2
[DomTree] fn=test_struct_tbaa  BB0->idom=BB0
[GVN]       redundant operations eliminated: 2

cc_func: test_cast_fallback
[EscapeAna] allocations promoted to stack: 3  (of 4 total)
[Mem2Reg]   single-block allocas promoted: 3
[DomTree] fn=test_cast_fallback  BB0->idom=BB0
[GVN]       redundant operations eliminated: 2
```

### Gate 3 — Corpus regression
- Command: `./tests/zcc_test_suite.sh --quick`
- Result:
```text
══════════════════════════════════════════════════
  PASS: 21  FAIL: 0  SKIP: 1
══════════════════════════════════════════════════
ALL TESTS PASSED
```

## Forensic Lineage
- `FORENSIC_CORRECTION_2026-04-19.md` (Gate discipline precedent)
- `FORENSIC_023A_PARSER001.md` (Parser scope)
- `ZCC_STATUS.md` (Active phase tracker)
- `compiler_passes.c` (Opt core)
