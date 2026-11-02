/*
 * ZCC Track 3 & Track 3C — C99 §6.5.16.2 / §6.3.1.1 / §6.3.1.8 / §6.5.2.3 Regressions
 *
 * Covers the 7 minimized counterexamples discovered during Track 3 Semantic Frontier
 * plus the 5 neighboring failure surfaces required by Milestone Track 3C:
 *   1. DEFECT-TRACK3-001  (repro_01_shl_assign_u8_mem):
 *      1-byte non-local left-shift compound assignment (`<<=`) emitted `shrb` instead of `shlb`.
 *   2. DEFECT-TRACK3-002a (repro_02_compound_div_wider_rhs):
 *      Compound assignment with wider 64-bit RHS (`int32_t /= int64_t`) truncated RHS to 32-bit
 *      prior to division instead of evaluating in the C99 §6.3.1.8 common type (`int64_t`).
 *   3. DEFECT-TRACK3-002b (repro_03_compound_div_signed_unsigned_uac):
 *      Compound assignment with mixed signedness (`int32_t /= uint32_t`) evaluated `/` in signed
 *      `int32_t` (`idivl`) instead of the C99 §6.3.1.8 usual arithmetic conversion type (`uint32_t`, `divl`).
 *   4. DEFECT-TRACK3-002c (repro_04_compound_mul_int_double):
 *      Compound assignment with floating-point RHS (`int32_t *= double`) truncated the `double` RHS
 *      to `int32_t` before integer multiplication instead of promoting LHS to `double`.
 *   5. DEFECT-TRACK3-003  (repro_05_member_signed_subword_div_assign):
 *      Compound assignment on signed 8-bit/16-bit struct members (`s.a /= 2`) zero-extended (`movzbl`/`movzwl`)
 *      instead of sign-extending (`movsbq`/`movswq`) non-bitfield `ND_MEMBER` lvalues.
 *   6. DEFECT-TRACK3-004  (repro_06_unary_minus_uint8_promotion):
 *      Unary `-`, `+`, and `~` on narrow unsigned types (`uint8_t`, `uint16_t`) skipped C99 §6.3.1.1
 *      integer promotion to signed `int`.
 *   7. DEFECT-TRACK3-005  (repro_07_bitfield_compound_assign_expr_val):
 *      Bitfield assignment expression value (`r = (bf.u3 += 6)`) returned unmasked pre-store value (`11`)
 *      instead of the truncated/sign-extended bitfield value (`3`), and `--ir` bypassed bitfield RMW in
 *      `ND_MEMBER`, `ND_ASSIGN`, and `ND_COMPOUND_ASSIGN`.
 *   8-12. Track 3C Neighboring Surfaces:
 *      - Bitfield compound assignments (`+=`, `/=`, `<<=`, expression value wrap)
 *      - Signed/unsigned casts at width boundaries (`INT8_MIN`, `UINT8_MAX`, `INT16_MIN`, `UINT16_MAX`, `INT32_MIN`, `UINT32_MAX`)
 *      - Shift counts (`0`, `width - 1`, subword/32-bit/64-bit signed & unsigned)
 *      - Compound lvalue single evaluation (`*get_tracked_ptr(...) += ...`, `get_holder(&h)->... *= ...`)
 *      - Floating-point-to-integer conversion boundaries (`double`/`float` <-> signed/unsigned 8/16/32-bit)
 */

#include <stdio.h>
#include <stdint.h>

struct Repro1 {
    uint8_t b;
};

struct Repro5 {
    int8_t a;
    int16_t b;
};

struct BF {
    signed int s5 : 5;
    unsigned int u5 : 5;
    signed int s3 : 3;
    unsigned int u3 : 3;
};

static int g_lval_evals = 0;
static int *get_tracked_ptr(int *base, int idx) {
    g_lval_evals++;
    return &base[idx];
}

struct Holder {
    int8_t s8;
    uint8_t u8;
    int32_t s32;
};

static int g_holder_evals = 0;
static struct Holder *get_holder(struct Holder *h) {
    g_holder_evals++;
    return h;
}

