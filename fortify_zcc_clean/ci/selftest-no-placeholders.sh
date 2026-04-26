#!/usr/bin/env bash
set -euo pipefail

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/src" "$tmp/tools"

cat > "$tmp/src/good.c" <<'EOF'
int main(void) {
    return 0;
}
EOF

cat > "$tmp/src/bad.c" <<'EOF'
int unfinished(void) {
    // TODO: complete this
    return 0;
}
EOF

set +e
python3 tools/check_no_placeholders.py \
  --root "$tmp" \
  --report "$tmp/report.fail.json" >/tmp/no-placeholder-fail.out 2>&1
status=$?
set -e

if [ "$status" -eq 0 ]; then
    echo "expected placeholder scanner to fail"
    cat /tmp/no-placeholder-fail.out
    exit 1
fi

if ! grep -q "placeholder.todo" "$tmp/report.fail.json"; then
    echo "expected placeholder.todo finding"
    cat "$tmp/report.fail.json"
    exit 1
fi

cat > "$tmp/src/bad.c" <<'EOF'
int intentional_negative_test(void) {
    // TODO: this fixture intentionally contains a token. zcc-no-placeholder-allow
    return 0;
}
EOF

python3 tools/check_no_placeholders.py \
  --root "$tmp" \
  --report "$tmp/report.pass.json"

if ! grep -q '"status": "pass"' "$tmp/report.pass.json"; then
    echo "expected placeholder scanner to pass with allow marker"
    cat "$tmp/report.pass.json"
    exit 1
fi

echo "[SELFTEST-NO-PLACEHOLDERS] passed"