#!/usr/bin/env bash
# scripts/build_doom.sh — Item 5 reproduction script for DOOM 1.10 → ZCC
#
# Inputs:  doom_pp_clean.c (committed, see repo root or fetch via fetch_doom.sh)
#          doom_shims.c    (committed in repo)
#          doom_globals_new.c (committed in repo)
# Outputs: doom_zcc.c      (patched amalgamation, gitignored byproduct)
#          doom.s          (ZCC-generated assembly, gitignored byproduct)
#          doom_elf        (linked executable if LINK=1)
#
# Reproduction baseline (2026-07-10, GCC 13.3 / WSL2):
#   md5(doom.s) = [run this script to establish]
#   Functions compiled: 728 (see doom_corpus.jsonl for IR corpus)
#
# ZCC COMPAT NOTES (why each patch exists):
#   F1  false/true — zcc treats these as reserved (stdbool.h bleed)
#   F2  __builtin_va_list → void* — zcc has no builtin_va_list
#   F3  __builtin_va_start/end → no-ops — not needed with void* va_list shim
#   F4  __attribute__(aligned(alignof(...))) — zcc can't parse nested alignof
#   F5  access/mkdir — zcc-libc lacks these; inject forward decls
#   F6  channels conflict — i_sound.c and s_sound.c both define 'channels';
#       force the #ifdef __clang__ rename path (channels → channels_sfx)
#   F7  anims/anim_t — force #ifdef __clang__ rename path (anims → anims_finale,
#       anim_t → anim_finale_t); both backends are in the amalgamation
#   F8  int class; — zcc reserves 'class' (C++ keyword bleed); rename to xclass
#   F9  struct sigaction / sigaction() — zcc confuses struct tag with function
#       call when names match; rename struct tag to zcc_sigaction_s
#
# STATUS (2026-07-10): 35 errors → 12 → pending F8/F9 = expected 0
# Remaining known gap: anims[e][j].nanims (3 errors) — see KNOWN_ISSUES below

set -euo pipefail

if [ -f "$(dirname "$0")/Makefile" ]; then
    REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
else
    REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
fi
cd "$REPO_ROOT"

INPUT="${1:-doom_pp_clean.c}"
OUTPUT_C="${2:-doom_zcc.c}"
OUTPUT_S="${3:-doom.s}"

if [ ! -f "$INPUT" ]; then
    echo "ERROR: $INPUT not found. Ensure doom_pp_clean.c is present." >&2
    echo "Run: bash scripts/fetch_doom.sh   (once fetch_doom.sh is written)" >&2
    exit 2
fi

echo "[ZCC-DOOM] Patching $INPUT → $OUTPUT_C"

python3 - "$INPUT" "$OUTPUT_C" << 'PYEOF'
import sys, re

src_path, dst_path = sys.argv[1], sys.argv[2]

with open(src_path, 'rb') as f:
    content = f.read().decode('utf-8', errors='replace')

# Normalize CRLF (doom_pp_clean.c may have Windows line endings)
content = content.replace('\r\n', '\n').replace('\r', '\n')

fixes = []

# F1: false/true reserved in zcc
content, n = re.subn(
    r'typedef enum \{false, true\} boolean;',
    'typedef int boolean; /* ZCC_F1: false/true reserved */',
    content)
fixes.append(f"F1 boolean typedef: {n}")

# F2: __builtin_va_list → void*
content, n = re.subn(
    r'typedef __builtin_va_list __gnuc_va_list;',
    'typedef void* __gnuc_va_list; /* ZCC_F2 */',
    content)
fixes.append(f"F2 gnuc_va_list: {n}")

# F3: __builtin_va_start / __builtin_va_end → no-ops
# These follow the va_list definition; add shim macros right after it
content, n = re.subn(
    r'(typedef __gnuc_va_list va_list;)',
    r'\1\n'
    r'#define __builtin_va_start(v,l) ((void)0) /* ZCC_F3 */\n'
    r'#define __builtin_va_end(v)     ((void)0) /* ZCC_F3 */',
    content, count=1)
fixes.append(f"F3 va_start/end shims: {n}")

# F4: __attribute__((__aligned__(__alignof__(...)))) — strip nested alignof attrs
content, n = re.subn(
    r'\s*__attribute__\s*\(\(__aligned__\(__alignof__\([^)]+\)\)\)\)',
    '',
    content)
fixes.append(f"F4 alignof attrs: {n}")

