/* ZCC fpx validation — uses double params (ZCC float param promotion) */
#include "zkaedi_fpx.h"
#include "zkaedi_fpx.c"
#include <stdio.h>

int main(void) {
    fp24_t a, b, c;
    float fa, fb, fc;

    /* Use fp24_from_double since ZCC promotes float args */
    a = fp24_from_double(2.5);
    fa = fp24_to_float(a);
    printf("fp24(2.5) = %f  bits=0x%X\n", (double)fa, a);

    b = fp24_from_double(3.5);
    fb = fp24_to_float(b);
    printf("fp24(3.5) = %f  bits=0x%X\n", (double)fb, b);

    c = fp24_add(a, b);
    fc = fp24_to_float(c);
    printf("2.5 + 3.5 = %f  bits=0x%X\n", (double)fc, c);

    /* Negative */
    a = fp24_from_double(-1.5);
    fa = fp24_to_float(a);
    printf("fp24(-1.5) = %f  bits=0x%X\n", (double)fa, a);

    /* Precision boundary */
    a = fp24_from_double(3.14159265);
    fa = fp24_to_float(a);
    printf("fp24(pi) = %f  bits=0x%X\n", (double)fa, a);

    /* Special values */
    printf("isnan(NaN) = %d\n", fp24_isnan(FP24_NAN));
    printf("isinf(Inf) = %d\n", fp24_isinf(FP24_INF));
    printf("iszero(0)  = %d\n", fp24_iszero(FP24_ZERO));

    /* fp48 */
    {
        fp48_t d48 = fp48_from_double(3.14159265358979);
        double dv = fp48_to_double(d48);
        printf("fp48(pi) = %.12f  bits=0x%llX\n", dv, d48);
    }

    /* Cross-format */
    {
        fp24_t narrow;
        fp48_t wide;
        wide = fp24_to_fp48(fp24_from_double(2.5));
        narrow = fp48_to_fp24(fp48_from_double(2.5));
        printf("fp24->fp48: %f\n", fp48_to_double(wide));
        printf("fp48->fp24: %f\n", (double)fp24_to_float(narrow));
    }

    printf("[OK] clean fpx test passed\n");
    return 0;
}
