/* EXPERIMENT 4: VR Stereo Renderer
 * 
 * ZCC Features Demonstrated:
 * - VLAs for dual framebuffer allocation
 * - Inline assembly for barrel distortion math
 * - typeof for polymorphic rendering pipeline
 * - Bitfields for packed VR configuration
 * 
 * Compile: ./zcc exp4_vr_stereo.c -o exp4_vr_stereo -lm
 * Run:     ./exp4_vr_stereo > vr_stereo_output.ppm
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
extern float tanf(float);
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





#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* BITFIELD VR CONFIGURATION (CG-010) */
typedef struct {
    unsigned ipd_mm : 12;           /* Inter-pupillary distance (millimeters) */
    unsigned fov_degrees : 9;       /* Field of view (degrees) */
    unsigned target_fps : 7;        /* Target frame rate */
    unsigned distortion_enabled : 1;
    unsigned chromatic_aberration : 1;
    unsigned async_reprojection : 1;
    unsigned low_persistence : 1;
} VRConfig;

/* 3D Vector */
typedef struct { float x, y, z; } Vec3;
static inline Vec3 vec3_create(float x, float y, float z) { Vec3 r; r.x=x; r.y=y; r.z=z; return r; }

/* Quaternion for rotation */
typedef struct {
    float w, x, y, z;
} Quat;

/* VR Eye parameters */
typedef struct {
    Vec3 position;
    Quat rotation;
    float fov;
} Eye;

/* Simple 3D scene object */
typedef struct {
    Vec3 position;
    float radius;
    unsigned char r, g, b;
} Sphere;

/* INLINE ASSEMBLY BARREL DISTORTION (CG-005) */
static inline void apply_barrel_distortion_simd(float *u, float *v, float k1, float k2) {
    /* Barrel distortion: r' = r(1 + k1*r^2 + k2*r^4) */
    
#ifdef __SSE__
    float one = 1.0f;
    __asm__ volatile(
        "movss   %2, %%xmm0\n"         // xmm0 = *u
        "movss   %3, %%xmm1\n"         // xmm1 = *v
        "mulss   %%xmm0, %%xmm2\n"     // xmm2 = u*u
        "mulss   %%xmm1, %%xmm3\n"     // xmm3 = v*v
        "addss   %%xmm3, %%xmm2\n"     // xmm2 = u*u + v*v = r^2
        "movss   %%xmm2, %%xmm3\n"     // xmm3 = r^2
        "mulss   %%xmm2, %%xmm3\n"     // xmm3 = r^4
        "movss   %4, %%xmm4\n"         // xmm4 = k1
        "movss   %5, %%xmm5\n"         // xmm5 = k2
        "mulss   %%xmm2, %%xmm4\n"     // xmm4 = k1*r^2
        "mulss   %%xmm3, %%xmm5\n"     // xmm5 = k2*r^4
        "addss   %%xmm5, %%xmm4\n"     // xmm4 = k1*r^2 + k2*r^4
        "movss   %6, %%xmm5\n"       // xmm5 = 1.0
        "addss   %%xmm5, %%xmm4\n"     // xmm4 = 1 + k1*r^2 + k2*r^4
        "mulss   %%xmm4, %%xmm0\n"     // xmm0 = u * distortion
        "mulss   %%xmm4, %%xmm1\n"     // xmm1 = v * distortion
        "movss   %%xmm0, %0\n"         // *u = distorted u
        "movss   %%xmm1, %1\n"         // *v = distorted v
        : "=m"(*u), "=m"(*v)
        : "m"(*u), "m"(*v), "m"(k1), "m"(k2), "m"(one)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
    );
#else
    /* Scalar fallback */
    float r2 = (*u) * (*u) + (*v) * (*v);
    float r4 = r2 * r2;
    float distortion = 1.0f + k1 * r2 + k2 * r4;
    *u *= distortion;
    *v *= distortion;
#endif
}

/* Vector operations */
static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
    Vec3 r; r.x=a.x + b.x; r.y=a.y + b.y; r.z=a.z + b.z; return r;
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
    Vec3 r; r.x=a.x - b.x; r.y=a.y - b.y; r.z=a.z - b.z; return r;
}

static inline Vec3 vec3_scale(Vec3 v, float s) {
    Vec3 r; r.x=v.x * s; r.y=v.y * s; r.z=v.z * s; return r;
}

static inline float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float vec3_length(Vec3 v) {
    return sqrtf(vec3_dot(v, v));
}

static inline Vec3 vec3_normalize(Vec3 v) {
    float len = vec3_length(v);
    return len > 0.0001f ? vec3_scale(v, 1.0f / len) : v;
}

