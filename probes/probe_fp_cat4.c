#include <stdio.h>
#include <math.h>
#include <float.h>

int main(void) {
    float a = 1.5f;
    float b = 2.5f;
    float c = 1.5f;

    /* ==, !=, <, >, <=, >= between float values */
    printf("a == b: %d\n", a == b);
    printf("a == c: %d\n", a == c);
    printf("a != b: %d\n", a != b);
    printf("a != c: %d\n", a != c);
    printf("a < b: %d\n", a < b);
    printf("a > b: %d\n", a > b);
    printf("a <= b: %d\n", a <= b);
    printf("a <= c: %d\n", a <= c);
    printf("a >= b: %d\n", a >= b);
    printf("a >= c: %d\n", a >= c);

    /* NaN comparisons (NaN == NaN should be false, NaN != NaN should be true) */
    float my_nan = NAN;
    printf("nan == nan: %d\n", my_nan == my_nan);
    printf("nan != nan: %d\n", my_nan != my_nan);
    printf("nan < 1.0: %d\n", my_nan < 1.0f);
    printf("nan > 1.0: %d\n", my_nan > 1.0f);

    /* INFINITY comparisons */
    float inf = INFINITY;
    float ninf = -INFINITY;
    printf("inf == inf: %d\n", inf == inf);
    printf("inf != inf: %d\n", inf != inf);
    printf("ninf < inf: %d\n", ninf < inf);
    printf("inf > FLT_MAX: %d\n", inf > FLT_MAX);

    /* Comparison results used in if/ternary/loop conditions */
    if (a < b) {
        printf("flow: a < b is true\n");
    } else {
        printf("flow: a < b is false\n");
    }

    if (my_nan == my_nan) {
        printf("flow: nan == nan is true (unexpected)\n");
    } else {
        printf("flow: nan == nan is false (correct)\n");
    }

    double t_res = (b > a) ? 42.0 : -42.0;
    printf("ternary_res: %f\n", t_res);

    float val = 0.0f;
    int loop_count = 0;
    while (val < 3.0f) {
        loop_count++;
        val += 1.0f;
    }
    printf("loop_count: %d\n", loop_count);

    return 0;
}
