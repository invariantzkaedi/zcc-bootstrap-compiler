#!/usr/bin/env bash
set -euo pipefail

ZCC="${ZCC:-./zcc}"
CC_ORACLE="${CC_ORACLE:-cc}"
TARGET="${TARGET:-x86_64-linux-gnu}"
COUNT="${COUNT:-500}"
SEED="${SEED:-1337}"

python3 tools/gen_random_layout_oracle.py --count "$COUNT" --seed "$SEED"

mkdir -p /tmp/zcc-layout-fuzz
mkdir -p artifacts/layout-fuzz-failures

for file in tests/layout/random/random_layout_*.c; do
  name="$(basename "$file" .c)"
  index="${name##random_layout_}"
  index="${index#0}"

  oracle_bin="/tmp/zcc-layout-fuzz/${name}.oracle"
  zcc_bin="/tmp/zcc-layout-fuzz/${name}.zcc"

  echo "[LAYOUT-FUZZ] target=$TARGET seed=$SEED file=$file"

  "$CC_ORACLE" "$file" -o "$oracle_bin"
  "$ZCC" --target="$TARGET" "$file" -o "$zcc_bin"

  oracle_out="$("$oracle_bin")"
  zcc_out="$("$zcc_bin")"

  if [ "$oracle_out" != "$zcc_out" ]; then
    case_artifact="artifacts/layout-fuzz-failures/${name}.c"
    reduced_artifact="artifacts/layout-fuzz-failures/${name}.reduced.c"
    oracle_artifact="artifacts/layout-fuzz-failures/${name}.oracle.txt"
    zcc_artifact="artifacts/layout-fuzz-failures/${name}.zcc.txt"
    manifest_artifact="artifacts/layout-fuzz-failures/${name}.manifest.json"

    cp "$file" "$case_artifact"
    printf '%s\n' "$oracle_out" > "$oracle_artifact"
    printf '%s\n' "$zcc_out" > "$zcc_artifact"

    python3 tools/reduce_layout_case.py "$file" \
      --zcc "$ZCC" \
      --cc "$CC_ORACLE" \
      --out "$reduced_artifact" || true

    python3 tools/write_layout_manifest.py \
      --kind layout-fuzz \
      --status fail \
      --target "$TARGET" \
      --seed "$SEED" \
      --count "$COUNT" \
      --case-index "$index" \
      --case-file "$case_artifact" \
      --reduced-case-file "$reduced_artifact" \
      --oracle-output "$oracle_artifact" \
      --zcc-output "$zcc_artifact" \
      --cc-oracle "$CC_ORACLE" \
      --zcc "$ZCC" \
      --out "$manifest_artifact"

    echo "layout fuzz mismatch"
    echo "target=$TARGET"
    echo "seed=$SEED"
    echo "case=$index"
    echo "manifest=$manifest_artifact"

    exit 1
  fi
done

python3 tools/write_layout_manifest.py \
  --kind layout-fuzz \
  --status pass \
  --target "$TARGET" \
  --seed "$SEED" \
  --count "$COUNT" \
  --cc-oracle "$CC_ORACLE" \
  --zcc "$ZCC" \
  --out artifacts/layout-fuzz-pass.manifest.json

echo "[LAYOUT-FUZZ] all $COUNT cases matched host ABI"