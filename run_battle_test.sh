#!/usr/bin/env bash
set -u

file="$1"
name="$(basename "$file" .c)"

zcc_s="${name}_zcc.s"
zcc_bin="${name}_zcc"
gcc_bin="${name}_gcc"
zcc_out="${name}_zcc.out"
gcc_out="${name}_gcc.out"
diff_out="${name}.diff"

fail() {
  echo "[FAIL] $name :: $1"
  [ -f "$zcc_s" ] && echo "  asm: $zcc_s"
  [ -f "$zcc_out" ] && echo "  zcc output:" && cat "$zcc_out"
  [ -f "$gcc_out" ] && echo "  gcc output:" && cat "$gcc_out"
  [ -f "$diff_out" ] && echo "  diff:" && cat "$diff_out"
  exit 1
}

ZCC="${2:-./zcc}"

echo "[RUN] $name (using $ZCC)"

"$ZCC" "$file" -o "$zcc_s" \
  || fail "ZCC compile failed"

gcc "$zcc_s" -o "$zcc_bin" -lm \
  || fail "GCC assembly link failed"

"./$zcc_bin" > "$zcc_out" \
  || fail "ZCC-produced binary crashed"

gcc -O0 "$file" -o "$gcc_bin" -lm \
  || fail "GCC reference compile failed"

"./$gcc_bin" > "$gcc_out" \
  || fail "GCC reference binary crashed"

diff -u "$zcc_out" "$gcc_out" > "$diff_out" \
  || fail "runtime parity mismatch"

echo "[PASS] $name"
rm -f "$diff_out"
rm -f "$zcc_s" "$zcc_bin" "$gcc_bin" "$zcc_out" "$gcc_out"
