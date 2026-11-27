#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PATCH_FILE="$REPO_ROOT/patches/mesa-25.2.8-u_cpu_detect-l3-teardown.patch"

ORIG_TAR="mesa_25.2.8.orig.tar.xz"
ORIG_SHA256="097842f3e49d996868b38688db87b006f7d4541e93ce86d2f341d8b3e7be7c93"
DEBIAN_TAR="mesa_25.2.8-0ubuntu0.24.04.5.debian.tar.xz"
DEBIAN_SHA256="edda95cf4f8cf4378a0e6cefc4a62b9db811e2e0d94cb498ea077357dda41ed5"
PATCH_SHA256="65a58ad9d9bad78aeee362304235cb30e80b65d45a0ae6b7571f9debcfea1ac6"

OUT_DIR="${ZCC_CLEAN_MESA_OUT:-/tmp/zcc_clean_mesa_lvp}"
CACHE_DIR="${ZCC_MESA_CACHE_DIR:-/tmp/zcc_mesa_src_cache}"
BUILD_JOBS="${ZCC_MESA_JOBS:-$(nproc)}"
UBUNTU_POOL_BASE="${ZCC_UBUNTU_MESA_URL:-https://archive.ubuntu.com/ubuntu/pool/main/m/mesa}"

while [ $# -gt 0 ]; do
    case "$1" in
        --out-dir) OUT_DIR="$2"; shift 2 ;;
        --cache-dir) CACHE_DIR="$2"; shift 2 ;;
        --jobs) BUILD_JOBS="$2"; shift 2 ;;
        *) echo "Unknown arg: $1" >&2; exit 2 ;;
    esac
done

mkdir -p "$OUT_DIR" "$CACHE_DIR"

sha256_of() {
    sha256sum "$1" | cut -d' ' -f1
}

verify_sha256() {
    local file="$1"
    local expected="$2"
    local actual
    actual="$(sha256_of "$file")"
    if [ "$actual" != "$expected" ]; then
        echo "FAIL: SHA-256 mismatch for $file (expected=$expected actual=$actual)" >&2
        exit 1
    fi
}

echo "=== [1/5] Verify Pinned Mesa 25.2.8-.5 Source Tarballs & Track 21B Patch ==="
verify_sha256 "$PATCH_FILE" "$PATCH_SHA256"

fetch_or_verify() {
    local tarname="$1"
    local expected="$2"
    local dst="$CACHE_DIR/$tarname"
    if [ -f "$dst" ]; then
        local got
        got="$(sha256_of "$dst")"
        if [ "$got" != "$expected" ]; then
            echo "WARN: Cached $tarname digest mismatch ($got), re-fetching..." >&2
            rm -f "$dst"
        fi
    fi
    if [ ! -f "$dst" ]; then
        if command -v curl >/dev/null 2>&1; then
            curl -fsSL "$UBUNTU_POOL_BASE/$tarname" -o "$dst"
        elif command -v wget >/dev/null 2>&1; then
            wget -qO "$dst" "$UBUNTU_POOL_BASE/$tarname"
        else
            python3 -c "import urllib.request; urllib.request.urlretrieve('$UBUNTU_POOL_BASE/$tarname', '$dst')"
        fi
    fi
    verify_sha256 "$dst" "$expected"
}

fetch_or_verify "$ORIG_TAR" "$ORIG_SHA256"
fetch_or_verify "$DEBIAN_TAR" "$DEBIAN_SHA256"

if [ -f "$OUT_DIR/libvulkan_lvp.so" ] && [ -f "$OUT_DIR/build_manifest.json" ]; then
    cached_patch="$(python3 -c "import json; print(json.load(open('$OUT_DIR/build_manifest.json')).get('patch_sha256', ''))" 2>/dev/null || true)"
    cached_so_sha="$(python3 -c "import json; print(json.load(open('$OUT_DIR/build_manifest.json')).get('libvulkan_lvp_sha256', ''))" 2>/dev/null || true)"
    actual_so_sha="$(sha256_of "$OUT_DIR/libvulkan_lvp.so" 2>/dev/null || true)"
    cached_unique="$(readelf --dyn-syms -W "$OUT_DIR/libvulkan_lvp.so" 2>/dev/null | grep -c UNIQUE || true)"
    if [ "$cached_patch" = "$PATCH_SHA256" ] && [ -n "$cached_so_sha" ] && [ "$cached_so_sha" = "$actual_so_sha" ] && [ "$cached_unique" = "0" ]; then
        echo "=== Reusing verified clean Lavapipe cache at $OUT_DIR ==="
        cat "$OUT_DIR/build_manifest.json"
        exit 0
    fi
fi

WORK_SRC="$CACHE_DIR/work_mesa_25.2.8"
rm -rf "$WORK_SRC"
mkdir -p "$WORK_SRC"

echo "=== [2/5] Unpack & Apply Ubuntu Debian Series + ZCC L3 Teardown Patch ==="
tar --no-same-owner -xf "$CACHE_DIR/$ORIG_TAR" -C "$WORK_SRC"
SRC_TREE="$(find "$WORK_SRC" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
tar --no-same-owner -xf "$CACHE_DIR/$DEBIAN_TAR" -C "$SRC_TREE"

