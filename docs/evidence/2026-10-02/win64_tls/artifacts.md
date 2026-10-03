# Artifacts: Win64 PE TLS - Thread Local Storage

## Touched Files
- `src/win64_pe_emit.h`: Defined `IMAGE_TLS_DIRECTORY64` (40 bytes), `win64_pe_tls_info_t`, `win64_pe_build_tls`, `zcc_emit_win64_pe_file_v6`, and `zcc_emit_win64_pe_dll_ex`.
- `src/win64_pe_emit.c`: Implemented `win64_pe_build_tls`, integrated `.tls` section header, populated Optional Header `DataDirectory[9]` (IMAGE_DIRECTORY_ENTRY_TLS), and generated TLS directory relocations.
- `src/codegen.c`: Added segment 4 for TLS data, wired `tls_seg` and `tls_zero_fill`, updated instruction parser to handle `%fs:0` and `%gs:0x58` TEB loads, supported `leaq @tpoff` with `encode_leaq_disp32`, and resolved `R_X86_64_TPOFF32` in Pass 2.
- `src/elf_emit.h`: Added `STT_TLS` (6) and `R_X86_64_TPOFF32` (23) constants.
- `tests/test_win64_pe_emitter.py`: Added `test_29_win64_pe_tls_directory_and_thread_local` and `test_30_win64_pe_tls_multi_variable_and_offsets`.
- Foundational files (`part0_pp.c`, `part3.c`, `part4.c`): 0 lines touched.

## Generated Artifacts
- `scratch/probe_tls_test.exe`: Executable PE32+ with `.tls` section, `IMAGE_TLS_DIRECTORY64`, executed natively on Windows.
- `scratch/probe_tls_multi.exe`: Executable PE32+ with multiple initialized thread-local variables and alignment.
