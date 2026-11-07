#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

EVIDENCE_DIR="$ROOT/docs/evidence/2026-11-01/wasm-mv02"
mkdir -p "$EVIDENCE_DIR"

echo "=== [WASM-MV-02] Gate 1: Full 3-Stage Self-Host Bootstrap ==="
make selfhost
cmp -s zcc2.s zcc3.s
cmp -s zcc2 zcc3
echo "Gate 1 PASS: zcc2.s == zcc3.s (0 byte delta) and zcc2 == zcc3 (0 byte delta)"

echo "=== [WASM-MV-02] Gate 2: Cross-Toolchain Interoperability ==="
cat << 'EOF' > /tmp/mv02_abi_lib.c
long long abi_mix(int a, long long b, int c, unsigned long long d) {
    return (long long)a * 1000LL + b - (long long)c + (long long)(d >> 16);
}
EOF
cat << 'EOF' > /tmp/mv02_abi_main.c
extern long long abi_mix(int a, long long b, int c, unsigned long long d);
int main(void) {
    long long r = abi_mix(7, 0x123456789ABCDEF0LL, 42, 0x0001000000000000ULL);
    return (r == (7000LL + 0x123456789ABCDEF0LL - 42LL + (long long)(0x0001000000000000ULL >> 16))) ? 0 : 1;
}
EOF
./zcc -c /tmp/mv02_abi_lib.c -o /tmp/mv02_zcc_lib.o
gcc -O2 -c /tmp/mv02_abi_main.c -o /tmp/mv02_gcc_main.o
gcc /tmp/mv02_zcc_lib.o /tmp/mv02_gcc_main.o -o /tmp/mv02_interop_1
/tmp/mv02_interop_1

gcc -O2 -c /tmp/mv02_abi_lib.c -o /tmp/mv02_gcc_lib.o
./zcc -c /tmp/mv02_abi_main.c -o /tmp/mv02_zcc_main.o
gcc /tmp/mv02_gcc_lib.o /tmp/mv02_zcc_main.o -o /tmp/mv02_interop_2
/tmp/mv02_interop_2
echo "Gate 2 PASS: zcc-lib+gcc-main and gcc-lib+zcc-main exit 0"

echo "=== [WASM-MV-02] Gate 3: Native Regression Corpus (zcc_test_suite.sh --quick) ==="
bash zcc_test_suite.sh --quick

echo "=== [WASM-MV-02] Gate 4A: WASM i64 & ILP32 Regression Suite (62 tests) ==="
python3 -m unittest discover -s tests -p "test_wasm_i64.py" -v

echo "=== [WASM-MV-02] Gate 4B: WASM-MV-02 Adversarial Semantic Suite ==="
python3 -m unittest discover -s tests -p "test_wasm_mv02.py" -v

echo "=== [WASM-MV-02] Gate 4C: Negative-Control Oracle Harness ==="
python3 ZCC_ORACLE_NEGATIVE_CONTROLS.py

echo "=== [ZCC-PROVE-22] Gate 4D: Peephole Truth Machine (Formal Z3 SMT + 5,000-State Differential Verification) ==="
python3 tools/zcc_prove_01.py --out-json "$EVIDENCE_DIR/zcc_prove_01.json"

echo "=== [WASM-MV-31 / BITOPS-AVG-ABSDIFF-01 / ZCC-PROVE-22] Gate 5: 112-Mutation Adversarial Kill Ledger (MV02..MV31) ==="
python3 tools/mv02_mutation.py --repo "$ROOT" --run-ledger --out-json "$EVIDENCE_DIR/mv02_mutation_ledger.json"

echo "=== [WASM-MV-31] Saving Patch & SHA-256 Artifact Hashes ==="
git diff -- part0_pp.c part2.c part3.c part4.c part5.c part6_arm.c part6_riscv.c part6_wasm.c src/arm64_codegen.c src/riscv_codegen.c src/codegen.c src/x86_codegen_sse.c src/wasm_emit.c src/wasm_emit.h zcc_test_suite.sh BOOTSTRAP_BASELINES.tsv > "$EVIDENCE_DIR/wasm-mv02.patch"
./zcc -S tests/x86_mv02_imm64.c -o "$EVIDENCE_DIR/x86_mv02_imm64.s"
sha256sum \
    "$EVIDENCE_DIR/wasm-mv02.patch" \
    "$EVIDENCE_DIR/x86_mv02_imm64.s" \
    "$EVIDENCE_DIR/zcc_prove_01.json" \
    part0_pp.c part2.c part3.c part4.c part5.c part6_arm.c part6_riscv.c part6_wasm.c src/arm64_codegen.c src/riscv_codegen.c src/codegen.c src/x86_codegen_sse.c src/wasm_emit.c src/wasm_emit.h \
    zcc zcc2.s zcc3.s zcc2 zcc3 \
    tests/wasm_mv02_signedness.c tests/x86_mv02_imm64.c \
    tests/typedef_compatible.c tests/typedef_incompatible.c \
    tests/test_wasm_i64.py tests/test_wasm_mv02.py \
    tools/mv02_mutation.py tools/zcc_prove_01.py scripts/run_mv02_gate.sh \
    "$EVIDENCE_DIR/mv02_mutation_ledger.json" | tee "$EVIDENCE_DIR/sha256sums.txt"

echo "ALL WASM-MV-02..31 / ARCH-AUDIT-01..02 / BITOPS-AVG-ABSDIFF-01 / ZCC-PROVE-01..22 GATES PASSED (EXIT 0)"

