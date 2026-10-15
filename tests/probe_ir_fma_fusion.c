#include <stdio.h>

double fma_add_test(double a, double b, double c) {
    return (a * b) + c;
}

double fma_add_comm_test(double a, double b, double c) {
    return c + (a * b);
}

float fma_float_test(float a, float b, float c) {
    return (a * b) + c;
}

double fnma_sub_test(double a, double b, double c) {
    return c - (a * b);
}

int main(void) {
    double r1 = fma_add_test(2.0, 3.0, 4.0);
    double r2 = fma_add_comm_test(2.0, 3.0, 4.0);
    float  r3 = fma_float_test(2.0f, 3.0f, 4.0f);
    double r4 = fnma_sub_test(2.0, 3.0, 10.0);
    printf("r1=%.1f r2=%.1f r3=%.1f r4=%.1f\n", r1, r2, r3, r4);
    if (r1 == 10.0 && r2 == 10.0 && r3 == 10.0f && r4 == 4.0) {
        printf("PROBE EXECUTION: PASS\n");
        return 0;
    }
    printf("PROBE EXECUTION: FAIL\n");
    return 1;
}
