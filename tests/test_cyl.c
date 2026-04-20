#include <stdio.h>
#include <stdlib.h>
double sin(double); double cos(double); double tan(double); double sqrt(double); double acos(double); double fabs(double);
typedef struct { double x, y, z; } Vec3;
double p_dot(const Vec3 *a, const Vec3 *b) { return a->x * b->x + a->y * b->y + a->z * b->z; }
void p_normalize(Vec3 *r, const Vec3 *v) { double l = sqrt(p_dot(v, v)); if (l > 0.0) { r->x=v->x/l; r->y=v->y/l; r->z=v->z/l; } else { r->x=0.0; r->y=0.0; r->z=0.0; } }
void p_cross(Vec3 *r, const Vec3 *a, const Vec3 *b) { r->x = a->y*b->z - a->z*b->y; r->y = a->z*b->x - a->x*b->z; r->z = a->x*b->y - a->y*b->x; }
typedef struct { Vec3 origin, dir; } Ray;

double intersect_cylinder(const Ray* ray, double radius, double length) {
    double a = ray->dir.x * ray->dir.x + ray->dir.z * ray->dir.z;
    double b = 2.0 * (ray->origin.x * ray->dir.x + ray->origin.z * ray->dir.z);
    double c = ray->origin.x * ray->origin.x + ray->origin.z * ray->origin.z - radius * radius;
    double desc = b * b - 4.0 * a * c;
    double t_best = 1000000000.0;
    
    printf("Ray: o=(%f,%f,%f) d=(%f,%f,%f)\n", ray->origin.x, ray->origin.y, ray->origin.z, ray->dir.x, ray->dir.y, ray->dir.z);
    printf("a=%f b=%f c=%f desc=%f\n", a, b, c, desc);

    if (fabs(a) > 0.000001 && desc >= -0.000001) {
        double d;
        if (desc < 0.0) desc = 0.0;
        d = sqrt(desc);
        {
            double t1 = (-b - d) / (2.0 * a);
            double t2 = (-b + d) / (2.0 * a);
            printf("t1=%f t2=%f\n", t1, t2);
            if (t1 > 0.001) {
                double y1 = ray->origin.y + t1 * ray->dir.y;
                if (y1 > 0.0 && y1 < length) t_best = t1;
            }
            if (t_best == 1000000000.0 && t2 > 0.001) {
                double y2 = ray->origin.y + t2 * ray->dir.y;
                if (y2 > 0.0 && y2 < length) t_best = t2;
            }
        }
    }
    printf("t_best=%f\n", t_best);
    return t_best == 1000000000.0 ? -1.0 : t_best;
}

int main() {
    int w = 1920, h = 1080;
    int i = 36, j = 0; /* from line 110 of ppm */
    double fov_scale = tan(3.14159265358979323846 / 4.0);
    double u = (2.0 * (i + 0.5) / w - 1.0) * ((double)w / h) * fov_scale;
    double v = (1.0 - 2.0 * (j + 0.5) / h) * fov_scale;
    
    Vec3 cam_pos, cam_dir, cam_right, cam_up;
    cam_pos.x = 0.0; cam_pos.y = 1.0; cam_pos.z = 4.0;
    cam_dir.x = 0.0; cam_dir.y = 0.0; cam_dir.z = -1.0;
    cam_up.x = 0.0; cam_up.y = 1.0; cam_up.z = 0.0;
    p_cross(&cam_right, &cam_dir, &cam_up);
    p_normalize(&cam_right, &cam_right);
    
    Ray ray;
    ray.origin = cam_pos;
    ray.dir.x = cam_dir.x + cam_right.x * u + cam_up.x * v;
    ray.dir.y = cam_dir.y + cam_right.y * u + cam_up.y * v;
    ray.dir.z = cam_dir.z + cam_right.z * u + cam_up.z * v;
    p_normalize(&ray.dir, &ray.dir);
    
    /* Spine uses identity matrices except it translates by 1 on y from hips */
    /* So its world transform is identity + translated up by 1.0 */
    /* Wait, hips is translated by 1.0, spine translated by 0.5 from hips => 1.5 total! */
    Ray local_ray;
    /* inv_world for spine: translate by -1.5 on Y */
    local_ray.origin = ray.origin;
    local_ray.origin.y -= 1.5; 
    local_ray.dir = ray.dir; /* no rotation */
    
    intersect_cylinder(&local_ray, 0.12, 0.5); /* spine length is 0.5, radius 0.12 */
    
    return 0;
}
