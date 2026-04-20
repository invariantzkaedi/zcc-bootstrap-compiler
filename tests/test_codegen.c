/* ============================================================================
 * 🔱 ZCC Codegen Regression Test Suite v1.0
 * ============================================================================
 * 100+ tests targeting every known codegen bug class (CG-001 through CG-010)
 * plus general correctness tests for arithmetic, control flow, pointers,
 * structs, strings, recursion, and varargs.
 *
 * Output format:  TEST_NAME|RESULT
 * The harness compares GCC vs ZCC output line-by-line.
 *
 * Build:  gcc -O0 -std=c99 -o test_ref test_codegen.c -lm
 * Run:    ./test_ref
 * ============================================================================
 */

/* Forward declarations — ZCC-compatible (no system headers needed) */
int printf(const char *fmt, ...);
void *malloc(unsigned long size);
void free(void *ptr);
void *memset(void *s, int c, unsigned long n);
void *memcpy(void *dest, const void *src, unsigned long n);
int strcmp(const char *a, const char *b);
unsigned long strlen(const char *s);

/* ── Helpers ── */
static int g_pass = 0;
static int g_fail = 0;

static void check(const char *name, int condition, long got, long expected) {
    if (condition) {
        printf("%s|PASS got=%ld\n", name, got);
        g_pass++;
    } else {
        printf("%s|FAIL got=%ld expected=%ld\n", name, got, expected);
        g_fail++;
    }
}

#define CHECK(name, expr, expected) \
    do { long _v = (long)(expr); check(name, _v == (long)(expected), _v, (long)(expected)); } while(0)

#define CHECK_TRUE(name, expr) CHECK(name, expr, 1)

/* ============================================================================
 * CG-001: Sign Extension vs Zero Extension
 *
 * Bug: unsigned 32-bit values sign-extended to 64-bit via cdqe/movsxd.
 * Values >= 0x80000000 get 0xFFFFFFFF in upper bits, corrupting pointers.
 * ============================================================================ */

static void test_cg001_sign_extension(void) {
    /* u32 → u64 zero extension */
    unsigned int u32_val = 0x80000000u;
    unsigned long u64_val = (unsigned long)u32_val;
    CHECK("CG001_u32_to_u64_high_bit", u64_val, 0x80000000UL);
    
    unsigned int u32_max = 0xFFFFFFFFu;
    unsigned long u64_max = (unsigned long)u32_max;
    CHECK("CG001_u32_max_to_u64", u64_max, 0xFFFFFFFFUL);
    
    /* Signed widening should sign-extend */
    int s32_neg = -1;
    long s64_neg = (long)s32_neg;
    CHECK("CG001_s32_neg1_to_s64", s64_neg, -1L);
    
    int s32_min = (int)0x80000000;
    long s64_min = (long)s32_min;
    CHECK("CG001_s32_min_to_s64", s64_min < 0, 1);
    
    /* u8 → u32 zero extension */
    unsigned char u8_val = 0xFF;
    unsigned int u32_from_u8 = (unsigned int)u8_val;
    CHECK("CG001_u8_ff_to_u32", u32_from_u8, 255);
    
    /* s8 → s32 sign extension */
    signed char s8_val = -128;
    int s32_from_s8 = (int)s8_val;
    CHECK("CG001_s8_neg128_to_s32", s32_from_s8, -128);
    
    /* Pointer-sized operations with high bits */
    unsigned long ptr_like = 0x7FFFFFFFFFFF;
    unsigned int trunc = (unsigned int)ptr_like;
    unsigned long restored = (unsigned long)trunc;
    CHECK("CG001_ptr_trunc_restore", restored, (unsigned long)trunc);
}

/* ============================================================================
 * CG-002: Signed vs Unsigned Division
 *
 * Bug: Using idiv for unsigned operands. cqo sign-extends rdx, corrupting
 * the dividend for large unsigned values.
 * ============================================================================ */

