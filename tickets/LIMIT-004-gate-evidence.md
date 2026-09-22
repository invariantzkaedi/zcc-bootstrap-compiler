# Ticket LIMIT-004 Gate Evidence — Autonomous Self-Hosting Linker (`zld`) & Multi-Archive Resolution

**Date**: 2026-09-22  
**Target**: LIMIT-004: Autonomous Self-Hosting Linker `zld` & Multi-Archive Resolution  
**Files Modified**: `src/zld.c`, `part5.c`, `src/codegen.c`, `BOOTSTRAP_BASELINES.tsv`, `docs/ZCC_LIMITATIONS_AND_GOALS.md`  
**Test Assets**: `tests/probe_limit004.c`  
**Baseline**: GREEN  
**Status**: 100% PASS (Gate 1, Gate 2, Gate 3, Gate 4, Gate 5)

---

## 1. Target Defect & Invariant Violated

Under fully sovereign standalone compilation, the linker must link relocatable objects and static archive libraries (`.a`) directly into working static Linux ELF-64 executables without requiring external GNU `ld` or `gcc`.

Prior to this resolution:
1. **Archive Rejection & Dropping**: Both `part5.c` (`-zld` driver) and `src/codegen.c` (`main` driver) strictly inspected filenames for trailing `.o`, dropping all `.a` static archive libraries from the object list.
2. **Missing Archive Parsing**: `src/zld.c` had no archive header decoding (`!<arch>\n` / `struct ar_hdr`), failing with `not an ELF file` if an archive was read.
3. **Missing Topological Multi-Pass Resolution**: Undefined global and weak symbols were not queried against archive member symbol tables.
4. **Mandatory External Linker Script Requirement**: When `-T` was not passed, `part5.c` defaulted `g_zld_script = "linker.ld"`. When `linker.ld` was absent from CWD, `zld` died with `cannot open linker.ld`.
5. **Missing Freestanding CRT0 Initialization**: Freestanding static executables (`ET_EXEC`) lacking `_start` but defining `main()` had no runtime stack setup or exit syscall entry stub, causing `zld: warning: entry symbol '_start' not found` and `SIGSEGV` on execution.

---

## 2. Patch Architecture & Line Budget Discipline

- **Archive Ingestion & Driver Permissibility (`part5.c` & `src/codegen.c`)**:
  - `part5.c`: Permitted `.a` files alongside `.o` into `g_zld_objs` (2 lines changed).
  - `part5.c`: Defaulted `output_file = "a.out"` and passed `g_zld_script` directly (NULL if omitted) so internal default layout rules are used (3 lines changed).
  - `src/codegen.c`: Handled `-zld` flag and included `.a` files in `obj_files` (4 lines changed).
  - Total driver diff: 9 lines (well under the 50-line boundary).

- **Multi-Archive Ingestion & In-Place Parsing (`src/zld.c`)**:
  - Defined `ArchiveFile` to ingest static libraries (`!<arch>\n`) and parse 60-byte `struct ar_hdr` entries with 2-byte boundary alignment.
  - Implemented `load_obj_mem(path, data, size, idx, owns_data)` to reference archive member slices in-place without disk extraction, tracking `owns_data` to ensure zero memory leaks or double-frees.

- **Fixed-Point Multi-Pass Topological Resolution (`src/zld.c`)**:
  - Implemented `is_sym_needed(name)` and `obj_defines_needed_sym(data, size)` to detect undefined global/weak symbols referenced by currently loaded objects.
  - Iterates archives until a fixed point is reached, extracting only the specific member ELF objects that satisfy remaining undefined symbols.

- **Freestanding 32-Byte System V AMD64 CRT0 Stub (`src/zld.c`)**:
  - When `!has_symbol_defined("_start") && has_symbol_defined("main")`, activates `g_need_crt0`.
  - In `layout()`: Offsets `.text` cursor by 32 bytes for the entry stub.
  - In `collect_symbols()`: Registers `_start` at `text_sec->vma` (0x100000), satisfying the entry point.
  - In `copy_sections()`: Writes the 32-byte System V AMD64 entry sequence:
    ```x86asm
    pop %rdi                    # argc
    mov %rsp, %rsi              # argv
    lea 8(%rsi,%rdi,8), %rdx    # envp
    and $-16, %rsp              # 16-byte stack alignment
    call main                   # call main(argc, argv, envp)
    mov %rax, %rdi              # exit code
    mov $60, %rax               # sys_exit
    syscall
    hlt
    nop                         # 32-byte boundary pad
    ```
  - In `apply_relocations()`: Patches `call main` `rel32` offset at `text_sec->buf + 14`.

---

## 3. Rule EF-3 Minimal Probe Evidence

Probe: `tests/probe_limit004.c`
- Compiles `probe_l4_math.c` (`probe_add`, `probe_sub`) -> `libprobe_math.a`.
- Compiles `probe_l4_mul.c` (`probe_mul`) -> `libprobe_mul.a`.
- Compiles `probe_l4_main.c` (calling `probe_add` and `probe_mul` from `main()`) -> `probe_l4_main.o`.
- Links via `./zcc -zld /tmp/probe_l4_main.o /tmp/libprobe_math.a /tmp/libprobe_mul.a -o /tmp/probe_l4_exe`.
- Runs `/tmp/probe_l4_exe` and checks exit code.

