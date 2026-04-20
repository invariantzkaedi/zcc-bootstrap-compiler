#include <stdio.h>
#include <string.h>

// Simulate what OP_Real does, but in a tiny function
void do_op_real(void *pOut_u_r, double *pReal) {
    double val = *pReal;
    memcpy(pOut_u_r, &val, 8);
}

int main() {
    double storage = 0.0;
    double val = 3.14;
    do_op_real(&storage, &val);
    printf("result=%.6f\n", storage);
    return 0;
}
