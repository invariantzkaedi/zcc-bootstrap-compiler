#include <stdio.h>
typedef struct { double x, y, z; } Vec3;
int main() {
    Vec3 v = (Vec3){1.0, 2.0, 3.0};
    printf("%.1f %.1f %.1f\n", v.x, v.y, v.z);
    double d = (double){3.14};
    printf("%.2f\n", d);
    return 0;
}
