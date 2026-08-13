/* ============================================================================
 * ZCC VERIFIABLE TEST GAUNTLET — 100% Deterministic & Self-Verifying Suite
 * ============================================================================
 * Computes a running 64-bit FNV-1a state hash across 5 execution phases:
 * Phase 1: 64-bit Integer & Bitwise Invariants
 * Phase 2: IEEE-754 Floating-Point Precision & Conversions
 * Phase 3: Struct Layouts, Alignments, & Bitfield Packing (Zero-Padded)
 * Phase 4: Indirect Callbacks & Hidden sret Return Buffers (Zero-Padded)
 * Phase 5: Verification & Hash Match Assertion
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* FNV-1a 64-bit Hash Constants */
#define FNV_OFFSET 0xcbf29ce484222325ULL
#define FNV_PRIME  0x100000001b3ULL

static unsigned long long g_hash = FNV_OFFSET;
static int g_failures = 0;

static void hash_update(unsigned long long val) {
    int i;
    for (i = 0; i < 8; i++) {
        unsigned char byte = (unsigned char)((val >> (i * 8)) & 0xFF);
        g_hash ^= byte;
        g_hash *= FNV_PRIME;
    }
}

static void hash_update_bytes(const void *ptr, size_t sz) {
    const unsigned char *p = (const unsigned char *)ptr;
    size_t i;
    for (i = 0; i < sz; i++) {
        g_hash ^= p[i];
        g_hash *= FNV_PRIME;
    }
}

static void assert_true(int cond, const char *msg) {
    if (!cond) {
        printf("FAIL: %s\n", msg);
        g_failures++;
    }
}

/* ----------------------------------------------------------------------------
 * Phase 1: 64-bit Integer & Bitwise Invariants
 * ---------------------------------------------------------------------------- */
static void test_phase1_integers(void) {
    unsigned long long a = 0xFEDCBA9876543210ULL;
    unsigned long long b = 0x0123456789ABCDEFULL;
    long long signed_neg = -9223372036854775807LL - 1LL;
    
    unsigned long long xor_val = a ^ b;
    unsigned long long and_val = a & b;
    unsigned long long or_val  = a | b;
    
    unsigned long long shift_l = (a << 17) | (b >> 47);
    unsigned long long shift_r = (a >> 19) ^ (b << 45);

    assert_true(xor_val == 0xFFFFFFFFFFFFFFFFULL, "Phase 1: XOR invert");
    assert_true(and_val == 0x0000000000000000ULL, "Phase 1: AND zero");
    assert_true(or_val  == 0xFFFFFFFFFFFFFFFFULL, "Phase 1: OR full");
    assert_true(signed_neg < 0LL, "Phase 1: Signed 64-bit min bound");

    hash_update(a);
    hash_update(b);
    hash_update(xor_val);
    hash_update(shift_l);
    hash_update(shift_r);
}

/* ----------------------------------------------------------------------------
 * Phase 2: IEEE-754 Floating Point & Conversion Precision
 * ---------------------------------------------------------------------------- */
static void test_phase2_floats(void) {
    double d1 = 3.14159265358979323846;
    double d2 = 2.71828182845904523536;
    float  f1 = (float)d1;
    float  f2 = (float)d2;

    double add = d1 + d2;
    double mul = d1 * d2;
    double div = d1 / d2;
    float  fmul = f1 * f2;

    int cmp_gt = (d1 > d2);
    int cmp_eq = (d1 == 3.14159265358979323846);

    assert_true(cmp_gt == 1, "Phase 2: Float comparison GT");
    assert_true(cmp_eq == 1, "Phase 2: Float equality exact");
    assert_true((double)f1 != d1, "Phase 2: Float cast truncation");

    hash_update(*(unsigned long long *)&add);
    hash_update(*(unsigned long long *)&mul);
    hash_update(*(unsigned long long *)&div);
    hash_update(*(unsigned int *)&fmul);
}

/* ----------------------------------------------------------------------------
 * Phase 3: Struct Layout, Alignments, & Bitfield Packing (Zero-Padded)
 * ---------------------------------------------------------------------------- */
typedef struct {
    char c;
    long l;
    short s;
    double d;
    int arr[3];
} AlignStruct;