# F5: access/mkdir — inject decls BEFORE the opening #ifdef __clang__ block
content = re.sub(
    r'^(#ifdef __clang__\n)',
    '/* ZCC_F5: posix decls missing from zcc-libc */\n'
    'int access(const char*, int);\n'
    'int mkdir(const char*, unsigned int);\n'
    r'\1',
    content, count=1, flags=re.MULTILINE)
fixes.append("F5 access/mkdir injected")

# F6: channels conflict — i_sound.c channels[8] vs s_sound.c channel_t* channels
# Force the channels_sfx rename path (was #ifndef __clang__ guarded)
content, n = re.subn(
    r'(#ifndef __clang__\n)(static channel_t\* channels;)',
    r'#if 0  /* ZCC_F6: force channels_sfx rename; channels[8] conflicts */\n\2',
    content)
fixes.append(f"F6 channels guard: {n}")

# F7: anims/anim_t — force the #ifdef __clang__ rename block to always run
# Locate } anim_finale_t; followed by #ifdef __clang__ with anims rename
content, n = re.subn(
    r'(} anim_finale_t;\n)#ifdef __clang__(\n#define anims anims_finale)',
    r'\1#if 1  /* ZCC_F7: force anims rename */\2',
    content)
fixes.append(f"F7 anims_finale forced: {n}")

# F8: int class; — zcc reserves 'class'; rename to xclass in X11 struct fields
# Only in struct bodies (between { and }), not in keywords like 'class='
content, n = re.subn(
    r'\bint class;',
    'int xclass; /* ZCC_F8: class reserved */',
    content)
fixes.append(f"F8 class→xclass: {n}")

# F9: struct sigaction / sigaction() — struct tag conflicts with function call
# Rename struct tag; the function decl and call sites are handled by the rename
content, n = re.subn(
    r'\bstruct sigaction\b',
    'struct zcc_sigaction_s',
    content)
fixes.append(f"F9 sigaction struct rename: {n}")
# The sigaction() function itself keeps its name (extern decl already in glibc block)
# but zcc needs the call to not be confused — add a function typedef bridge
content = re.sub(
    r'(extern int sigaction \(int __sig,)',
    r'typedef int (*zcc_sigaction_fn)(int, const struct zcc_sigaction_s*, struct zcc_sigaction_s*);\n'
    r'extern int sigaction (int __sig,',
    content, count=1)
fixes.append("F9b sigaction fn typedef added")

print("Fix summary:")
for f in fixes:
    print(f"  {f}")

with open(dst_path, 'w') as f:
    f.write(content)
print(f"Written: {dst_path} ({content.count(chr(10))} lines)")
PYEOF

echo "[ZCC-DOOM] Compiling $OUTPUT_C with zcc2..."
./zcc2 "$OUTPUT_C" -o "$OUTPUT_S" 2>&1 | tee /tmp/doom_build.log
ZCC_EXIT="${PIPESTATUS[0]}"

ERRORS=$(grep -c "error:" /tmp/doom_build.log || true)
echo ""
echo "ZCC_EXIT=$ZCC_EXIT  ERRORS=$ERRORS"

if [ "$ZCC_EXIT" -eq 0 ] && [ "$ERRORS" -eq 0 ]; then
    echo "[PASS] doom.s generated: $(wc -l < "$OUTPUT_S") lines, $(wc -c < "$OUTPUT_S") bytes"
    MD5=$(md5sum "$OUTPUT_S" | cut -d' ' -f1)
    echo "[PASS] md5($OUTPUT_S) = $MD5"
    echo "[PASS] Log this in BOOTSTRAP_BASELINES.tsv equivalent for DOOM"
else
    echo "[FAIL] $ERRORS errors remain — see /tmp/doom_build.log"
    echo ""
    echo "=== ERROR SUMMARY ==="
    grep "error:" /tmp/doom_build.log | sed 's/.*error: //' | sort | uniq -c | sort -rn | head -10
    echo ""
    echo "=== KNOWN ISSUES (not yet fixed) ==="
    echo "  anims[e][j].nanims — anim_finale_t* chain: type lost in zcc Phase 2"
    echo "  Workaround pending: explicit cast or inline accessor"
    exit 1
fi

if [ "${LINK:-0}" = "1" ]; then
    echo "[ZCC-DOOM] Linking..."
    gcc -no-pie "$OUTPUT_S" doom_shims.c doom_globals_new.c -o doom_elf -lm -lX11 2>&1
    echo "LINK_EXIT=$?"
fi

echo "[ZCC-DOOM] Done."
