/* CG-FLOAT-001 diagnostic */
#include <stdio.h>

void test_float(void) {
    float f = 2.5f;
    double d = (double)f;
    printf("f=%f d=%f\n", (double)f, d);
}

int main(void) {
    test_float();
    return 0;
}
