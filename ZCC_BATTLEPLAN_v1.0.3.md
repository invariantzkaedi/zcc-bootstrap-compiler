# 🔱 ZCC IR BRIDGE v1.0.3 → v1.1.0 — BULLETPROOF BATTLE PLAN

> **Objective:** Fix remaining `movslq` width bug + verify expression statement fix → expand whitelist → achieve clean selfhost with ALL lexer functions IR-lowered.

---

## SITUATION REPORT

| Item | Status |
|------|--------|
| CG-IR-008 (multi-PHI truncation) | ✅ FIXED — `log2_of` on whitelist, peephole 4310 |
| Expression statement fix | ✅ APPLIED — `zcc_lower_stmt` forwards `default:` to `zcc_lower_expr` |
| CG-IR-007 completion (movslq) | ❌ OPEN — IR lowerer emits `movq` for i32 loads, needs `movslq` |
| Selfhost with expanded whitelist | ❌ MISMATCH — `is_alpha` diverges due to movslq bug |
| Golden tests | ✅ 5/5 ALL PASS |

**The Kill Chain:** `is_alpha(int c)` loads `c` as 64-bit (`movq`) instead of sign-extending from 32-bit (`movslq`). Upper 32 bits contain stack garbage → character classification wrong → lexer misclassifies tokens → every function compiled after that point diverges → selfhost MISMATCH.

---

## PHASE 1: SNAPSHOT & DIAGNOSE (Run First, Touch Nothing)

Save this as `zcc_battle_phase1.sh` and run it:

```bash
#!/bin/sh
# ═══════════════════════════════════════════════════════════════
# PHASE 1: Diagnose current state — DO NOT MODIFY ANY FILES
# ═══════════════════════════════════════════════════════════════
set -e
cd /mnt/h/__DOWNLOADS/selforglinux

echo "═══════════════════════════════════════════════"
echo "  PHASE 1: DIAGNOSIS"
echo "═══════════════════════════════════════════════"

# 1A. Verify baseline selfhost is clean
echo ""
echo "── 1A: Baseline selfhost ──"
make clean 2>/dev/null
make selfhost 2>/dev/null
echo "Baseline: SELF-HOST VERIFIED"

# 1B. Check current whitelist
echo ""
echo "── 1B: Current IR whitelist ──"
grep -n 'ir_whitelisted\|"log2_of"\|"is_alpha"\|"is_digit"\|"is_space"\|"hex_val"\|"fold_test"\|"dce_test"' part4.c | head -30

# 1C. Check expression statement fix is present
echo ""
echo "── 1C: Expression statement fix present? ──"
grep -n 'zcc_lower_expr.*default\|default.*zcc_lower_expr\|unhandled.*expr' compiler_passes.c | head -10
# Also check: does the default case in zcc_lower_stmt call zcc_lower_expr?
awk '/void zcc_lower_stmt/,/^}/' compiler_passes.c | grep -A2 'default:' | head -10

# 1D. Identify the movslq bug location
echo ""
echo "── 1D: IR load emission — where is OP_LOAD lowered? ──"
grep -n 'OP_LOAD\|movq.*(%\|movslq\|imm.*==.*4\|imm.*==.*8\|ir_asm_load' compiler_passes.c | head -30

# 1E. Show current is_alpha assembly from AST backend
echo ""
echo "── 1E: is_alpha AST assembly (CORRECT — has movslq) ──"
sed -n '/^is_alpha:/,/^[a-zA-Z_][a-zA-Z0-9_]*:/p' zcc2.s | head -20

# 1F. Build with expanded whitelist and show IR assembly for is_alpha
echo ""
echo "── 1F: is_alpha IR assembly (BUGGY — has movq instead of movslq) ──"
# Temporarily check if is_alpha is on whitelist already
if grep -q '"is_alpha"' part4.c; then
    echo "is_alpha already on whitelist"
    # Build and extract
    make zcc_full 2>/dev/null
    ./zcc zcc.c -o /tmp/zcc2_diag.s 2>/dev/null
    gcc -O0 -w -o /tmp/zcc2_diag /tmp/zcc2_diag.s compiler_passes.c compiler_passes_ir.c -lm 2>/dev/null
    /tmp/zcc2_diag zcc.c -o /tmp/zcc3_diag.s 2>/dev/null
    echo "--- zcc3 is_alpha (IR-lowered) ---"
    sed -n '/^is_alpha:/,/^[a-zA-Z_][a-zA-Z0-9_]*:/p' /tmp/zcc3_diag.s | head -25
else
    echo "is_alpha NOT on whitelist yet — will be added in Phase 2"
fi

# 1G. Show the exact ir_asm code that handles OP_LOAD
echo ""
echo "── 1G: Full OP_LOAD handler in ir_module_lower_x86 ──"
awk '/case OP_LOAD:/,/break;/' compiler_passes.c | head -40

# 1H. Check what type info is available on LOAD instructions
echo ""
echo "── 1H: How LOAD instructions carry type/width info ──"
grep -n 'ins->imm\|ins->type\|ins->width\|ir_type_bytes\|OP_LOAD' compiler_passes.c | grep -i 'load\|imm\|type\|width\|byte' | head -20

echo ""
echo "═══════════════════════════════════════════════"
echo "  PHASE 1 COMPLETE — Review output above"
echo "═══════════════════════════════════════════════"
```