static void test_cg002_division(void) {
    /* Unsigned division — large values */
    unsigned long u_big = 0xFFFFFFFFFFFFFFFFUL;
    unsigned long u_div = u_big / 2;
    CHECK("CG002_unsigned_div_max", u_div, 0x7FFFFFFFFFFFFFFFUL);
    
    unsigned int u32_big = 0xFFFFFFFFu;
    unsigned int u32_div = u32_big / 2;
    CHECK("CG002_u32_div_max", u32_div, 0x7FFFFFFFu);
    
    /* Unsigned modulo */
    unsigned int u_mod = 0xFFFFFFFFu % 10;
    CHECK("CG002_unsigned_mod", u_mod, 5);
    
    /* Signed division — negative values */
    int s_neg = -7;
    int s_div = s_neg / 2;
    CHECK("CG002_signed_div_neg", s_div, -3);
    
    int s_mod = -7 % 2;
    CHECK("CG002_signed_mod_neg", s_mod, -1);
    
    /* Mixed: unsigned numerator, small divisor */
    unsigned long big_num = 1000000000000UL;
    unsigned long small_div = big_num / 7;
    CHECK("CG002_large_unsigned_div", small_div, 142857142857UL);
    
    /* Division by 1 */
    CHECK("CG002_div_by_one", 42 / 1, 42);
    CHECK("CG002_mod_by_one", 42 % 1, 0);
}

/* ============================================================================
 * CG-003: Arithmetic vs Logical Right Shift
 *
 * Bug: Using sar (arithmetic shift) for unsigned right shift.
 * sar propagates sign bit; shr fills with zeros.
 * ============================================================================ */

static void test_cg003_shift(void) {
    /* Unsigned right shift — must zero-fill */
    unsigned int u = 0x80000000u;
    unsigned int u_shr = u >> 1;
    CHECK("CG003_unsigned_shr", u_shr, 0x40000000u);
    
    unsigned long ul = 0x8000000000000000UL;
    unsigned long ul_shr = ul >> 1;
    CHECK("CG003_unsigned_shr64", ul_shr, 0x4000000000000000UL);
    
    /* Signed right shift — must sign-fill */
    int s = (int)0x80000000;
    int s_sar = s >> 1;
    CHECK("CG003_signed_sar", s_sar < 0, 1);
    CHECK("CG003_signed_sar_val", (unsigned int)s_sar, 0xC0000000u);
    
    /* Left shift (both signed and unsigned use shl) */
    unsigned int u_shl = 1u << 31;
    CHECK("CG003_shl_31", u_shl, 0x80000000u);
    
    /* Multi-bit shifts */
    unsigned int u_shr8 = 0xFF000000u >> 8;
    CHECK("CG003_shr_8", u_shr8, 0x00FF0000u);
    
    /* Shift by 0 */
    CHECK("CG003_shr_zero", 42u >> 0, 42u);
    CHECK("CG003_shl_zero", 42 << 0, 42);
}

/* ============================================================================
 * CG-005: Signed vs Unsigned Comparison Branch
 *
 * Bug: Using signed jumps (jg/jl) for unsigned comparisons.
 * 0xFFFFFFFF > 0 unsigned, but -1 < 0 signed.
 * ============================================================================ */

static void test_cg005_comparison(void) {
    unsigned int a = 0xFFFFFFFFu;
    unsigned int b = 0;
    
    CHECK("CG005_unsigned_gt", a > b, 1);
    CHECK("CG005_unsigned_lt", b < a, 1);
    CHECK("CG005_unsigned_ge", a >= b, 1);
    
    /* Signed comparison — same bit pattern */
    int sa = (int)0xFFFFFFFFu;  /* = -1 */
    int sb = 0;
    CHECK("CG005_signed_lt", sa < sb, 1);
    CHECK("CG005_signed_gt", sb > sa, 1);
    
    /* Pointer-like comparisons */
    unsigned long p1 = 0x7FFFFFFFFFFF;
    unsigned long p2 = 0x800000000000;
    CHECK("CG005_ptr_compare", p2 > p1, 1);
    
    /* Edge: equal values */
    CHECK("CG005_equal_unsigned", (0u == 0u), 1);
    CHECK("CG005_equal_ne", (1u != 2u), 1);
    
    /* Loop with unsigned counter (classic bug: i >= 0 is always true for unsigned) */
    unsigned int count = 0;
    unsigned int i;
    for (i = 5; i > 0; i--) {
        count++;
    }
    CHECK("CG005_unsigned_loop_count", count, 5);
}

