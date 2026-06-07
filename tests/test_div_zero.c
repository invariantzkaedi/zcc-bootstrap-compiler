#include <stdio.h>

int test_static_div_zero() {
    int x = 10 / 0;
    int y = 10 % 0;
    return x + y;
}

int test_var_div_zero() {
    int zero = 0;
    int x = 20 / zero;
    int y = 20 % zero;
    return x + y;
}

int main() {
    printf("Testing static division/modulo by zero...\n");
    int r1 = test_static_div_zero();
    printf("r1 = %d (expected 0)\n", r1);

    printf("Testing variable-propagated division/modulo by zero...\n");
    int r2 = test_var_div_zero();
    printf("r2 = %d (expected 0)\n", r2);

    if (r1 == 0 && r2 == 0) {
        printf("DIV_ZERO_TEST: PASS\n");
        return 0;
    } else {
        printf("DIV_ZERO_TEST: FAIL\n");
        return 1;
    }
}
