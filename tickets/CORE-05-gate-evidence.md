# ZCC-CORE-05: Class II Tri-Compiler Differential Gate Evidence

## Milestone Identity & Contract Adjudication

| Attribute | Value |
| :--- | :--- |
| **Milestone** | ZCC-CORE-05 — Class II Tri-Compiler Differential on Standalone C Corpus |
| **Governing Rule** | ZCC-EF-7 v1.0.0 |
| **Evidence Tier** | E2 (`evidence_tier_assigned_at: 2026-10-26T07:42:00Z`, predates sweep `07:47:11Z`) |
| **Evidence Class** | OBSERVED (harness, controls, frozen fixture list, contract, ledgers); REPORTED (execution tuples) |
| **Adjudication Verdict** | **FAIL (E2; 131/136 Bit-Exact Passes [96.32%], 4 Root-Cause Defect Deliverables)** |
| **Adjudication Authority** | Verification Architect |
| **Lifecycle State** | **CLOSED (Completed Milestone Outcome per §5: FAIL with 4 Root-Cause Defect Deliverables)** |
| **Primary Ledger (Active)** | `docs/evidence/2026-10-26/core_05_classII/ledger_20261026_074711_675727.json` |
| **Primary Ledger SHA-256** | `83bc7b9dd427745f4670a90095b91fd9a64a7d7fe99e4bc2a618b34b84464326` |
| **Retained Prior Ledger** | `docs/evidence/2026-10-26/core_05_classII/ledger_20261026_074437_719501.json` (superseded; retained per A5) |
| **Frozen Fixture List** | `tests/core05_fixtures.txt` (136 fixtures) |
| **Fixture List SHA-256** | `3d35b5432c827f0d95da23243ece62efd981c0d110535c65016b5f1a7829f131` |
| **Baseline SHA** | `1a56cd6a6` |

---

## 1. Process Deviations (Amendment A1)

- **§3 Pre-Stamp Transcript Synchronization**:
  The raw outputs for `./tests/test_corpus.sh` and `python3 tools/test_zcc_opt_passes.py | tail -20` were executed, observed, and reviewed in the session prior to and during contract finalization. However, the contract timestamp (`07:42:00Z`) was recorded on disk before the verbatim terminal transcript was formally synced into the gate ticket. This does not invalidate the sweep (as fixture filtering under §2 was strictly ZCC-independent), but is formally recorded as a **PROCESS DEVIATION** in accordance with Rule ZCC-EF-0 and ZCC-EF-7 §1.

---

## 2. Fixture Funnel & Mechanical Filter Verification (Amendment A2)

### Funnel Metrics
- **Initial Uncurated Corpus**: 526 candidate files (385 in `tests/test_*.c` + 141 in `./test_*.c`).
- **Target Candidate Pool for CORE-05**: 385 candidate files (`tests/test_*.c`).
- **Evaluated under §2**: 385 files.

### Per-Criterion Exclusion Breakdown (§2a–f)
| Criterion | Description | Excluded Count | Notes |
| :--- | :--- | :--- | :--- |
| **Excluded by Name** | Known negative/diagnostic tests | 1 | `tests/test_ir_bridge_guard.c` (asserts compile failure) |
| **§2e Banned Pattern** | Non-deterministic / environmental calls | 44 | Matches `time(`, `clock(`, `rand(`, `srand(`, `getpid(`, `argv`, `getenv(`, `fopen(`, `/dev/urandom` |
| **§2a Preprocess Fail** | `gcc -E -P` failure | 3 | Syntax/macro preprocessing aborts |
| **§2b GCC Build Fail** | `gcc -O0 -fno-builtin -no-pie` build failure | 156 | Multi-unit fragments, missing companion `.o` objects |
| **§2c GCC Non-Zero Exit**| GCC binary runtime exit $\neq 0$ | 20 | Test expected command-line flags or failed assertion |
| **§2d Non-Deterministic**| Consecutive GCC runs yield different output | 2 | Run 1 output $\neq$ Run 2 output |
| **§2b Clang Build Fail**| `clang -O0 -fno-builtin -no-pie` build failure | 23 | Clang-specific warnings treated as errors or Clang rejections |
| **§2f Reference Divergence** | GCC vs Clang runtime output mismatch | 0 | 0 reference divergences among the 385 `tests/test_*.c` candidates |
| **Total Included** | Passed all criteria (a)–(f) | **136** | Frozen into `tests/core05_fixtures.txt` |

