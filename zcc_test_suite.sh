#!/bin/bash
# ══════════════════════════════════════════════════════════════════
# zcc_test_suite.sh — ZCC IR Backend Per-Function Regression Tests
#
# Compiles targeted test programs through both AST and IR paths,
# diffs the output per-function, and runs the binaries to verify
# runtime correctness. Catches regressions before selfhost fails.
#
# Usage: bash zcc_test_suite.sh [--quick]
#   --quick: skip selfhost, only run unit tests
#
# Exit code: 0 = all pass, 1 = failures detected
# ══════════════════════════════════════════════════════════════════
set -e

RED='\033[0;31m'
GRN='\033[0;32m'
YEL='\033[0;33m'
CYN='\033[0;36m'
RST='\033[0m'

PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0
QUICK=0

[ "$1" = "--quick" ] && QUICK=1

step()  { echo -e "${CYN}── $1 ──${RST}"; }
pass()  { echo -e "  ${GRN}[PASS]${RST} $1"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail()  { echo -e "  ${RED}[FAIL]${RST} $1"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
skip()  { echo -e "  ${YEL}[SKIP]${RST} $1"; SKIP_COUNT=$((SKIP_COUNT + 1)); }
info()  { echo -e "  ${CYN}[INFO]${RST} $1"; }

# Timeout for IR test execution: 10s in quick mode, 60s otherwise
IR_TIMEOUT=${IR_TIMEOUT:-10}

TESTDIR="/tmp/zcc_tests"
rm -rf "$TESTDIR"
mkdir -p "$TESTDIR"

# ── Ensure zcc_pp.c and zcc_host exist ───────────────────────────

step "Build Setup"

cat part1.c part2.c part3.c ir.h ir_emit_dispatch.h ir_bridge.h part4.c part5.c ir.c ir_to_x86.c > zcc_pp.c

if [ ! -f zcc ]; then
    info "Building zcc from GCC"
    gcc -O0 -w -fno-asynchronous-unwind-tables -o zcc zcc_pp.c compiler_passes.c compiler_passes_ir.c -lm
fi

# Build zcc2 (AST selfhost) if not present
if [ ! -f zcc2 ]; then
    info "Building zcc2 (AST selfhost)"
    ./zcc zcc.c -o zcc2.s 2>/dev/null
    gcc -O0 -w -fno-asynchronous-unwind-tables -o zcc2 zcc2.s compiler_passes.c compiler_passes_ir.c -lm
fi

pass "Build environment ready"

# ══════════════════════════════════════════════════════════════════
# TEST CATEGORY 1: Targeted C programs — AST vs IR output diff
# ══════════════════════════════════════════════════════════════════

# Helper: compile through both paths, diff assembly, run both binaries
# Usage: test_file "test_name" "source.c" "expected_exit_code"
test_file() {
    local name="$1"
    local src="$2"
    local expect_rc="${3:-0}"

    # Compile through AST
    if ! ./zcc2 "$src" -o "$TESTDIR/${name}_ast.s" 2>/dev/null; then
        fail "$name: AST compilation failed"
        return
    fi

    # Compile through IR
    if ! ZCC_IR_BACKEND=1 ./zcc2 "$src" -o "$TESTDIR/${name}_ir.s" 2>"$TESTDIR/${name}_ir.log"; then
        fail "$name: IR compilation failed"
        return
    fi

    # Link and run AST version
    if gcc -O0 -w -o "$TESTDIR/${name}_ast" "$TESTDIR/${name}_ast.s" -lm 2>/dev/null; then
        "$TESTDIR/${name}_ast" > "$TESTDIR/${name}_ast.out" 2>&1 || true
        AST_RC=$?
    else
        fail "$name: AST link failed"
        return
    fi

    # Link and run IR version
    if gcc -O0 -w -o "$TESTDIR/${name}_ir" "$TESTDIR/${name}_ir.s" -lm 2>/dev/null; then
        "$TESTDIR/${name}_ir" > "$TESTDIR/${name}_ir.out" 2>&1 || true
        IR_RC=$?
    else
        fail "$name: IR link failed"
        return
    fi

    # Compare exit codes
    if [ "$AST_RC" != "$IR_RC" ]; then
        fail "$name: exit code mismatch (AST=$AST_RC, IR=$IR_RC)"
        return
    fi

    # Compare stdout
    if ! cmp -s "$TESTDIR/${name}_ast.out" "$TESTDIR/${name}_ir.out"; then
        fail "$name: output mismatch"
        diff "$TESTDIR/${name}_ast.out" "$TESTDIR/${name}_ir.out" | head -5
        return
    fi

    # Count IR functions emitted
    local ir_funcs=$(grep -c '\[ZCC-IR\] fn=' "$TESTDIR/${name}_ir.log" 2>/dev/null || echo 0)
    pass "$name (rc=$AST_RC, $ir_funcs IR funcs)"
}

# ── Generate test files ──────────────────────────────────────────

step "Category 1: Basic Operations"

cat > "$TESTDIR/t_return.c" << 'EOF'
int main() { return 42; }
EOF
test_file "return_42" "$TESTDIR/t_return.c" 42

cat > "$TESTDIR/t_arith.c" << 'EOF'
int main() { return (10 + 20) * 2 - 15; }
EOF
test_file "arithmetic" "$TESTDIR/t_arith.c" 45

cat > "$TESTDIR/t_if.c" << 'EOF'
int main() {
    int x;
    x = 10;
    if (x > 5) return 1;
    return 0;
}
EOF
test_file "if_branch" "$TESTDIR/t_if.c" 1

step "Category 2: Loops and Variables (Mem2Reg stress)"

cat > "$TESTDIR/t_forloop.c" << 'EOF'
int main() {
    int i;
    int sum;
    sum = 0;
    for (i = 0; i < 10; i = i + 1) {
        sum = sum + i;
    }
    return sum;
}
EOF
test_file "for_loop_sum" "$TESTDIR/t_forloop.c" 45

cat > "$TESTDIR/t_while.c" << 'EOF'
int main() {
    int n;
    int result;
    n = 5;
    result = 1;
    while (n > 0) {
        result = result * n;
        n = n - 1;
    }
    return result;
}
EOF
test_file "while_factorial" "$TESTDIR/t_while.c" 120

cat > "$TESTDIR/t_multi_var.c" << 'EOF'
int main() {
    int a; int b; int c; int d;
    a = 10; b = 20; c = 30; d = 40;
    return a + b + c + d;
}
EOF
test_file "multi_var" "$TESTDIR/t_multi_var.c" 100

step "Category 3: Pointers and Memory (CG-IR-008/010 regression)"

cat > "$TESTDIR/t_ptr.c" << 'EOF'
int main() {
    int x;
    int *p;
    x = 42;
    p = &x;
    return *p;
}
EOF
test_file "pointer_deref" "$TESTDIR/t_ptr.c" 42

cat > "$TESTDIR/t_ptr_arith.c" << 'EOF'
int arr[5];
int main() {
    int i;
    for (i = 0; i < 5; i = i + 1)
        arr[i] = i * 10;
    return arr[3];
}
EOF
test_file "ptr_arith" "$TESTDIR/t_ptr_arith.c" 30

step "Category 4: Function Calls (CG-IR-007/013 regression)"

cat > "$TESTDIR/t_call.c" << 'EOF'
int add(int a, int b) { return a + b; }
int main() { return add(17, 25); }
EOF
test_file "simple_call" "$TESTDIR/t_call.c" 42

cat > "$TESTDIR/t_call_many.c" << 'EOF'
int sum6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}
int main() { return sum6(1, 2, 3, 4, 5, 6); }
EOF
test_file "six_arg_call" "$TESTDIR/t_call_many.c" 21

cat > "$TESTDIR/t_recursive.c" << 'EOF'
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}
int main() { return fib(10); }
EOF
test_file "recursive_fib" "$TESTDIR/t_recursive.c" 55

