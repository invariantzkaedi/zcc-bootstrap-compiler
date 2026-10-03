# Artifacts for Win64 SEH and Extended Unwind Tables

- `src/win64_pe_emit.h`: Prototypes and definitions for `win64_pe_seh_scope_t` and updated `win64_pe_func_extent_t`.
- `src/win64_pe_emit.c`: `win64_pe_build_pdata` synthesizing `UNWIND_INFO` with `UNW_FLAG_EHANDLER`, personality `ExceptionHandler` RVA, and `SCOPE_TABLE` entries.
- `src/codegen.c`: Function extent harvester identifying companion handlers (`<func>_handler`, `_seh_handler`) and SEH try/except scope blocks (`seh_try_begin`, `seh_try_end`, `seh_target`).
- `tests/test_win64_pe_emitter.py`: Added `test_25` (SEH .pdata unwind info and scope table validation) and `test_26` (host runtime execution of binary with SEH unwind tables).
