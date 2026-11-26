#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${1:-/tmp/t21b_build}"
SO_OUT="${2:-/tmp/libzcc_compute_v2.so}"
CLI_OUT="${3:-/tmp/zcc-spirv}"
mkdir -p "$BUILD_DIR"
rm -f "$BUILD_DIR"/*.o "$SO_OUT" "$CLI_OUT"

PFLAGS="-std=c99 -O2 -mno-avx -mno-avx2 -mno-fma -fno-unsafe-math-optimizations -fPIC -Wall -Wextra -Werror -Iinclude"
AFLAGS="-std=c99 -O2 -mavx2 -mfma -fno-unsafe-math-optimizations -fPIC -Wall -Wextra -Werror -Iinclude"

PORTABLE_SRCS=(
  runtime/compute_sha256.c
  runtime/cpu_capabilities.c
  runtime/compute_scalar.c
  runtime/compute_ptx.c
  runtime/compute_dispatch.c
  runtime/execution_receipt.c
  src/part6_spirv.c
  runtime/compute_vulkan.c
  runtime/compute_dispatch_v2.c
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

gcc -shared -o "$SO_OUT" "${PORTABLE_OBJS[@]}" "$AVX2_OBJ" -lm -ldl
gcc -std=c99 -O2 -mno-avx -mno-avx2 -mno-fma -Wall -Wextra -Werror -Iinclude src/part6_spirv.c tools/zcc_spirv_cli.c -o "$CLI_OUT"

for obj in "${PORTABLE_OBJS[@]}"; do
  hits=$(objdump -d "$obj" | grep -cE 'ymm|zmm|vfmadd|vfnmadd|vaddps|vmulps|vpadd|vpbroadcast' || true)
  echo "PORTABLE_ISA $obj avx_hits=$hits"
  if [ "$hits" -ne 0 ]; then
    echo "FAIL: AVX opcode leaked into portable TU $obj" >&2
    exit 1
  fi
done

avx_hits=$(objdump -d "$AVX2_OBJ" | grep -cE 'ymm|vfmadd|vaddps|vmulps' || true)
echo "AVX2_ISA $AVX2_OBJ avx_hits=$avx_hits"
if [ "$avx_hits" -eq 0 ]; then
  echo "FAIL: compute_avx2.o contains zero AVX2/FMA opcodes" >&2
  exit 1
fi

echo "TRACK21B_BUILD_AND_ISA_ISOLATION_OK so=$SO_OUT cli=$CLI_OUT"
