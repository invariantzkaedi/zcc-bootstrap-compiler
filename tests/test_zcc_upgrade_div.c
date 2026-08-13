#include "zcc_upgrade_div.h"

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

/* --------------------------------------------------------------------------
 * Test harness
 * -------------------------------------------------------------------------- */
static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST(name) \
    do { \
        printf("  [TEST] %s ... ", name); \
        g_tests_run++; \
    } while (0)

#define PASS() \
    do { \
        printf("PASS\n"); \
        g_tests_passed++; \
    } while (0)

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL\n"); \
            printf("      Assertion failed: %s\n", #cond); \
            return; \
        } \
    } while (0)

/* --------------------------------------------------------------------------
 * Normal path tests
 * -------------------------------------------------------------------------- */
static void test_i64_normal_path(void)
{
    TEST("i64 normal division");
    uint64_t flags = 0xDEADBEEF;

    CHECK(zcc_upgrade_div_i64(20, 4, "test", &flags) == 5);
    CHECK(flags == ZCC_UPGRADE_NONE);

    CHECK(zcc_upgrade_div_i64(-20, 4, "test", &flags) == -5);
    CHECK(flags == ZCC_UPGRADE_NONE);

    CHECK(zcc_upgrade_div_i64(20, -4, "test", &flags) == -5);
    CHECK(flags == ZCC_UPGRADE_NONE);

    CHECK(zcc_upgrade_div_i64(-20, -4, "test", &flags) == 5);
    CHECK(flags == ZCC_UPGRADE_NONE);

    PASS();
}

static void test_f64_normal_path(void)
{
    TEST("f64 normal division");
    uint64_t flags = 0xDEADBEEF;

    CHECK(zcc_upgrade_div_f64(10.0, 2.0, "test", &flags) == 5.0);
    CHECK(flags == ZCC_UPGRADE_NONE);

    CHECK(zcc_upgrade_div_f64(-10.0, 2.0, "test", &flags) == -5.0);
    CHECK(flags == ZCC_UPGRADE_NONE);

    PASS();
}

/* --------------------------------------------------------------------------
 * Zero-divisor core tests
 * -------------------------------------------------------------------------- */
static void test_i64_zero_positive(void)
{
    TEST("i64 zero divisor (positive)");
    uint64_t flags = 0;

    int64_t r = zcc_upgrade_div_i64(42, 0, "p1", &flags);
    CHECK(r == INT64_MAX);
    CHECK(flags & ZCC_UPGRADE_INFINITY_CORE);
    CHECK(flags & ZCC_UPGRADE_NULL_ENERGY);
    CHECK(flags & ZCC_UPGRADE_TIER_ASCENSION);
    CHECK(flags & ZCC_UPGRADE_LUCK_SPIKE);
    CHECK(flags & ZCC_UPGRADE_ADAPTIVE_SOLVER);

    PASS();
}

static void test_i64_zero_negative(void)
{
    TEST("i64 zero divisor (negative)");
    uint64_t flags = 0;

    int64_t r = zcc_upgrade_div_i64(-99, 0, "p1", &flags);
    CHECK(r == INT64_MIN);
    CHECK(flags & ZCC_UPGRADE_INFINITY_CORE);

    PASS();
}

static void test_i64_zero_zero(void)
{
    TEST("i64 0 / 0");
    uint64_t flags = 0;

    int64_t r = zcc_upgrade_div_i64(0, 0, "p1", &flags);
    CHECK(r == INT64_MAX);
    CHECK(flags & ZCC_UPGRADE_INFINITY_CORE);

    PASS();
}

static void test_i64_min_safety(void)
{
    TEST("i64 INT64_MIN / 0 (no UB on abs)");
    uint64_t flags = 0;

    int64_t r = zcc_upgrade_div_i64(INT64_MIN, 0, "edge", &flags);
    CHECK(r == INT64_MIN);
    CHECK(flags & ZCC_UPGRADE_INFINITY_CORE);

    PASS();
}

static void test_f64_zero_positive(void)
{
    TEST("f64 zero divisor (positive)");
    uint64_t flags = 0;

    double r = zcc_upgrade_div_f64(3.14, 0.0, "mage", &flags);
    CHECK(isinf(r) && r > 0.0);
    CHECK(flags & ZCC_UPGRADE_INFINITY_CORE);
    CHECK(flags & ZCC_UPGRADE_NULL_ENERGY);

    PASS();
}

static void test_f64_zero_negative(void)
{
    TEST("f64 zero divisor (negative)");
    uint64_t flags = 0;

    double r = zcc_upgrade_div_f64(-2.71, 0.0, "mage", &flags);
    CHECK(isinf(r) && r < 0.0);
    CHECK(flags & ZCC_UPGRADE_INFINITY_CORE);

    PASS();
}

