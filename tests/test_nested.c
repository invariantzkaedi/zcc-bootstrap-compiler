#include <stdio.h>
typedef struct { double x, y, z; } Vec3;
Vec3 make_vec3(double x, double y, double z) {
    Vec3 r; r.x = x; r.y = y; r.z = z; return r;
}
Vec3 mul(Vec3 a, double d) { return make_vec3(a.x*d, a.y*d, a.z*d); }
Vec3 add(Vec3 a, Vec3 b) { return make_vec3(a.x+b.x, a.y+b.y, a.z+b.z); }
int main() {
    double a = 0.5;
    Vec3 col = add(mul(make_vec3(1.0, 1.0, 1.0), 1.0 - a), mul(make_vec3(0.5, 0.7, 1.0), a));
    printf("%.2f %.2f %.2f\n", col.x, col.y, col.z);
    return 0;
}