/* ============================================================================
 * CG-006: Missing Stack Alignment (16-byte)
 *
 * Bug: Stack not 16-byte aligned before call instruction.
 * ============================================================================ */

static int align_callee(int a, int b, int c, int d, int e, int f, int g, int h) {
    /* 8 args — forces stack usage on SysV ABI (args 7,8 on stack) */
    return a + b + c + d + e + f + g + h;
}

static void test_cg006_alignment(void) {
    int result = align_callee(1, 2, 3, 4, 5, 6, 7, 8);
    CHECK("CG006_8arg_call", result, 36);
    
    /* Nested calls */
    int r2 = align_callee(
        align_callee(1,1,1,1,1,1,1,1),
        2, 3, 4, 5, 6, 7, 8
    );
    CHECK("CG006_nested_8arg", r2, 43);
}

/* ============================================================================
 * CG-007: Struct Layout / Padding
 *
 * Bug: Wrong struct member offsets due to missing alignment padding.
 * ============================================================================ */

struct Packed1 {
    char  a;     /* offset 0 */
    int   b;     /* offset 4 (padded) */
    char  c;     /* offset 8 */
    long  d;     /* offset 16 (padded) */
};

struct Nested {
    int x;
    struct Packed1 inner;
    int y;
};

static void test_cg007_struct_layout(void) {
    struct Packed1 p;
    p.a = 'A';
    p.b = 0x12345678;
    p.c = 'C';
    p.d = 0x0102030405060708L;
    
    CHECK("CG007_struct_a", p.a, 'A');
    CHECK("CG007_struct_b", p.b, 0x12345678);
    CHECK("CG007_struct_c", p.c, 'C');
    CHECK("CG007_struct_d_low", (int)(p.d & 0xFFFF), 0x0708);
    
    /* Struct size with padding */
    CHECK("CG007_sizeof_packed1", (long)sizeof(struct Packed1), 24);
    
    /* Nested struct */
    struct Nested n;
    n.x = 100;
    n.inner.b = 200;
    n.y = 300;
    CHECK("CG007_nested_x", n.x, 100);
    CHECK("CG007_nested_inner_b", n.inner.b, 200);
    CHECK("CG007_nested_y", n.y, 300);
    
    /* Pointer to struct member */
    int *bp = &p.b;
    CHECK("CG007_member_ptr", *bp, 0x12345678);
}

/* ============================================================================
 * CG-010: Global vs Local Label Collision
 *
 * Bug: Local branch labels collide across functions.
 * We test by having multiple functions with identical control flow.
 * ============================================================================ */

static int label_func_a(int x) {
    if (x > 0) return 1;
    else return -1;
}

static int label_func_b(int x) {
    if (x > 0) return 1;
    else return -1;
}

static int label_func_c(int x) {
    int r = 0;
    if (x > 10) r = 3;
    else if (x > 5) r = 2;
    else if (x > 0) r = 1;
    return r;
}

static void test_cg010_labels(void) {
    CHECK("CG010_func_a_pos", label_func_a(5), 1);
    CHECK("CG010_func_a_neg", label_func_a(-5), -1);
    CHECK("CG010_func_b_pos", label_func_b(5), 1);
    CHECK("CG010_func_b_neg", label_func_b(-5), -1);
    CHECK("CG010_func_c_high", label_func_c(15), 3);
    CHECK("CG010_func_c_mid", label_func_c(7), 2);
    CHECK("CG010_func_c_low", label_func_c(3), 1);
    CHECK("CG010_func_c_zero", label_func_c(0), 0);
}

