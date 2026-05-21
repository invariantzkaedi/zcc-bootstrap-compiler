# tests/abi/gate2_fwd.sh
set -e
cd "$(dirname "$0")/../.."
echo "Starting Gate 2: Forward Inter-op"
for t in ret_int_int ret_sse_sse ret_sse_int ret_int_sse ret_tvalue; do
    echo "Testing $t..."
    ./zcc -c tests/abi/${t}_lib.c -o ${t}.o
    gcc tests/abi/${t}_main.c ${t}.o -o ${t}_fwd -lm
    ./${t}_fwd
    echo "PASS: $t forward"
    rm -f ${t}.o ${t}_fwd
done
echo "Gate 2: ALL PASS"