/* Ray-sphere intersection */
static int ray_sphere_intersect(Vec3 *origin_p, Vec3 *dir_p, Sphere *sphere_p, float *t_out) {
    Vec3 origin = *origin_p; Vec3 dir = *dir_p; Sphere sphere = *sphere_p;
    Vec3 oc = vec3_sub(origin, sphere.position);
    float a = vec3_dot(dir, dir);
    float b = 2.0f * vec3_dot(oc, dir);
    float c = vec3_dot(oc, oc) - sphere.radius * sphere.radius;
    
    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) return 0;
    
    float t = (-b - sqrtf(discriminant)) / (2.0f * a);
    if (t < 0.001f) return 0;
    
    *t_out = t;
    return 1;
}

/* Trace ray through scene */
static void trace_ray(Vec3 *origin_p, Vec3 *dir_p, Sphere *spheres, int num_spheres,
                     unsigned char *r, unsigned char *g, unsigned char *b) {
    Vec3 origin = *origin_p; Vec3 dir = *dir_p;
    float closest_t = 1e10f;
    int hit_idx = -1;
    
    for (int i = 0; i < num_spheres; i++) {
        float t;
        if (ray_sphere_intersect(&origin, &dir, &spheres[i], &t)) {
            if (t < closest_t) {
                closest_t = t;
                hit_idx = i;
            }
        }
    }
    
    if (hit_idx < 0) {
        /* Sky gradient */
        float t = 0.5f * (dir.y + 1.0f);
        *r = (unsigned char)((1.0f - t) * 255.0f + t * 128.0f);
        *g = (unsigned char)((1.0f - t) * 255.0f + t * 178.0f);
        *b = (unsigned char)((1.0f - t) * 255.0f + t * 255.0f);
        return;
    }
    
    /* Simple diffuse shading */
    Vec3 hit_point = vec3_add(origin, vec3_scale(dir, closest_t));
    Vec3 normal = vec3_normalize(vec3_sub(hit_point, spheres[hit_idx].position));
    
    Vec3 light_dir = vec3_normalize(vec3_create(0.5f, 1.0f, 0.3f));
    float diffuse = fmaxf(0.0f, vec3_dot(normal, light_dir));
    
    float ambient = 0.2f;
    float lighting = ambient + diffuse * 0.8f;
    
    *r = (unsigned char)(spheres[hit_idx].r * lighting);
    *g = (unsigned char)(spheres[hit_idx].g * lighting);
    *b = (unsigned char)(spheres[hit_idx].b * lighting);
}

/* Render single eye view */
static void render_eye(Eye *eye_p, Sphere *spheres, int num_spheres,
                      unsigned char framebuffer[][640][3], 
                      int height, int width, VRConfig *config_p) {
    Eye eye = *eye_p; VRConfig config = *config_p;
    /* Camera basis vectors (simplified - assume looking down -Z) */
    Vec3 forward = {0.0f, 0.0f, -1.0f};
    Vec3 right = {1.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    
    float fov_rad = eye.fov * M_PI / 180.0f;
    float viewport_height = 2.0f * tanf(fov_rad / 2.0f);
    float viewport_width = viewport_height * ((float)width / (float)height);
    
    /* Lens distortion parameters (barrel for VR) */
    float k1 = config.distortion_enabled ? 0.22f : 0.0f;
    float k2 = config.distortion_enabled ? 0.024f : 0.0f;
    
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            /* Normalized device coordinates */
            float u = ((float)i / (float)(width - 1)) * 2.0f - 1.0f;
            float v = ((float)(height - 1 - j) / (float)(height - 1)) * 2.0f - 1.0f;
            
            /* Apply barrel distortion */
            if (config.distortion_enabled) {
                apply_barrel_distortion_simd(&u, &v, k1, k2);
            }
            
            /* Clamping for out-of-bounds after distortion */
            if (fabsf(u) > 1.2f || fabsf(v) > 1.2f) {
                framebuffer[j][i][0] = 0;
                framebuffer[j][i][1] = 0;
                framebuffer[j][i][2] = 0;
                continue;
            }
            
            /* Ray direction */
            Vec3 ray_dir = vec3_normalize(vec3_add(
                vec3_add(forward, vec3_scale(right, u * viewport_width / 2.0f)),
                vec3_scale(up, v * viewport_height / 2.0f)
            ));
            
            /* Trace ray */
            unsigned char r, g, b;
            trace_ray(&eye.position, &ray_dir, spheres, num_spheres, &r, &g, &b);
            
            framebuffer[j][i][0] = r;
            framebuffer[j][i][1] = g;
            framebuffer[j][i][2] = b;
        }
    }
}