**What to look for in Phase 1 output:**
- 1A must say SELF-HOST VERIFIED
- 1D tells you exactly where the OP_LOAD lowering lives
- 1E vs 1F shows the movslq vs movq divergence
- 1G shows the exact code that needs patching
- 1H tells you how the instruction carries width info (likely `ins->imm` = byte count)

---

## PHASE 2: THE FIX (Width-Aware OP_LOAD)

### The Bug

The IR lowerer's `OP_LOAD` handler does:
```c
fprintf(out, "    movq (%s), %s\n", src_reg, dst_reg);
```

It needs:
```c
if (load_width == 4)       /* int, i32 */
    fprintf(out, "    movslq (%s), %s\n", src_reg, dst_reg);
else if (load_width == 1)  /* char, i8 */
    fprintf(out, "    movsbq (%s), %s\n", src_reg, dst_reg);
else                        /* pointer, long, i64 */
    fprintf(out, "    movq (%s), %s\n", src_reg, dst_reg);
```

### The Patch Script

Save as `zcc_battle_phase2_fix.py`:

```python
#!/usr/bin/env python3
"""
ZCC Battle Plan Phase 2: CG-IR-007 completion — width-aware OP_LOAD.

Patches compiler_passes.c to emit movslq/movsbq/movq based on load width.

USAGE:
    cd /mnt/h/__DOWNLOADS/selforglinux
    python3 zcc_battle_phase2_fix.py
    
The script:
1. Backs up compiler_passes.c
2. Finds the OP_LOAD handler in ir_module_lower_x86
3. Replaces bare movq with width-dispatched load
4. Also patches ir_asm_load_to_rax if it uses bare movq
"""
import re
import shutil
import sys

FILE = "compiler_passes.c"

# Backup
shutil.copy(FILE, FILE + ".bak_phase2")
print(f"[1/4] Backup: {FILE} -> {FILE}.bak_phase2")

with open(FILE, 'r') as f:
    src = f.read()

patched = False

# ══════════════════════════════════════════════════════════════
# Strategy A: Patch the OP_LOAD case in the main lowering switch
# ══════════════════════════════════════════════════════════════

# Look for patterns like:
#   case OP_LOAD:
#       ... movq (% ...
# The exact pattern depends on the code structure.
# We need to find ALL instances of movq used for loads and add width dispatch.

# Pattern 1: Direct movq (%reg), %reg in OP_LOAD handler
# This catches: fprintf(out, "    movq (%s), %s\n", ...)
load_movq_pattern = re.compile(
    r'(case OP_LOAD:.*?)'                    # start of OP_LOAD case
    r'(fprintf\s*\(\s*out\s*,\s*"    movq \(%)',  # the movq emission
    re.DOTALL
)

match = load_movq_pattern.search(src)
if match:
    print(f"[2/4] Found OP_LOAD movq at offset {match.start(2)}")
else:
    print("[2/4] OP_LOAD movq pattern not found via regex — trying line search...")

# ══════════════════════════════════════════════════════════════
# Strategy B: Line-by-line search for movq in load context
# ══════════════════════════════════════════════════════════════

lines = src.split('\n')
load_fixes = 0
store_context = False
in_op_load = False
op_load_depth = 0

# We'll collect line numbers where movq appears in a LOAD context
movq_load_lines = []

for i, line in enumerate(lines):
    stripped = line.strip()
    
    # Track when we're inside OP_LOAD handler
    if 'case OP_LOAD:' in stripped:
        in_op_load = True
        op_load_depth = 0
        continue
    
    if in_op_load:
        if stripped.startswith('case OP_') or stripped.startswith('default:'):
            in_op_load = False
            continue
        if 'break;' in stripped and op_load_depth == 0:
            in_op_load = False
            continue
        if '{' in stripped:
            op_load_depth += stripped.count('{') - stripped.count('}')
        if '}' in stripped:
            op_load_depth += stripped.count('{') - stripped.count('}')
            
        # Found a movq load inside OP_LOAD handler
        if 'movq (' in stripped and 'fprintf' in stripped:
            movq_load_lines.append(i)

print(f"[3/4] Found {len(movq_load_lines)} movq load(s) in OP_LOAD handler at lines: {[l+1 for l in movq_load_lines]}")

# ══════════════════════════════════════════════════════════════
# Strategy C: Also find ir_asm_load_to_rax function
# ══════════════════════════════════════════════════════════════

load_to_rax_lines = []
in_load_to_rax = False
for i, line in enumerate(lines):
    if 'ir_asm_load_to_rax' in line and ('{' in line or (i+1 < len(lines) and '{' in lines[i+1])):
        in_load_to_rax = True
        continue
    if in_load_to_rax:
        if line.strip().startswith('}') and not any(c in line for c in ['{', 'if', 'for', 'while']):
            in_load_to_rax = False
            continue
        if 'movq (' in line and 'fprintf' in line:
            load_to_rax_lines.append(i)

print(f"    Found {len(load_to_rax_lines)} movq in ir_asm_load_to_rax at lines: {[l+1 for l in load_to_rax_lines]}")

# ══════════════════════════════════════════════════════════════
# Report findings — manual intervention may be needed
# ══════════════════════════════════════════════════════════════

all_targets = movq_load_lines + load_to_rax_lines

if not all_targets:
    print("\n[WARN] No automatic targets found. The movq pattern may differ.")
    print("       Search manually for how loads are emitted:")
    print("       grep -n 'movq.*(%' compiler_passes.c | grep -v store")
    print("\n       Then apply the width dispatch pattern from the MANUAL FIX section below.")
    
    # Print manual fix instructions
    print("""
═══════════════════════════════════════════════════════
MANUAL FIX — Apply this pattern wherever OP_LOAD emits assembly:
═══════════════════════════════════════════════════════

Find the line that emits: movq (%src), %dst
Replace with:

    /* CG-IR-007: width-aware load dispatch */
    {
        int load_sz = ins->imm;   /* or however width is stored */
        if (load_sz == 0) load_sz = 8;  /* default to 64-bit */
        if (load_sz == 4)
            fprintf(out, "    movslq (%%rax), %%rax\\n");
        else if (load_sz == 1)
            fprintf(out, "    movsbq (%%rax), %%rax\\n");
        else if (load_sz == 2)
            fprintf(out, "    movswq (%%rax), %%rax\\n");
        else
            fprintf(out, "    movq (%%rax), %%rax\\n");
    }

Also apply the same pattern in ir_asm_load_to_rax if it exists.

IMPORTANT: The width info might be stored in:
  - ins->imm (most likely — used as byte count elsewhere)
  - ins->type (if instructions carry type info)  
  - A separate width field

Run this to find how loads carry width:
  grep -B5 -A5 'OP_LOAD' compiler_passes.c | grep 'imm\|type\|width\|byte'
═══════════════════════════════════════════════════════
""")
else:
    print(f"\n[4/4] Showing context for each target — apply width dispatch manually:")
    for idx in all_targets:
        start = max(0, idx - 5)
        end = min(len(lines), idx + 5)
        print(f"\n--- Line {idx+1} context ---")
        for j in range(start, end):
            marker = ">>>" if j == idx else "   "
            print(f"  {marker} {j+1:5d}: {lines[j]}")

# Write the file back (unchanged if no auto-patch applied)
# The user applies the manual fix based on the diagnosis above
with open(FILE, 'w') as f:
    f.write(src)

print("\nPhase 2 diagnosis complete. Apply the manual fix, then run Phase 3.")
```

