# Artifacts & Checksums: 2026-09-13 Compiler Hardening

## Compiler Executable & Assembler Outputs
- `zcc`: executable ELF binary
- `zcc2.s`: `4b6fdbe6221c4910c7cdbaac02d8d46b` (8,741,191 bytes)
- `zcc3.s`: `4b6fdbe6221c4910c7cdbaac02d8d46b` (8,741,191 bytes)

## Gate Verification Outputs
- `/tmp/simple_ret.yul`: Yul EVM contract output
- `/tmp/simple_ret.proof.hex`: STARK proof calldata (356 bytes, Merkle root `0x85ad3ffe71217e056286a14abd1d2e9949bf1298f9e2176afcb6023a9e0bd1dd`)
- `/tmp/instcombine_green.log`: 672 boundary vector tests verified against GCC reference oracle
- `/tmp/interop_zcc_lib_gcc_main`: verified tag `3/2` interop consensus
- `/tmp/interop_gcc_lib_zcc_main`: verified tag `3/2` interop consensus
