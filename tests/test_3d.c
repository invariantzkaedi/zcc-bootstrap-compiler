#include <stdio.h>
#include <math.h>

typedef struct { double x, y, z; } Vec3;

Vec3 make_vec3(double x, double y, double z) {
    Vec3 r;
    r.x = x; r.y = y; r.z = z;
    return r;
}

Vec3 cross(Vec3 a, Vec3 b) {
    return make_vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

double dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

double vlength(Vec3 v) { return sqrt(dot(v, v)); }

Vec3 normalize(Vec3 v) {
    double l = vlength(v);
    return make_vec3(v.x/l, v.y/l, v.z/l);
}

int main() {
    Vec3 a = make_vec3(1.0, 0.0, 0.0);
    Vec3 b = make_vec3(0.0, 1.0, 0.0);
    Vec3 c = cross(a, b);
    printf("cross: %.1f %.1f %.1f\n", c.x, c.y, c.z);
    printf("dot: %.1f\n", dot(a, b));
    printf("len: %.4f\n", vlength(make_vec3(1.0, 1.0, 1.0)));
    Vec3 n = normalize(make_vec3(3.0, 4.0, 0.0));
    printf("norm: %.2f %.2f %.2f\n", n.x, n.y, n.z);
    return 0;
}
