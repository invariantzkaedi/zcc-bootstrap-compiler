#include <stdio.h>
#include <stdint.h>
#include <string.h>

static double test_fround(double a) {
    return (float)a;
}

int main(void) {
    double in_val = 9007199254740994.0;
    double res = test_fround(in_val);
    uint64_t u;
    memcpy(&u, &res, sizeof(u));

    printf("Input:    %.1f\n", in_val);
    printf("Actual:   %.15e (0x%016lx)\n", res, u);
    printf("Expected: 9007199254740992.0 (0x4340000000000000)\n");

    int pass = (u == 0x4340000000000000ULL);
    printf("VERDICT:  %s\n", pass ? "PASS" : "FAIL (DEFECT CONFIRMED)");
    return pass ? 0 : 1;
}
