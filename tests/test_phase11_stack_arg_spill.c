#include <stdio.h>

long f(long a,long b,long c,long d,long e,long f,long g,long h,long i,long j) {
    return a + b*2 + c*3 + d*4 + e*5 + f*6 + g*7 + h*8 + i*9 + j*10;
}

int main(void) {
    printf("out=%ld\n", f(1,2,3,4,5,6,7,8,9,10));
    return 0;
}
