#include <stdio.h>
#include <stdlib.h>
#include "character_engine.c"

int main() {
    int w = 1920, h = 1080;
    int j = 0, i = 0; // The very first pixel
    
    double pi = 3.14159265358979323846;
    double fov_scale = tan(pi / 4.0);
    double u = (2.0 * ((double)i + 0.5) / w - 1.0) * ((double)w / h) * fov_scale;
    double v = (1.0 - 2.0 * ((double)j + 0.5) / h) * fov_scale;
    
    Vec3 cam_pos, cam_dir, cam_right, cam_up, light_dir;
    cam_pos.x = 0.0; cam_pos.y = 1.0; cam_pos.z = 4.0;
    cam_dir.x = 0.0; cam_dir.y = 0.0; cam_dir.z = -1.0;
    cam_up.x = 0.0; cam_up.y = 1.0; cam_up.z = 0.0;
    p_cross(&cam_right, &cam_dir, &cam_up);
    p_normalize(&cam_right, &cam_right);
    light_dir.x = 1.0; light_dir.y = 1.0; light_dir.z = 1.0;
    p_normalize(&light_dir, &light_dir);
    
    init_skeleton();
    
    Ray ray;
    ray.origin = cam_pos;
    ray.dir.x = cam_dir.x + cam_right.x * u + cam_up.x * v;
    ray.dir.y = cam_dir.y + cam_right.y * u + cam_up.y * v;
    ray.dir.z = cam_dir.z + cam_right.z * u + cam_up.z * v;
    p_normalize(&ray.dir, &ray.dir);
    
    double best_t = 1000000000.0;
    int hit_bone = -1;
    Vec3 hit_normal;
    hit_normal.x = 0.0; hit_normal.y = 0.0; hit_normal.z = 0.0;
    
    if (is_negative(ray.dir.y)) {
        double t_g = (-0.5 - ray.origin.y) / ray.dir.y;
        if (t_g > 0.001) {
            best_t = t_g;
            hit_bone = -2;
            hit_normal.x = 0.0; hit_normal.y = 1.0; hit_normal.z = 0.0;
        }
    }
    
    int b;
    for (b = 0; b < num_bones; b++) {
        Ray local_ray;
        double t = -1.0;
        Bone *b_ptr = bones + b;
        
        if (b_ptr->length == 0.0 && b_ptr->is_sphere == 0) continue;
        
        p_mat4_transform_point(&local_ray.origin, &b_ptr->inv_world_transform, &ray.origin);
        p_mat4_transform_dir(&local_ray.dir, &b_ptr->inv_world_transform, &ray.dir);
        
        if (b_ptr->is_sphere) {
            t = intersect_sphere(&local_ray, 0.15);
        } else {
            t = intersect_cylinder(&local_ray, 0.12, b_ptr->length);
        }
        
        if (t > 0.0 && t < best_t) {
            best_t = t;
            hit_bone = b;
            if (b_ptr->is_sphere) {
                Vec3 hp;
                hp.x = local_ray.origin.x + local_ray.dir.x * t;
                hp.y = local_ray.origin.y + local_ray.dir.y * t;
                hp.z = local_ray.origin.z + local_ray.dir.z * t;
                p_sphere_normal(&hit_normal, &hp);
            } else {
                Vec3 hp;
                hp.x = local_ray.origin.x + local_ray.dir.x * t;
                hp.y = local_ray.origin.y + local_ray.dir.y * t;
                hp.z = local_ray.origin.z + local_ray.dir.z * t;
                p_cylinder_normal(&hit_normal, &hp, b_ptr->length);
            }
            p_mat4_transform_dir(&hit_normal, &b_ptr->world_transform, &hit_normal);
            p_normalize(&hit_normal, &hit_normal);
        }
    }

    printf("hit_bone=%d best_t=%f normal=(%f, %f, %f)\n", hit_bone, best_t, hit_normal.x, hit_normal.y, hit_normal.z);

    return 0;
}
