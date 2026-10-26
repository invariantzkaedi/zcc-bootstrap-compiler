#!/usr/bin/env bash
# scripts/build_curl.sh — Sovereign Build & Verification Harness for libcurl 8.7.1 → ZCC
#
# Downloads, configures, compiles core libcurl modules with ZCC, builds libzcc_curl.a,
# links tests/test_curl_smoke.c, and executes verification gauntlet.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

CURL_VERSION="8.7.1"
CURL_DIR="/tmp/curl-${CURL_VERSION}"
CURL_TAR="/tmp/curl-${CURL_VERSION}.tar.gz"
OBJ_DIR="/tmp/zcc_curl_objs"

echo "╔════════════════════════════════════════════════════════════════════════╗"
echo "║          ZCC SOVEREIGN LIBCURL 8.7.1 BUILD & VERIFY HARNESS            ║"
echo "╚════════════════════════════════════════════════════════════════════════╝"

# 1. Fetch curl source tarball if not present
if [ ! -d "$CURL_DIR" ]; then
    if [ ! -f "$CURL_TAR" ]; then
        echo "[ZCC-CURL] Downloading libcurl ${CURL_VERSION}..."
        curl -sSL "https://curl.se/download/curl-${CURL_VERSION}.tar.gz" -o "$CURL_TAR"
    fi
    echo "[ZCC-CURL] Extracting libcurl ${CURL_VERSION}..."
    tar -xzf "$CURL_TAR" -C /tmp
fi

# 2. Configure curl for static build if curl_config.h is missing
if [ ! -f "${CURL_DIR}/lib/curl_config.h" ]; then
    echo "[ZCC-CURL] Generating curl_config.h via configure..."
    (cd "$CURL_DIR" && ./configure \
        --disable-shared \
        --enable-static \
        --without-ssl \
        --without-zlib \
        --without-zstd \
        --disable-threaded-resolver \
        --disable-pthreads \
        --disable-ldap \
        --disable-ldaps \
        --disable-rtsp \
        --disable-dict \
        --disable-telnet \
        --disable-tftp \
        --disable-pop3 \
        --disable-imap \
        --disable-smb \
        --disable-smtp \
        --disable-gopher \
        --disable-mqtt \
        --prefix=/tmp/curl_install >/dev/null 2>&1)
fi

# 3. Compile core libcurl C files with ZCC
rm -rf "$OBJ_DIR"
mkdir -p "$OBJ_DIR"
CURL_SRCS=(
    escape
    urlapi
    strerror
    mprintf
    dynbuf
    strdup
    timediff
    warnless
    base64
    version
    strcase
)

echo "[ZCC-CURL] Compiling ${#CURL_SRCS[@]} core libcurl source units with ZCC..."

ZCC_BIN="/tmp/zcc_exec_curl"
cp -f "${REPO_ROOT}/zcc" "$ZCC_BIN"
chmod +x "$ZCC_BIN"

ASM_FILES=()
for src in "${CURL_SRCS[@]}"; do
    c_file="${CURL_DIR}/lib/${src}.c"
    pp_file="${OBJ_DIR}/${src}_pp.c"
    s_file="${OBJ_DIR}/${src}.s"
    o_file="${OBJ_DIR}/${src}.o"

    gcc -E -P -nostdinc \
        -I "${REPO_ROOT}/zcc_sys_includes" \
        -I "${REPO_ROOT}/zcc-libc" \
        -I "${REPO_ROOT}/include" \
        -include "${REPO_ROOT}/include/zcc_curl_compat.h" \
        -I "${CURL_DIR}/include" \
        -I "${CURL_DIR}/lib" \
        -DBUILDING_LIBCURL \
        -DCURL_STATICLIB \
        -DHAVE_CONFIG_H \
        "$c_file" -o "$pp_file"

    "$ZCC_BIN" "$pp_file" -o "$s_file" >/dev/null 2>&1
    gcc -c "$s_file" -o "$o_file"
    ASM_FILES+=("$o_file")
    echo "  ✔ ${src}.c -> ${src}.s -> ${src}.o"
done

# 4. Create libzcc_curl.a archive
echo "[ZCC-CURL] Creating libzcc_curl.a..."
ar rcs "${OBJ_DIR}/libzcc_curl.a" "${ASM_FILES[@]}"

# 5. Compile and link test harness
echo "[ZCC-CURL] Building test_curl_smoke harness..."
gcc -no-pie -I "${CURL_DIR}/include" "${REPO_ROOT}/tests/test_curl_smoke.c" "${REPO_ROOT}/tests/curl_stubs.c" "${OBJ_DIR}/libzcc_curl.a" -o "${OBJ_DIR}/test_curl_smoke" -lm

# 6. Execute test harness
echo "[ZCC-CURL] Executing live curl verification gauntlet..."
"${OBJ_DIR}/test_curl_smoke"
