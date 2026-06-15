/* EXPERIMENT 5: Physics Engine with Rigid Body Dynamics
 * 
 * ZCC Features Demonstrated:
 * - typeof for polymorphic collision handlers
 * - Inline assembly for SIMD distance computation
 * - Fixed-size arrays for rigid body simulation
 * 
 * Compile: ./zcc exp5_physics_engine.c -o exp5_physics_engine.s
 * Link:    gcc -o exp5_physics_engine exp5_physics_engine.s -lm
 * Run:     ./exp5_physics_engine > physics_output.ppm
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
extern float expf(float);
extern void *malloc(unsigned long size);
extern void free(void *ptr);
extern void *memset(void *s, int c, unsigned long n);
extern void *memcpy(void *dest, const void *src, unsigned long n);
extern int rand(void);

/* 3D Vector */
typedef struct {
    float x, y, z;
} Vec3;

/* Quaternion for 3D rotation */
typedef struct {
    float w, x, y, z;
} Quat;

/* 3x3 Matrix for inertia tensor */
typedef struct {
    float m[3][3];
} Mat3;

/* Rigid body state */
typedef struct {
    Vec3 position;
    Vec3 velocity;
    Vec3 force;
    float mass;
    float inv_mass;
    Quat orientation;
    Vec3 angular_velocity;
    Vec3 torque;
    Mat3 inertia_tensor;
    Mat3 inv_inertia_tensor;
    float radius;
    unsigned char r, g, b;
    int is_static;
} RigidBody;

/* Collision contact */
typedef struct {
    Vec3 point;
    Vec3 normal;
    float penetration;
    RigidBody *body_a;
    RigidBody *body_b;
} Contact;

/* typeof for polymorphic collision (CG-007) */
typedef typeof(RigidBody) body_t;

/* === ALL VECTOR OPS USE SCALARS OR POINTERS === */

static void vec3_set(Vec3 *out, float x, float y, float z) {
    out->x = x; out->y = y; out->z = z;
}

static void vec3_add_to(Vec3 *out, Vec3 *a, Vec3 *b) {
    out->x = a->x + b->x; out->y = a->y + b->y; out->z = a->z + b->z;
}

static void vec3_sub_to(Vec3 *out, Vec3 *a, Vec3 *b) {
    out->x = a->x - b->x; out->y = a->y - b->y; out->z = a->z - b->z;
}

static void vec3_scale_to(Vec3 *out, Vec3 *v, float s) {
    out->x = v->x * s; out->y = v->y * s; out->z = v->z * s;
}

