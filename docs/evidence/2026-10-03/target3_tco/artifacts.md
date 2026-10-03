# Artifacts: Target 3 — Tail Call Optimization (TCO)

## Core Files Modified
- \part5.c\: Tail call optimization pass in peephole optimizer (+35 lines).
- \	ests/test_tco.c\: 5-suite comprehensive verification harness testing deep 10M recursion, sibling calls, mutual recursion, 6-arg System V register preservation, and accumulator recursion.

## Bootstrap Artifacts
- \stage2.s\: \6f52df1d1d5e23c100f960e78dc40144beb70143ffe4e4a44cae7f69883db5ed\
- \stage3.s\: \6f52df1d1d5e23c100f960e78dc40144beb70143ffe4e4a44cae7f69883db5ed\
- \cmp stage2.s stage3.s\: Byte-identical (exit 0)
- \provenance ledger\: \vidence/zcc-omega-1791067731-944/provenance.jsonl\
