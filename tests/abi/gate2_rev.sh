# tests/abi/gate4_rev.sh
set -e
cd "$(dirname "$0")/../.."
echo "Starting Gate 4: Reverse Inter-op"
for t in ret_int_int ret_sse_sse ret_sse_int ret_int_sse ret_tvalue; do
    echo "Testing $t..."
    # 1. Compile library with GCC
    gcc -c tests/abi/${t}_lib.c -o ${t}_gcc.o
    # 2. Compile main with ZCC to assembly
    ./zcc tests/abi/${t}_main.c -o ${t}_main.s
    # 3. Link together with GCC
    gcc ${t}_main.s ${t}_gcc.o -o ${t}_rev -lm
    # 4. Run verification
    ./${t}_rev
    echo "PASS: $t reverse"
    rm -f ${t}_gcc.o ${t}_main.s ${t}_rev
done
echo "Gate 4: ALL PASS"