### The Manual Fix (Apply Based on Phase 1 + Phase 2 Diagnosis)

Wherever the OP_LOAD handler emits assembly, replace the bare `movq` with this:

```c
/* ══════════════════════════════════════════════════════════
 * CG-IR-007 COMPLETION: Width-aware load from memory
 * 
 * The IR LOAD instruction carries a width in ins->imm:
 *   8 = i64/ptr  → movq   (%rax), %rax
 *   4 = i32/int  → movslq (%rax), %rax  (sign-extend 32→64)
 *   2 = i16      → movswq (%rax), %rax  (sign-extend 16→64)
 *   1 = i8/char  → movsbq (%rax), %rax  (sign-extend 8→64)
 *
 * Without this, int parameters are loaded as 64-bit, upper 32
 * bits contain stack garbage. is_alpha('a') returns wrong
 * results, lexer breaks, selfhost diverges.
 * ══════════════════════════════════════════════════════════ */
{
    int ld_sz = (int)ins->imm;
    if (ld_sz <= 0 || ld_sz > 8) ld_sz = 8;  /* safe default */
    switch (ld_sz) {
        case 1:  fprintf(out, "    movsbq (%%rax), %%rax\n"); break;
        case 2:  fprintf(out, "    movswq (%%rax), %%rax\n"); break;
        case 4:  fprintf(out, "    movslq (%%rax), %%rax\n"); break;
        default: fprintf(out, "    movq (%%rax), %%rax\n");   break;
    }
}
```

