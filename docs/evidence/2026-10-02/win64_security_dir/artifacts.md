# Artifacts: Win64 PE Authenticode Security Directory & Certificate Table

- `src/win64_pe_emit.h`: Defined `IMAGE_DIRECTORY_ENTRY_SECURITY` (4), `WIN_CERTIFICATE` struct (dwLength, wRevision, wCertificateType), `win64_pe_cert_info_t`, and declared `win64_pe_build_security_dir`, `zcc_emit_win64_pe_file_v7`, and `zcc_emit_win64_pe_dll_v7`.
- `src/win64_pe_emit.c`: Implemented `win64_pe_build_security_dir` with 8-byte aligned entry padding; wired `IMAGE_DIRECTORY_ENTRY_SECURITY` absolute file offset and size into PE32+ OptionalHeader DataDirectory[4]; appended Attribute Certificate Table to raw end of file for both EXE and DLL.
- `src/codegen.c`: Added `--embed-certificate=<path>` and `--cert-sig=<path>` command line argument parsing, loading signature payload, and passing to PE/DLL emitters.
- `tests/test_win64_pe_emitter.py`: Added `test_31_win64_pe_security_directory_and_certificate_table` verifying 8-byte boundary alignment, file offset pointing to exact certificate header, structure field parsing, payload bit-identity, and execution on Windows host.
