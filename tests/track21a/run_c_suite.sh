#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

mkdir -p /tmp/t21a_asan /tmp/t21a_ubsan /tmp/t21a_build
rm -f /tmp/t21a_asan/*.o /tmp/t21a_ubsan/*.o /tmp/t21a_build/*.o /tmp/libzcc_compute.so

PFLAGS_ASAN="-std=c99 -O1 -g -mno-avx -mno-avx2 -mno-fma -fno-unsafe-math-optimizations -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"
AFLAGS_ASAN="-std=c99 -O1 -g -mavx2 -mfma -fno-unsafe-math-optimizations -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"

for src in runtime/compute_sha256.c runtime/cpu_capabilities.c runtime/compute_scalar.c runtime/compute_ptx.c runtime/compute_dispatch.c runtime/execution_receipt.c tests/track21a/test_track21a_compute.c; do
  b="$(basename "${src%.c}")"
  gcc $PFLAGS_ASAN -c "$src" -o "/tmp/t21a_asan/${b}.o"
done
gcc $AFLAGS_ASAN -c runtime/compute_avx2.c -o /tmp/t21a_asan/compute_avx2.o
gcc $PFLAGS_ASAN /tmp/t21a_asan/*.o -o /tmp/test_track21a_asan -lm -ldl

echo "=== [1/5] ASan + LSan + UBSan (CPU paths, bounds, overflow, OOM, receipts) ==="
ASAN_OPTIONS=detect_leaks=1 /tmp/test_track21a_asan --cpu-only

echo "=== [2/5] Full UBSan (Scalar + AVX2 + PTX_SM120) ==="
PFLAGS_UBSAN="-std=c99 -O1 -g -mno-avx -mno-avx2 -mno-fma -fno-unsafe-math-optimizations -fsanitize=undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"
AFLAGS_UBSAN="-std=c99 -O1 -g -mavx2 -mfma -fno-unsafe-math-optimizations -fsanitize=undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"
for src in runtime/compute_sha256.c runtime/cpu_capabilities.c runtime/compute_scalar.c runtime/compute_ptx.c runtime/compute_dispatch.c runtime/execution_receipt.c tests/track21a/test_track21a_compute.c; do
  b="$(basename "${src%.c}")"
  gcc $PFLAGS_UBSAN -c "$src" -o "/tmp/t21a_ubsan/${b}.o"
done
gcc $AFLAGS_UBSAN -c runtime/compute_avx2.c -o /tmp/t21a_ubsan/compute_avx2.o
gcc $PFLAGS_UBSAN /tmp/t21a_ubsan/*.o -o /tmp/test_track21a_ubsan -lm -ldl
/tmp/test_track21a_ubsan

echo "=== [3/5] Release Shared Library + ISA Isolation Check ==="
bash tests/track21a/build_runtime.sh /tmp/t21a_build /tmp/libzcc_compute.so

echo "=== [4/5] O2 Execution & Real Hardware Benchmarks ==="
gcc -std=c99 -O2 -Wall -Wextra -Werror -Iinclude tests/track21a/test_track21a_compute.c /tmp/libzcc_compute.so -Wl,-rpath,/tmp -o /tmp/test_track21a_o2 -lm -ldl
/tmp/test_track21a_o2
/tmp/test_track21a_o2 --bench TRACK21A_PERFORMANCE_REPORT.json

echo "=== [5/5] Dual-Compiler T21-G04 (./zcc Compiling Scalar C99 Runtime) ==="
./zcc -c -Iinclude runtime/compute_scalar.c -o /tmp/zcc_cscalar.s
./zcc -c -Iinclude tests/track21a/test_scalar_zcc.c -o /tmp/zcc_test_scalar.s
gcc -no-pie /tmp/zcc_cscalar.s /tmp/zcc_test_scalar.s -o /tmp/test_scalar_zcc -lm
/tmp/test_scalar_zcc
