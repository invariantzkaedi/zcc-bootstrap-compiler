#include <stdio.h>
int main() {
    double a = 3.14;
    double b = 2.0;
    double c = a * b;
    printf("a=%.2f b=%.2f c=%.2f\n", a, b, c);

    double arr[2] = {10.5, 20.5};
    printf("arr[0]=%.1f arr[1]=%.1f\n", arr[0], arr[1]);

    // Test memcpy double through pointer
    double *p = &a;
    printf("*p=%.2f\n", *p);

    // Test int-to-double cast
    int x = 314;
    double d = (double)x;
    printf("(double)314=%.1f\n", d);

    // Test double multiply
    double e = d * 0.01;
    printf("314*0.01=%.2f\n", e);
    return 0;
}
