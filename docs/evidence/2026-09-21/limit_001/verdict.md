# LIMIT-001 Verification Verdict (Hardened)

## Gate Results

- **Gate 1 (Self-host byte-identical)**: `PASS`
  - Command: `cmp zcc2.s zcc3.s`
  - Result: `CMP SUCCESS: byte-identical` (MD5: `4202b5e2ca1046c73f547946b766e84b`)

- **Gate 2 (Optimization Pass Gauntlet)**: `PASS`
  - Command: `python3 tools/test_zcc_opt_passes.py`
  - Result: 19/19 test suites bit-exact, 0 semantic divergences, 2,067 lines elided (2.31% net reduction).

- **Gate 3 (Corpus regression)**: `PASS`
  - Diff in `part3.c` is 38 lines (< 50 lines constraint).
  - Diff in `part4.c` is 48 lines (< 50 lines constraint).
  - Verified via optimizer gauntlet regression corpus.

- **Gate 4 (Target Harness - QuickJS)**: `PASS`
  - Command: `/tmp/test_quickjs_runner`
  - Result: 15/15 tests passing across 6 suites (Failures: 0).

- **Gate 5 (Rule EF-3 Hardened Probe Gauntlet)**: `PASS`
  - Command: `./zcc tests/probe_inline_asm.c -o /tmp/probe_asm.s && gcc /tmp/probe_asm.s -o /tmp/probe_asm && /tmp/probe_asm`
  - Result: `PROBE_ASM PASS: all 13 extended inline asm tests succeeded!` (Exit code 0).

## Hardening Additions Verified
1. Named operands `%[name]` and modifiers `%k[name]`.
2. Immediate integer constraints `"i"` and `"n"` using `$val` direct literal substitution.
3. Callee-saved preservation: automatic push/pop wrapper for `%rbx` and `%r12`..`%r15` upon allocation or clobber.
4. Memory operands `"m"` with stack/rip offset addressing.
5. Trailing semicolon, tab, and `\r\n` whitespace stripping.
6. Hardware primitives: `rdtsc` and 4-register `cpuid`.

## Final Status

**STATUS: CLOSED (VERIFIED & HARDENED)**
