# Artifacts: Win64 PE GUI Subsystem & Direct Resource Emission (-mwindows, .rsrc)

## Key Files Modified
- `part5.c`: CLI flags `-mwindows` and `--subsystem=windows`.
- `src/codegen.c`: CLI parsing for GUI subsystem; `WinMain` / `wWinMain` entry symbol lookup fallback to `main`; passing target subsystem to PE emitter.
- `src/win64_pe_emit.c`: Added `zcc_emit_win64_pe_file_v5` with customizable `Subsystem` (2 for GUI, 3 for Console) and direct `.rsrc` section emission; added `win64_pe_build_default_manifest_rsrc` synthesizing Common-Controls 6.0 manifest.
- `src/win64_pe_emit.h`: Prototypes for `zcc_emit_win64_pe_file_v5` and `win64_pe_build_default_manifest_rsrc`.
- `tests/test_win64_pe_emitter.py`: Added `test_21_win64_gui_subsystem_and_winmain` and `test_22_win64_pe_resource_directory_and_manifest`.

## Checksums
- `zcc2.s`: `3529cb16c3c62de443aa772b5db9ae0c3a0ff5fee0c1b8a528298f1181c06344`
- `zcc3.s`: `3529cb16c3c62de443aa772b5db9ae0c3a0ff5fee0c1b8a528298f1181c06344`
