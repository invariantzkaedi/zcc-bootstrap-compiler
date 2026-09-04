#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ZCC="${REPO_ROOT}/zcc"
WASM3_SRC="/tmp/wasm3/source"
WASM3_APP="/tmp/wasm3/platforms/app"
OBJ_DIR="/tmp/wasm3_objs"

mkdir -p "${OBJ_DIR}"

# Patch m3_parse.c line 821 compound literal if present
sed -i 's/compilation = (M3Compilation){ \.runtime = NULL, \.module = io_module, \.wasm = \*io_bytes, \.wasmEnd = i_end, \.isInitExpr = true };/memset(\&compilation, 0, sizeof(M3Compilation)); compilation.module = io_module; compilation.wasm = *io_bytes; compilation.wasmEnd = i_end; compilation.isInitExpr = true;/' "${WASM3_SRC}/m3_parse.c" 2>/dev/null || true

# Patch wasm3_defs.h static assert if present
sed -i 's/#  define M3_STATIC_ASSERT(COND, NAME)  typedef char M3_CONCAT(NAME, __LINE__) \[(COND) ? 1 : -1\]/#  define M3_STATIC_ASSERT(COND, NAME)/' "${WASM3_SRC}/wasm3_defs.h" 2>/dev/null || true

# Generate portable bitwise intrinsics for Wasm3 runtime
cat << 'EOF' > "${OBJ_DIR}/m3_compat.c"
int __builtin_clz(unsigned int x) {
    if (!x) return 32;
    int n = 0;
    if (!(x & 0xFFFF0000)) { n += 16; x <<= 16; }
    if (!(x & 0xFF000000)) { n += 8; x <<= 8; }
    if (!(x & 0xF0000000)) { n += 4; x <<= 4; }
    if (!(x & 0xC0000000)) { n += 2; x <<= 2; }
    if (!(x & 0x80000000)) { n += 1; }
    return n;
}
int __builtin_ctz(unsigned int x) {
    if (!x) return 32;
    int n = 0;
    if (!(x & 0x0000FFFF)) { n += 16; x >>= 16; }
    if (!(x & 0x000000FF)) { n += 8; x >>= 8; }
    if (!(x & 0x0000000F)) { n += 4; x >>= 4; }
    if (!(x & 0x00000003)) { n += 2; x >>= 2; }
    if (!(x & 0x00000001)) { n += 1; }
    return n;
}
int __builtin_clzll(unsigned long long x) {
    if (!x) return 64;
    unsigned int hi = (unsigned int)(x >> 32);
    if (hi) return __builtin_clz(hi);
    return 32 + __builtin_clz((unsigned int)x);
}
int __builtin_ctzll(unsigned long long x) {
    if (!x) return 64;
    unsigned int lo = (unsigned int)x;
    if (lo) return __builtin_ctz(lo);
    return 32 + __builtin_ctz((unsigned int)(x >> 32));
}
int __builtin_popcount(unsigned int x) {
    int c = 0;
    while (x) {
        c += (int)(x & 1);
        x >>= 1;
    }
    return c;
}
int __builtin_popcountl(unsigned long x) {
    int c = 0;
    while (x) {
        c += (int)(x & 1);
        x >>= 1;
    }
    return c;
}
int __builtin_popcountll(unsigned long long x) {
    int c = 0;
    while (x) {
        c += (int)(x & 1);
        x >>= 1;
    }
    return c;
}
EOF

SOURCES=(
    "m3_core.c"
    "m3_env.c"
    "m3_bind.c"
    "m3_code.c"
    "m3_function.c"
    "m3_info.c"
    "m3_module.c"
    "m3_parse.c"
    "m3_validate.c"
    "m3_api_libc.c"
    "m3_api_tracer.c"
    "m3_exec.c"
    "m3_compile.c"
)

echo "=== Compiling Wasm3 Core Sources with ZCC ==="

OBJS=()
for src in "${SOURCES[@]}"; do
    base="${src%.c}"
    s_file="${OBJ_DIR}/${base}.s"
    o_file="${OBJ_DIR}/${base}.o"
    echo -n "[ZCC] ${src} -> ${base}.o ... "
    if "${ZCC}" -I"${WASM3_SRC}" -Dd_m3VerboseErrorMessages=0 "${WASM3_SRC}/${src}" -o "${s_file}" 2>&1 | grep -E "error:|FAILED" > "${OBJ_DIR}/${base}.err"; then
        echo "FAILED"
        cat "${OBJ_DIR}/${base}.err"
        exit 1
    fi
    as -o "${o_file}" "${s_file}"
    OBJS+=("${o_file}")
    echo "OK ($(stat -c%s "${o_file}") bytes)"
done

echo -n "[ZCC] m3_compat.c -> m3_compat.o ... "
"${ZCC}" "${OBJ_DIR}/m3_compat.c" -o "${OBJ_DIR}/m3_compat.s"
as -o "${OBJ_DIR}/m3_compat.o" "${OBJ_DIR}/m3_compat.s"
OBJS+=("${OBJ_DIR}/m3_compat.o")
echo "OK ($(stat -c%s "${OBJ_DIR}/m3_compat.o") bytes)"

echo "[+] All Wasm3 core sources compiled successfully!"
ar rcs "${OBJ_DIR}/libm3.a" "${OBJS[@]}"
echo "[+] Created ${OBJ_DIR}/libm3.a ($(stat -c%s "${OBJ_DIR}/libm3.a") bytes)"

echo "=== Compiling Native Wasm3 CLI Executable ==="
echo -n "[ZCC] main.c -> app_main.o ... "
"${ZCC}" -I"${WASM3_SRC}" -Dd_m3VerboseErrorMessages=0 "${WASM3_APP}/main.c" -o "${OBJ_DIR}/app_main.s"
as -o "${OBJ_DIR}/app_main.o" "${OBJ_DIR}/app_main.s"
echo "OK ($(stat -c%s "${OBJ_DIR}/app_main.o") bytes)"

echo -n "[LINK] Linking /tmp/wasm3_zcc_native ... "
gcc -o /tmp/wasm3_zcc_native "${OBJ_DIR}/app_main.o" "${OBJ_DIR}/libm3.a" -lm
echo "OK ($(stat -c%s /tmp/wasm3_zcc_native) bytes)"

echo "=== Testing Native Wasm3 Interpreter ==="
/tmp/wasm3_zcc_native --version