/* ============================================================================
 * General: Arithmetic Correctness
 * ============================================================================ */

static void test_arithmetic(void) {
    /* Basic operations */
    CHECK("ARITH_add", 3 + 4, 7);
    CHECK("ARITH_sub", 10 - 7, 3);
    CHECK("ARITH_mul", 6 * 7, 42);
    CHECK("ARITH_div", 100 / 3, 33);
    CHECK("ARITH_mod", 100 % 3, 1);
    
    /* Negative arithmetic */
    CHECK("ARITH_neg_add", -5 + 3, -2);
    CHECK("ARITH_neg_mul", -3 * -4, 12);
    CHECK("ARITH_neg_div", -10 / 3, -3);
    
    /* Overflow edge cases */
    int imax = 2147483647;
    CHECK("ARITH_int_max", imax, 2147483647);
    CHECK("ARITH_int_max_plus1", (unsigned int)imax + 1u, 2147483648u);
    
    /* 64-bit arithmetic */
    long l1 = 1000000000L * 1000000000L;
    CHECK("ARITH_64bit_mul", l1, 1000000000000000000L);
    
    /* Bitwise */
    CHECK("ARITH_and", 0xFF & 0x0F, 0x0F);
    CHECK("ARITH_or", 0xF0 | 0x0F, 0xFF);
    CHECK("ARITH_xor", 0xFF ^ 0x0F, 0xF0);
    CHECK("ARITH_not", ~0, -1);
    
    /* Unary minus */
    int neg = -42;
    CHECK("ARITH_unary_neg", -neg, 42);
    
    /* Increment / compound */
    int x = 10;
    x += 5;
    CHECK("ARITH_compound_add", x, 15);
    x -= 3;
    CHECK("ARITH_compound_sub", x, 12);
    x *= 2;
    CHECK("ARITH_compound_mul", x, 24);
    x /= 6;
    CHECK("ARITH_compound_div", x, 4);
}

/* ============================================================================
 * General: Control Flow
 * ============================================================================ */

static void test_control_flow(void) {
    /* If-else chains */
    int x = 5;
    int r;
    if (x == 5) r = 1;
    else r = 0;
    CHECK("CTRL_if_eq", r, 1);
    
    if (x != 5) r = 0;
    else r = 1;
    CHECK("CTRL_if_ne", r, 1);
    
    /* While loop */
    int sum = 0;
    int i = 0;
    while (i < 10) {
        sum += i;
        i++;
    }
    CHECK("CTRL_while_sum", sum, 45);
    
    /* For loop */
    sum = 0;
    for (i = 1; i <= 100; i++) {
        sum += i;
    }
    CHECK("CTRL_for_gauss", sum, 5050);
    
    /* Do-while */
    i = 0;
    sum = 0;
    do {
        sum += i;
        i++;
    } while (i < 5);
    CHECK("CTRL_do_while", sum, 10);
    
    /* Nested loops */
    int count = 0;
    int j;
    for (i = 0; i < 10; i++) {
        for (j = 0; j < 10; j++) {
            count++;
        }
    }
    CHECK("CTRL_nested_loop", count, 100);
    
    /* Break */
    sum = 0;
    for (i = 0; i < 100; i++) {
        if (i == 10) break;
        sum += i;
    }
    CHECK("CTRL_break", sum, 45);
    
    /* Continue */
    sum = 0;
    for (i = 0; i < 10; i++) {
        if (i % 2 == 0) continue;
        sum += i;
    }
    CHECK("CTRL_continue_odd", sum, 25);
    
    /* Switch */
    x = 3;
    switch (x) {
        case 1: r = 10; break;
        case 2: r = 20; break;
        case 3: r = 30; break;
        default: r = 0; break;
    }
    CHECK("CTRL_switch", r, 30);
    
    /* Switch fallthrough */
    sum = 0;
    x = 2;
    switch (x) {
        case 1: sum += 1;
        case 2: sum += 2;
        case 3: sum += 3;
        default: sum += 10;
    }
    CHECK("CTRL_switch_fallthrough", sum, 15);
    
    /* Ternary */
    CHECK("CTRL_ternary_t", (5 > 3 ? 1 : 0), 1);
    CHECK("CTRL_ternary_f", (5 < 3 ? 1 : 0), 0);
    
    /* Logical operators short-circuit */
    int side_effect = 0;
    if (0 && (side_effect = 1)) {}
    CHECK("CTRL_short_circuit_and", side_effect, 0);
    
    side_effect = 0;
    if (1 || (side_effect = 1)) {}
    CHECK("CTRL_short_circuit_or", side_effect, 0);
}