static int test_repro_01_shl_assign_u8_mem(void) {
    struct Repro1 s = { 0x11u };
    s.b <<= 2;
    uint8_t arr[2] = { 0x07u, 0x03u };
    arr[0] <<= 3;
    if (s.b != 68u) return 1;
    if (arr[0] != 56u) return 2;
    return 0;
}

static int test_repro_02_compound_div_wider_rhs(void) {
    int32_t s32 = 100;
    int64_t d64 = 0x100000002LL;
    s32 /= d64;
    if (s32 != 0) return 10;

    int32_t m32 = 100;
    m32 %= d64;
    if (m32 != 100) return 11;

    int16_t s16 = 300;
    int64_t add64 = 0x100000010LL;
    s16 += add64;
    if (s16 != 316) return 12;
    return 0;
}

static int test_repro_03_compound_div_signed_unsigned_uac(void) {
    int32_t s32 = -10;
    uint32_t u32 = 2u;
    s32 /= u32;
    /* (uint32_t)(-10) == 4294967286u; 4294967286u / 2u == 2147483643u */
    if (s32 != 2147483643) return 20;

    int32_t r32 = -10;
    uint32_t mod_u32 = 10u;
    r32 %= mod_u32;
    /* 4294967286u % 10u == 6u */
    if (r32 != 6) return 21;
    return 0;
}

static int test_repro_04_compound_mul_int_double(void) {
    int32_t x = 10;
    double scale = 2.5;
    x *= scale;
    if (x != 25) return 30;

    int32_t y = 9;
    float fdiv = 2.0f;
    y /= fdiv;
    if (y != 4) return 31;

    double d = 1.5;
    int32_t k = 3;
    d += k;
    if (d < 4.49 || d > 4.51) return 32;
    return 0;
}

static int test_repro_05_member_signed_subword_div_assign(void) {
    struct Repro5 s = { -8, -9 };
    s.a /= 2;
    s.b /= 2;
    if (s.a != -4) return 40;
    if (s.b != -4) return 41;

    struct Repro5 s2 = { -16, -32 };
    s2.a >>= 2;
    s2.b >>= 3;
    if (s2.a != -4) return 42;
    if (s2.b != -4) return 43;
    return 0;
}

static int test_repro_06_unary_minus_uint8_promotion(void) {
    uint8_t u8 = 250;
    uint16_t u16 = 60000;
    if (!(-u8 < 0)) return 50;
    if (!(-u16 < 0)) return 51;
    if ((~u8) >= 0) return 52;
    return 0;
}

static int test_repro_07_and_bitfield_neighbors(void) {
    struct BF bf = { -8, 17, -4, 5 };
    bf.s5 += 3;        /* -5 */
    bf.s5 /= 2;        /* -2 */
    bf.u5 <<= 1;       /* (17 << 1) & 31 = 2 */
    bf.u5 += 15;       /* 17 */
    bf.u5 /= 3;        /* 5 */
    int r_bf_s3 = (bf.s3 += 7); /* -4 + 7 = 3 */
    int r_bf_u3 = (bf.u3 += 6); /* (5 + 6) & 7 = 3 */
    if (bf.s5 != -2) return 60;
    if (bf.u5 != 5u) return 61;
    if (bf.s3 != 3) return 62;
    if (bf.u3 != 3u) return 63;
    if (r_bf_s3 != 3) return 64;
    if (r_bf_u3 != 3) return 65;
    return 0;
}

static int test_neighbor_casts_width_boundaries(void) {
    int8_t s8_min = -128;
    uint8_t u8_max = 255u;
    uint16_t u16_max = 65535u;
    int32_t s32_min = (-2147483647 - 1);
    uint32_t u32_max = 4294967295u;
    uint32_t s8_to_u32 = (uint32_t)s8_min;
    int64_t u32_to_s64 = (int64_t)u32_max;
    uint64_t s32_to_u64 = (uint64_t)s32_min;
    int32_t u16_to_s32 = (int32_t)u16_max;
    int16_t u8_to_s16 = (int16_t)u8_max;
    uint16_t s8_to_u16 = (uint16_t)s8_min;
    if (s8_to_u32 != 4294967168u) return 70;
    if (u32_to_s64 != 4294967295LL) return 71;
    if (s32_to_u64 != 18446744071562067968ULL) return 72;
    if (u16_to_s32 != 65535) return 73;
    if (u8_to_s16 != 255) return 74;
    if (s8_to_u16 != 65408u) return 75;
    if ((int)(int8_t)u16_max != -1) return 76;
    return 0;
}

