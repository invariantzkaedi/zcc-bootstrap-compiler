/* regression: CG-ABI-STRUCT-002 — minimal chain reproducer
 *
 * Minimal case: D (24B, memory-class) passed by value through a chain
 * of two function calls, where all three functions use sret+2-struct ABI.
 *
 * Before fix: d_op output d=0.00 (y parameter loaded from wrong offset)
 * After fix:  d_op output d=99.41 (correct)
 *
 * Run:
 *   ./zcc -I./zcc_sys_includes tests/regressions/repro_chain_min.c -o /tmp/t.s
 *   gcc /tmp/t.s -o /tmp/t && /tmp/t
 */
#include <stdio.h>

typedef struct { double d; unsigned u; int i; float f; int j; } D;

D d_op(D x, D y) {
    D r;
    r.d = x.d * y.d;   /* y.d was corrupted pre-fix: loaded from -64(%rbp) */
    r.u = x.u + y.u;
    r.i = x.i - y.i;
    r.f = x.f + y.f;
    r.j = x.j ^ y.j;
    return r;
}

D d_chain2(D x, D y) {
    D tmp = d_op(x, y);
    return d_op(tmp, y);
}

int main(void) {
    D a = {4.46, 135, 75, 32.90f, 89};
    D b = {22.29, 75, 90, 39.97f, 13};
    int failed = 0;

    D r1 = d_op(a, b);
    /* expected: d=99.41 u=210 i=-15 f=72.87 j=84 */
    if (r1.d < 99.40 || r1.d > 99.42 || r1.u != 210 || r1.i != -15 || r1.j != 84) {
        printf("FAIL d_op: d=%.2f u=%u i=%d j=%d\n", r1.d, r1.u, r1.i, r1.j);
        failed++;
    }

    D r2 = d_chain2(a, b);
    /* expected: d=2215.92 u=285 i=-105 f=112.84 j=89 */
    if (r2.d < 2215.91 || r2.d > 2215.93 || r2.u != 285 || r2.i != -105 || r2.j != 89) {
        printf("FAIL d_chain2: d=%.2f u=%u i=%d j=%d\n", r2.d, r2.u, r2.i, r2.j);
        failed++;
    }

    if (failed == 0) {
        printf("PASS CG-ABI-STRUCT-002 chain regression\n");
        return 0;
    }
    return 1;
}
