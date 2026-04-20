#include <stdio.h>
typedef struct { double m[16]; } Mat4;
int main() {
    Mat4 a; 
    int i;
    for(i=0; i<16; i++) a.m[i] = 0.0;
    a.m[0] = 1.0; a.m[3] = 1.5;
    printf("%f %f\n", a.m[0], a.m[3]);
    return 0;
}
