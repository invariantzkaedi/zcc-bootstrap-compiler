/* Placement matrix for C23 [[...]] attributes */
#include <stdio.h>

/* 1. Global declaration */
[[maybe_unused]] int g_global = 100;

/* 2. Function declaration */
[[nodiscard]] int compute_sum(int a, int b);

/* 3. Function definition with parameter attribute */
[[nodiscard]] int compute_sum([[maybe_unused]] int a, int b) {
    return a + b;
}

/* 4. Typedef placement */
[[maybe_unused]] typedef int custom_int_t;

int main(void) {
    /* 5. Local variable declaration */
    [[maybe_unused]] int local_val = 50;

    /* 6. Statement decoration & fallthrough */
    int x = 2;
    int outcome = 0;
    switch (x) {
        case 1:
            outcome += 10;
            [[fallthrough]];
        case 2:
            outcome += 20;
            [[fallthrough]];
        default:
            outcome += 5;
            break;
    }

    if (compute_sum(10, 20) != 30) return 1;
    if (outcome != 25) return 2;

    return 0;
}
