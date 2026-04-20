#include <stdio.h>

union p4u {
    int i;
    void *p;
    double *pReal;
};

struct Op {
    int opcode;
    int p1;
    int p2;
    int p3;
    union p4u p4;
};

struct Mem {
    union { double r; long long i; } u;
    int flags;
};

int main() {
    double val = 3.14;
    struct Op op;
    op.p4.pReal = &val;

    struct Mem out;
    out.u.r = *op.p4.pReal;
    out.flags = 8;

    printf("val=%.6f out.u.r=%.6f flags=%d\n", val, out.u.r, out.flags);

    // Also test through pointer (like VDBE does)
    struct Op *pOp = &op;
    struct Mem *pOut = &out;
    pOut->u.r = *pOp->p4.pReal;
    printf("via ptr: %.6f\n", pOut->u.r);
    return 0;
}
