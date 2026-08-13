#!/usr/bin/env bash
set -Eeuo pipefail

# -------------------------------------------------------------------
# ZCC Fault Injection v2 (struct-slot collision class)
# Preserves prior capabilities + adds deterministic line-mode mutation.
#
# Modes:
#   FI_MODE=anchor (default) : mutate first slot progression near anchor
#   FI_MODE=line             : mutate exact FI_LINE_NUMBER in FI_TARGET_FILE
#
# Exit codes:
#   0   RED observed (expected)
#   10  Injection applied but RED command passed (insensitive gate)
#   2+  Setup/mutation errors
# -------------------------------------------------------------------

# ---- Config (env-overridable) ----
FI_MODE="${FI_MODE:-anchor}"                              # anchor | line
FI_TARGET_FILE="${FI_TARGET_FILE:-part4.c}"
FI_ANCHOR_PATTERN="${FI_ANCHOR_PATTERN:-codegen_func}"
FI_LINE_NUMBER="${FI_LINE_NUMBER:-}"                      # required for FI_MODE=line
FI_ALLOW_DIRTY="${FI_ALLOW_DIRTY:-0}"                     # 0/1
FI_RED_CMD="${FI_RED_CMD:-bash scripts/fault_probe_struct_slot.sh}"
FI_STATE_DIR="${FI_STATE_DIR:-.fi_state}"

# Mutation family: currently no-op progression to induce alias collision
FI_MUTATION_KIND="${FI_MUTATION_KIND:-progression_noop}"

PATCH_FILE="$FI_STATE_DIR/fault_inject.patch"
META_FILE="$FI_STATE_DIR/fault_inject.meta"
BACKUP_FILE="$FI_STATE_DIR/original.$(basename "$FI_TARGET_FILE")"

mkdir -p "$FI_STATE_DIR"

log(){ printf '[fault_inject] %s\n' "$*"; }
warn(){ printf '[fault_inject][warn] %s\n' "$*" >&2; }
die(){ printf '[fault_inject][error] %s\n' "$*" >&2; exit 2; }

need_cmd() { command -v "$1" >/dev/null 2>&1 || die "required command missing: $1"; }
is_true() {
  case "${1:-0}" in
    1|true|TRUE|yes|YES|on|ON) return 0 ;;
    *) return 1 ;;
  esac
}

write_meta() {
  local key="$1" val="$2"
  printf '%s=%s\n' "$key" "$val" >> "$META_FILE"
}

# ---- Preconditions ----
need_cmd git
need_cmd python3
[[ -f "$FI_TARGET_FILE" ]] || die "target file not found: $FI_TARGET_FILE"
[[ "$FI_MODE" == "anchor" || "$FI_MODE" == "line" ]] || die "FI_MODE must be anchor|line"

if [[ -f "$PATCH_FILE" ]]; then
  die "existing injection patch found at $PATCH_FILE; run fault_restore.sh first"
fi

if ! is_true "$FI_ALLOW_DIRTY"; then
  if ! git diff --quiet || ! git diff --cached --quiet; then
    die "working tree has unstaged/staged changes (set FI_ALLOW_DIRTY=1 to override)"
  fi
else
  warn "FI_ALLOW_DIRTY=1 set; restore guarantees are weaker on dirty trees"
fi

cp "$FI_TARGET_FILE" "$BACKUP_FILE"
: > "$META_FILE"

write_meta mode "$FI_MODE"
write_meta target_file "$FI_TARGET_FILE"
write_meta anchor "$FI_ANCHOR_PATTERN"
write_meta line_number "${FI_LINE_NUMBER:-}"
write_meta mutation_kind "$FI_MUTATION_KIND"
write_meta red_cmd "$FI_RED_CMD"
write_meta created_at_utc "$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

# ---- Mutation engine (python) ----
python3 - "$FI_TARGET_FILE" "$FI_MODE" "$FI_ANCHOR_PATTERN" "$FI_LINE_NUMBER" "$FI_MUTATION_KIND" <<'PY'
import re
import sys
from pathlib import Path

path = Path(sys.argv[1])
mode = sys.argv[2]
anchor = sys.argv[3]
line_number = sys.argv[4].strip()
mutation_kind = sys.argv[5]

