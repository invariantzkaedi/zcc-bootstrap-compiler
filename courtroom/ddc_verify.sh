#!/bin/bash
set -e

echo "============================================================"
echo " [ZCC-HOOK-17] DIVERSE DOUBLE COMPILATION (DDC) CRUCIBLE"
echo "============================================================"

if ! command -v clang &> /dev/null; then
    echo "[!] Clang is not installed. DDC requires a diverse host compiler."
    exit 1
fi

SRC_FILES="zcc.c compiler_passes.c compiler_passes_ir.c ir_pass_manager.c ir_pass_warden.c ir_symbolic_cfg.c ir_dominance.c ir_ssa.c evm_lifter.c ir_vuln_tag.c src/ir_lower_float.c src/x86_codegen_sse.c src/evm/decompiler.c src/evm/jit.c src/evm/symbolic.c src/evm/memory_v2.c src/evm/abi_extractor.c src/evm/jit_memory.c src/evm/proof_export.c src/evm/ipc_bridge.c src/evm/yul_weaver.c src/evm/yul_fixed_point.c src/gfx/sdf_compiler.c src/gfx/mesh_warden.c"

echo "[1/4] Building ZCC Reference Compiler (GCC)..."
gcc -O0 -w -no-pie -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -g0 -o zcc_gcc $SRC_FILES -lm

echo "[2/4] Building ZCC Alternative Compiler (Clang)..."
clang -O0 -w -no-pie -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -g0 -o zcc_clang $SRC_FILES -lm

echo "[3/4] Stage 2 Self-Hosting..."
echo "  -> zcc_gcc   compiles zcc.c => zcc_stage2_gcc"
./zcc_gcc zcc.c -o zcc_stage2_gcc
strip --strip-all zcc_stage2_gcc

echo "  -> zcc_clang compiles zcc.c => zcc_stage2_clang"
./zcc_clang zcc.c -o zcc_stage2_clang
strip --strip-all zcc_stage2_clang

echo "[4/4] Cryptographic Provenance Verification..."
HASH_GCC=$(sha256sum zcc_stage2_gcc | awk '{print $1}')
HASH_CLANG=$(sha256sum zcc_stage2_clang | awk '{print $1}')

echo "  [Reference] GCC-host   => $HASH_GCC"
echo "  [Diverse]   Clang-host => $HASH_CLANG"

if [ "$HASH_GCC" == "$HASH_CLANG" ]; then
    echo ""
    echo "✅ [PASS] Bootstrap Exorcist: Binary identity proven across diverse host compilers."
    echo "   The ZCC optimization pipeline is formally determinism-clean."
    exit 0
else
    echo ""
    echo "❌ [FAIL] DDC mismatch! The build process contains non-deterministic host contamination."
    echo "   Check memory layouts, pointer hashes, or uninitialized padding."
    exit 1
fi
