/* 
 * fhn_gen.c — Geometric generator for ZKAEDI PRIME FHN attractor 
 * Outputs a C header with Triangle mesh_tris_attractor[]
 */
#include <stdio.h>
#include <math.h>

typedef struct { double x, y, z; } Vec3;

void v_add(Vec3 a, Vec3 b, Vec3 *o) { o->x=a.x+b.x; o->y=a.y+b.y; o->z=a.z+b.z; }
void v_sub(Vec3 a, Vec3 b, Vec3 *o) { o->x=a.x-b.x; o->y=a.y-b.y; o->z=a.z-b.z; }
void v_mul(Vec3 a, double d, Vec3 *o) { o->x=a.x*d; o->y=a.y*d; o->z=a.z*d; }
double v_dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
void v_norm(Vec3 *v) {
    double l = sqrt(v->x*v->x + v->y*v->y + v->z*v->z);
    if(l>0) { v->x/=l; v->y/=l; v->z/=l; }
}
void v_cross(Vec3 a, Vec3 b, Vec3 *o) {
    o->x = a.y*b.z - a.z*b.y;
    o->y = a.z*b.x - a.x*b.z;
    o->z = a.x*b.y - a.y*b.x;
}

int main() {
    double h = 0.1, v = 0.1;
    double a = 0.7, b = 0.8, eps = 0.08, dt = 0.1;
    int steps = 10000;
    
    printf("#ifndef ATTRACTOR_H\n#define ATTRACTOR_H\n\n");
    printf("Triangle attractor_tris[] = {\n");

    Vec3 prev_p = {0,0,0};
    Vec3 prev_binorm = {0, 1, 0};

    for(int i=0; i<steps; i++) {
        double dh = h - (h*h*h)/3.0 - v;
        double dv = eps * (h + a - b*v);
        h += dh * dt;
        v += dv * dt;

        /* Map (h, v, t) to (x, y, z) */
        Vec3 curr_p = { h * 2.0, v * 2.0, (i - steps/2.0) * 0.01 };
        
        if(i > 0) {
            Vec3 dir; v_sub(curr_p, prev_p, &dir); v_norm(&dir);
            Vec3 side; v_cross(dir, prev_binorm, &side); v_norm(&side);
            v_cross(side, dir, &prev_binorm); // Update binormal for continuity
            
            double width = 0.05;
            Vec3 p0, p1, p2, p3;
            Vec3 off; v_mul(side, width, &off);
            
            v_add(prev_p, off, &p0);
            v_sub(prev_p, off, &p1);
            v_add(curr_p, off, &p2);
            v_sub(curr_p, off, &p3);

            /* Two triangles for the ribbon segment */
            /* Tri 1: Cyan-ish */
            printf("  { {%f, %f, %f}, {%f, %f, %f}, {%f, %f, %f}, {0.1, 0.8, 0.9}, 0.5, 50.0 },\n",
                    p0.x, p0.y, p0.z, p1.x, p1.y, p1.z, p2.x, p2.y, p2.z);
            /* Tri 2: Magenta-ish */
            printf("  { {%f, %f, %f}, {%f, %f, %f}, {%f, %f, %f}, {0.9, 0.1, 0.8}, 0.5, 50.0 },\n",
                    p1.x, p1.y, p1.z, p3.x, p3.y, p3.z, p2.x, p2.y, p2.z);
        }
        prev_p = curr_p;
    }

    printf("};\n\n");
    printf("#define ATTRACTOR_TRI_COUNT %d\n", (steps-1)*2);
    printf("#endif\n");

    return 0;
}
