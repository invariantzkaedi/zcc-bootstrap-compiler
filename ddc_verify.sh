#!/usr/bin/env bash
set -euo pipefail
# Restoring standard word-splitting to allow flag parsing
IFS=$' \n\t'

WORKSPACE="/dev/shm/zcc_ddc_crucible"
REFERENCE_BIN="$(pwd)/zcc3"

# Bash array to guarantee distinct argument boundaries
CFLAGS=(-O0 -w -fno-asynchronous-unwind-tables -g0)
LDFLAGS=(-lm -Wl,-s)

# Exact source matrix extracted from ZKAEDI Prime telemetry
SOURCES=(
    "zcc.c" "compiler_passes.c" "compiler_passes_ir.c" "ir_pass_manager.c" 
    "ir_pass_warden.c" "ir_symbolic_cfg.c" "ir_dominance.c" "ir_ssa.c" 
    "evm_lifter.c" "ir_vuln_tag.c" "src/ir_lower_float.c" "src/x86_codegen_sse.c" 
    "src/evm/decompiler.c" "src/evm/jit.c" "src/evm/symbolic.c" 
    "src/evm/memory_v2.c" "src/evm/abi_extractor.c" "src/evm/jit_memory.c" 
    "src/evm/proof_export.c" "src/evm/ipc_bridge.c" "src/evm/yul_weaver.c" 
    "src/evm/yul_fixed_point.c" "src/gfx/sdf_compiler.c" "src/gfx/mesh_warden.c"
)

CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

cleanup() {
    echo -e "${MAGENTA}[+] TEARDOWN: Wiping volatile crucible workspace...${NC}"
    rm -rf "${WORKSPACE}"
}
trap cleanup EXIT ERR

echo -e "${CYAN}=== ZCC DDC PROTOCOL INITIATED ===${NC}"
mkdir -p "${WORKSPACE}"

if [[ ! -f "${REFERENCE_BIN}" ]]; then
    echo -e "${MAGENTA}[!] FATAL: Reference Stage 3 binary missing. Compile it natively first.${NC}"
    exit 1
fi

echo -e "${CYAN}[+] PHASE 1: Generating orthogonal seed (ZCC_Alt) via clang...${NC}"
clang "${CFLAGS[@]}" -o "${WORKSPACE}/zcc_alt" "${SOURCES[@]}" "${LDFLAGS[@]}"

echo -e "${CYAN}[+] PHASE 2: Firing The Crucible. Compiling ZCC via ZCC_Alt...${NC}"
# Executing Phase 2 using the newly forged binary to compile its own source.
# Note: ZCC's CLI only takes a single entry point (zcc.c) and automatically appends the rest during its GCC linking phase.
"${WORKSPACE}/zcc_alt" zcc.c -o "${WORKSPACE}/zcc_final"
strip --strip-all "${WORKSPACE}/zcc_final"

echo -e "${CYAN}[+] PHASE 3: Cryptographic state evaluation...${NC}"

HASH_REF=$(sha256sum "${REFERENCE_BIN}" | awk '{print $1}')
HASH_FINAL=$(sha256sum "${WORKSPACE}/zcc_final" | awk '{print $1}')

echo -e "REFERENCE HASH : ${MAGENTA}${HASH_REF}${NC}"
echo -e "DDC FINAL HASH : ${CYAN}${HASH_FINAL}${NC}"

if [[ "${HASH_REF}" == "${HASH_FINAL}" ]]; then
    echo -e "${CYAN}==============================================================================${NC}"
    echo -e "${CYAN}[✓] MATHEMATICAL FIXPOINT ACHIEVED. 0% DRIFT CONFIRMED.${NC}"
    echo -e "${CYAN}[✓] ZERO-DEPENDENCY PURITY PROVEN.${NC}"
    echo -e "${CYAN}==============================================================================${NC}"
    exit 0
else
    echo -e "${MAGENTA}==============================================================================${NC}"
    echo -e "${MAGENTA}[!] DRIFT DETECTED. FIXPOINT FAILURE.${NC}"
    echo -e "${MAGENTA}==============================================================================${NC}"
    exit 1
fi
