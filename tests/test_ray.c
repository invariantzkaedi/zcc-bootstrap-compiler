#include <stdio.h>
#include <stdlib.h>
double sin(double);
double cos(double);
double acos(double);
double sqrt(double);
double fabs(double);
double tan(double);

int is_negative(double x) {
    unsigned char *p = (unsigned char*)&x;
    return (p[7] & 0x80) != 0;
}

int main() {
    int w = 1920, h = 1080;
    int j = 0, i = 1; // pixel (1, 0)
    double fov_scale = tan(3.14159265358979323846 / 4.0);
    double u = (2.0 * ((double)i + 0.5) / w - 1.0) * ((double)w / h) * fov_scale;
    double v = (1.0 - 2.0 * ((double)j + 0.5) / h) * fov_scale;

    printf("u = %f, v = %f\n", u, v);
    printf("is_negative(v) = %d\n", is_negative(v));
    return 0;
}
