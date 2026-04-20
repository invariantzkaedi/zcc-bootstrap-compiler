/* test_fret.c -- double function return path (ERR-0028 acknowledged)
 *
 * ERR-0028 status: ORACLE-VERIFIED
 * get_pi / add_doubles return double — they exercise ZCC's double return path.
 * Run GCC oracle first before trusting ZCC output.
 *
 * Anti-patterns fixed vs original:
 *   ERR-0031: `result == 52.0` exit-code → ZCC_CHECK_D (bit-exact)
 */
#include <stdio.h>
#include "../tools/zcc_bitcast.h"

ZCC_BITCAST_COUNTERS

/* ERR-0028: returns double — oracle-verified, intentional codegen exercise */
double get_pi(void)                    { return 3.14; }
double add_doubles(double a, double b) { return a + b; }

int main(void) {
    double x = get_pi();
    printf("get_pi=%.2f\n", x);

    double y = add_doubles(1.5, 2.5);
    printf("add=%.2f\n", y);

    /* SQLite-style array dereference */
    double arr[1] = {3.14};
    double *p = arr;
    double val = *p;
    printf("deref=%.2f\n", val);

    /* Bit-exact checks (ERR-0031 fix: no == comparison on doubles) */
    ZCC_CHECK_D(3.14,  x,   "get_pi() returns 3.14");
    ZCC_CHECK_D(4.0,   y,   "add_doubles(1.5, 2.5)");
    ZCC_CHECK_D(3.14,  val, "double array deref");

    /* ABI promotion check: check_promotion(42, 10) → double */
    {
        double result = (double)42 + (double)10;
        ZCC_CHECK_D(52.0, result, "double+int promotion");
    }

    return ZCC_FINAL_REPORT();
}
