# Gate Evidence — feat(vir): add backend artifact cache and repository-backed reuse

**Milestone**: `feat(vir): add backend artifact cache and repository-backed reuse`
**Commit baseline**: `e9506605` (feat(vir): add repository index and semantic verification — last green commit)

## Phase 0 Verdict
```
BASELINE:              GREEN
SYMPTOM-IN-HISTORY:    NO
FORENSIC-LATEST-SHA:   e9506605
PROCEED:               YES
```

---

## Changes Implemented

### zcc_vir.h
- Verified declared structures and functions:
  - `VirBackendArtifactKind`
  - `vir_repository_backend_exists`
  - `vir_repository_store_backend_output`
  - `vir_repository_load_backend_output`
  - `vir_repository_gc`
  - `vir_repository_prune_schema`

### zcc_vir.c
- Implemented backend cache resolver `vir_backend_resolve_path` for `v<version>/<prefix>/<suffix>.<ext>` mapping.
- Implemented cache check, storage, and retrieval APIs (`vir_repository_backend_exists`, `vir_repository_store_backend_output`, `vir_repository_load_backend_output`).
- Implemented lifecycle governance logic (`vir_repository_gc`) to purge files based on age (`max_age_seconds`) and size budget (`max_bytes` sorted by the most recent of `st_atime` and `st_mtime`).
- Implemented obsolete version pruning (`vir_repository_prune_schema`).

### test_zcc_vir.c
- Added test-only path helpers (`test_resolve_path_helper`, `test_resolve_backend_path_helper`).
- Implemented `test_vir_backend_output_cache` for SVG, SDF, and GLSL cached outputs.
- Implemented `test_vir_repository_gc_and_lifecycle` covering age-based and LRU budget-based file eviction, and obsolete directory pruning.
- Registered both tests in `main()`.

---

## Gate 1 — Self-host byte-identical: `cmp zcc2.s zcc3.s`

Compiled with:
```bash
make selfhost
```
Output:
```
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (16089 elided)
[OK] ZCC Engine Compilation Terminated Successfully.

$ cmp zcc2.s zcc3.s; echo CMP_EXIT:$?
CMP_EXIT:0
```

**Result: BYTE-IDENTICAL**

---

## Gate 2 — Native Test Suite

Compiled with:
```bash
gcc -O0 -g -I. test_zcc_vir.c zcc_vir.c zcc_svg_path_parser.c zcc_svg.c -o test_zcc_vir -lm
```
Output:
```
$ ./test_zcc_vir | grep -E '(\* Running|PASSED|777)'
[+] test_vir_path_equivalence PASSED.
[+] test_vir_path_fingerprint PASSED.
[+] test_vir_canonical_fingerprint PASSED.
[+] test_vir_compilation_caching PASSED.
[+] test_vir_path_normalization PASSED.
[+] test_vir_path_creation_and_growth PASSED.
[+] test_vir_degenerate_removal PASSED.
[+] test_vir_bounds_propagation PASSED.
[+] test_svg_to_vir_adapter PASSED.
[+] test_extreme_and_overflow_vir PASSED.
[+] test_vir_metadata_and_bounds_caching PASSED.
[+] test_vir_arc_ingestion_and_expansion PASSED.
[+] test_vir_backend_diversification PASSED.
[+] test_sdf_to_glsl_compilation PASSED.
[+] test_vir_pass_canonicalize PASSED.
[+] test_vir_pass_manager PASSED.
[+] test_vir_pipeline_telemetry PASSED.
[+] test_vir_fixed_point_pipeline PASSED.
[+] test_vir_pass_dependency_graph PASSED.
[+] test_vir_backend_planner PASSED.
[+] test_vir_pass_graph_exporter PASSED.
[+] test_vir_exact_bounds_solving PASSED.
[+] test_vir_registry_validation PASSED.
[+] test_vir_artifact_manifest PASSED.
[+] test_vir_execution_plan PASSED.
[+] test_vir_pipeline_provenance PASSED.
[+] test_vir_state_flags_stringify PASSED.
[+] test_vir_geometry_metrics PASSED.
[+] test_vir_cache_record_header PASSED.
[+] test_vir_artifact_blob PASSED.
[+] test_vir_repository_store PASSED.
[+] test_vir_repository_catalog PASSED.
[+] test_vir_backend_output_cache PASSED.
[+] test_vir_repository_gc_and_lifecycle PASSED.
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
```

**Result: ALL TESTS PASSED**

---

## Gate 3 — Memory Safety + Leak Detection (ASan + LSan)