*(Corpus Finding on §2f: When evaluating the wider 526 set including root `test_*.c`, exactly 1 file—`test_bitfield.c` in root—exhibited GCC vs Clang runtime output divergence under `-O0`, and was excluded. Registered in `BUGS.md` as `BUG-CORE05-002`).*

### Confirmation: `tools/filter_core05_corpus.py` Zero-ZCC Execution
Inspection of [tools/filter_core05_corpus.py](file:///h:/__DOWNLOADS/zcc_github_upload/tools/filter_core05_corpus.py) confirms that ZCC is never called. The only occurrences of the string `zcc` in the script are the module docstring and the argparse help string:
```python
# Lines 1-18 of tools/filter_core05_corpus.py:
"""
tools/filter_core05_corpus.py
Implements §2 mechanical, ZCC-independent filtering for ZCC-CORE-05.
Evaluates candidates strictly against host GCC 13.3.0 and Clang 18.1.3:
...
"""
```
Execution is performed strictly via:
```python
# (a) gcc -E -P
cmd_pp = ["gcc", "-E", "-P"] + INCLUDES + [rel_path, "-o", tu_i]
# (b) gcc compile/link
cmd_gcc_build = ["gcc", "-O0", "-fno-builtin", "-no-pie", tu_i, "-lm", "-o", bin_gcc]
# (f) clang compile/link
cmd_clang_build = ["clang", "-O0", "-fno-builtin", "-no-pie", tu_i, "-lm", "-o", bin_clang]
```
ZCC behavior had zero influence on fixture selection.

---

## 3. ZCC CLI `.i` Delegation Hazard & Prior Harness Audit (Amendment A3)

### The Defect Mechanism
In `part5.c:2038`, ZCC's command-line argument parser classifies source input files strictly by file extension:
```c
int len = strlen(argv[i]);
if (len > 2 && (strcmp(argv[i] + len - 2, ".c") == 0 || (len > 4 && strcmp(argv[i] + len - 4, ".cpp") == 0) || ...)) {
    input_file = argv[i];
} else {
    if (extra_link_args[0]) strncat(extra_link_args, " ", 4095 - (int)strlen(extra_link_args));
    strncat(extra_link_args, argv[i], 4095 - (int)strlen(extra_link_args));
}
```
When passed a `.i` file:
1. `input_file` remains `NULL`.
2. `argv[i]` (`X.i`) falls into `extra_link_args`.
3. At line 2180, ZCC encounters `!input_file && extra_link_args[0] && output_file` and executes:
   ```c
   snprintf(cmd, sizeof(cmd), "gcc %s -O0 -w -no-pie ... -o %s %s -lm -lpthread -ldl 2>&1", prefix_map, output_file, extra_link_args);
   return system(cmd);
   ```
4. ZCC silently invokes host `gcc` as a linker/compiler wrapper, delegating the entire compilation to GCC and creating a catastrophic silent fail-open.

### Registration
- Registered in `AGENTS.md` and `.agents/AGENTS.md` as **E-LEARN-024**.
- Registered in `BUGS.md` as **BUG-CORE05-001**.
- Mitigation: All preprocessed translation units must be named `.tu.c` or `.i.c` to force ZCC's native C compiler frontend to parse and compile the source.

### Audit of Prior CORE Harnesses (CORE-01 through CORE-04)
A grep audit across all prior `tools/run_core_*.py` harnesses confirmed that **no prior CORE harness fed raw `.i` files to ZCC**:
- **CORE-01** (`tools/run_core_01_60_repro_sweep.py`): Invoked `./zcc` on `tests/repro_case_*.c` (ends in `.c`).
- **CORE-02 Stage 1** (`tools/run_core_02_stage1_differential.py`): Invoked `./zcc` on `zcc_svg_harness.c`, `sqlite3.c` (ends in `.c`).
- **CORE-02 Stage 2** (`tools/run_core_02_stage2_differential.py`): Explicitly generated `out_i = os.path.join(workdir, f"{mod}.i.c")` (ends in `.c`).
- **CORE-03** (`tools/run_core_03_sqlite_differential.py`): Invoked `./zcc` on `tests/sqlite3_zcc_amalgamation.c` (ends in `.c`).
- **CORE-04** (`tools/run_core_04_curl_differential.py`): Preprocessed to `pp_file = os.path.join(zcc_pp_dir, f"{src}_pp.c")` (ends in `.c`).

**Conclusion**: All prior CORE milestones executed native ZCC compilation. The `.i` silent fallback vector was discovered during CORE-05 pre-flight checks and mitigated before execution.

---

## 4. Ledger Lineage (Amendment A5)

Two ledgers exist under `docs/evidence/2026-10-26/core_05_classII/`:
1. `ledger_20261026_074437_719501.json` (**SUPERSEDED, RETAINED**):
   - First complete sweep run.
   - Truncated compile error diagnostics because the harness captured `stderr` only, while ZCC prints compilation error banners to `stdout`.
2. `ledger_20261026_074711_675727.json` (**ACTIVE, DEFINITIVE**):
   - SHA-256: `83bc7b9dd427745f4670a90095b91fd9a64a7d7fe99e4bc2a618b34b84464326`.
   - Updated harness to capture combined `stdout` and `stderr` on compile failures.
   - Retains full diagnostic error strings for all 5 defect fixtures.

---

## 5. Verified Gate Results

### Gate 1 — Self-Host Fixed Point (Mandatory)
```text
cmp zcc2.s zcc3.s
```
- **Exit Code**: `0`
- **MD5 Hashes**:
  - `zcc2.s`: `8196edc7abca56711f743ecacd8f0284`
  - `zcc3.s`: `8196edc7abca56711f743ecacd8f0284`
- **Verdict**: **PASS**

---

### Verification Controls (§6)
- **POS-01 (Known-Good Control)**: `tests/test_7arg_test.c` compiled by ZCC, GCC 13.3, and Clang 18.1 from byte-identical `test_7arg.tu.c`.
  - Exit code: `0` across all three compilers.
  - Stdout: `SUCCESS: 7-arg passing works (28)` (3-way byte-identical).
  - Status: **PASS**.
- **NEG-01 (Jump/Logic Mutation Control)**: Mutated `movq $28, %r11` $\to$ `movq $99, %r11` and `sete %al` $\to$ `setne %al` in emitted ZCC assembly.
  - Observed: Binary exited `1` with stdout `FAILURE: Got 28`.
  - Status: **PASS** (harness successfully detects output divergence and rejects mutated candidate).
- **NEG-02 (Truncated TU Syntax Fail-Closed Control)**: Severed closing block of `main` in `pos_01.tu.c`.
  - Observed: ZCC compile failed with exit `1` (`error: expected RBRACE (93), got EOF (0)`).
  - Status: **PASS** (compiler and harness fail closed on truncated inputs).

---

## 6. Main Differential Sweep Results

- **Total Fixtures Evaluated**: 136
- **Bit-Exact 3-Way Passes**: 131 (96.32%)
- **Divergences**: 5 (3.68%)
- **Runtime Divergences**: **0** (no runtime divergence observed across 131 fixtures at `-O0`).
- **Composite FNV-1a Digest**:
  - GCC:   `0xf03c7f817c17685`
  - Clang: `0xf03c7f817c17685` (100% reference consensus across all 136 fixtures)
  - ZCC:   `0x8abd7b56269fa7e2` (matches GCC/Clang on all 131 passing fixtures)

---

## 7. The 4 Root-Cause Defect Deliverables (Amendment A4)

Per Contract §5, a milestone verdict of FAIL on $N$ fixtures is a successful, closable milestone outcome where the defects constitute the deliverable. The 5 observed divergences deduplicate into **4 distinct root causes**, all classified as **INFERRED** from compiler diagnostics:

### DEFECT-CORE05-01: Statement Expressions in Macros (`({ ... })`)
- **Evidence Class**: **INFERRED**
- **Symptom Fixture**: `tests/test_flipper_dpo_policy.c`
- **Phase**: `compile`
- **Observed Diagnostic**: `/tmp/...:545: error: unexpected token 92 (LBRACE) in expression`
- **Inference**: Inlined glibc `<assert.h>` expands assertion checks into GNU statement expressions `({ ... })`. ZCC's expression parser (`part2.c`) does not parse statement expressions in expression position.
- **Reference Consensus**: GCC exit `0`, Clang exit `0`.
- **Proposed Decisive Experiment (Minimal Repro Oracle)**:
  ```c
  int main(void) {
      int x = ({ int y = 1; y + 1; });
      return (x == 2) ? 0 : 1;
  }
  ```

### DEFECT-CORE05-02: Undeclared `__PRETTY_FUNCTION__` Identifier
- **Evidence Class**: **INFERRED**
- **Symptom Fixture**: `tests/test_ir_spill_forced.c`
- **Phase**: `compile`
- **Observed Diagnostic**: `/tmp/...:338: error: use of undeclared identifier '__PRETTY_FUNCTION__'`
- **Inference**: Glibc assertion handlers pass `__PRETTY_FUNCTION__` as a string argument. ZCC does not inject `__PRETTY_FUNCTION__` into function scope during lexical analysis or AST creation.
- **Reference Consensus**: GCC exit `0`, Clang exit `0`.
- **Proposed Decisive Experiment (Minimal Repro Oracle)**:
  ```c
  int main(void) {
      const char *s = __PRETTY_FUNCTION__;
      return (s != 0) ? 0 : 1;
  }
  ```

### DEFECT-CORE05-03: Missing Byte-Swap Builtins (`__builtin_bswap16/32/64`)
- **Evidence Class**: **INFERRED**
- **Symptom Fixture**: `tests/test_posix_socket_native.c`
- **Phase**: `compile`
- **Observed Diagnostic**: `/tmp/...:106: error: use of undeclared identifier '__builtin_bswap16'`
- **Inference**: Inlined `<netinet/in.h>` and `<byteswap.h>` lower `htons()` / `htonl()` to `__builtin_bswap*`. ZCC's symbol table does not register `__builtin_bswap*` functions.
- **Reference Consensus**: GCC exit `0`, Clang exit `0`.
- **Proposed Decisive Experiment (Minimal Repro Oracle)**:
  ```c
  int main(void) {
      unsigned short x = __builtin_bswap16(0x1234);
      return (x == 0x3412) ? 0 : 1;
  }
  ```

### DEFECT-CORE05-04: Undeclared `__builtin_va_start` (Deduped from `test_va.c` & `test_va2.c`)
- **Evidence Class**: **INFERRED**
- **Symptom Fixtures**: `tests/test_va.c` and `tests/test_va2.c`
- **Phase**: `compile`
- **Observed Diagnostic**: `/tmp/...:310: error: use of undeclared identifier '__builtin_va_start'`
- **Inference**: Inlined glibc `<stdarg.h>` expands `va_start` into `__builtin_va_start`. ZCC does not register `__builtin_va_start` as a recognized builtin identifier in standard translation units.
- **Reference Consensus**: GCC exit `0`, Clang exit `0`.
- **Proposed Decisive Experiment (Minimal Repro Oracle)**:
  ```c
  #include <stdarg.h>
  int foo(int n, ...) {
      va_list ap;
      va_start(ap, n);
      int x = va_arg(ap, int);
      va_end(ap);
      return x;
  }
  int main(void) {
      return (foo(1, 42) == 42) ? 0 : 1;
  }
  ```

---

## 8. Milestone Conclusion

**Final Verdict: FAIL (E2; Bounded Closure per Contract §5)**

- **Observed Truth**: No runtime divergence observed across 131 fixtures at `-O0`.
- **Hypothesis Alignment**: Consistent with the hypothesis that Class III adaptation in CORE-03 and CORE-04 was required by inlined glibc header constructs; CORE-03/04 shims also covered socket/VFS typedefs not exercised here.
- **Next Steps**: Each of the 4 root-cause defects constitutes an independent E1 remediation target with its minimal reproducer as the correctness oracle. Re-running the 136-fixture corpus after defect remediation will constitute a distinct follow-up milestone (CORE-05b).
