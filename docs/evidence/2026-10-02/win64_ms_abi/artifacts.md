# Artifacts: Win64 Native Microsoft x64 Calling Convention Lowering (-mabi=ms)

## Key Files Modified
- `part4.c`: Codegen support for MS ABI register arguments (`%rcx, %rdx, %r8, %r9`), 4 GP argument ceiling, and 32-byte caller shadow space allocation.
- `part5.c`: CLI flags `-mabi=ms`, `-mabi=win64`, `-mabi=sysv`.
- `src/codegen.c`: In-memory driver propagation of `-mabi=ms` flag to internal pipeline.
- `src/win64_pe_emit.c`: Standalone PE emitter with base relocation synthesis and dynamic function extents.
- `src/win64_pe_emit.h`: Exported prototypes and PE structures.
- `tests/test_win64_pe_emitter.py`: Unit tests including `test_20_win64_ms_abi_calling_convention`.

## Checksums
- `zcc2.s`: `a21d1967056fcd081e0543448276a57605cabc59df65ba2993d3d0219033989b`
- `zcc3.s`: `a21d1967056fcd081e0543448276a57605cabc59df65ba2993d3d0219033989b`