static float vec3_dot_p(Vec3 *a, Vec3 *b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static float vec3_length_p(Vec3 *v) {
    return sqrtf(vec3_dot_p(v, v));
}

static void vec3_normalize_to(Vec3 *out, Vec3 *v) {
    float len = vec3_length_p(v);
    if (len > 0.0001f) {
        float inv = 1.0f / len;
        out->x = v->x * inv; out->y = v->y * inv; out->z = v->z * inv;
    } else {
        out->x = v->x; out->y = v->y; out->z = v->z;
    }
}

/* SIMD distance (scalar fallback, avoids struct-by-value) */
static float sphere_distance(Vec3 *a, Vec3 *b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/* Quaternion ops */
static void quat_identity(Quat *q) {
    q->w = 1.0f; q->x = 0.0f; q->y = 0.0f; q->z = 0.0f;
}

static void quat_multiply(Quat *out, Quat *a, Quat *b) {
    out->w = a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z;
    out->x = a->w * b->x + a->x * b->w + a->y * b->z - a->z * b->y;
    out->y = a->w * b->y - a->x * b->z + a->y * b->w + a->z * b->x;
    out->z = a->w * b->z + a->x * b->y - a->y * b->x + a->z * b->w;
}

static void quat_from_axis_angle(Quat *out, Vec3 *axis, float angle) {
    float half_angle = angle * 0.5f;
    float s = sinf(half_angle);
    out->w = cosf(half_angle);
    out->x = axis->x * s; out->y = axis->y * s; out->z = axis->z * s;
}

/* Matrix ops */
static void mat3_identity(Mat3 *m) {
    memset(m, 0, sizeof(Mat3));
    m->m[0][0] = m->m[1][1] = m->m[2][2] = 1.0f;
}

static void mat3_sphere_inertia(Mat3 *m, float mass, float radius) {
    float I = 0.4f * mass * radius * radius;
    mat3_identity(m);
    m->m[0][0] = m->m[1][1] = m->m[2][2] = I;
}

/* Initialize rigid body */
static void rigidbody_init(RigidBody *body, float px, float py, float pz,
                           float mass, float radius,
                           unsigned char r, unsigned char g, unsigned char b) {
    vec3_set(&body->position, px, py, pz);
    vec3_set(&body->velocity, 0.0f, 0.0f, 0.0f);
    vec3_set(&body->force, 0.0f, 0.0f, 0.0f);
    body->mass = mass;
    body->inv_mass = mass > 0.0f ? 1.0f / mass : 0.0f;
    
    quat_identity(&body->orientation);
    vec3_set(&body->angular_velocity, 0.0f, 0.0f, 0.0f);
    vec3_set(&body->torque, 0.0f, 0.0f, 0.0f);
    mat3_sphere_inertia(&body->inertia_tensor, mass, radius);
    mat3_identity(&body->inv_inertia_tensor);
    if (mass > 0.0f) {
        float inv_I = 1.0f / (0.4f * mass * radius * radius);
        body->inv_inertia_tensor.m[0][0] = inv_I;
        body->inv_inertia_tensor.m[1][1] = inv_I;
        body->inv_inertia_tensor.m[2][2] = inv_I;
    }
    
    body->radius = radius;
    body->r = r; body->g = g; body->b = b;
    body->is_static = (mass == 0.0f);
}

/* Sphere-sphere collision detection */
static int detect_collision(body_t *a, body_t *b, Contact *contact) {
    float distance = sphere_distance(&a->position, &b->position);
    float min_distance = a->radius + b->radius;
    
    if (distance < min_distance) {
        contact->penetration = min_distance - distance;
        
        Vec3 diff;
        vec3_sub_to(&diff, &b->position, &a->position);
        vec3_normalize_to(&contact->normal, &diff);
        
        Vec3 scaled;
        vec3_scale_to(&scaled, &contact->normal, a->radius);
        vec3_add_to(&contact->point, &a->position, &scaled);
        contact->body_a = a;
        contact->body_b = b;
        
        return 1;
    }
    
    return 0;
}

/* Resolve collision with impulse */
static void resolve_collision(Contact *contact, float restitution) {
    RigidBody *a = contact->body_a;
    RigidBody *b = contact->body_b;
    
    Vec3 relative_vel;
    vec3_sub_to(&relative_vel, &b->velocity, &a->velocity);
    
    float vel_along_normal = vec3_dot_p(&relative_vel, &contact->normal);
    
    if (vel_along_normal > 0.0f) return;
    
    float e = restitution;
    float j = -(1.0f + e) * vel_along_normal;
    j /= a->inv_mass + b->inv_mass;
    
    Vec3 impulse;
    vec3_scale_to(&impulse, &contact->normal, j);
    
    if (!a->is_static) {
        Vec3 scaled; vec3_scale_to(&scaled, &impulse, a->inv_mass);
        vec3_sub_to(&a->velocity, &a->velocity, &scaled);
    }
    
    if (!b->is_static) {
        Vec3 scaled; vec3_scale_to(&scaled, &impulse, b->inv_mass);
        vec3_add_to(&b->velocity, &b->velocity, &scaled);
    }
    
    /* Positional correction */
    float percent = 0.2f;
    float slop = 0.01f;
    float pen_corr = contact->penetration - slop;
    if (pen_corr < 0.0f) pen_corr = 0.0f;
    float correction_magnitude = pen_corr / (a->inv_mass + b->inv_mass) * percent;
    Vec3 correction;
    vec3_scale_to(&correction, &contact->normal, correction_magnitude);
    
    if (!a->is_static) {
        Vec3 sc; vec3_scale_to(&sc, &correction, a->inv_mass);
        vec3_sub_to(&a->position, &a->position, &sc);
    }
    
    if (!b->is_static) {
        Vec3 sc; vec3_scale_to(&sc, &correction, b->inv_mass);
        vec3_add_to(&b->position, &b->position, &sc);
    }
}

/* Integrate rigid body physics */
static void rigidbody_integrate(RigidBody *body, float dt) {
    if (body->is_static) return;
    
    Vec3 acceleration;
    vec3_scale_to(&acceleration, &body->force, body->inv_mass);
    Vec3 accel_dt;
    vec3_scale_to(&accel_dt, &acceleration, dt);
    vec3_add_to(&body->velocity, &body->velocity, &accel_dt);
    
    Vec3 vel_dt;
    vec3_scale_to(&vel_dt, &body->velocity, dt);
    vec3_add_to(&body->position, &body->position, &vel_dt);
    
    vec3_set(&body->force, 0.0f, 0.0f, 0.0f);
    
    /* Angular integration (simplified) */
    Vec3 angular_accel;
    vec3_scale_to(&angular_accel, &body->torque, body->inv_mass);
    Vec3 ang_dt;
    vec3_scale_to(&ang_dt, &angular_accel, dt);
    vec3_add_to(&body->angular_velocity, &body->angular_velocity, &ang_dt);
    
    float ang_len = vec3_length_p(&body->angular_velocity);
    if (ang_len > 0.001f) {
        Vec3 axis;
        vec3_normalize_to(&axis, &body->angular_velocity);
        float angle = ang_len * dt;
        Quat rotation;
        quat_from_axis_angle(&rotation, &axis, angle);
        Quat new_orient;
        quat_multiply(&new_orient, &rotation, &body->orientation);
        body->orientation = new_orient;
    }
    
    vec3_set(&body->torque, 0.0f, 0.0f, 0.0f);
}

/* Render physics simulation to framebuffer */
static void render_physics(RigidBody *bodies, int num_bodies,
                          unsigned char *fb, int height, int width) {
    memset(fb, 20, height * width * 3);
    
    float cam_x = 0.0f, cam_y = 5.0f, cam_z = 15.0f;
    
    for (int b = 0; b < num_bodies; b++) {
        RigidBody *body = &bodies[b];
        
        float to_z = body->position.z - cam_z;
        if (to_z > -0.1f) continue;
        
        float scale = 400.0f / (-to_z);
        int px = (int)((body->position.x - cam_x) * scale) + width / 2;
        int py = (int)(-(body->position.y - cam_y) * scale) + height / 2;
        int radius = (int)(body->radius * scale);
        
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx * dx + dy * dy <= radius * radius) {
                    int x = px + dx;
                    int y = py + dy;
                    
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        float light = 1.0f - sqrtf((float)(dx * dx + dy * dy)) / (float)radius;
                        light = light * 0.7f + 0.3f;
                        
                        int idx = (y * width + x) * 3;
                        fb[idx + 0] = (unsigned char)(body->r * light);
                        fb[idx + 1] = (unsigned char)(body->g * light);
                        fb[idx + 2] = (unsigned char)(body->b * light);
                    }
                }
            }
        }
    }
}