/* ============================================================================
 * General: Pointers and Arrays
 * ============================================================================ */

static void test_pointers(void) {
    /* Basic pointer operations */
    int val = 42;
    int *p = &val;
    CHECK("PTR_deref", *p, 42);
    
    *p = 99;
    CHECK("PTR_write", val, 99);
    
    /* Array access */
    int arr[10];
    int i;
    for (i = 0; i < 10; i++) arr[i] = i * i;
    CHECK("PTR_array_0", arr[0], 0);
    CHECK("PTR_array_5", arr[5], 25);
    CHECK("PTR_array_9", arr[9], 81);
    
    /* Pointer arithmetic */
    int *q = arr + 3;
    CHECK("PTR_arith_add", *q, 9);
    CHECK("PTR_arith_diff", (long)(q - arr), 3);
    
    q++;
    CHECK("PTR_arith_inc", *q, 16);
    
    /* Array via pointer */
    CHECK("PTR_index", *(arr + 7), 49);
    
    /* Pointer to pointer */
    int **pp = &p;
    CHECK("PTR_ptr_to_ptr", **pp, 99);
    
    /* Null pointer comparison */
    int *null_p = 0;
    CHECK("PTR_null_cmp", null_p == 0, 1);
    CHECK("PTR_nonnull_cmp", p != 0, 1);
    
    /* Char array / string */
    char str[] = "hello";
    CHECK("PTR_str_0", str[0], 'h');
    CHECK("PTR_str_4", str[4], 'o');
    CHECK("PTR_str_5", str[5], 0);
    CHECK("PTR_strlen", (long)strlen(str), 5);
}

/* ============================================================================
 * General: Recursion
 * ============================================================================ */

static int fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

static long factorial(int n) {
    if (n <= 1) return 1;
    return (long)n * factorial(n - 1);
}

static int ackermann(int m, int n) {
    if (m == 0) return n + 1;
    if (n == 0) return ackermann(m - 1, 1);
    return ackermann(m - 1, ackermann(m, n - 1));
}

static void test_recursion(void) {
    CHECK("REC_fib_0", fibonacci(0), 0);
    CHECK("REC_fib_1", fibonacci(1), 1);
    CHECK("REC_fib_10", fibonacci(10), 55);
    CHECK("REC_fib_20", fibonacci(20), 6765);
    
    CHECK("REC_fact_1", factorial(1), 1);
    CHECK("REC_fact_5", factorial(5), 120);
    CHECK("REC_fact_12", factorial(12), 479001600L);
    CHECK("REC_fact_20", factorial(20), 2432902008176640000L);
    
    CHECK("REC_ack_0_0", ackermann(0, 0), 1);
    CHECK("REC_ack_1_1", ackermann(1, 1), 3);
    CHECK("REC_ack_2_2", ackermann(2, 2), 7);
    CHECK("REC_ack_3_3", ackermann(3, 3), 61);
}

