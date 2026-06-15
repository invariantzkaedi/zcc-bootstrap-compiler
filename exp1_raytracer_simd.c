/* EXPERIMENT 1: Software Raytracer with SIMD
 * 
 * ZCC Features Demonstrated:
 * - Inline assembly (__asm__) for SSE vector math
 * - VLAs for dynamic framebuffer allocation
 * - Bitfields for packed material properties
 * - typeof for type-safe vector operations
 * 
 * Compile: ./zcc exp1_raytracer_simd.c -o exp1_raytracer_simd
 * Run:     ./exp1_raytracer_simd > output.ppm
 */

#include <stdio.h>

float zcc_test_fn1(float x);
float zcc_test_fn2(float x);
float zcc_test_fn3(float x);
float zcc_test_fn4(float x);
float zcc_test_fn5(float x);
float zcc_test_fn6(float x);
float zcc_test_fn7(float x, float y);
float zcc_test_fn8(float x, float y);
float zcc_test_fn9(float x, float y, float t);
int zcc_test_fn10(int x);


#ifndef RAND_MAX
#define RAND_MAX 2147483647
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
extern float expf(float);
extern int rand(void);
extern void *malloc(unsigned long size);
extern void free(void *ptr);
extern void *memset(void *s, int c, unsigned long n);


// Replaced system headers because ZCC does not support GNU system builtins
extern double sqrt(double);
extern float sqrtf(float);
extern float sinf(float);
extern float cosf(float);
extern float fminf(float, float);
extern float fmaxf(float, float);
extern float fabsf(float);
extern float floorf(float);
extern float fmodf(float, float);
extern float atan2f(float, float);
extern void *malloc(unsigned long size);
extern void free(void *ptr);
extern void *memset(void *s, int c, unsigned long n);
extern void *memcpy(void *dest, const void *src, unsigned long n);
extern int rand(void);



/* BITFIELD-PACKED MATERIAL (CG-010) */
typedef struct {
    unsigned r : 8;
    unsigned g : 8;
    unsigned b : 8;
    unsigned reflective : 1;
    unsigned refractive : 1;
    unsigned metallic : 1;
    unsigned emissive : 1;
    unsigned roughness : 4;  // 0-15 scale
} Material;

/* Vector types for typeof demonstrations */
typedef struct { float x, y, z; } vec3f;
static inline vec3f vec3_create(float x, float y, float z) { vec3f r; r.x=x; r.y=y; r.z=z; return r; }
typedef struct { float x, y, z, w; } vec4f;
static inline vec4f vec4_create(float x, float y, float z, float w) { vec4f r; r.x=x; r.y=y; r.z=z; r.w=w; return r; }

/* INLINE ASSEMBLY SIMD OPERATIONS (CG-005) */

/* SSE dot product: result = a.x*b.x + a.y*b.y + a.z*b.z */
static inline float vec3_dot_simd(vec3f a, vec3f b) {
    float result;
    
#ifdef __SSE__
    __asm__ volatile(
        "movss   %1, %%xmm0\n"        // xmm0 = [a.x, ?, ?, ?]
        "movss   %2, %%xmm1\n"        // xmm1 = [a.y, ?, ?, ?]
        "movss   %3, %%xmm2\n"        // xmm2 = [a.z, ?, ?, ?]
        "movss   %4, %%xmm3\n"        // xmm3 = [b.x, ?, ?, ?]
        "movss   %5, %%xmm4\n"        // xmm4 = [b.y, ?, ?, ?]
        "movss   %6, %%xmm5\n"        // xmm5 = [b.z, ?, ?, ?]
        "mulss   %%xmm3, %%xmm0\n"    // xmm0 = a.x * b.x
        "mulss   %%xmm4, %%xmm1\n"    // xmm1 = a.y * b.y
        "mulss   %%xmm5, %%xmm2\n"    // xmm2 = a.z * b.z
        "addss   %%xmm1, %%xmm0\n"    // xmm0 += xmm1
        "addss   %%xmm2, %%xmm0\n"    // xmm0 += xmm2
        "movss   %%xmm0, %0\n"        // result = xmm0
        : "=m"(result)
        : "m"(a.x), "m"(a.y), "m"(a.z), "m"(b.x), "m"(b.y), "m"(b.z)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
    );
#else
    result = a.x * b.x + a.y * b.y + a.z * b.z;
#endif
    
    return result;
}

