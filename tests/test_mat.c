#include <stdio.h>
typedef struct { double m[16]; } Mat4;

Mat4 mat4_identity(void) {
    Mat4 r; int i;
    for(i=0; i<16; i++) r.m[i] = 0.0;
    r.m[0] = 1.0; r.m[5] = 1.0; r.m[10] = 1.0; r.m[15] = 1.0;
    return r;
}

Mat4 mat4_multiply(Mat4 a, Mat4 b) {
    Mat4 r; int i, j, k;
    for(i=0; i<4; i++) {
        for(j=0; j<4; j++) {
            double sum = 0.0;
            for(k=0; k<4; k++) {
                sum += a.m[i*4 + k] * b.m[k*4 + j];
            }
            r.m[i*4 + j] = sum;
        }
    }
    return r;
}

int main() {
    Mat4 a = mat4_identity();
    Mat4 b = mat4_identity();
    a.m[3] = 1.5; a.m[7] = 2.5; a.m[11] = 3.5;
    b.m[3] = 4.0; b.m[7] = 5.0; b.m[11] = 6.0;
    Mat4 c = mat4_multiply(a, b);
    printf("%f %f %f\n", c.m[3], c.m[7], c.m[11]);
    return 0;
}