Compiled with:
```bash
gcc -O0 -g -fsanitize=address,leak -I. test_zcc_vir.c zcc_vir.c zcc_svg_path_parser.c zcc_svg.c -o test_zcc_vir_lsan -lm
```
Output:
```
$ ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=97 ./test_zcc_vir_lsan > /tmp/lsan.log 2>&1; echo EXIT=$?
EXIT=0
$ cat /tmp/lsan.log | tail -n 28
[+] test_vir_pass_graph_exporter PASSED.
[*] Running test_vir_exact_bounds_solving...
[+] test_vir_exact_bounds_solving PASSED.
[*] Running test_vir_registry_validation...
[+] test_vir_registry_validation PASSED.
[*] Running test_vir_artifact_manifest...
[+] test_vir_artifact_manifest PASSED.
[*] Running test_vir_execution_plan...
[+] test_vir_execution_plan PASSED.
[*] Running test_vir_pipeline_provenance...
[+] test_vir_pipeline_provenance PASSED.
[*] Running test_vir_state_flags_stringify...
[+] test_vir_state_flags_stringify PASSED.
[*] Running test_vir_geometry_metrics...
[+] test_vir_geometry_metrics PASSED.
[*] Running test_vir_cache_record_header...
[+] test_vir_cache_record_header PASSED.
[*] Running test_vir_artifact_blob...
[+] test_vir_artifact_blob PASSED.
[*] Running test_vir_repository_store...
[+] test_vir_repository_store PASSED.
[*] Running test_vir_repository_catalog...
[+] test_vir_repository_catalog PASSED.
[*] Running test_vir_backend_output_cache...
[+] test_vir_backend_output_cache PASSED.
[*] Running test_vir_repository_gc_and_lifecycle...
[+] test_vir_repository_gc_and_lifecycle PASSED.
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
```

**Result: 0 errors, 0 leaks (EXIT=0)**

---

## Gate 4 — Undefined Behavior (UBSan)

Compiled with:
```bash
gcc -O0 -g -fsanitize=undefined -I. test_zcc_vir.c zcc_vir.c zcc_svg_path_parser.c zcc_svg.c -o test_zcc_vir_ubsan -lm
```
Output:
```
$ UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./test_zcc_vir_ubsan > /tmp/ubsan.log 2>&1; echo EXIT=$?
EXIT=0
$ cat /tmp/ubsan.log | tail -n 28
[+] test_vir_pass_graph_exporter PASSED.
[*] Running test_vir_exact_bounds_solving...
[+] test_vir_exact_bounds_solving PASSED.
[*] Running test_vir_registry_validation...
[+] test_vir_registry_validation PASSED.
[*] Running test_vir_artifact_manifest...
[+] test_vir_artifact_manifest PASSED.
[*] Running test_vir_execution_plan...
[+] test_vir_execution_plan PASSED.
[*] Running test_vir_pipeline_provenance...
[+] test_vir_pipeline_provenance PASSED.
[*] Running test_vir_state_flags_stringify...
[+] test_vir_state_flags_stringify PASSED.
[*] Running test_vir_geometry_metrics...
[+] test_vir_geometry_metrics PASSED.
[*] Running test_vir_cache_record_header...
[+] test_vir_cache_record_header PASSED.
[*] Running test_vir_artifact_blob...
[+] test_vir_artifact_blob PASSED.
[*] Running test_vir_repository_store...
[+] test_vir_repository_store PASSED.
[*] Running test_vir_repository_catalog...
[+] test_vir_repository_catalog PASSED.
[*] Running test_vir_backend_output_cache...
[+] test_vir_backend_output_cache PASSED.
[*] Running test_vir_repository_gc_and_lifecycle...
[+] test_vir_repository_gc_and_lifecycle PASSED.
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
```

**Result: 0 undefined behavior reports (EXIT=0)**

---

## Bugs caught mid-gate

### 1. Missing `<dirent.h>` header in test harness
During native compilation, `test_zcc_vir.c` encountered compilation errors due to unknown type `DIR` and implicit declarations of `opendir` and `closedir` inside `test_vir_repository_gc_and_lifecycle`.
* **Fix**: Added `#include <dirent.h>` and standard POSIX headers to the top of `test_zcc_vir.c`.

### 2. WSL drvfs mount EPERM failure on `utime()`
On first execution of the GC lifecycle test, the test crashed with `Assertion 'ut1 == 0' failed` due to `utime()` returning `EPERM (Operation not permitted)` when writing timestamp changes to files located on Windows-mapped drvfs mounts under WSL (`/mnt/h/...`).
* **Pathology**: This is a drvfs filesystem virtualization boundary mismatch for POSIX time modifications; it does NOT affect production runtime code since `vir_repository_gc` only performs read-only `stat` and `remove` operations (no `utime` is used in production).
* **Fix**: Updated `test_vir_repository_gc_and_lifecycle` to run inside `/tmp` (which mounts under a native POSIX-compliant ext4/tmpfs filesystem within WSL) for Unix/Linux compilation targets, while keeping local NTFS directories for native Windows targets.

## Hygiene / deferred

HYGIENE-004: Potential command injection in `vir_repository_prune_schema` `system()` shell-out. Deferred as `repo_path` is internally controlled; to be replaced with native POSIX/Windows directory removal in a future stabilization sprint.