static int test_neighbor_shift_counts(void) {
    uint8_t sh_u8 = 0x81u;
    int8_t sh_s8 = -64;
    uint32_t sh_u32 = 1u;
    int32_t sh_s32 = -1073741824;
    uint64_t sh_u64 = 1ULL;
    sh_u8 <<= 0;
    sh_s8 >>= 5;
    sh_u32 <<= 31;
    sh_s32 >>= 30;
    sh_u64 <<= 63;
    if (sh_u8 != 129u) return 80;
    if (sh_s8 != -2) return 81;
    if (sh_u32 != 2147483648u) return 82;
    if (sh_s32 != -1) return 83;
    if (sh_u64 != 9223372036854775808ULL) return 84;
    return 0;
}

static int test_neighbor_lvalue_evaluated_once(void) {
    g_lval_evals = 0;
    g_holder_evals = 0;
    int arr[4] = { 10, 20, -30, 40 };
    *get_tracked_ptr(arr, 0) += 5;
    *get_tracked_ptr(arr, 1) *= 2.5;
    *get_tracked_ptr(arr, 2) /= 4u;
    struct Holder h = { -12, 0x11u, 100 };
    get_holder(&h)->s8 /= 3;
    get_holder(&h)->u8 <<= 2;
    get_holder(&h)->s32 *= 1.5;
    if (g_lval_evals != 3) return 90;
    if (g_holder_evals != 3) return 91;
    if (arr[0] != 15 || arr[1] != 50 || arr[2] != 1073741816) return 92;
    if (h.s8 != -4 || h.u8 != 68u || h.s32 != 150) return 93;
    return 0;
}

static int test_neighbor_fp_to_int_boundaries(void) {
    double d_neg = -127.9;
    double d_pos_u8 = 254.9;
    double d_pos_u16 = 65534.9;
    double d_pos_u32 = 4000000000.0;
    float f_neg = -32767.5f;
    int8_t fi_s8 = (int8_t)d_neg;
    uint8_t fi_u8 = (uint8_t)d_pos_u8;
    uint16_t fi_u16 = (uint16_t)d_pos_u16;
    uint32_t fi_u32 = (uint32_t)d_pos_u32;
    int16_t fi_s16 = (int16_t)f_neg;
    uint8_t ca_u8 = 100u;
    ca_u8 *= 2.5;
    int16_t ca_s16 = -1000;
    ca_s16 *= 3.25;
    uint32_t ca_u32 = 1000000000u;
    ca_u32 *= 3.5;
    if (fi_s8 != -127 || fi_u8 != 254u || fi_u16 != 65534u || fi_u32 != 4000000000u || fi_s16 != -32767) return 100;
    if (ca_u8 != 250u || ca_s16 != -3250 || ca_u32 != 3500000000u) return 101;
    return 0;
}

int main(void) {
    int rc = 0;
    if ((rc = test_repro_01_shl_assign_u8_mem()) != 0) return rc;
    if ((rc = test_repro_02_compound_div_wider_rhs()) != 0) return rc;
    if ((rc = test_repro_03_compound_div_signed_unsigned_uac()) != 0) return rc;
    if ((rc = test_repro_04_compound_mul_int_double()) != 0) return rc;
    if ((rc = test_repro_05_member_signed_subword_div_assign()) != 0) return rc;
    if ((rc = test_repro_06_unary_minus_uint8_promotion()) != 0) return rc;
    if ((rc = test_repro_07_and_bitfield_neighbors()) != 0) return rc;
    if ((rc = test_neighbor_casts_width_boundaries()) != 0) return rc;
    if ((rc = test_neighbor_shift_counts()) != 0) return rc;
    if ((rc = test_neighbor_lvalue_evaluated_once()) != 0) return rc;
    if ((rc = test_neighbor_fp_to_int_boundaries()) != 0) return rc;
    printf("TRACK3_COMPOUND_ASSIGN_C99_REGRESSIONS: 11/11 PASS\n");
    return 0;
}
