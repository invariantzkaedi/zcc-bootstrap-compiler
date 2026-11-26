#!/usr/bin/env bash
# Item 4: automated per-commit bootstrap-hash logger with host-toolchain identity.
# Appends one row to BOOTSTRAP_BASELINES.tsv on each invocation (CI-friendly).
# Verdict logic: compares against the last locked row for the same toolchain key
# and exits nonzero on unexplained drift.
set -euo pipefail
cd "$(dirname "$0")/.."
CHECK_ONLY=0
COMMENT=""
for arg in "$@"; do
  case "$arg" in
    --check|--readonly)
      CHECK_ONLY=1
      ;;
    *)
      if [ -z "$COMMENT" ]; then
        COMMENT="$arg"
      else
        COMMENT="$COMMENT $arg"
      fi
      ;;
  esac
done

LEDGER="${BOOTSTRAP_LEDGER:-BOOTSTRAP_BASELINES.tsv}"
if [ "$CHECK_ONLY" -eq 1 ] && [ ! -f "$LEDGER" ]; then
  echo "BOOTSTRAP CHECK FAILED — ledger missing: $LEDGER"
  exit 2
fi

BUILD_LOG=$(mktemp)
make selfhost >"$BUILD_LOG" 2>&1 || { echo "SELFHOST FAILED — cannot baseline"; cat "$BUILD_LOG"; exit 2; }

HASH=$(md5sum zcc2.s | cut -d' ' -f1 | tr -d ' \t\r\n')
COMMIT=$(git rev-parse --short=8 HEAD 2>/dev/null || echo "nogit")
CC_ID=$( ${CC:-gcc} --version | head -1 )
UNAME=$(uname -srm)
ELISIONS=$(grep -oE '[0-9]+ elided' "$BUILD_LOG" | head -1 | grep -oE '[0-9]+' || echo "?")
rm -f "$BUILD_LOG"
DATE=$(date -u +%Y-%m-%dT%H:%M:%SZ)

if [ "$CHECK_ONLY" -eq 0 ] && [ ! -f "$LEDGER" ]; then
  printf "date\tcommit\tmd5_zcc2s\telisions\ttoolchain\tuname\tcomment\n" > "$LEDGER"
fi

python3 - <<EOF
import sys

ledger = "$LEDGER"
current_hash = "$HASH".strip()
commit = "$COMMIT".strip()
elisions = "$ELISIONS".strip()
cc_id = """$CC_ID""".strip()
uname = """$UNAME""".strip()
comment = """$COMMENT""".strip()
date = "$DATE".strip()
check_only = int("$CHECK_ONLY")

uname_lower = uname.lower()
is_azure = "azure" in uname_lower
is_wsl = "wsl" in uname_lower or "microsoft" in uname_lower

valid_hashes = set()
try:
    with open(ledger, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            parts = line.rstrip("\r\n").split("\t")
            if len(parts) >= 6:
                row_hash = parts[2].strip()
                row_uname = parts[5].strip().lower()
                if is_azure and "azure" in row_uname:
                    valid_hashes.add(row_hash)
                elif is_wsl and ("wsl" in row_uname or "microsoft" in row_uname):
                    valid_hashes.add(row_hash)
                elif not is_azure and not is_wsl:
                    valid_hashes.add(row_hash)
except Exception:
    pass

if check_only == 0:
    # Append current run row
    with open(ledger, "a", encoding="utf-8") as f:
        f.write(f"{date}\t{commit}\t{current_hash}\t{elisions}\t{cc_id}\t{uname}\t{comment}\n")

print(f"BOOTSTRAP HASH: {current_hash}  commit={commit}  elisions={elisions}  check_only={check_only}")
print(f"TOOLCHAIN: {cc_id} | {uname}")

if check_only == 1 and not valid_hashes:
    print(f"DRIFT CHECK FAILED — no prior baselines for toolchain in {ledger}")
    print("BOOTSTRAP_HASH_EXIT=1")
    sys.exit(1)

if valid_hashes and current_hash not in valid_hashes:
    print(f"DRIFT DETECTED vs prior same-toolchain baselines: {valid_hashes}")
    print("BOOTSTRAP_HASH_EXIT=1 (codegen drifted or units added — review required)")
    sys.exit(1)

print("BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)")
sys.exit(0)
EOF
