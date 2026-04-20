#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double test_dup8(double val) {
    // Mimic sqlite3VdbeAddOp4Dup8
    unsigned char *p = malloc(8);
    memcpy(p, &val, 8);

    // Mimic OP_Real reading it back
    double *pReal = (double*)p;
    double result = *pReal;
    free(p);
    return result;
}

int main() {
    double r = test_dup8(3.14);
    printf("dup8: %.6f\n", r);

    r = test_dup8(0.5);
    printf("dup8: %.6f\n", r);
    return 0;
}