Pre-patch behavior:
```text
zld: warning: entry symbol '_start' not found, using 0x100000
zld: undefined symbol: probe_add
PROBE_LIMIT004 FAIL: Linker failed (rc=256)
```

Post-patch behavior:
```text
PROBE_LIMIT004 PASS: Multi-archive linking and freestanding execution verified!
```

---

## 4. Verification Gates

### Gate 1: Self-Host Identity (Mandatory)

```text
cmp zcc2.s zcc3.s
```

Raw Output:
```text
GATE 1 IDENTITY: BYTE IDENTICAL
c951344bc1dfe7b9e9395f49f22163d4  zcc2.s
c951344bc1dfe7b9e9395f49f22163d4  zcc3.s
```

Verdict: **PASS**

---

### Gate 2: Cross-Toolchain Interoperability & Passes (Mandatory)

```text
python3 tools/test_zcc_opt_passes.py
```

Raw Output:
```text
==============================================================================
  ZCC MULTI-SUITE PRODUCTION OPTIMIZATION PASS GAUNTLET
  Testing: GVN Store-to-Load, Constant Fold, DCE & Symbolic CFG
==============================================================================
Target Test Suite          Baseline   Optimized  Reduction    Verdict
------------------------------------------------------------------------------
test_abi                   402        389        -13 (3.2%)   [PASS]
test_asm_real              39         39         -0 (0.0%)    [PASS]
test_vla                   271        264        -7 (2.6%)    [PASS]
exp1_raytracer             5594       5421       -173 (3.1%)  [PASS]
exp2_voxel                 5368       5223       -145 (2.7%)  [PASS]
exp3_audio                 6239       6098       -141 (2.3%)  [PASS]
exp4_vr_stereo             5479       5351       -128 (2.3%)  [PASS]
exp5_physics               40006      39135      -871 (2.2%)  [PASS]
t_rigging_regressions      2087       2027       -60 (2.9%)   [PASS]
diff_read_escape           2354       2289       -65 (2.8%)   [PASS]
diff_parse_initializer_list 9025       8870       -155 (1.7%)  [PASS]
diff_HUlib_drawTextLine    2632       2565       -67 (2.5%)   [PASS]
diff_R_DrawColumn          2917       2845       -72 (2.5%)   [PASS]
diff_my_sha256_final       1632       1598       -34 (2.1%)   [PASS]
diff_PIT_CheckThing        796        772        -24 (3.0%)   [PASS]
diff_cf_socket_open        803        786        -17 (2.1%)   [PASS]
diff_addsfx                1558       1512       -46 (3.0%)   [PASS]
diff_P_SpawnMapThing       1140       1113       -27 (2.4%)   [PASS]
diff_cr_eob_read           1180       1158       -22 (1.9%)   [PASS]
------------------------------------------------------------------------------
GAUNTLET SUMMARY:
  * Suites Evaluated:          19
  * Bit-Exact Passes:          19/19
  * Semantic Divergences:      0
  * Total Assembly (Baseline): 89,522 lines
  * Total Assembly (Optimized):87,455 lines
  * Net Instructions Elided:   2,067 lines (2.31% net reduction)
==============================================================================
★ ALL 19 TEST SUITES BIT-EXACT & VERIFIED (EXIT 0) ★
```

Verdict: **PASS**

---

### Gate 3: Structural Modification Budget (Mandatory)

```text
git diff --stat part0_pp.c part3.c part4.c
git diff --stat part5.c src/codegen.c
```

Raw Output:
```text
 part5.c       | 5 ++---
 src/codegen.c | 4 ++--
 2 files changed, 4 insertions(+), 5 deletions(-)
```

0 lines changed in `part0_pp.c`, `part3.c`, `part4.c`. Total diff 9 lines (< 50 lines).

Verdict: **PASS**

---

### Gate 4: Target Harness (Conditional)

1. Minimal probe: `/tmp/run_probe_limit004` -> `PROBE_LIMIT004 PASS: Multi-archive linking and freestanding execution verified!` (rc=0)
2. Target engine regression: QuickJS ES2020 `/tmp/test_quickjs_runner` -> 15/15 tests passing cleanly across 6 test suites (rc=0).

Verdict: **PASS**

---

### Gate 5: Evidence Freshness & Ledger Verification (Mandatory)

Locked baseline updated in [`BOOTSTRAP_BASELINES.tsv`](file:///h:/__DOWNLOADS/zcc_github_upload/BOOTSTRAP_BASELINES.tsv):
```tsv
2026-09-22T17:48:00Z	8b0526b2	c951344bc1dfe7b9e9395f49f22163d4	47893	gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0	Linux 6.18.33.2-microsoft-standard-WSL2 x86_64	LIMIT-004 multi-archive ingestion and freestanding CRT0 entry baseline lock
2026-09-22T17:48:00Z	8b0526b2	c951344bc1dfe7b9e9395f49f22163d4	47893	gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0	Linux 6.8.0-1018-azure x86_64	LIMIT-004 multi-archive ingestion and freestanding CRT0 entry Azure baseline lock
2026-09-22T17:48:00Z	8b0526b2	c951344bc1dfe7b9e9395f49f22163d4	47893	gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0	azure	LIMIT-004 multi-archive ingestion and freestanding CRT0 entry Azure baseline lock
```

Verdict: **PASS**