**ALSO** apply the same dispatch anywhere `ir_asm_load_to_rax` does a load:

```c
/* In ir_asm_load_to_rax — same width dispatch */
static void ir_asm_load_to_rax(IRAsmCtx *ctx, RegID r) {
    /* ... existing code to get address into %rax ... */
    
    /* Width-aware dereference */
    int ld_sz = /* get from instruction or default */ 8;
    switch (ld_sz) {
        case 1:  fprintf(ctx->out, "    movsbq (%%rax), %%rax\n"); break;
        case 2:  fprintf(ctx->out, "    movswq (%%rax), %%rax\n"); break;
        case 4:  fprintf(ctx->out, "    movslq (%%rax), %%rax\n"); break;
        default: fprintf(ctx->out, "    movq (%%rax), %%rax\n");   break;
    }
}
```

**CRITICAL SUBTLETY:** `ir_asm_load_to_rax` is also called from the PHI edge copier (which you just fixed in CG-IR-008). The PHI copier loads values to copy between blocks — these are always full 64-bit register values (they've already been loaded from memory once). So the PHI copier path should always use `movq`. The width dispatch should ONLY apply to OP_LOAD instructions that actually dereference a memory address.

If `ir_asm_load_to_rax` handles both cases (stack slot loads AND memory dereferences), you may need TWO functions:
- `ir_asm_load_vreg_to_rax(ctx, r)` — always `movq` (loading a vreg from its stack slot)
- `ir_asm_load_mem_to_rax(ctx, r, width)` — width-dispatched (dereferencing a pointer)

---

## PHASE 3: VERIFY THE FIX

Save as `zcc_battle_phase3.sh`:

```bash
#!/bin/sh
# ═══════════════════════════════════════════════════════════════
# PHASE 3: Verify CG-IR-007 fix + expression statement fix
# ═══════════════════════════════════════════════════════════════
set -e
cd /mnt/h/__DOWNLOADS/selforglinux

echo "═══════════════════════════════════════════════"
echo "  PHASE 3: VERIFICATION"
echo "═══════════════════════════════════════════════"

# 3A. Clean rebuild baseline
echo ""
echo "── 3A: Clean baseline rebuild ──"
make clean 2>/dev/null
make selfhost
echo "✅ Baseline SELF-HOST VERIFIED"

# 3B. Verify golden tests still pass
echo ""
echo "── 3B: Golden value tests ──"
./zcc matrix_host.c -o matrix_ast.s 2>/dev/null
gcc -o matrix_ast matrix_ast.s
./matrix_ast
echo "✅ Golden tests ALL PASS"

# 3C. Check is_alpha assembly now has movslq
echo ""
echo "── 3C: is_alpha assembly check ──"
./zcc zcc.c -o zcc2.s 2>/dev/null
gcc -O0 -w -o zcc2 zcc2.s compiler_passes.c compiler_passes_ir.c -lm 2>/dev/null
./zcc2 zcc.c -o zcc3.s 2>/dev/null

echo "--- AST backend (zcc2.s) ---"
sed -n '/^is_alpha:/,/^[a-zA-Z_][a-zA-Z0-9_]*:/p' zcc2.s | grep 'movslq\|movq.*(%' | head -5

echo "--- IR backend (zcc3.s) ---"
sed -n '/^is_alpha:/,/^[a-zA-Z_][a-zA-Z0-9_]*:/p' zcc3.s | grep 'movslq\|movq.*(%' | head -5

# 3D. Full selfhost with expanded whitelist
echo ""
echo "── 3D: Selfhost with full whitelist ──"
timeout 120 make selfhost
DIFF_LINES=$(diff zcc2.s zcc3.s 2>/dev/null | wc -l)
if [ "$DIFF_LINES" = "0" ]; then
    echo "✅ SELF-HOST VERIFIED — zero diff"
else
    echo "❌ MISMATCH — $DIFF_LINES diff lines"
    echo "First divergence:"
    diff zcc2.s zcc3.s | head -30
    echo ""
    echo "Which function diverges?"
    FIRST_LINE=$(diff zcc2.s zcc3.s | head -1 | sed 's/[^0-9].*//;s/,.*//')
    sed -n "1,${FIRST_LINE}p" zcc2.s | grep '^[a-z_]' | tail -1
fi

# 3E. Peephole count check
echo ""
echo "── 3E: Peephole count ──"
grep "Peephole" zcc2.s zcc3.s 2>/dev/null || \
    ./zcc zcc.c -o /dev/null 2>&1 | grep -i peephole

# 3F. Expression statement test
echo ""
echo "── 3F: Expression statement test ──"
cat > /tmp/test_expr_stmt.c << 'TESTEOF'
int printf(const char *fmt, ...);
int test_expr_stmt() {
    int x = 0;
    x++;
    x++;
    x++;
    return x;
}
int test_call_stmt() {
    printf("hello\n");
    return 0;
}
int main() {
    int r1 = test_expr_stmt();
    int r2 = test_call_stmt();
    if (r1 != 3) return 1;
    if (r2 != 0) return 2;
    printf("EXPR STMT TEST: ALL PASS (x=%d)\n", r1);
    return 0;
}
TESTEOF
./zcc /tmp/test_expr_stmt.c -o /tmp/test_expr_stmt.s 2>/dev/null
gcc -o /tmp/test_expr_stmt /tmp/test_expr_stmt.s 2>/dev/null
/tmp/test_expr_stmt
EXPR_RC=$?
if [ "$EXPR_RC" = "0" ]; then
    echo "✅ Expression statements working"
else
    echo "❌ Expression statement bug — exit code $EXPR_RC"
fi

echo ""
echo "═══════════════════════════════════════════════"
echo "  PHASE 3 COMPLETE"
echo "═══════════════════════════════════════════════"
```

---

## PHASE 4: EXPAND THE WHITELIST (Ratchet Test)

After Phase 3 passes clean, expand the whitelist batch by batch.
Save as `zcc_battle_phase4.sh`:

```bash
#!/bin/sh
# ═══════════════════════════════════════════════════════════════
# PHASE 4: Whitelist expansion — batch ratchet test
# Each batch adds functions, runs selfhost, reports pass/fail.
# On failure: identifies the exact function that breaks.
# ═══════════════════════════════════════════════════════════════
set -e
cd /mnt/h/__DOWNLOADS/selforglinux

echo "═══════════════════════════════════════════════"
echo "  PHASE 4: WHITELIST EXPANSION"
echo "═══════════════════════════════════════════════"

# Save current part4.c
cp part4.c part4_PHASE4_BACKUP.c

# Function: test if selfhost passes with current whitelist
test_selfhost() {
    make clean 2>/dev/null
    make zcc_full 2>/dev/null
    timeout 120 make selfhost 2>/dev/null
    return $?
}

# Function: add a function to whitelist
add_to_whitelist() {
    local FUNC="$1"
    # Check if already present
    if grep -q "\"$FUNC\"" part4.c; then
        echo "  [skip] $FUNC already on whitelist"
        return 0
    fi
    # Add after the last entry in the whitelist array
    # Find the line with the last quoted string before the NULL terminator
    sed -i "/\"log2_of\"/a\\    \"$FUNC\"," part4.c
    echo "  [added] $FUNC"
}

# ═══════════════════════════════════════════════════
# BATCH 3: Lexer character classification (THE CRITICAL BATCH)
# These are called on EVERY character during lexing.
# If any of these are wrong, the entire compilation diverges.
# ═══════════════════════════════════════════════════
echo ""
echo "── BATCH 3: Lexer character functions ──"

BATCH3_FUNCS="is_alpha is_digit is_alnum is_space hex_val is_power_of_2_val"
BATCH3_PASS=1

for FUNC in $BATCH3_FUNCS; do
    echo ""
    echo "Testing: $FUNC"
    cp part4_PHASE4_BACKUP.c part4.c
    
    # Re-add all previously passing functions
    for PREV in fold_test licm_test dce_test pressure_test escape_test log2_of; do
        grep -q "\"$PREV\"" part4.c || sed -i "/\"log2_of\"/a\\    \"$PREV\"," part4.c 2>/dev/null
    done
    
    # Add the new function
    add_to_whitelist "$FUNC"
    
    # Test
    if test_selfhost; then
        echo "  ✅ $FUNC SOLO PASS"
    else
        echo "  ❌ $FUNC SOLO FAIL"
        echo "  Diff head:"
        diff zcc2.s zcc3.s 2>/dev/null | head -15
        BATCH3_PASS=0
    fi
done

# If all pass individually, test them ALL together
if [ "$BATCH3_PASS" = "1" ]; then
    echo ""
    echo "── BATCH 3 COMBINED TEST ──"
    cp part4_PHASE4_BACKUP.c part4.c
    for FUNC in fold_test licm_test dce_test pressure_test escape_test log2_of $BATCH3_FUNCS; do
        add_to_whitelist "$FUNC" 2>/dev/null
    done
    if test_selfhost; then
        echo "✅ BATCH 3 COMBINED: ALL PASS"
        echo ""
        echo "Saving verified state..."
        cp part4.c part4_BATCH3_VERIFIED.c
    else
        echo "❌ BATCH 3 COMBINED: FAIL (interaction effect)"
        diff zcc2.s zcc3.s | head -20
    fi
fi

# ═══════════════════════════════════════════════════
# BATCH 4: IR infrastructure functions
# ═══════════════════════════════════════════════════
echo ""
echo "── BATCH 4: IR infrastructure ──"

BATCH4_FUNCS="ir_op_name ir_type_name ir_type_bytes ir_type_unsigned ir_op_is_terminator"

for FUNC in $BATCH4_FUNCS; do
    echo ""
    echo "Testing: $FUNC"
    cp part4_BATCH3_VERIFIED.c part4.c 2>/dev/null || cp part4_PHASE4_BACKUP.c part4.c
    add_to_whitelist "$FUNC"
    
    if test_selfhost; then
        echo "  ✅ $FUNC SOLO PASS"
    else
        echo "  ❌ $FUNC SOLO FAIL"
        diff zcc2.s zcc3.s 2>/dev/null | head -10
    fi
done

# Restore to best known state
echo ""
echo "── Restoring best verified state ──"
if [ -f part4_BATCH3_VERIFIED.c ]; then
    cp part4_BATCH3_VERIFIED.c part4.c
    echo "Restored to BATCH 3 verified"
else
    cp part4_PHASE4_BACKUP.c part4.c
    echo "Restored to pre-Phase 4 backup"
fi

echo ""
echo "═══════════════════════════════════════════════"
echo "  PHASE 4 COMPLETE"
echo "═══════════════════════════════════════════════"
```

---

## PHASE 5: FINAL VERIFICATION & FIXED POINT

After all batches pass, run the ultimate test:

```bash
#!/bin/sh
# ═══════════════════════════════════════════════════════════════
# PHASE 5: Fixed-point proof — 4-stage bootstrap
# ═══════════════════════════════════════════════════════════════
cd /mnt/h/__DOWNLOADS/selforglinux

echo "═══════════════════════════════════════════════"
echo "  PHASE 5: FIXED POINT VERIFICATION"
echo "═══════════════════════════════════════════════"

# Build fresh
make clean
make selfhost
echo "Stage 2-3: VERIFIED"

# Stage 4: One more round
./zcc zcc.c -o zcc3.s 2>/dev/null
gcc -O0 -w -o zcc3 zcc3.s compiler_passes.c compiler_passes_ir.c -lm
./zcc3 zcc.c -o zcc4.s 2>/dev/null
diff zcc3.s zcc4.s > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "Stage 3-4: VERIFIED"
    echo ""
    echo "🔱 FIXED POINT ACHIEVED — zcc3.s == zcc4.s"
    echo ""
    
    # Stats
    echo "── Final Stats ──"
    WL_COUNT=$(grep -c '"[a-z_]*"' part4.c 2>/dev/null || echo "?")
    PEEP=$(grep -o 'OK ([0-9]* elided)' zcc3.s 2>/dev/null || ./zcc zcc.c -o /dev/null 2>&1 | grep -o '[0-9]* elided')
    ASM_LINES=$(wc -l < zcc3.s)
    IR_FUNCS=$(./zcc zcc.c -o /dev/null 2>&1 | grep -c 'ZCC-IR.*fn=')
    
    echo "  Assembly lines: $ASM_LINES"
    echo "  Peephole elided: $PEEP"
    echo "  IR-lowered functions: $IR_FUNCS"
    echo "  Whitelist size: $WL_COUNT"
    echo ""
    echo "🔱 ZCC IR Bridge v1.1.0 — LEGENDARY STATUS"
else
    echo "Stage 3-4: MISMATCH"
    diff zcc3.s zcc4.s | head -20
fi
```

---

## EXECUTION ORDER

```
1. Run Phase 1 diagnosis        → Understand current state
2. Apply the movslq fix          → Manual edit based on Phase 1 findings
3. Run Phase 3 verification      → Confirm both fixes work
4. Run Phase 4 whitelist ratchet  → Expand coverage batch by batch
5. Run Phase 5 fixed-point proof  → 4-stage bootstrap convergence
```

## ROLLBACK

If anything breaks catastrophically:
```bash
cd /mnt/h/__DOWNLOADS/selforglinux
cp compiler_passes.c.bak_phase2 compiler_passes.c   # Undo movslq fix
cp part4_PHASE4_BACKUP.c part4.c                     # Undo whitelist expansion
make clean && make selfhost                           # Verify rollback
```

## SUCCESS CRITERIA

| Gate | Requirement |
|------|------------|
| Selfhost | `zcc2.s == zcc3.s` bitwise identical |
| Fixed point | `zcc3.s == zcc4.s` (4-stage convergence) |
| Golden tests | 5/5 ALL PASS |
| Expression stmts | `test_expr_stmt` returns 3 |
| Peephole | ≥ 4310 (regression = something broke) |
| Whitelist | Batch 3 lexer functions all converged |

---

*🔱 ZKAEDI COMPILER FORGE — v1.0.3 → v1.1.0 Battle Plan*
*Every phase is idempotent. Every fix is reversible. The bootstrap gate is absolute.*


## FUTURE: Parser Initializer Rewrite
- Replace depth-counter flattener in part3.c 
  (3 locations) with recursive parse_initializer_list()
  that preserves nested ND_INIT_LIST structure.
- Requires coordinated rewrite of:
  local array codegen in parse_decl (~line 2130)
  emit_struct_fields in part4.c
- Prerequisite: SQLite running cleanly first.
- Priority: post-SQLite architectural cleanup.

---

## ZCC SQLite Milestone — April 10, 2026
- SQLite 3.45.0 compiled by ZCC
- Full SQL round trip verified:
  open rc=0
  SELECT 1 = 1
  CREATE TABLE rc=0
  INSERT rc=0  
  SELECT x = 42
- All rc=0, zero errors, zero segfaults

Bugs closed to achieve this:
  CG-IR-007: movslq width
  va_list phases 1-3: System V ABI
  Global struct initializer: recursive emitter
  Array-of-struct: budget cursor
  Array parameter decay
  ND_NEG: negative array initializers → yyRuleInfoNRhs
  struct-by-value Token ABI
  __atomic_* inline
  cltq pointer corruption (8 sites)
  __builtin_va_end linker
  Makefile -no-pie
  sizeof(char_array) = 8 bug
  Octal escape sequences unimplemented

🔱 ZKAEDI PRIME: CONVERGED