typedef struct {
    unsigned int a : 5;
    unsigned int b : 11;
    unsigned int c : 16;
    unsigned long long x : 48;
} BitfieldStruct;

static void test_phase3_structs(void) {
    AlignStruct st;
    BitfieldStruct bf;

    /* Zero out all struct memory including padding bytes for deterministic hashing */
    memset(&st, 0, sizeof(st));
    memset(&bf, 0, sizeof(bf));

    size_t sz_align = sizeof(AlignStruct);
    size_t sz_bf    = sizeof(BitfieldStruct);
    size_t off_l    = offsetof(AlignStruct, l);
    size_t off_d    = offsetof(AlignStruct, d);

    st.c = 'Z';
    st.l = 0x123456789ABCDEF0LL;
    st.s = 0x7FFF;
    st.d = 1.4142135623730951;
    st.arr[0] = 10;
    st.arr[1] = 20;
    st.arr[2] = 30;

    bf.a = 31;
    bf.b = 2047;
    bf.c = 65535;
    bf.x = 0xFFFFFFFFFFFFULL;

    assert_true(off_l == 8, "Phase 3: AlignStruct l offset");
    assert_true(off_d == 24, "Phase 3: AlignStruct d offset");
    assert_true(bf.a == 31, "Phase 3: Bitfield a max");
    assert_true(bf.b == 2047, "Phase 3: Bitfield b max");
    assert_true(bf.c == 65535, "Phase 3: Bitfield c max");

    hash_update((unsigned long long)sz_align);
    hash_update((unsigned long long)sz_bf);
    hash_update_bytes(&st, sizeof(st));
    hash_update_bytes(&bf, sizeof(bf));
}

/* ----------------------------------------------------------------------------
 * Phase 4: Indirect Callbacks & Hidden sret Return Buffers (Zero-Padded)
 * ---------------------------------------------------------------------------- */
typedef struct {
    long x;
    long y;
} Vector2;

typedef struct {
    Vector2 pos;
    double speed;
    int state;
} Payload;

typedef Payload (*CallbackFn)(Payload input, long delta, double factor);

static Payload transform_payload(Payload p, long delta, double factor) {
    Payload r;
    memset(&r, 0, sizeof(r));
    r = p;
    r.pos.x += delta;
    r.pos.y -= delta * 2;
    r.speed *= factor;
    r.state ^= (int)delta;
    return r;
}

static Payload execute_callback(CallbackFn fn, Payload p, long delta, double factor) {
    return fn(p, delta, factor);
}

static void test_phase4_callbacks(void) {
    Payload in;
    memset(&in, 0, sizeof(in));
    in.pos.x = 500;
    in.pos.y = 1000;
    in.speed = 12.5;
    in.state = 0xAA;

    Payload out = execute_callback(transform_payload, in, 42, 1.6);

    assert_true(out.pos.x == 542, "Phase 4: sret pos.x mutation");
    assert_true(out.pos.y == 916, "Phase 4: sret pos.y mutation");
    assert_true(out.state == (0xAA ^ 42), "Phase 4: sret state mutation");

    hash_update_bytes(&out, sizeof(out));
}

/* ----------------------------------------------------------------------------
 * Phase 5: Verification & Hash Match Assertion
 * ---------------------------------------------------------------------------- */
int main(void) {
    printf("=== RUNNING ZCC VERIFIABLE TEST GAUNTLET ===\n");

    test_phase1_integers();
    test_phase2_floats();
    test_phase3_structs();
    test_phase4_callbacks();

    printf("FINAL HASH: 0x%016llx\n", g_hash);

    if (g_failures > 0) {
        printf("RESULT: FAILED (%d assertion errors)\n", g_failures);
        return 1;
    }

    /* Target expected FNV-1a 64-bit checksum for zero-padded structs */
    const unsigned long long expected_hash = 0xf56fe9d24ccf866eULL;
    if (g_hash != expected_hash) {
        printf("RESULT: HASH MISMATCH (Got 0x%016llx, Expected 0x%016llx)\n", g_hash, expected_hash);
        return 2;
    }

    printf("VERIFIED: checksum = 0x%016llx [PASS]\n", g_hash);
    return 0;
}