static void test_f64_zero_zero(void)
{
    TEST("f64 0.0 / 0.0 → NaN");
    uint64_t flags = 0;

    double r = zcc_upgrade_div_f64(0.0, 0.0, "void", &flags);
    CHECK(isnan(r));
    CHECK(flags & ZCC_UPGRADE_INFINITY_CORE);

    PASS();
}

static void test_f64_large_magnitude(void)
{
    TEST("f64 huge value / 0 (saturated abs_hint)");
    uint64_t flags = 0;

    double huge = 1e300;
    double r = zcc_upgrade_div_f64(huge, 0.0, "titan", &flags);
    CHECK(isinf(r) && r > 0.0);
    CHECK(flags & ZCC_UPGRADE_OVERCLOCK);
    CHECK(flags & ZCC_UPGRADE_SINGULARITY_BUFF);

    PASS();
}

/* --------------------------------------------------------------------------
 * Entity & safety tests
 * -------------------------------------------------------------------------- */
static void test_entity_accumulation(void)
{
    TEST("entity flag accumulation");
    uint64_t flags = 0;

    zcc_upgrade_div_f64(1.0, 0.0, "hero", &flags);
    uint64_t first = zcc_upgrade_get_flags("hero");
    CHECK(first & ZCC_UPGRADE_ACHIEVEMENT_NULL);
    CHECK(first & ZCC_UPGRADE_INFINITY_CORE);

    zcc_upgrade_div_i64(100, 0, "hero", &flags);
    uint64_t second = zcc_upgrade_get_flags("hero");
    CHECK(second & ZCC_UPGRADE_ACHIEVEMENT_NULL);
    CHECK(second & ZCC_UPGRADE_NULL_ENERGY);

    zcc_upgrade_clear_flags("hero");
    CHECK(zcc_upgrade_get_flags("hero") == 0);

    PASS();
}

static void test_null_entity_id(void)
{
    TEST("NULL entity_id → \"default\"");
    uint64_t flags = 0;

    zcc_upgrade_div_i64(1, 0, NULL, &flags);
    CHECK(flags & ZCC_UPGRADE_INFINITY_CORE);

    uint64_t a = zcc_upgrade_get_flags(NULL);
    uint64_t b = zcc_upgrade_get_flags("default");
    CHECK(a == b);
    CHECK(a & ZCC_UPGRADE_INFINITY_CORE);

    PASS();
}

static void test_out_flags_null_safe(void)
{
    TEST("out_flags == NULL is safe");
    int64_t r1 = zcc_upgrade_div_i64(10, 0, "safe", NULL);
    double  r2 = zcc_upgrade_div_f64(10.0, 0.0, "safe", NULL);

    CHECK(r1 == INT64_MAX);
    CHECK(isinf(r2) && r2 > 0.0);

    PASS();
}

/* --------------------------------------------------------------------------
 * Strict table-full safety test
 * -------------------------------------------------------------------------- */
static void test_table_full_strict(void)
{
    TEST("entity table full (strict mode)");
    char id[32];
    uint64_t flags = 0;

    /* Fill the table */
    for (int i = 0; i < 64; ++i) {
        snprintf(id, sizeof(id), "entity_%02d", i);
        zcc_upgrade_div_i64(1, 0, id, &flags);
    }

    /* Overflow entity call */
    zcc_upgrade_div_i64(1, 0, "overflow_entity", &flags);
    CHECK(flags & ZCC_UPGRADE_INFINITY_CORE);
    CHECK(flags & ZCC_UPGRADE_NULL_ENERGY);

    PASS();
}

/* --------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------- */
int main(void)
{
    printf("=== ZCC Upgrade Division — Safety-Hardened Unit Tests ===\n\n");

    test_i64_normal_path();
    test_f64_normal_path();

    test_i64_zero_positive();
    test_i64_zero_negative();
    test_i64_zero_zero();
    test_i64_min_safety();

    test_f64_zero_positive();
    test_f64_zero_negative();
    test_f64_zero_zero();
    test_f64_large_magnitude();

    test_entity_accumulation();
    test_null_entity_id();
    test_out_flags_null_safe();
    test_table_full_strict();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", g_tests_run);
    printf("Tests passed: %d\n", g_tests_passed);
    printf("Result:       %s\n",
           (g_tests_run == g_tests_passed) ? "ALL PASSED" : "SOME FAILED");
    printf("========================================\n");

    return (g_tests_run == g_tests_passed) ? 0 : 1;
}