/* ============================================================================
 * General: Dynamic Memory
 * ============================================================================ */

static void test_malloc(void) {
    /* Basic allocation */
    int *p = (int *)malloc(sizeof(int) * 10);
    if (!p) {
        printf("MALLOC_basic|FAIL got=NULL\n");
        g_fail++;
        return;
    }
    
    int i;
    for (i = 0; i < 10; i++) p[i] = i * 10;
    CHECK("MALLOC_write_read", p[5], 50);
    CHECK("MALLOC_write_read_9", p[9], 90);
    
    /* Rewrite */
    p[0] = 999;
    CHECK("MALLOC_rewrite", p[0], 999);
    
    free(p);
    
    /* memset */
    char *buf = (char *)malloc(100);
    memset(buf, 0, 100);
    CHECK("MALLOC_memset", buf[50], 0);
    
    memset(buf, 'A', 50);
    CHECK("MALLOC_memset_A", buf[0], 'A');
    CHECK("MALLOC_memset_boundary", buf[49], 'A');
    CHECK("MALLOC_memset_after", buf[50], 0);
    
    free(buf);
    
    /* memcpy */
    char src[8];
    char dst[8];
    memset(src, 0, 8);
    memset(dst, 0, 8);
    src[0] = 'X';
    src[1] = 'Y';
    src[2] = 'Z';
    memcpy(dst, src, 3);
    CHECK("MALLOC_memcpy", dst[0], 'X');
    CHECK("MALLOC_memcpy_2", dst[2], 'Z');
}

/* ============================================================================
 * General: Function Pointers
 * ============================================================================ */

static int add_fn(int a, int b) { return a + b; }
static int mul_fn(int a, int b) { return a * b; }
static int sub_fn(int a, int b) { return a - b; }

typedef int (*binop_t)(int, int);

static int apply(binop_t fn, int a, int b) {
    return fn(a, b);
}

static void test_function_pointers(void) {
    CHECK("FPTR_add", apply(add_fn, 3, 4), 7);
    CHECK("FPTR_mul", apply(mul_fn, 3, 4), 12);
    CHECK("FPTR_sub", apply(sub_fn, 10, 3), 7);
    
    /* Array of function pointers */
    binop_t ops[3];
    ops[0] = add_fn;
    ops[1] = mul_fn;
    ops[2] = sub_fn;
    
    CHECK("FPTR_array_0", ops[0](5, 3), 8);
    CHECK("FPTR_array_1", ops[1](5, 3), 15);
    CHECK("FPTR_array_2", ops[2](5, 3), 2);
    
    /* Null function pointer check */
    binop_t null_fn = 0;
    CHECK("FPTR_null_cmp", null_fn == 0, 1);
}

/* ============================================================================
 * General: String Operations
 * ============================================================================ */

static void test_strings(void) {
    char *s = "hello world";
    CHECK("STR_index_0", s[0], 'h');
    CHECK("STR_index_5", s[5], ' ');
    CHECK("STR_len", (long)strlen(s), 11);
    
    /* strcmp */
    CHECK("STR_cmp_eq", strcmp("abc", "abc"), 0);
    CHECK("STR_cmp_lt", strcmp("abc", "abd") < 0, 1);
    CHECK("STR_cmp_gt", strcmp("abd", "abc") > 0, 1);
    
    /* String in array */
    char buf[32];
    buf[0] = 'Z';
    buf[1] = 'C';
    buf[2] = 'C';
    buf[3] = 0;
    CHECK("STR_manual_build", strcmp(buf, "ZCC"), 0);
    
    /* Char arithmetic */
    char c = 'A';
    CHECK("STR_char_arith", c + 1, 'B');
    CHECK("STR_char_digit", '9' - '0', 9);
    CHECK("STR_char_case", 'a' - 'A', 32);
}

/* ============================================================================
 * General: Global Variables
 * ============================================================================ */

static int g_counter = 0;
static long g_accumulator = 0;

