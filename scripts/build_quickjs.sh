#!/bin/bash
set -e

REPO_DIR="/mnt/h/__DOWNLOADS/zcc_github_upload"
ZCC="$REPO_DIR/zcc"
QJS_SRC="/tmp/quickjs"
OBJ_DIR="/tmp/quickjs_objs"

mkdir -p "$OBJ_DIR"

echo "=== Compiling QuickJS with ZCC Native ==="

compile_one() {
    local src="$1"
    local name="$(basename "$src" .c)"
    echo "[ZCC] Compiling $name.c..."
    # 1. Preprocess with ZCC
    "$ZCC" --pp-only "$src" -D__EMSCRIPTEN__ '-DCONFIG_VERSION="2024-01-13"' -I"$QJS_SRC" -I"$REPO_DIR/include" -I"$REPO_DIR" > "$OBJ_DIR/$name.i.c"
    # 2. Compile to assembly with ZCC
    "$ZCC" -S "$OBJ_DIR/$name.i.c" -o "$OBJ_DIR/$name.s"
    # 3. Assemble with as
    as -o "$OBJ_DIR/$name.o" "$OBJ_DIR/$name.s"
    echo "  -> $OBJ_DIR/$name.o ($(wc -c < "$OBJ_DIR/$name.o") bytes)"
}

compile_one "$QJS_SRC/cutils.c"
compile_one "$QJS_SRC/libunicode.c"
compile_one "$QJS_SRC/dtoa.c"
compile_one "$QJS_SRC/libregexp.c"
compile_one "$QJS_SRC/quickjs.c"

echo "=== All Core QuickJS Modules Compiled Successfully! ==="
ls -lh "$OBJ_DIR"/*.o