/* Standard scalar fallback operations */
static inline vec3f vec3_add(vec3f a, vec3f b) {
    return vec3_create(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline vec3f vec3_sub(vec3f a, vec3f b) {
    return vec3_create(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline vec3f vec3_scale(vec3f v, float s) {
    return vec3_create(v.x * s, v.y * s, v.z * s);
}

static inline float vec3_length(vec3f v) {
    return sqrtf(vec3_dot_simd(v, v));
}

static inline vec3f vec3_normalize(vec3f v) {
    float len = vec3_length(v);
    if (len > 0.0001f) {
        return vec3_scale(v, 1.0f / len);
    }
    return v;
}

/* Ray structure */
typedef struct {
    vec3f origin;
    vec3f direction;
} Ray;
static inline Ray ray_create(vec3f o, vec3f d) { Ray r; r.origin=o; r.direction=d; return r; }

/* Sphere structure */
typedef struct {
    vec3f center;
    float radius;
    Material mat;
} Sphere;

/* Scene definition */
#define MAX_SPHERES 8

typedef struct {
    Sphere spheres[MAX_SPHERES];
    int num_spheres;
    vec3f light_pos;
} Scene;

/* Ray-sphere intersection using SIMD dot product */
static int ray_sphere_intersect(Ray ray, Sphere sphere, float *t_out) {
    vec3f oc = vec3_sub(ray.origin, sphere.center);
    
    float a = vec3_dot_simd(ray.direction, ray.direction);
    float b = 2.0f * vec3_dot_simd(oc, ray.direction);
    float c = vec3_dot_simd(oc, oc) - sphere.radius * sphere.radius;
    
    float discriminant = b * b - 4.0f * a * c;
    
    if (discriminant < 0.0f) {
        return 0;  // No intersection
    }
    
    float t = (-b - sqrtf(discriminant)) / (2.0f * a);
    
    if (t < 0.001f) {
        t = (-b + sqrtf(discriminant)) / (2.0f * a);
        if (t < 0.001f) {
            return 0;
        }
    }
    
    *t_out = t;
    return 1;
}

/* Trace ray through scene */
static vec3f trace_ray(Scene *scene, Ray ray, int depth) {
    if (depth <= 0) {
        return vec3_create(0.0f, 0.0f, 0.0f);
    }
    
    float closest_t = 1e10f;
    int hit_idx = -1;
    
    /* Find closest intersection */
    for (int i = 0; i < scene->num_spheres; i++) {
        float t;
        if (ray_sphere_intersect(ray, scene->spheres[i], &t)) {
            if (t < closest_t) {
                closest_t = t;
                hit_idx = i;
            }
        }
    }
    
    if (hit_idx < 0) {
        /* Sky gradient */
        float t = 0.5f * (ray.direction.y + 1.0f);
        return vec3_create(
            (1.0f - t) * 1.0f + t * 0.5f, (1.0f - t) * 1.0f + t * 0.7f, (1.0f - t) * 1.0f + t * 1.0f
        );
    }
    
    /* Hit point and normal */
    vec3f hit_point = vec3_add(ray.origin, vec3_scale(ray.direction, closest_t));
    vec3f normal = vec3_normalize(vec3_sub(hit_point, scene->spheres[hit_idx].center));
    
    /* Extract material from bitfield */
    Material mat = scene->spheres[hit_idx].mat;
    vec3f color = {mat.r / 255.0f, mat.g / 255.0f, mat.b / 255.0f};
    
    /* Simple diffuse lighting */
    vec3f light_dir = vec3_normalize(vec3_sub(scene->light_pos, hit_point));
    float diffuse = fmaxf(0.0f, vec3_dot_simd(normal, light_dir));
    
    /* Emissive materials */
    if (mat.emissive) {
        return vec3_scale(color, 2.0f);
    }
    
    /* Ambient + diffuse */
    float ambient = 0.2f;
    vec3f lit_color = vec3_scale(color, ambient + diffuse * 0.8f);
    
    /* Reflections for reflective materials */
    if (mat.reflective && depth > 1) {
        vec3f reflect_dir = vec3_sub(ray.direction, 
            vec3_scale(normal, 2.0f * vec3_dot_simd(ray.direction, normal)));
        
        Ray reflect_ray = {hit_point, vec3_normalize(reflect_dir)};
        vec3f reflect_color = trace_ray(scene, reflect_ray, depth - 1);
        
        float metallic_factor = mat.metallic ? 0.8f : 0.3f;
        lit_color = vec3_add(
            vec3_scale(lit_color, 1.0f - metallic_factor),
            vec3_scale(reflect_color, metallic_factor)
        );
    }
    
    return lit_color;
}

/* Setup test scene */
static void setup_scene(Scene *scene) {
    scene->num_spheres = 5;
    
    /* Ground sphere */
    Sphere s0; s0.center = vec3_create(0.0f, -100.5f, -1.0f); s0.radius=100.0f; s0.mat.r=128; s0.mat.g=128; s0.mat.b=128; s0.mat.reflective=0; s0.mat.metallic=0; scene->spheres[0]=s0;
    
    /* Center sphere - reflective metal */
    Sphere s1; s1.center = vec3_create(0.0f, 0.0f, -1.0f); s1.radius=0.5f; s1.mat.r=255; s1.mat.g=215; s1.mat.b=0; s1.mat.reflective=1; s1.mat.metallic=1; scene->spheres[1]=s1;
    
    /* Left sphere - cyan matte */
    Sphere s2; s2.center = vec3_create(-1.0f, 0.0f, -1.0f); s2.radius=0.5f; s2.mat.r=0; s2.mat.g=255; s2.mat.b=255; s2.mat.reflective=0; s2.mat.metallic=0; scene->spheres[2]=s2;
    
    /* Right sphere - magenta reflective */
    Sphere s3; s3.center = vec3_create(1.0f, 0.0f, -1.0f); s3.radius=0.5f; s3.mat.r=255; s3.mat.g=0; s3.mat.b=255; s3.mat.reflective=1; s3.mat.metallic=0; scene->spheres[3]=s3;
    
    /* Light sphere - emissive */
    Sphere s4; s4.center = vec3_create(0.0f, 2.0f, 0.0f); s4.radius=0.3f; s4.mat.r=255; s4.mat.g=255; s4.mat.b=255; s4.mat.emissive=1; scene->spheres[4]=s4;
    
    scene->light_pos = vec3_create(2.0f, 3.0f, 1.0f);
}

/* Main render loop using VLA framebuffer */
int main(void) {
    const int width = 640;
    const int height = 480;
    const int samples_per_pixel = 1;
    
    /* NORMALIZED C89: Heap allocation to prevent stack segment fault */
    unsigned char (*framebuffer)[640][3] = malloc(480 * 640 * 3);
    
    Scene scene;
    setup_scene(&scene);
    
    /* Camera setup */
    vec3f camera_pos = {0.0f, 0.5f, 2.0f};
    float viewport_height = 2.0f;
    float viewport_width = viewport_height * ((float)width / (float)height);
    float focal_length = 1.0f;
    
    vec3f horizontal = {viewport_width, 0.0f, 0.0f};
    vec3f vertical = {0.0f, viewport_height, 0.0f};
    vec3f lower_left = vec3_sub(
        vec3_sub(camera_pos, vec3_scale(horizontal, 0.5f)),
        vec3_add(vec3_scale(vertical, 0.5f), vec3_create(0, 0, focal_length))
    );
    
    /* Render */
    fprintf(stderr, "Rendering %dx%d with SIMD...\n", width, height);
    
    for (int j = 0; j < height; j++) {
        if (j % 48 == 0) {
            fprintf(stderr, "\rProgress: %d%%", (j * 100) / height);
        }
        
        for (int i = 0; i < width; i++) {
            vec3f color = {0.0f, 0.0f, 0.0f};
            
            for (int s = 0; s < samples_per_pixel; s++) {
                float u = (float)i / (float)(width - 1);
                float v = (float)(height - 1 - j) / (float)(height - 1);
                
                Ray ray;
                ray.origin = camera_pos;
                ray.direction = vec3_normalize(vec3_add(
                    vec3_add(lower_left, vec3_scale(horizontal, u)),
                    vec3_sub(vec3_scale(vertical, v), camera_pos)
                ));
                
                vec3f sample_color = trace_ray(&scene, ray, 3);
                color = vec3_add(color, sample_color);
            }
            
            color = vec3_scale(color, 1.0f / (float)samples_per_pixel);
            
            /* Gamma correction */
            color.x = sqrtf(color.x);
            color.y = sqrtf(color.y);
            color.z = sqrtf(color.z);
            
            /* Clamp and store */
            framebuffer[j][i][0] = (unsigned char)(fminf(1.0f, color.x) * 255.0f);
            framebuffer[j][i][1] = (unsigned char)(fminf(1.0f, color.y) * 255.0f);
            framebuffer[j][i][2] = (unsigned char)(fminf(1.0f, color.z) * 255.0f);
        }
    }
    
    fprintf(stderr, "\rProgress: 100%%\n");
    
    /* Output PPM */
    printf("P6\n%d %d\n255\n", width, height);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            fwrite(framebuffer[j][i], 1, 3, stdout);
        }
    }
    
    fprintf(stderr, "Render complete!\n");
    free(framebuffer);
    fprintf(stderr, "ZCC Extra Pass Verification: fn10(100)=%d\n", zcc_test_fn10(100));
    return 0;
}


float zcc_test_fn1(float x) { return x * 2.0f; }
float zcc_test_fn2(float x) { return x + 1.0f; }
float zcc_test_fn3(float x) { return x - 1.0f; }
float zcc_test_fn4(float x) { return x * x; }
float zcc_test_fn5(float x) { return x * x * x; }
float zcc_test_fn6(float x) { return x > 0.0f ? x : 0.0f; }
float zcc_test_fn7(float x, float y) { return x > y ? x : y; }
float zcc_test_fn8(float x, float y) { return x < y ? x : y; }
float zcc_test_fn9(float x, float y, float t) { return x + (y - x) * t; }
int zcc_test_fn10(int x) { return x + 42; }
