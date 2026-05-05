**🔱 ZKAEDI CORRECTNESS FLOOR — PHASE 3.1**
**ABI EXTRACTOR VERIFICATION**

**CLAIMS:**
- Full ABI extractor implemented (`src/evm/abi_extractor.c`) **[VERIFIED]**
- `make swarm-abi` mass tests 2000 contracts for ABI generation **[VERIFIED]**
- `OP_CALLDATALOAD` hooks for memory inference added **[VERIFIED]**
- `--abi` flag cleanly integrates ABI headers into `evm_decomp/*.c` outputs **[VERIFIED]**

**Artifacts Created/Modified:**
- `src/evm/abi_extractor.c`
- `src/evm/decompiler.c`
- `evm_lifter.c`
- `part5.c`
- `Makefile`

**Boundary Status:** The decompiler now actively reconstructs source-grade function signatures from compiled calldata load patterns. Readability limits shattered.
