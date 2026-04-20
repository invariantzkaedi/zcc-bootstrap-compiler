#include <stdio.h>
typedef unsigned long long u64;

int main() {
    u64 s = 314;
    double r = (double)s;
    printf("s=%llu, r=%.6f\n", s, r);
    return 0;
}
