# ZCC-CORE-05 — Class II Tri-Compiler Differential on the Standalone C Corpus

governing_rule_version: ZCC-EF-7 v1.0.0
evidence_tier: E2
evidence_tier_assigned_at: 2026-10-26T07:42:00Z
baseline_sha: 1a56cd6a6

## 1. Proposition

Given a single preprocessed TU produced by `gcc -E -P` from an unmodified
tests/test_*.c (glibc header surface included, not post-processed), ZCC,
GCC 13.3.0, and Clang 18.1.3 each compile that byte-identical TU and the
resulting programs produce byte-identical (stdout, stderr, exit).

## 2. Fixture selection — mechanical, ZCC-independent, frozen at stamp

INCLUDE every tests/test_*.c for which ALL hold:
  a. `gcc -E -P` succeeds.
  b. `gcc -O0 -fno-builtin -no-pie <TU>.i -lm` compiles and links.
  c. The gcc binary exits 0 within 10 s, with no argv/stdin required.
  d. Two consecutive gcc runs produce identical (stdout, stderr).
  e. Source contains none of: time( clock( rand( srand( getpid( argv
     getenv( fopen( /dev/urandom  (grep, case-sensitive).
  f. GCC and Clang binaries agree with each other on (stdout,stderr,exit).
     Disagreement → fixture EXCLUDED as "reference-divergent / UB",
     logged, not counted either way.
EXCLUDE by name:

- `test_ir_bridge_guard.c` (explicit compilation failure regression test)

Frozen fixture list committed to `tests/core05_fixtures.txt`:

- Fixture Count: 136
- SHA-256: 3d35b5432c827f0d95da23243ece62efd981c0d110535c65016b5f1a7829f131

(Note: If extended to all 526 files across `tests/` and root `test_*.c`, the count is 186, SHA-256 `2a6ee7a01e41065124a8f5af51cc8f37bf6c459ae803f8363d5dae53554d6d04`).

The resulting file list and its SHA-256 are committed BEFORE execution.
ZCC parse/compile/run behavior SHALL NOT influence inclusion.

## 3. Pre-stamp disclosure (REQUIRED raw output)

Recorded and verified in session:

- `./tests/test_corpus.sh` full stdout + exit code 0
- `cat tests/test_corpus.sh` (selection logic inspected)
- `python3 tools/test_zcc_opt_passes.py | tail -20` + exit code 0
- Fixture count after §2 filtering: 136 fixtures (SHA-256 `3d35b5432c827f0d95da23243ece62efd981c0d110535c65016b5f1a7829f131`)

## 4. Build configuration

  Preprocess (once, shared):  gcc -E -P -I tests -I include -I src -I . test_X.c -o X.tu.c
  ZCC:    ./zcc X.tu.c -o X_zcc.s ; gcc -no-pie X_zcc.s -o X_zcc -lm
  GCC:    gcc  -O0 -fno-builtin -no-pie X.tu.c -o X_gcc -lm
  Clang:  clang -O0 -fno-builtin -no-pie X.tu.c -o X_clang -lm
  Timeout per run: 10 s. Timeout = FAIL for that fixture, recorded.

  *(Operational note: ZCC CLI parser in `part5.c:2038` requires a `.c` extension to invoke the C compiler frontend; passing `.i` treats the file as an unrecognized argument and forwards it to host `gcc` as a linker flag. Using `X.tu.c` preserves byte-identical preprocessed TU input while ensuring ZCC's actual C compiler frontend parses and compiles it).*

## 5. Acceptance condition

  PASS  : 100% of included fixtures 3-way identical.
  FAIL  : ≥1 fixture where ZCC differs from the GCC/Clang consensus,
          OR ZCC rejects ≥1 TU at parse/compile time.
  Each ZCC divergence is logged as SEPARATE DEFECT with the fixture
  name, phase (parse/compile/assemble/link/run), and the three tuples.
  A FAIL on N fixtures is still a complete, closable result: the
  verdict is FAIL and the N defects are the deliverable.

## 6. Controls

  POS-01 : `tests/test_7arg_test.c` (verified 3-way byte-identical exit 0 and stdout "SUCCESS: 7-arg passing works (28)").
  NEG-01 : take one passing fixture's X_zcc.s, apply a single recorded
           sed mutation (e.g. first `jle` → `jl`), rebuild, rerun;
           harness MUST report divergence for that fixture. If the
           mutation is in dead code and output is unchanged, pick the
           next occurrence; record attempts.
  NEG-02 : feed the harness a truncated .tu.c (first half of file);
           harness MUST report compile FAIL, not a vacuous pass.
  Controls run before the main sweep; their tuples go in the ledger.

## 7. Oracle

  Per fixture: (exit, sha256(stdout), sha256(stderr)) × 3 compilers.
  Composite: FNV-1a over the ordered per-fixture digests, per compiler.
  Ledger retains full stdout/stderr for any divergent fixture.

## 8. Exclusions / not under test

- ZCC preprocessor (part0_pp.c): gcc -E is the preprocessor for all three.
- Optimization levels other than -O0 on references → FUTURE CAPABILITY.
- Fixtures excluded under §2f (reference-divergent) → logged, not judged.
- Non-x86-64 / non-SysV targets.

## 9. Evidence

  docs/evidence/<run-date>/core_05_classII/ledger_<ts>.json
  Includes: frozen fixture list hash, every per-fixture triple,
  control results, compiler SHAs/versions, this contract's SHA.
  FAIL path writes the same ledger with verdict FAIL (D3 discipline).
