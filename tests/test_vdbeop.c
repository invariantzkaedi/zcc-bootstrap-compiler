#include <stdio.h>

typedef long long i64;
typedef unsigned short u16;
typedef unsigned char u8;
typedef unsigned int u32;

union p4union {
    int i;
    void *p;
    char *z;
    i64 *pI64;
    double *pReal;
    void *pFunc;
    void *pCtx;
    void *pColl;
    void *pMem;
    void *pVtab;
    void *pKeyInfo;
    u32 *ai;
    void *pProgram;
    void *pTab;
};

struct VdbeOp {
    u8 opcode;
    signed char p4type;
    u16 p5;
    int p1;
    int p2;
    int p3;
    union p4union p4;
};

int main() {
    struct VdbeOp op;
    printf("sizeof VdbeOp=%lu\n", sizeof(op));
    printf("offset opcode=%lu\n", (char*)&op.opcode - (char*)&op);
    printf("offset p1=%lu\n", (char*)&op.p1 - (char*)&op);
    printf("offset p2=%lu\n", (char*)&op.p2 - (char*)&op);
    printf("offset p3=%lu\n", (char*)&op.p3 - (char*)&op);
    printf("offset p4=%lu\n", (char*)&op.p4 - (char*)&op);
    printf("offset p4.pReal=%lu\n", (char*)&op.p4.pReal - (char*)&op);

    double val = 3.14;
    op.p4.pReal = &val;
    printf("*p4.pReal=%.6f\n", *op.p4.pReal);

    struct VdbeOp *pOp = &op;
    printf("via ptr: %.6f\n", *pOp->p4.pReal);
    return 0;
}
