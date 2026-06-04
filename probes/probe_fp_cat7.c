#include <stdio.h>

struct Mixed {
    int i1;
    float f1;
    int i2;
    double d1;
};

int main(void) {
    /* float array read/write */
    float f_arr[5] = { 1.1f, 2.2f, 3.3f, 4.4f, 5.5f };
    printf("f_arr before: %f, %f, %f\n", f_arr[0], f_arr[2], f_arr[4]);
    f_arr[0] = -1.1f;
    f_arr[2] = -3.3f;
    f_arr[4] = -5.5f;
    printf("f_arr after: %f, %f, %f\n", f_arr[0], f_arr[2], f_arr[4]);

    /* double array read/write */
    double d_arr[5] = { 10.1, 20.2, 30.3, 40.4, 50.5 };
    printf("d_arr before: %f, %f, %f\n", d_arr[0], d_arr[2], d_arr[4]);
    d_arr[0] = -10.1;
    d_arr[2] = -30.3;
    d_arr[4] = -50.5;
    printf("d_arr after: %f, %f, %f\n", d_arr[0], d_arr[2], d_arr[4]);

    /* Struct with mixed int/float members */
    struct Mixed m = { 10, 3.14f, 20, 2.71828 };
    printf("m.i1: %d\n", m.i1);
    printf("m.f1: %f\n", m.f1);
    printf("m.i2: %d\n", m.i2);
    printf("m.d1: %f\n", m.d1);

    return 0;
}
