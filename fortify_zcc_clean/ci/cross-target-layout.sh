#!/usr/bin/env bash
set -euo pipefail

ZCC="${ZCC:-./zcc}"
CLANG="${CLANG:-clang}"
TARGETS="${TARGETS:-x86_64-linux-gnu aarch64-linux-gnu riscv64-linux-gnu x86_64-windows-mingw}"
COUNT="${COUNT:-250}"
SEED="${SEED:-7331}"

python3 tools/gen_random_layout_oracle.py --count "$COUNT" --seed "$SEED"

mkdir -p artifacts/layout-cross-target

for target in $TARGETS; do
  echo "[CROSS-LAYOUT] target=$target"

  for file in tests/layout/random/random_layout_*.c; do
    name="$(basename "$file" .c)"

    clang_raw="artifacts/layout-cross-target/${target}.${name}.clang.raw.txt"
    clang_norm="artifacts/layout-cross-target/${target}.${name}.clang.layout.txt"
    zcc_norm="artifacts/layout-cross-target/${target}.${name}.zcc.layout.txt"

    "$CLANG" -target "$target" -Xclang -fdump-record-layouts -fsyntax-only "$file" > "$clang_raw" 2>&1 || {
      echo "clang layout dump failed: target=$target file=$file"
      cat "$clang_raw"
      exit 1
    }

    python3 tools/normalize_clang_layout.py < "$clang_raw" > "$clang_norm"

    "$ZCC" --target="$target" -fdump-layout "$file" > "$zcc_norm"

    if ! diff -u "$clang_norm" "$zcc_norm"; then
      mkdir -p artifacts/layout-cross-target/failures
      cp "$file" "artifacts/layout-cross-target/failures/${target}.${name}.c"
      cp "$clang_norm" "artifacts/layout-cross-target/failures/${target}.${name}.clang.layout.txt"
      cp "$zcc_norm" "artifacts/layout-cross-target/failures/${target}.${name}.zcc.layout.txt"

      echo "cross-target layout mismatch"
      echo "target=$target"
      echo "file=$file"
      echo "saved=artifacts/layout-cross-target/failures/${target}.${name}.c"
      exit 1
    fi
  done
done

echo "[CROSS-LAYOUT] all targets matched"