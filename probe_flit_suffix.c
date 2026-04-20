#include <stdio.h>
int main(void) {
    float  f_suffix    = 0.1f;
    float  f_no_suffix = 0.1;
    double d           = 0.1;

    unsigned int *bf1 = (unsigned int *)&f_suffix;
    unsigned int *bf2 = (unsigned int *)&f_no_suffix;
    unsigned long long *bd = (unsigned long long *)&d;

    printf("float 0.1f    bits = 0x%08x  (want 0x3dcccccd)\n", *bf1);
    printf("float 0.1     bits = 0x%08x  (want 0x3dcccccd, via narrowing)\n", *bf2);
    printf("double 0.1    bits = 0x%016llx  (want 0x3fb999999999999a)\n", *bd);

    double promoted = (double)0.1f;
    unsigned long long *bp = (unsigned long long *)&promoted;
    printf("(double)0.1f  bits = 0x%016llx  (want 0x3fb99999a0000000 - float precision)\n", *bp);
    return 0;
}
