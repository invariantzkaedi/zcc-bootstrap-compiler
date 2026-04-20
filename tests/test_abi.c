#include <stdio.h>
typedef struct { double x, y, z; } Vec3;
Vec3 make_vec3(double x, double y, double z) {
    Vec3 v; v.x = x; v.y = y; v.z = z; return v;
}
Vec3 add(Vec3 a, Vec3 b) {
    return make_vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}
int main() {
    Vec3 a = make_vec3(1.1, 2.2, 3.3);
    Vec3 b = make_vec3(4.4, 5.5, 6.6);
    Vec3 c = add(a, b);
    printf("%f %f %f\n", c.x, c.y, c.z);
    return 0;
}
