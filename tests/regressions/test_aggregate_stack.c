/* test_aggregate_stack.c — Hardened aggregate stack alignment regression test */
#include <stdio.h>
#include <assert.h>

struct LargeAgg {
    double data[4];
};

static struct LargeAgg get_agg(double v) {
    struct LargeAgg a = {{v, v * 2.0, v * 3.0, v * 4.0}};
    return a;
}

__attribute__((noinline))
static double verify_call(
    struct LargeAgg a,
    double x1,
    double x2,
    double x3,
    double x4
) {
    assert(a.data[0] == 5.0);
    assert(a.data[1] == 10.0);
    assert(a.data[2] == 15.0);
    assert(a.data[3] == 20.0);
    return x1 + x2 + x3 + x4;
}

int main(void) {
    struct LargeAgg source = get_agg(5.0);
    struct LargeAgg destination = {{0}};

    destination = source; /* Explicit aggregate ND_ASSIGN path */

    double result = verify_call(destination, 1.0, 2.0, 3.0, 4.0);
    assert(result == 10.0);

    puts("[OK] Aggregate assignment and call alignment tests passed!");
    return 0;
}
