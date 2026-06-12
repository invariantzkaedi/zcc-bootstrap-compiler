/* fp_conv_harness.c — compiler regression test for floating-point conversions.
 * Exits with 0 on success, and non-zero on failure. */
#include <stdio.h>
#include <stdint.h>

static double u64_to_double(uint64_t u) {
    return (double)u;
}

static float u64_to_float(uint64_t u) {
    return (float)u;
}

static uint64_t double_to_u64(double d) {
    return (uint64_t)d;
}

static uint64_t float_to_u64(float f) {
    return (uint64_t)f;
}

static uint32_t double_to_u32(double d) {
    return (uint32_t)d;
}

int main(void) {
    int fails = 0;

    printf("=== Test 1: Explicit cast uint64_t -> double ===\n");
    struct {
        uint64_t u;
        double expected;
    } cases1[] = {
        { 0ULL, 0.0 },
        { 100ULL, 100.0 },
        { 9223372036854775807ULL, 9223372036854775808.0 },
        { 9223372036854775808ULL, 9223372036854775808.0 },
        { 18446744073709551615ULL, 18446744073709551616.0 }
    };
    for (int i = 0; i < 5; i++) {
        double r = u64_to_double(cases1[i].u);
        int ok = (r == cases1[i].expected);
        if (!ok) fails++;
        printf("  u=%-20llu  got=%-24g expected=%-24g %s\n",
               (unsigned long long)cases1[i].u, r, cases1[i].expected, ok ? "OK" : "FAIL");
    }

    printf("\n=== Test 2: Explicit cast uint64_t -> float ===\n");
    struct {
        uint64_t u;
        float expected;
    } cases2[] = {
        { 0ULL, 0.0f },
        { 100ULL, 100.0f },
        { 9223372036854775808ULL, 9223372036854775808.0f },
        { 18446744073709551615ULL, 18446744073709551616.0f }
    };
    for (int i = 0; i < 4; i++) {
        float r = u64_to_float(cases2[i].u);
        int ok = (r == cases2[i].expected);
        if (!ok) fails++;
        printf("  u=%-20llu  got=%-24g expected=%-24g %s\n",
               (unsigned long long)cases2[i].u, r, cases2[i].expected, ok ? "OK" : "FAIL");
    }

    printf("\n=== Test 3: Explicit cast double/float -> uint64_t ===\n");
    struct {
        double d;
        uint64_t expected;
    } cases3[] = {
        { 0.0, 0ULL },
        { 100.5, 100ULL },
        { 2147483647.0, 2147483647ULL },
        { 9223372036854775808.0, 9223372036854775808ULL },
        { 10000000000000000000.0, 10000000000000000000ULL }
    };
    for (int i = 0; i < 5; i++) {
        uint64_t r = double_to_u64(cases3[i].d);
        int ok = (r == cases3[i].expected);
        if (!ok) fails++;
        printf("  d=%-22g  got=%-22llu expected=%-22llu %s\n",
               cases3[i].d, (unsigned long long)r, (unsigned long long)cases3[i].expected, ok ? "OK" : "FAIL");
    }

    printf("\n=== Test 4: Implicit conversions in binary operators ===\n");
    uint64_t u = 9223372036854775808ULL;
    
    double add_l = u + 1.0;
    int add_l_ok = (add_l == 9223372036854775808.0);
    if (!add_l_ok) fails++;
    printf("  u + 1.0            got=%-24g expected=%-24g %s\n",
           add_l, 9223372036854775808.0, add_l_ok ? "OK" : "FAIL");

    double add_r = 1.0 + u;
    int add_r_ok = (add_r == 9223372036854775808.0);
    if (!add_r_ok) fails++;
    printf("  1.0 + u            got=%-24g expected=%-24g %s\n",
           add_r, 9223372036854775808.0, add_r_ok ? "OK" : "FAIL");

    double mul_l = u * 2.0;
    int mul_l_ok = (mul_l == 18446744073709551616.0);
    if (!mul_l_ok) fails++;
    printf("  u * 2.0            got=%-24g expected=%-24g %s\n",
           mul_l, 18446744073709551616.0, mul_l_ok ? "OK" : "FAIL");

    int lt = (u < 0.0);
    int lt_ok = (lt == 0);
    if (!lt_ok) fails++;
    printf("  u < 0.0            got=%-24d expected=%-24d %s\n",
           lt, 0, lt_ok ? "OK" : "FAIL");

    printf("\n=== Test 5: Explicit cast double -> uint32_t ===\n");
    struct {
        double d;
        uint32_t expected;
    } cases5[] = {
        { 0.0, 0U },
        { 100.5, 100U },
        { 3000000000.0, 3000000000U },
        { 10000000000.0, 1410065408U }
    };
    for (int i = 0; i < 4; i++) {
        uint32_t r = double_to_u32(cases5[i].d);
        int ok = (r == cases5[i].expected);
        if (!ok) fails++;
        printf("  d=%-22g  got=%-22u expected=%-22u %s\n",
               cases5[i].d, r, cases5[i].expected, ok ? "OK" : "FAIL");
    }

    printf("\n=== Final: %d failure(s) ===\n", fails);
    return fails;
}
