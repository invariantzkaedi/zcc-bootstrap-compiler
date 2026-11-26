#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${1:-/tmp/t21a_build}"
mkdir -p "$BUILD_DIR"

PFLAGS="-std=c99 -O2 -mno-avx -mno-avx2 -mno-fma -fno-unsafe-math-optimizations -fPIC -Wall -Wextra -Werror -Iinclude"
AFLAGS="-std=c99 -O2 -mavx2 -mfma -fno-unsafe-math-optimizations -fPIC -Wall -Wextra -Werror -Iinclude"

PORTABLE_SRCS=(
  runtime/compute_sha256.c
  runtime/cpu_capabilities.c
  runtime/compute_scalar.c
  runtime/compute_ptx.c
  runtime/compute_dispatch.c
  runtime/execution_receipt.c
)

PORTABLE_OBJS=()
for src in "${PORTABLE_SRCS[@]}"; do
  base="$(basename "${src%.c}")"
  obj="$BUILD_DIR/${base}.o"
  gcc $PFLAGS -c "$src" -o "$obj"
  PORTABLE_OBJS+=("$obj")
done

AVX2_OBJ="$BUILD_DIR/compute_avx2.o"
gcc $AFLAGS -c runtime/compute_avx2.c -o "$AVX2_OBJ"

SO_OUT="${2:-/tmp/libzcc_compute.so}"
gcc -shared -o "$SO_OUT" "${PORTABLE_OBJS[@]}" "$AVX2_OBJ" -lm -ldl

# Verify portable objects contain zero AVX/AVX2/FMA instructions
for obj in "${PORTABLE_OBJS[@]}"; do
  hits=$(objdump -d "$obj" | grep -cE 'ymm|zmm|vfmadd|vfnmadd|vaddps|vmulps|vpadd|vpbroadcast' || true)
  echo "PORTABLE_ISA $obj avx_hits=$hits"
  if [ "$hits" -ne 0 ]; then
    echo "FAIL: AVX/AVX2/FMA opcode leaked into portable TU $obj" >&2
    exit 1
  fi
done

avx_hits=$(objdump -d "$AVX2_OBJ" | grep -cE 'ymm|vfmadd|vaddps|vmulps' || true)
echo "AVX2_ISA $AVX2_OBJ avx_hits=$avx_hits"
if [ "$avx_hits" -eq 0 ]; then
  echo "FAIL: compute_avx2.o contains zero AVX2/FMA opcodes" >&2
  exit 1
fi

echo "RUNTIME_BUILD_AND_ISA_ISOLATION_OK so=$SO_OUT"