pushd "$SRC_TREE" >/dev/null
if [ -f debian/patches/series ]; then
    while IFS= read -r p || [ -n "$p" ]; do
        p="${p%%#*}"
        p="${p#"${p%%[![:space:]]*}"}"
        p="${p%"${p##*[![:space:]]}"}"
        [ -z "$p" ] && continue
        patch -p1 -s < "debian/patches/$p"
    done < debian/patches/series
fi
patch -p1 --dry-run -s < "$PATCH_FILE"
patch -p1 -s < "$PATCH_FILE"

echo "=== [3/5] Configure & Build Clean Lavapipe ICD (-fno-gnu-unique + --exclude-libs SPIRV-Tools) ==="
MESON_BIN="meson"
if ! "$MESON_BIN" --version >/dev/null 2>&1 || [ "$(python3 -c "import subprocess; v=subprocess.check_output(['meson','--version'],text=True).strip().split('.'); print(int(v[0])*100+int(v[1]))")" -lt 104 ]; then
    if command -v meson-1.7 >/dev/null 2>&1; then
        MESON_BIN="meson-1.7"
    fi
fi

BUILD_DIR="$SRC_TREE/build-lvp-clean"
"$MESON_BIN" setup "$BUILD_DIR" "$SRC_TREE" \
    --buildtype=release \
    -Db_ndebug=true \
    -Dc_args="-fno-optimize-sibling-calls" \
    -Dcpp_args="-fno-optimize-sibling-calls -fno-gnu-unique" \
    -Dcpp_link_args="-Wl,--exclude-libs,libSPIRV-Tools.a:libSPIRV-Tools-opt.a:libSPIRV-Tools-link.a" \
    -Dplatforms= \
    -Dgallium-drivers= \
    -Dvulkan-drivers=swrast \
    -Dglx=disabled \
    -Dglvnd=disabled \
    -Degl=disabled \
    -Dgbm=disabled \
    -Dgles1=disabled \
    -Dgles2=disabled \
    -Dllvm=enabled \
    -Dshared-llvm=enabled \
    -Dbuild-tests=false \
    -Dtools= \
    -Dvalgrind=disabled

ninja -C "$BUILD_DIR" -j"$BUILD_JOBS" src/gallium/targets/lavapipe/libvulkan_lvp.so
BUILT_SO="$BUILD_DIR/src/gallium/targets/lavapipe/libvulkan_lvp.so"
popd >/dev/null

echo "=== [4/5] Audit Exported Symbols, Zero STB_GNU_UNIQUE, Build ID & Digest ==="
cp -f "$BUILT_SO" "$OUT_DIR/libvulkan_lvp.so"
SO_OUT="$OUT_DIR/libvulkan_lvp.so"
UNIQUE_CNT="$(readelf --dyn-syms -W "$SO_OUT" | grep -c "UNIQUE" || true)"
VK_EXPORT_CNT="$(readelf --dyn-syms -W "$SO_OUT" | grep -c "vk_icdGetInstanceProcAddr" || true)"
if [ "$UNIQUE_CNT" != "0" ]; then
    echo "FAIL: libvulkan_lvp.so still exposes $UNIQUE_CNT STB_GNU_UNIQUE symbol(s) causing NODELETE" >&2
    exit 1
fi
if [ "$VK_EXPORT_CNT" -lt 1 ]; then
    echo "FAIL: libvulkan_lvp.so missing vk_icdGetInstanceProcAddr export" >&2
    exit 1
fi

SO_SHA256="$(sha256_of "$SO_OUT")"
SO_BUILD_ID="$(readelf -n "$SO_OUT" | sed -n 's/.*Build ID: //p' | head -n 1 | tr -d ' \t\r')"
LLVM_SONAME="$(readelf -d "$SO_OUT" | sed -n 's/.*Shared library: \[\(libLLVM[^]]*\)\].*/\1/p' | head -n 1)"

cat > "$OUT_DIR/lvp_clean_icd.json" <<EOF
{
  "ICD": {
    "api_version": "1.4.318",
    "library_path": "$SO_OUT"
  },
  "file_format_version": "1.0.0"
}
EOF

echo "=== [5/5] Emit Provenance Manifest ==="
cat > "$OUT_DIR/build_manifest.json" <<EOF
{
  "schema_version": "zcc.mesa_lvp_build_manifest.v1",
  "mesa_version": "25.2.8-0ubuntu0.24.04.5",
  "orig_tarball": "$ORIG_TAR",
  "orig_sha256": "$ORIG_SHA256",
  "debian_tarball": "$DEBIAN_TAR",
  "debian_sha256": "$DEBIAN_SHA256",
  "patch_file": "patches/mesa-25.2.8-u_cpu_detect-l3-teardown.patch",
  "patch_sha256": "$PATCH_SHA256",
  "libvulkan_lvp_path": "$SO_OUT",
  "libvulkan_lvp_sha256": "$SO_SHA256",
  "libvulkan_lvp_build_id": "$SO_BUILD_ID",
  "icd_json_path": "$OUT_DIR/lvp_clean_icd.json",
  "stb_gnu_unique_count": $UNIQUE_CNT,
  "vk_icd_exports": $VK_EXPORT_CNT,
  "llvm_needed": "$LLVM_SONAME",
  "meson_flags": "--buildtype=release -Db_ndebug=true -Dcpp_args='-fno-optimize-sibling-calls -fno-gnu-unique' -Dcpp_link_args='-Wl,--exclude-libs,libSPIRV-Tools.a:libSPIRV-Tools-opt.a:libSPIRV-Tools-link.a'"
}
EOF
cat "$OUT_DIR/build_manifest.json"
