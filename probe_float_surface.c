#include <stdio.h>

/* ---- helpers using integer return to avoid float-codegen trust issues ---- */
static unsigned int fbits(float f) {
    unsigned int b;
    unsigned int *p = (unsigned int *)&f;
    b = *p;
    return b;
}
static unsigned long long dbits(double d) {
    unsigned long long b;
    unsigned long long *p = (unsigned long long *)&d;
    b = *p;
    return b;
}

/* ---- [1] Compound assignment: x += 0.1f ---- */
static int test_compound_assign(void) {
    float x = 0.0f;
    x += 0.1f;
    x += 0.1f;
    x += 0.1f;
    /* GCC: 3 * 0.1f in float = 0x3e4ccccd (0.30000001192...) */
    /* If += silently uses double 0.1, bits will differ */
    unsigned int b = fbits(x);
    printf("[1] compound x += 0.1f (x3): bits=0x%08x  ", b);
    /* 3 * float(0.1) = 0x3e4ccccd */
    if (b == 0x3e99999a) { printf("PASS\n"); return 1; }
    printf("FAIL (want 0x3e4ccccd)\n"); return 0;
}

/* ---- [2] Unary minus on float literal ---- */
static int test_unary_minus(void) {
    float f = -0.1f;
    unsigned int b = fbits(f);
    printf("[2] unary minus -0.1f: bits=0x%08x  ", b);
    /* -0.1f = 0xbdcccccd */
    if (b == 0xbdcccccd) { printf("PASS\n"); return 1; }
    printf("FAIL (want 0xbdcccccd)\n"); return 0;
}

/* ---- [3] Float comparison ---- */
static int test_float_cmp(void) {
    float f = 0.15f;
    int lt = (f < 0.2f);
    int gt = (f > 0.1f);
    int eq = (f == 0.15f);
    printf("[3] float cmp (0.15f < 0.2f=%d, > 0.1f=%d, == 0.15f=%d)  ", lt, gt, eq);
    if (lt == 1 && gt == 1 && eq == 1) { printf("PASS\n"); return 1; }
    printf("FAIL\n"); return 0;
}

/* ---- [4] Array store/load with float element ---- */
static int test_float_array(void) {
    float arr[4];
    arr[0] = 0.1f;
    arr[1] = 0.2f;
    arr[2] = 0.3f;
    arr[3] = 0.4f;
    unsigned int b0 = fbits(arr[0]);
    unsigned int b1 = fbits(arr[1]);
    unsigned int b2 = fbits(arr[2]);
    unsigned int b3 = fbits(arr[3]);
    printf("[4] float arr[4]={0.1f,0.2f,0.3f,0.4f}: 0x%08x 0x%08x 0x%08x 0x%08x  ",
           b0, b1, b2, b3);
    if (b0==0x3dcccccd && b1==0x3e4ccccd && b2==0x3e99999a && b3==0x3ecccccd) {
        printf("PASS\n"); return 1;
    }
    printf("FAIL\n"); return 0;
}

/* ---- [5] Float through function pointer ---- */
static void fp_sink(float f, unsigned int *out) { *out = fbits(f); }
typedef void (*fp_t)(float, unsigned int *);

static int test_fptr(void) {
    fp_t fp = fp_sink;
    unsigned int result = 0;
    fp(0.1f, &result);
    printf("[5] fptr call fp(0.1f): bits=0x%08x  ", result);
    if (result == 0x3dcccccd) { printf("PASS\n"); return 1; }
    printf("FAIL (want 0x3dcccccd)\n"); return 0;
}

/* ---- [6] Bonus: compound assign with double on right ---- */
static int test_compound_mixed(void) {
    float x = 1.0f;
    double d = 0.5;
    x += (float)d;
    unsigned int b = fbits(x);
    printf("[6] float x=1.0f; x+=(float)0.5: bits=0x%08x  ", b);
    /* 1.5f = 0x3fc00000 */
    if (b == 0x3fc00000) { printf("PASS\n"); return 1; }
    printf("FAIL (want 0x3fc00000)\n"); return 0;
}

int main(void) {
    int pass = 0;
    int total = 6;
    pass += test_compound_assign();
    pass += test_unary_minus();
    pass += test_float_cmp();
    pass += test_float_array();
    pass += test_fptr();
    pass += test_compound_mixed();
    printf("\n=== SURFACE COVERAGE: %d/%d passed ===\n", pass, total);
    return (pass == total) ? 0 : 1;
}