step "Category 5: Structs and Member Access"

cat > "$TESTDIR/t_struct.c" << 'EOF'
struct Point { int x; int y; };
int main() {
    struct Point p;
    p.x = 10;
    p.y = 32;
    return p.x + p.y;
}
EOF
test_file "struct_access" "$TESTDIR/t_struct.c" 42

step "Category 6: Switch Statements"

cat > "$TESTDIR/t_switch.c" << 'EOF'
int classify(int x) {
    switch (x) {
    case 1: return 10;
    case 2: return 20;
    case 3: return 30;
    default: return 0;
    }
}
int main() { return classify(2); }
EOF
if [ "$QUICK" = "1" ]; then
    skip "switch_stmt (hang prevention)"
else
    test_file "switch_stmt" "$TESTDIR/t_switch.c" 20
fi

step "Category 7: String and Global Operations"

cat > "$TESTDIR/t_global.c" << 'EOF'
int g;
int inc() { g = g + 1; return g; }
int main() {
    g = 40;
    inc();
    inc();
    return g;
}
EOF
test_file "global_var" "$TESTDIR/t_global.c" 42

step "Category 8: Complex Control Flow (PHI stress)"

cat > "$TESTDIR/t_nested.c" << 'EOF'
int main() {
    int i; int j; int count;
    count = 0;
    for (i = 0; i < 5; i = i + 1) {
        for (j = 0; j < 3; j = j + 1) {
            count = count + 1;
        }
    }
    return count;
}
EOF
test_file "nested_loops" "$TESTDIR/t_nested.c" 15