int main(void) {
    int width = 640;
    int height = 480;
    int num_bodies = 8;
    float dt = 0.016f;
    int num_steps = 120;
    
    fprintf(stderr, "EXPERIMENT 5: Physics Engine\n");
    fprintf(stderr, "Rigid bodies: %d\n", num_bodies);
    fprintf(stderr, "Simulation steps: %d\n", num_steps);
    fprintf(stderr, "Time step: %.3f ms\n", dt * 1000.0f);
    
    RigidBody bodies[8];
    
    rigidbody_init(&bodies[0], 0.0f, -5.0f, -8.0f, 0.0f, 10.0f, 128, 128, 128);
    rigidbody_init(&bodies[1], 0.0f, 5.0f, -8.0f, 1.0f, 0.5f, 255, 0, 0);
    rigidbody_init(&bodies[2], -2.0f, 7.0f, -8.0f, 1.0f, 0.5f, 0, 255, 0);
    rigidbody_init(&bodies[3], 2.0f, 6.0f, -8.0f, 1.0f, 0.5f, 0, 0, 255);
    rigidbody_init(&bodies[4], -1.0f, 9.0f, -8.0f, 0.8f, 0.4f, 255, 255, 0);
    rigidbody_init(&bodies[5], 1.0f, 8.0f, -8.0f, 0.8f, 0.4f, 255, 0, 255);
    rigidbody_init(&bodies[6], 0.0f, 11.0f, -8.0f, 0.6f, 0.3f, 0, 255, 255);
    rigidbody_init(&bodies[7], -0.5f, 13.0f, -8.0f, 0.5f, 0.3f, 255, 128, 0);
    
    bodies[2].velocity.x = 1.0f;
    bodies[3].velocity.x = -1.0f;
    bodies[4].angular_velocity.z = 2.0f;
    
    unsigned char *framebuffer = (unsigned char *)malloc(height * width * 3);
    
    fprintf(stderr, "Running physics simulation...\n");
    
    for (int step = 0; step < num_steps; step++) {
        for (int i = 0; i < num_bodies; i++) {
            if (!bodies[i].is_static) {
                Vec3 grav;
                vec3_set(&grav, 0.0f, -9.81f * bodies[i].mass, 0.0f);
                vec3_add_to(&bodies[i].force, &bodies[i].force, &grav);
            }
        }
        
        Contact contacts[64];
        int num_contacts = 0;
        
        for (int i = 0; i < num_bodies; i++) {
            for (int j = i + 1; j < num_bodies; j++) {
                Contact contact;
                if (detect_collision(&bodies[i], &bodies[j], &contact)) {
                    contacts[num_contacts] = contact;
                    num_contacts++;
                }
            }
        }
        
        for (int i = 0; i < num_contacts; i++) {
            resolve_collision(&contacts[i], 0.6f);
        }
        
        for (int i = 0; i < num_bodies; i++) {
            rigidbody_integrate(&bodies[i], dt);
        }
        
        if (step % 12 == 0) {
            fprintf(stderr, "\rProgress: %d%%", (step * 100) / num_steps);
        }
    }
    
    fprintf(stderr, "\rProgress: 100%%\n");
    
    fprintf(stderr, "Rendering final frame...\n");
    render_physics(bodies, num_bodies, framebuffer, height, width);
    
    printf("P6\n%d %d\n255\n", width, height);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            int idx = (j * width + i) * 3;
            fwrite(&framebuffer[idx], 1, 3, stdout);
        }
    }
    
    free(framebuffer);
    
    fprintf(stderr, "Render complete!\n");
    fprintf(stderr, "Final positions:\n");
    for (int i = 1; i < num_bodies; i++) {
        fprintf(stderr, "  Body %d: (%.2f, %.2f, %.2f)\n", 
                i, bodies[i].position.x, bodies[i].position.y, bodies[i].position.z);
    }
    
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
