#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

mkdir -p /tmp/t21b_asan /tmp/t21b_ubsan /tmp/t21b_build /tmp/t21b_spv
rm -f /tmp/t21b_asan/*.o /tmp/t21b_ubsan/*.o /tmp/t21b_build/*.o /tmp/t21b_spv/*.spv /tmp/libzcc_compute_v2.so /tmp/zcc-spirv

echo "=== [1/6] Build Portable vs AVX2 Runtime & Standalone SPIR-V CLI ==="
bash tests/track21b/build_runtime.sh /tmp/t21b_build /tmp/libzcc_compute_v2.so /tmp/zcc-spirv

echo "=== [2/6] Independent Khronos spirv-val Validation (All 13 Ops + Determinism) ==="
if ! command -v spirv-val >/dev/null 2>&1; then
    echo "BLOCKED: independent external spirv-val tool is missing" >&2
    exit 3
fi
! /tmp/zcc-spirv >/dev/null 2>&1
/tmp/zcc-spirv --self-check
/tmp/zcc-spirv --emit-op GEMM_F32 -o /tmp/t21b_spv/single_gemm.spv
/tmp/zcc-spirv --emit-op 1 -o /tmp/t21b_spv/single_vec_add.spv
spirv-val --target-env vulkan1.0 /tmp/t21b_spv/single_gemm.spv
spirv-val --target-env vulkan1.0 /tmp/t21b_spv/single_vec_add.spv
! /tmp/zcc-spirv --emit-op NO_SUCH_OP -o /tmp/t21b_spv/bad.spv >/dev/null 2>&1
! /tmp/zcc-spirv --unknown-flag >/dev/null 2>&1
rm -f /tmp/t21b_spv/single_gemm.spv /tmp/t21b_spv/single_vec_add.spv
/tmp/zcc-spirv --emit-all /tmp/t21b_spv
mkdir -p /tmp/t21b_spv_2
rm -f /tmp/t21b_spv_2/*.spv
/tmp/zcc-spirv --emit-all /tmp/t21b_spv_2 >/dev/null
for f in /tmp/t21b_spv/*.spv; do
    base="$(basename "$f")"
    spirv-val --target-env vulkan1.0 "$f"
    cmp "$f" "/tmp/t21b_spv_2/$base"
    echo "SPIRV_VAL_AND_DETERMINISM_PASS file=$base sha256=$(sha256sum "$f" | awk '{print $1}')"
done

echo "=== [3/6] Full UBSan (-fsanitize=undefined -fno-sanitize-recover=all) Live llvmpipe + Validation Layer ==="
PFLAGS_UBSAN="-std=c99 -O1 -g -mno-avx -mno-avx2 -mno-fma -fno-unsafe-math-optimizations -fsanitize=undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"
AFLAGS_UBSAN="-std=c99 -O1 -g -mavx2 -mfma -fno-unsafe-math-optimizations -fsanitize=undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"
for src in runtime/compute_sha256.c runtime/cpu_capabilities.c runtime/compute_scalar.c runtime/compute_ptx.c runtime/compute_dispatch.c runtime/execution_receipt.c src/part6_spirv.c runtime/compute_vulkan.c runtime/compute_dispatch_v2.c tests/track21b/test_track21b_vulkan.c; do
    b="$(basename "${src%.c}")"
    gcc $PFLAGS_UBSAN -c "$src" -o "/tmp/t21b_ubsan/${b}.o"
done
gcc $AFLAGS_UBSAN -c runtime/compute_avx2.c -o /tmp/t21b_ubsan/compute_avx2.o
gcc $PFLAGS_UBSAN /tmp/t21b_ubsan/*.o -o /tmp/test_track21b_ubsan -lm -ldl
/tmp/test_track21b_ubsan

echo "=== [4/6] ASan + UBSan (+ LSan Probe) ==="
PFLAGS_ASAN="-std=c99 -O1 -g -mno-avx -mno-avx2 -mno-fma -fno-unsafe-math-optimizations -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"
AFLAGS_ASAN="-std=c99 -O1 -g -mavx2 -mfma -fno-unsafe-math-optimizations -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"
for src in runtime/compute_sha256.c runtime/cpu_capabilities.c runtime/compute_scalar.c runtime/compute_ptx.c runtime/compute_dispatch.c runtime/execution_receipt.c src/part6_spirv.c runtime/compute_vulkan.c runtime/compute_dispatch_v2.c tests/track21b/test_track21b_vulkan.c; do
    b="$(basename "${src%.c}")"
    gcc $PFLAGS_ASAN -c "$src" -o "/tmp/t21b_asan/${b}.o"
done
gcc $AFLAGS_ASAN -c runtime/compute_avx2.c -o /tmp/t21b_asan/compute_avx2.o
gcc $PFLAGS_ASAN /tmp/t21b_asan/*.o -o /tmp/test_track21b_asan -lm -ldl
ASAN_OPTIONS=detect_leaks=1 /tmp/test_track21b_asan --skip-ptx

echo "=== [5/6] O2 Execution & Multi-Backend Benchmarks ==="
gcc -std=c99 -O2 -Wall -Wextra -Werror -Iinclude tests/track21b/test_track21b_vulkan.c /tmp/libzcc_compute_v2.so -Wl,-rpath,/tmp -o /tmp/test_track21b_o2 -lm -ldl
/tmp/test_track21b_o2
/tmp/test_track21b_o2 --bench TRACK21B_PERFORMANCE_REPORT.json

echo "=== [6/6] MJS v2 SDK Contract Check ==="
if command -v node >/dev/null 2>&1; then
    node tests/track21b/test_sdk_v2_mjs.mjs
elif [ -x /home/ZKAEDI/.local/bin/node ]; then
    /home/ZKAEDI/.local/bin/node tests/track21b/test_sdk_v2_mjs.mjs
else
    echo "WARN: node not in PATH"
fi
