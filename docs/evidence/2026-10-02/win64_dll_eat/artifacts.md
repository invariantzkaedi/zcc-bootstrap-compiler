# Artifacts for Win64 PE DLL and Export Address Table Emission

- `src/win64_pe_emit.h`: Prototypes for `win64_pe_export_t` and `zcc_emit_win64_pe_dll`.
- `src/win64_pe_emit.c`: Complete PE dynamic library emitter with `.edata`, `IMAGE_EXPORT_DIRECTORY`, EAT, ENT, EOT, and `IMAGE_FILE_DLL` (0x2000).
- `src/codegen.c`: Function export symbol harvester and `-shared` dynamic library dispatch.
- `part5.c`: CLI flags `-shared`.
- `tests/test_win64_pe_emitter.py`: Added `test_23` (PE DLL export directory validation) and `test_24` (PE DLL runtime execution via ctypes LoadLibrary on Windows host).