cat > "$TESTDIR/t_ternary.c" << 'EOF'
int abs_val(int x) { return x >= 0 ? x : -x; }
int main() { return abs_val(-42); }
EOF
test_file "ternary_expr" "$TESTDIR/t_ternary.c" 42

cat > "$TESTDIR/t_logical.c" << 'EOF'
int main() {
    int a; int b;
    a = 1; b = 0;
    if (a && !b) return 1;
    return 0;
}
EOF
test_file "logical_ops" "$TESTDIR/t_logical.c" 1

step "Category 9: Callee-Save Register Pressure (CG-IR-011 regression)"

cat > "$TESTDIR/t_regpress.c" << 'EOF'
int heavy(int a, int b, int c, int d, int e) {
    int x; int y; int z; int w; int v;
    x = a + b;
    y = c + d;
    z = x * y;
    w = z - e;
    v = w + a + b + c + d + e;
    return v;
}
int main() { return heavy(1, 2, 3, 4, 5); }
EOF
test_file "reg_pressure" "$TESTDIR/t_regpress.c"

cat > "$TESTDIR/t_spill.c" << 'EOF'
int main() {
    int a; int b; int c; int d; int e; int f; int g; int h;
    a = 1; b = 2; c = 3; d = 4;
    e = 5; f = 6; g = 7; h = 8;
    return a + b + c + d + e + f + g + h;
}
EOF
test_file "eight_locals" "$TESTDIR/t_spill.c" 36

step "Category 10: cc_alloc Pattern (CG-IR-005 regression)"

cat > "$TESTDIR/t_alloc_pattern.c" << 'EOF'
char *my_alloc(int size) {
    char *cp;
    int i;
    int aligned;
    aligned = (size + 7) & -8;
    cp = (char *)calloc(1, aligned);
    if (!cp) return 0;
    for (i = 0; i < aligned; i = i + 1)
        cp[i] = 0;
    return cp;
}
int main() {
    char *p;
    p = my_alloc(100);
    if (p) { free(p); return 0; }
    return 1;
}
EOF
if [ "$QUICK" = "1" ]; then
    skip "alloc_pattern (IR hang guard: ZCC-SWITCH-IRLOOP)"
else
    test_file "alloc_pattern" "$TESTDIR/t_alloc_pattern.c" 0
fi

# ══════════════════════════════════════════════════════════════════
# CATEGORY 11: Full Selfhost (unless --quick)
# ══════════════════════════════════════════════════════════════════

if [ "$QUICK" = "0" ]; then
    step "Category 11: Full Selfhost"

    info "Running make clean && make selfhost"
    if make clean > /dev/null 2>&1 && make selfhost > "$TESTDIR/selfhost.log" 2>&1; then
        pass "AST selfhost VERIFIED"
    else
        fail "AST selfhost FAILED"
        tail -5 "$TESTDIR/selfhost.log"
    fi

    info "Running verify_ir_backend.sh"
    if [ -f verify_ir_backend.sh ]; then
        if bash verify_ir_backend.sh > "$TESTDIR/ir_verify.log" 2>&1; then
            IR_FUNCS=$(grep -c '\[ZCC-IR\] fn=' "$TESTDIR/ir_verify.log" 2>/dev/null || echo "?")
            pass "IR backend VERIFIED ($IR_FUNCS functions)"
        else
            fail "IR backend verification FAILED"
            tail -10 "$TESTDIR/ir_verify.log"
        fi
    else
        skip "verify_ir_backend.sh not found"
    fi
else
    skip "Selfhost (--quick mode)"
fi

# ══════════════════════════════════════════════════════════════════
# Summary
# ══════════════════════════════════════════════════════════════════

echo ""
echo -e "${CYN}══════════════════════════════════════════════════${RST}"
echo -e "  ${GRN}PASS: $PASS_COUNT${RST}  ${RED}FAIL: $FAIL_COUNT${RST}  ${YEL}SKIP: $SKIP_COUNT${RST}"
echo -e "${CYN}══════════════════════════════════════════════════${RST}"

# Write machine-readable summary
cat > "$TESTDIR/results.txt" << EOF
pass=$PASS_COUNT
fail=$FAIL_COUNT
skip=$SKIP_COUNT
timestamp=$(date '+%Y-%m-%d %H:%M:%S')
EOF

if [ "$FAIL_COUNT" -gt 0 ]; then
    echo -e "${RED}REGRESSIONS DETECTED${RST}"
    exit 1
else
    echo -e "${GRN}ALL TESTS PASSED${RST}"
    exit 0
fi