src = path.read_text()
lines = src.splitlines()

def mutate_progression_stmt(stmt: str):
    """
    Transform offset progression into no-op:
      x += something;  -->  /* FI ... */ x += 0;
      x -= something;  -->  /* FI ... */ x -= 0;
    """
    m = re.search(r'\b([A-Za-z_]\w*)\s*(\+=|-=)\s*[^;]+;', stmt)
    if not m:
        return None
    lhs = m.group(1)
    op = m.group(2)
    return re.sub(r'\b([A-Za-z_]\w*)\s*(\+=|-=)\s*[^;]+;',
                  f'/* FI: forced slot collision */ {lhs} {op} 0;',
                  stmt, count=1)

def mutate_line_mode(lines, line_no_1):
    if line_no_1 < 1 or line_no_1 > len(lines):
        raise RuntimeError(f"line out of range: {line_no_1}")

    original = lines[line_no_1 - 1]
    mutated = mutate_progression_stmt(original)
    if mutated is None:
        raise RuntimeError(f"line {line_no_1} is not a progression statement: {original}")
    lines[line_no_1 - 1] = mutated
    return lines, ("line", line_no_1, original, mutated)

def mutate_anchor_mode(src, anchor):
    if anchor not in src:
        raise RuntimeError(f"anchor not found: {anchor}")

    idx = src.find(anchor)
    start = max(0, idx - 600)
    end = min(len(src), idx + 6400)
    region = src[start:end]

    patterns = [
        r'\bparam_offset\s*-=\s*[^;\n]+;\s*//[^\n]*',
        r'\bparam_offset\s*-=\s*[^;\n]+;',
        r'\boffset\s*\+=\s*[^;\n]+;',
        r'\bslot\s*\+=\s*[^;\n]+;',
        r'\bstack_offset\s*\+=\s*[^;\n]+;',
        r'\btop\s*\+=\s*[^;\n]+;',
        r'\bcur\s*\+=\s*[^;\n]+;',
    ]

    for pat in patterns:
        m = re.search(pat, region)
        if m:
            stmt = m.group(0)
            mutated = mutate_progression_stmt(stmt)
            if not mutated:
                continue
            region2 = region[:m.start()] + mutated + region[m.end():]
            out = src[:start] + region2 + src[end:]
            global_pos = start + m.start()
            line_no = src.count('\n', 0, global_pos) + 1
            return out, ("anchor", line_no, stmt, mutated)

    raise RuntimeError("no slot progression statement found near anchor window")

if mutation_kind != "progression_noop":
    raise RuntimeError(f"unsupported mutation kind: {mutation_kind}")

if mode == "line":
    if not line_number:
        raise RuntimeError("FI_LINE_NUMBER required for mode=line")
    lno = int(line_number)
    out_lines, detail = mutate_line_mode(lines, lno)
    out = "\n".join(out_lines) + ("\n" if src.endswith("\n") else "")
elif mode == "anchor":
    out, detail = mutate_anchor_mode(src, anchor)
else:
    raise RuntimeError(f"unsupported mode: {mode}")

path.write_text(out)

kind, line_no, before, after = detail
print(f"mutation=applied")
print(f"mode={kind}")
print(f"line={line_no}")
print(f"before={before}")
print(f"after={after}")
PY

# ---- Build reversible patch ----
git diff -- "$FI_TARGET_FILE" > "$PATCH_FILE"
[[ -s "$PATCH_FILE" ]] || die "generated patch is empty"

{
  echo "patch_file=$PATCH_FILE"
  echo "backup_file=$BACKUP_FILE"
} >> "$META_FILE"

log "Injection patch created: $PATCH_FILE"
log "Running RED command (expect non-zero): $FI_RED_CMD"

set +e
bash -lc "$FI_RED_CMD"
RC=$?
set -e

echo "red_exit=$RC"
echo "red_exit=$RC" >> "$META_FILE"

if [[ "$RC" -eq 0 ]]; then
  warn "RED command exited 0; gate may be insensitive or mutation path not covered"
  exit 10
fi

log "RED observed as expected."
exit 0
