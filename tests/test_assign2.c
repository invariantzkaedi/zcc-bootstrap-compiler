#include <stdio.h>
typedef struct { double m[16]; } Mat4;
int main() {
    Mat4 a, b; int i;
    for(i=0; i<16; i++) a.m[i] = i * 1.5;
    b = a;
    printf("%f %f %f\n", b.m[0], b.m[10], b.m[15]);
    return 0;
}