static void increment_global(void) {
    g_counter++;
    g_accumulator += g_counter;
}

static void test_globals(void) {
    g_counter = 0;
    g_accumulator = 0;
    
    int i;
    for (i = 0; i < 10; i++) increment_global();
    
    CHECK("GLOB_counter", g_counter, 10);
    CHECK("GLOB_accumulator", g_accumulator, 55);
}

/* ============================================================================
 * General: Casting and Type Promotion
 * ============================================================================ */

static void test_casting(void) {
    /* Integer promotion */
    char a = 100;
    char b = 100;
    int product = a * b;
    CHECK("CAST_char_mul", product, 10000);
    
    /* Unsigned preservation through cast chain */
    unsigned char uc = 200;
    unsigned int ui = uc;
    unsigned long ul = ui;
    CHECK("CAST_u8_chain", ul, 200);
    
    /* Truncation */
    int big = 0x12345678;
    char trunc = (char)big;
    CHECK("CAST_trunc_to_char", trunc & 0xFF, 0x78);
    
    /* Void pointer round-trip */
    int val = 42;
    void *vp = &val;
    int *ip = (int *)vp;
    CHECK("CAST_void_roundtrip", *ip, 42);
    
    /* Size_t operations */
    unsigned long sz = sizeof(long);
    CHECK("CAST_sizeof_long", sz, 8);
    CHECK("CAST_sizeof_int", (long)sizeof(int), 4);
    CHECK("CAST_sizeof_char", (long)sizeof(char), 1);
    CHECK("CAST_sizeof_ptr", (long)sizeof(void*), 8);
}

/* ============================================================================
 * Stress: Linked List (combines malloc, structs, pointers, recursion)
 * ============================================================================ */

struct Node {
    int value;
    struct Node *next;
};

static struct Node *make_list(int n) {
    struct Node *head = 0;
    int i;
    for (i = n - 1; i >= 0; i--) {
        struct Node *node = (struct Node *)malloc(sizeof(struct Node));
        node->value = i;
        node->next = head;
        head = node;
    }
    return head;
}

static int list_sum(struct Node *head) {
    int sum = 0;
    while (head) {
        sum += head->value;
        head = head->next;
    }
    return sum;
}

static void free_list(struct Node *head) {
    while (head) {
        struct Node *tmp = head;
        head = head->next;
        free(tmp);
    }
}

static void test_linked_list(void) {
    struct Node *list = make_list(10);
    
    CHECK("LIST_head", list->value, 0);
    CHECK("LIST_sum", list_sum(list), 45);
    
    /* Count nodes */
    int count = 0;
    struct Node *p = list;
    while (p) { count++; p = p->next; }
    CHECK("LIST_count", count, 10);
    
    /* Access last node */
    p = list;
    while (p->next) p = p->next;
    CHECK("LIST_tail", p->value, 9);
    
    free_list(list);
}

/* ============================================================================
 * Stress: Binary Search
 * ============================================================================ */

static int binary_search(int *arr, int len, int target) {
    int lo = 0;
    int hi = len - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] == target) return mid;
        else if (arr[mid] < target) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

static void test_binary_search(void) {
    int arr[20];
    int i;
    for (i = 0; i < 20; i++) arr[i] = i * 3;
    
    CHECK("BSEARCH_found_0", binary_search(arr, 20, 0), 0);
    CHECK("BSEARCH_found_15", binary_search(arr, 20, 15), 5);
    CHECK("BSEARCH_found_57", binary_search(arr, 20, 57), 19);
    CHECK("BSEARCH_not_found", binary_search(arr, 20, 4), -1);
}

/* ============================================================================
 * Stress: Sorting (insertion sort — exercises many codegen paths)
 * ============================================================================ */

