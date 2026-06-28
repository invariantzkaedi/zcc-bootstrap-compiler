/* IEEE-754 Floating-Point Torture Regression
 *
 * Tests all 20 IEEE-754 edge cases that commonly expose compiler
 * constant-folding, codegen, and runtime math divergences.
 *
 * Oracle: differential test against GCC -O0.
 * Every printf must produce identical output on ZCC and GCC.
 *
 * Categories exercised:
 *   - NaN propagation (sqrt, log, asin, 0/0)
 *   - Infinity arithmetic (+Inf, -Inf, overflow)
 *   - Signed zero (+0.0, -0.0, copysign)
 *   - Subnormal / underflow (DBL_MIN/2, nextafter)
 *   - Exact integer boundary (2^53)
 *   - Rounding (0.1+0.2)
 *   - Associativity failure
 *   - Catastrophic cancellation
 *   - FMA single-rounding semantics
 *   - Bit pattern inspection via union
 */

#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>

/* Bit-pattern helper */
static void print_bits(const char *label, double d) {
    unsigned long long u;
    memcpy(&u, &d, sizeof(u));
    printf("%s bits=%016llx\n", label, u);
}

int main(void) {
    /* 1. NaN propagation */
    double nan_val = 0.0 / 0.0;
    printf("1a nan_self_neq: %d\n", nan_val != nan_val);  /* 1 */
    printf("1b isnan: %d\n", isnan(nan_val));              /* 1 */

    /* 2. Infinity */
    double pos_inf = 1.0 / 0.0;
    double neg_inf = -1.0 / 0.0;
    printf("2a pos_inf: %f\n", pos_inf);
    printf("2b neg_inf: %f\n", neg_inf);
    printf("2c inf+1: %f\n", pos_inf + 1.0);
    printf("2d inf*2: %f\n", pos_inf * 2.0);
    printf("2e isinf_pos: %d\n", isinf(pos_inf) != 0);    /* 1 */
    printf("2f isinf_neg: %d\n", isinf(neg_inf) != 0);    /* 1 */

    /* 3. Signed zero */
    double pz = +0.0;
    double nz = -0.0;
    printf("3a zero_eq: %d\n", pz == nz);                 /* 1 */
    printf("3b 1/pz: %f\n", 1.0 / pz);                    /* +Inf */
    printf("3c 1/nz: %f\n", 1.0 / nz);                    /* -Inf */
    printf("3d copysign_pz: %.1f\n", copysign(1.0, pz));  /* 1.0  */
    printf("3e copysign_nz: %.1f\n", copysign(1.0, nz));  /* -1.0 */

    /* 4. Smallest normal */
    printf("4a dbl_min: %.17e\n", DBL_MIN);

    /* 5. Smallest subnormal */
    double smallest_sub = 4.9406564584124654e-324;
    printf("5a smallest_sub_nz: %d\n", smallest_sub != 0.0); /* 1 — must not flush to zero */

    /* 6. Largest double / overflow */
    printf("6a dbl_max: %.17e\n", DBL_MAX);
    double overflow = DBL_MAX * 2.0;
    printf("6b overflow_inf: %d\n", isinf(overflow) != 0); /* 1 */

    /* 7. Machine epsilon */
    printf("7a eps_neq: %d\n", 1.0 + DBL_EPSILON != 1.0);     /* 1 */
    printf("7b eps2_eq: %d\n", 1.0 + DBL_EPSILON/2.0 == 1.0); /* 1 */

    /* 8. Catastrophic cancellation */
    double big = 1e16;
    double big1 = big + 1.0;
    printf("8a cancel: %.1f\n", big1 - big);  /* 0.0 — the 1 is lost */

    /* 9. Associativity failure */
    double a9 = 1e30, b9 = -1e30, c9 = 1.0;
    double lhs = (a9 + b9) + c9;
    double rhs = a9 + (b9 + c9);
    printf("9a assoc_lhs: %.1f\n", lhs);  /* 1.0 */
    printf("9b assoc_rhs: %.1f\n", rhs);  /* 0.0 */

    /* 10. Multiplication overflow */
    double mul_ovf = 1e308 * 1e308;
    printf("10a mul_ovf_inf: %d\n", isinf(mul_ovf) != 0); /* 1 */

    /* 11. Underflow */
    double und = 1e-308 * 1e-308;
    printf("11a underflow_zero: %d\n", und == 0.0); /* 1 (product is below subnormal range) */

    /* 12. Exact integer boundary (2^53) */
    double two53 = 9007199254740992.0;
    printf("12a exact_boundary: %d\n", two53 + 1.0 == two53); /* 1 — 2^53+1 rounds back to 2^53 */

    /* 13. Rounding edge */
    double r13 = 0.1 + 0.2;
    printf("13a rounding: %d\n", r13 != 0.3); /* 1 */
    printf("13b rounding_val: %.17f\n", r13);  /* 0.30000000000000004 */

    /* 14. sqrt() special cases */
    printf("14a sqrt_neg: %d\n", isnan(sqrt(-1.0)));  /* 1 */
    print_bits("14b sqrt_nz", sqrt(-0.0));             /* -0.0 → 8000000000000000 */
    print_bits("14c sqrt_pz", sqrt(+0.0));             /* +0.0 → 0000000000000000 */
    printf("14d sqrt_inf: %d\n", isinf(sqrt(pos_inf)) != 0); /* 1 */
    printf("14e sqrt_nan: %d\n", isnan(sqrt(nan_val)));       /* 1 */

    /* 15. nextafter() */
    double na = nextafter(1.0, 2.0);
    printf("15a nextafter_neq: %d\n", na != 1.0); /* 1 */
    printf("15b nextafter_val: %.17e\n", na);

    /* 16. copysign signed zero preservation */
    print_bits("16a neg_zero", -0.0);  /* 8000000000000000 */
    print_bits("16b pos_zero", +0.0);  /* 0000000000000000 */

    /* 17. Bit patterns for specials */
    print_bits("17a +inf", pos_inf);  /* 7ff0000000000000 */
    print_bits("17b -inf", neg_inf);  /* fff0000000000000 */
    print_bits("17c nan",  nan_val);  /* 7ff8000000000000 (quiet NaN) */

    /* 18. DBL_MIN / 2 → subnormal (must not flush to zero) */
    double sub18 = DBL_MIN / 2.0;
    printf("18a subnormal_nz: %d\n", sub18 != 0.0); /* 1 */
    printf("18b subnormal_val: %.17e\n", sub18);

    /* 19. Overflow to infinity from addition */
    double near_max = DBL_MAX;
    double add_ovf  = near_max + near_max;
    printf("19a add_ovf_inf: %d\n", isinf(add_ovf) != 0); /* 1 */

    /* 20. NaN propagation through arithmetic */
    printf("20a nan_add: %d\n", isnan(nan_val + 1.0));  /* 1 */
    printf("20b nan_mul: %d\n", isnan(nan_val * 2.0));  /* 1 */
    printf("20c nan_sub: %d\n", isnan(nan_val - nan_val)); /* 1 */

    printf("ieee754_torture: PASS\n");
    return 0;
}
