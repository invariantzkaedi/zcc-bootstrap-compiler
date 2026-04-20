#include <stdio.h>
int main() {
    double arr[2] = {10.0, 20.0};
    double v = 5.0;
    arr[0] += v;
    arr[1] += v;
    printf("arr[0]=%.1f arr[1]=%.1f\n", arr[0], arr[1]);
    // Expected: arr[0]=15.0 arr[1]=25.0

    double *p = arr;
    p[0] += 1.0;
    p[1] += 1.0;
    printf("p[0]=%.1f p[1]=%.1f\n", p[0], p[1]);
    // Expected: p[0]=16.0 p[1]=26.0
    return 0;
}