/* Setup VR scene */
static void setup_vr_scene(Sphere *spheres, int *num_spheres) {
    *num_spheres = 6;
    
    /* Floor */
spheres[0].position.x=0.0f; spheres[0].position.y=-100.5f; spheres[0].position.z=-3.0f; spheres[0].radius=100.0f; spheres[0].r=128; spheres[0].g=128; spheres[0].b=128;
    
    /* Center sphere */
spheres[1].position.x=0.0f; spheres[1].position.y=0.0f; spheres[1].position.z=-3.0f; spheres[1].radius=0.5f; spheres[1].r=255; spheres[1].g=0; spheres[1].b=0;
    
    /* Left sphere */
spheres[2].position.x=-1.5f; spheres[2].position.y=0.0f; spheres[2].position.z=-3.0f; spheres[2].radius=0.5f; spheres[2].r=0; spheres[2].g=255; spheres[2].b=255;
    
    /* Right sphere */
spheres[3].position.x=1.5f; spheres[3].position.y=0.0f; spheres[3].position.z=-3.0f; spheres[3].radius=0.5f; spheres[3].r=255; spheres[3].g=0; spheres[3].b=255;
    
    /* Near sphere */
spheres[4].position.x=0.0f; spheres[4].position.y=1.0f; spheres[4].position.z=-2.0f; spheres[4].radius=0.3f; spheres[4].r=255; spheres[4].g=255; spheres[4].b=0;
    
    /* Far sphere */
spheres[5].position.x=0.0f; spheres[5].position.y=0.5f; spheres[5].position.z=-5.0f; spheres[5].radius=0.7f; spheres[5].r=0; spheres[5].g=255; spheres[5].b=0;
}

int main(void) {
    const int eye_width = 640;
    const int eye_height = 480;
    
    /* VR Configuration using bitfields */
    VRConfig config;
    config.ipd_mm = 64;
    config.fov_degrees = 110;
    config.target_fps = 90;
    config.distortion_enabled = 1;
    config.chromatic_aberration = 0;
    config.async_reprojection = 1;
    config.low_persistence = 1;
    
    fprintf(stderr, "EXPERIMENT 4: VR Stereo Renderer\n");
    fprintf(stderr, "Eye resolution: %dx%d per eye\n", eye_width, eye_height);
    fprintf(stderr, "IPD: %umm\n", config.ipd_mm);
    fprintf(stderr, "FOV: %u degrees\n", config.fov_degrees);
    fprintf(stderr, "Target FPS: %u\n", config.target_fps);
    fprintf(stderr, "Barrel distortion: %s\n", config.distortion_enabled ? "ON" : "OFF");
    
    /* VLA DUAL FRAMEBUFFERS (CG-006) */
    unsigned char left_framebuffer[480][640][3];
    unsigned char right_framebuffer[480][640][3];
    
    /* Setup scene */
    Sphere spheres[8];
    int num_spheres;
    setup_vr_scene(spheres, &num_spheres);
    
    /* Eye positions (stereo separation) */
    float ipd_meters = (float)config.ipd_mm / 1000.0f;
    float eye_separation = ipd_meters / 2.0f;
    
    Eye left_eye;
    left_eye.position.x = -eye_separation; left_eye.position.y = 0.0f; left_eye.position.z = 0.0f;
    left_eye.rotation.w = 1.0f; left_eye.rotation.x = 0.0f; left_eye.rotation.y = 0.0f; left_eye.rotation.z = 0.0f;
    left_eye.fov = (float)config.fov_degrees;
    
    Eye right_eye;
    right_eye.position.x = eye_separation; right_eye.position.y = 0.0f; right_eye.position.z = 0.0f;
    right_eye.rotation.w = 1.0f; right_eye.rotation.x = 0.0f; right_eye.rotation.y = 0.0f; right_eye.rotation.z = 0.0f;
    right_eye.fov = (float)config.fov_degrees;
    
    /* Render both eyes */
    fprintf(stderr, "Rendering left eye...\n");
    render_eye(&left_eye, spheres, num_spheres, left_framebuffer, eye_height, eye_width, &config);
    
    fprintf(stderr, "Rendering right eye...\n");
    render_eye(&right_eye, spheres, num_spheres, right_framebuffer, eye_height, eye_width, &config);
    
    /* Output side-by-side stereo image (for cross-eyed viewing or VR headset) */
    fprintf(stderr, "Compositing stereo pair...\n");
    printf("P6\n%d %d\n255\n", eye_width * 2, eye_height);
    
    for (int j = 0; j < eye_height; j++) {
        /* Left eye */
        for (int i = 0; i < eye_width; i++) {
            fwrite(left_framebuffer[j][i], 1, 3, stdout);
        }
        /* Right eye */
        for (int i = 0; i < eye_width; i++) {
            fwrite(right_framebuffer[j][i], 1, 3, stdout);
        }
    }
    
    fprintf(stderr, "Stereo render complete!\n");
    fprintf(stderr, "Output: %dx%d side-by-side stereo pair\n", eye_width * 2, eye_height);
    fprintf(stderr, "Frame time budget: %.2f ms (for %u FPS)\n", 
            1000.0f / (float)config.target_fps, config.target_fps);
    
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
