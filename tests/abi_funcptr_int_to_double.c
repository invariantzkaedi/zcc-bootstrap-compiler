#include <stdio.h>

/* HYGIENE-ABI-004: Regression test for function-pointer call passing
 * int literal through a double-typed parameter slot.
 *
 * This tests the indirect-call coercion path in parse_postfix (part3.c
 * ~L1233), which was added preemptively as a mirror of the direct-call
 * fix. No gate in ABI-INT-TO-DOUBLE-001 exercised this specific shape.
 *
 * Expected output (gcc and zcc, patched):
 *   via_ptr: d=504.000000 x=42
 *   via_cast: d=504.000000 x=42
 *   via_field: d=504.000000 x=42
 *
 * Unpatched zcc output would be:
 *   via_ptr: d=0.000000 x=504   (wrong -- int 504 misrouted to %rsi)
 */

void sink(double d, unsigned long x) {
    printf("d=%f x=%lu\n", d, x);
}

typedef void (*fn_t)(double, unsigned long);

struct dispatch {
    fn_t call;
};

int main(void) {
    /* Leg A: direct function pointer variable */
    fn_t fp = sink;
    printf("via_ptr: ");
    fp(504, 42);

    /* Leg B: cast expression — (fn_t)&sink */
    printf("via_cast: ");
    ((fn_t)sink)(504, 42);

    /* Leg C: function pointer through struct field */
    struct dispatch d;
    d.call = sink;
    printf("via_field: ");
    d.call(504, 42);

    return 0;
}