static void insertion_sort(int *arr, int n) {
    int i, j, key;
    for (i = 1; i < n; i++) {
        key = arr[i];
        j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static void test_sorting(void) {
    int arr[] = {9, 3, 7, 1, 8, 2, 6, 4, 5, 0};
    insertion_sort(arr, 10);
    
    CHECK("SORT_0", arr[0], 0);
    CHECK("SORT_4", arr[4], 4);
    CHECK("SORT_9", arr[9], 9);
    
    /* Verify sorted */
    int sorted = 1;
    int i;
    for (i = 1; i < 10; i++) {
        if (arr[i] < arr[i-1]) sorted = 0;
    }
    CHECK("SORT_ordered", sorted, 1);
    
    /* Already sorted */
    int arr2[] = {1, 2, 3, 4, 5};
    insertion_sort(arr2, 5);
    CHECK("SORT_presorted", arr2[2], 3);
    
    /* Reverse sorted */
    int arr3[] = {5, 4, 3, 2, 1};
    insertion_sort(arr3, 5);
    CHECK("SORT_reverse", arr3[0], 1);
    CHECK("SORT_reverse_4", arr3[4], 5);
}

/* ============================================================================
 * Stress: Tower of Hanoi (deep recursion + multiple args)
 * ============================================================================ */

static int hanoi_count;
static void hanoi(int n, int from, int to, int aux) {
    if (n == 1) {
        hanoi_count++;
        return;
    }
    hanoi(n - 1, from, aux, to);
    hanoi_count++;
    hanoi(n - 1, aux, to, from);
}

static void test_hanoi(void) {
    hanoi_count = 0;
    hanoi(1, 1, 3, 2);
    CHECK("HANOI_1", hanoi_count, 1);
    
    hanoi_count = 0;
    hanoi(3, 1, 3, 2);
    CHECK("HANOI_3", hanoi_count, 7);
    
    hanoi_count = 0;
    hanoi(10, 1, 3, 2);
    CHECK("HANOI_10", hanoi_count, 1023);
    
    hanoi_count = 0;
    hanoi(15, 1, 3, 2);
    CHECK("HANOI_15", hanoi_count, 32767);
}

/* ============================================================================
 * Edge: Comma Operator, Sizeof in Expressions, Cast Chains
 * ============================================================================ */

static void test_edge_cases(void) {
    /* Comma operator */
    int x = (1, 2, 3);
    CHECK("EDGE_comma", x, 3);
    
    /* Sizeof in expression */
    int sz = sizeof(int) + sizeof(long);
    CHECK("EDGE_sizeof_expr", sz, 12);
    
    /* Cast chain */
    long val = 0x0102030405060708L;
    int trunc = (int)val;
    char trunc2 = (char)trunc;
    CHECK("EDGE_cast_chain", trunc2 & 0xFF, 0x08);
    
    /* Pointer difference */
    int arr[10];
    long diff = &arr[7] - &arr[2];
    CHECK("EDGE_ptr_diff", diff, 5);
    
    /* Conditional as lvalue operand */
    int a = 1, b = 2;
    *(a > b ? &a : &b) = 42;
    CHECK("EDGE_cond_lvalue", b, 42);
    
    /* Nested ternary */
    int v = 5;
    int r = (v > 10 ? 3 : (v > 3 ? 2 : 1));
    CHECK("EDGE_nested_ternary", r, 2);
    
    /* Assignment in condition */
    int c = 0;
    if ((c = 42)) {
        CHECK("EDGE_assign_cond", c, 42);
    }
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(void) {
    test_cg001_sign_extension();
    test_cg002_division();
    test_cg003_shift();
    test_cg005_comparison();
    test_cg006_alignment();
    test_cg007_struct_layout();
    test_cg010_labels();
    test_arithmetic();
    test_control_flow();
    test_pointers();
    test_recursion();
    test_malloc();
    test_function_pointers();
    test_strings();
    test_globals();
    test_casting();
    test_linked_list();
    test_binary_search();
    test_sorting();
    test_hanoi();
    test_edge_cases();
    
    printf("SUMMARY|%d pass %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
