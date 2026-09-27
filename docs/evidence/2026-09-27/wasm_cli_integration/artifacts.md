# Artifacts Manifest — WASM CLI Target Integration

**Date**: 2026-09-27  
**Topic**: WebAssembly CLI Target Wiring (`-target wasm`, `-wasm`, `--target wasm`, `--target=wasm`)

## Modified Source Files
- `part5.c`: CLI argument parsing loop and driver cleanup.
- `src/codegen.c`: Top-level main driver passthrough and target interceptor.

## Test Artifacts
- `tests/probe_wasm.c`: Probe C source compiling functions `add` and `main`.
- `tests/run_wasm.js`: Node.js WebAssembly execution and verification harness.
- `/tmp/probe.wasm`: Valid emitted WebAssembly binary (magic `\0asm\1\0\0\0`).

## Checksums
| File | SHA-256 Checksum |
|---|---|
| `part5.c` | `ac8831766be65507d9435723b961a7df8b667fc69dd3bf60b384be918480a580` |
| `src/codegen.c` | `e627a3d9229f5c1f88070a09a51535cbd456e50732effc91666079845a6b2615` |
| `tests/probe_wasm.c` | `3fa22bb194c5a3379a300e1bc6340d8ca3e757d8a16a1b5e938a4f812334afd3` |
| `tests/run_wasm.js` | `08c80c448d9993913e16ed23285fe22a382022a69213c11abe8f62978edce8a9` |
