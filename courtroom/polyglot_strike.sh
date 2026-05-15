#!/usr/bin/env bash
set -euo pipefail

CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

echo -e "${CYAN}=== ZCC POLYGLOT SINGULARITY INITIATED ===${NC}"

# Phase 1: Lower the Rust Guest
echo -e "${MAGENTA}[+] Lowering Rust AST -> SystemV ABI (Guest)...${NC}"
ZCC_OPT=1 ./zcc courtroom/polyglot_guest.rs --rust-backend-v1 --peephole --manifold-deterministic -o courtroom/polyglot_guest.s

# Phase 2: Lower the C Host
echo -e "${MAGENTA}[+] Lowering C AST -> SystemV ABI (Host)...${NC}"
ZCC_OPT=1 ./zcc courtroom/polyglot_host.c --peephole --manifold-deterministic -c -o courtroom/polyglot_host.s

# Phase 3: The Weld
echo -e "${CYAN}[+] Welding IR domains at Link-Time...${NC}"
gcc -O0 -w -no-pie -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables \
    -o courtroom/polyglot_singularity \
    courtroom/polyglot_host.s courtroom/polyglot_guest.s -lm

# Phase 4: Execution
echo -e "${CYAN}[+] Executing Polyglot Binary...${NC}"
set +e
./courtroom/polyglot_singularity
RESULT=$?
set -e

echo -e "EXPECTED TENSOR : ${MAGENTA}253${NC}"
echo -e "ACTUAL TENSOR   : ${CYAN}${RESULT}${NC}"

if [[ "${RESULT}" == "253" ]]; then
    echo -e "${CYAN}==============================================================================${NC}"
    echo -e "${CYAN}[✓] POLYGLOT SINGULARITY ACHIEVED. 0% FFI DRIFT.${NC}"
    echo -e "${CYAN}[✓] RUST AND C EXECUTING IN PERFECT HARMONY.${NC}"
    echo -e "${CYAN}==============================================================================${NC}"
else
    echo -e "${MAGENTA}[!] IMPEDANCE MISMATCH. FFI BOUNDARY COLLAPSE.${NC}"
fi
