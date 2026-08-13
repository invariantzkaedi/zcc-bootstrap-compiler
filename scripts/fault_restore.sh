#!/usr/bin/env bash
set -Eeuo pipefail

# -------------------------------------------------------------------
# ZCC Fault Restore v2
# Reverses patch created by scripts/fault_inject.sh and validates restore.
# Preserves prior capabilities; adds stronger verification.
# -------------------------------------------------------------------

FI_STATE_DIR="${FI_STATE_DIR:-.fi_state}"
PATCH_FILE="$FI_STATE_DIR/fault_inject.patch"
META_FILE="$FI_STATE_DIR/fault_inject.meta"

log(){ printf '[fault_restore] %s\n' "$*"; }
warn(){ printf '[fault_restore][warn] %s\n' "$*" >&2; }
die(){ printf '[fault_restore][error] %s\n' "$*" >&2; exit 2; }

need_cmd() { command -v "$1" >/dev/null 2>&1 || die "required command missing: $1"; }

meta_get() {
  local key="$1"
  [[ -f "$META_FILE" ]] || return 1
  grep -E "^${key}=" "$META_FILE" | tail -n1 | sed "s/^${key}=//"
}

need_cmd git
[[ -f "$PATCH_FILE" ]] || die "missing patch file: $PATCH_FILE"

TARGET_FILE="$(meta_get target_file || true)"
BACKUP_FILE="$(meta_get backup_file || true)"

log "Reverting injected patch..."
git apply -R "$PATCH_FILE" || die "failed to reverse-apply patch"

if [[ -n "$TARGET_FILE" && -f "$TARGET_FILE" ]]; then
  if ! git diff --quiet -- "$TARGET_FILE"; then
    die "target file still dirty after restore: $TARGET_FILE"
  fi
fi

# Optional content equivalence check against backup if present
if [[ -n "$BACKUP_FILE" && -f "$BACKUP_FILE" && -n "$TARGET_FILE" && -f "$TARGET_FILE" ]]; then
  if ! cmp -s "$BACKUP_FILE" "$TARGET_FILE"; then
    warn "target file differs from backup after restore (check unrelated edits)"
  fi
fi

rm -f "$PATCH_FILE" "$META_FILE"

log "Restore complete."
exit 0
