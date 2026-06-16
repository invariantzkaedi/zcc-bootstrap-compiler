/* EXPERIMENT 5: Advanced 3D Physics Engine with PGS Solver, Capsules, and Hinge Joints
 * 
 * ZCC Features Demonstrated:
 * - typeof for static polymorphism checks
 * - Stack boundary alignments for nested constraint loops
 * - Cofactor matrix inversion for arbitrary coordinate transformations
 * 
 * Compile: ./zcc exp5_physics_engine.c -o exp5.s
 * Link:    gcc -o exp5 exp5.s -lm
 * Run:     ./exp5 3 > exp5_output.ppm
 */

#include <stdio.h>

/* Explicit external declarations to bypass math/alloc header variations */
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

#ifndef RAND_MAX
#define RAND_MAX 2147483647
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* === 3D VECTOR MATH LIBRARY === */

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float w, x, y, z;
} Quat;

typedef struct {
    float m[3][3];
} Mat3;

static void vec3_set(Vec3 *out, float x, float y, float z) {
    out->x = x; out->y = y; out->z = z;
}

static void vec3_add_to(Vec3 *out, const Vec3 *a, const Vec3 *b) {
    out->x = a->x + b->x; out->y = a->y + b->y; out->z = a->z + b->z;
}

static void vec3_sub_to(Vec3 *out, const Vec3 *a, const Vec3 *b) {
    out->x = a->x - b->x; out->y = a->y - b->y; out->z = a->z - b->z;
}

static void vec3_scale_to(Vec3 *out, const Vec3 *v, float s) {
    out->x = v->x * s; out->y = v->y * s; out->z = v->z * s;
}

static float vec3_dot_p(const Vec3 *a, const Vec3 *b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static void vec3_cross_to(Vec3 *out, const Vec3 *a, const Vec3 *b) {
    out->x = a->y * b->z - a->z * b->y;
    out->y = a->z * b->x - a->x * b->z;
    out->z = a->x * b->y - a->y * b->x;
}

static float vec3_length_p(const Vec3 *v) {
    return sqrtf(vec3_dot_p(v, v));
}

static void vec3_normalize_to(Vec3 *out, const Vec3 *v) {
    float len = vec3_length_p(v);
    if (len > 0.0001f) {
        float inv = 1.0f / len;
        out->x = v->x * inv; out->y = v->y * inv; out->z = v->z * inv;
    } else {
        out->x = v->x; out->y = v->y; out->z = v->z;
    }
}

static float sphere_distance(const Vec3 *a, const Vec3 *b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/* === QUATERNION MATH === */

static void quat_identity(Quat *q) {
    q->w = 1.0f; q->x = 0.0f; q->y = 0.0f; q->z = 0.0f;
}

static void quat_multiply(Quat *out, const Quat *a, const Quat *b) {
    out->w = a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z;
    out->x = a->w * b->x + a->x * b->w + a->y * b->z - a->z * b->y;
    out->y = a->w * b->y - a->x * b->z + a->y * b->w + a->z * b->x;
    out->z = a->w * b->z + a->x * b->y - a->y * b->x + a->z * b->w;
}

static void quat_from_axis_angle(Quat *out, const Vec3 *axis, float angle) {
    float half_angle = angle * 0.5f;
    float s = sinf(half_angle);
    out->w = cosf(half_angle);
    out->x = axis->x * s; out->y = axis->y * s; out->z = axis->z * s;
}

static void quat_rotate_vec(Vec3 *out, const Quat *q, const Vec3 *v) {
    Vec3 q_xyz;
    vec3_set(&q_xyz, q->x, q->y, q->z);
    
    Vec3 cross1;
    vec3_cross_to(&cross1, &q_xyz, v);
    
    Vec3 q_w_v;
    vec3_scale_to(&q_w_v, v, q->w);
    
    Vec3 sum;
    vec3_add_to(&sum, &cross1, &q_w_v);
    
    Vec3 cross2;
    vec3_cross_to(&cross2, &q_xyz, &sum);
    
    Vec3 scaled;
    vec3_scale_to(&scaled, &cross2, 2.0f);
    
    vec3_add_to(out, v, &scaled);
}

/* === 3X3 MATRIX MATH === */

static void mat3_identity(Mat3 *m) {
    memset(m, 0, sizeof(Mat3));
    m->m[0][0] = m->m[1][1] = m->m[2][2] = 1.0f;
}

static void mat3_transpose_to(Mat3 *out, const Mat3 *in) {
    out->m[0][0] = in->m[0][0]; out->m[0][1] = in->m[1][0]; out->m[0][2] = in->m[2][0];
    out->m[1][0] = in->m[0][1]; out->m[1][1] = in->m[1][1]; out->m[1][2] = in->m[2][1];
    out->m[2][0] = in->m[0][2]; out->m[2][1] = in->m[1][2]; out->m[2][2] = in->m[2][2];
}

static void mat3_multiply(Mat3 *out, const Mat3 *a, const Mat3 *b) {
    int i, j, k;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            out->m[i][j] = 0.0f;
            for (k = 0; k < 3; k++) {
                out->m[i][j] += a->m[i][k] * b->m[k][j];
            }
        }
    }
}

static void mat3_vec3_multiply(Vec3 *out, const Mat3 *m, const Vec3 *v) {
    out->x = m->m[0][0] * v->x + m->m[0][1] * v->y + m->m[0][2] * v->z;
    out->y = m->m[1][0] * v->x + m->m[1][1] * v->y + m->m[1][2] * v->z;
    out->z = m->m[2][0] * v->x + m->m[2][1] * v->y + m->m[2][2] * v->z;
}

static void mat3_from_quat(Mat3 *m, const Quat *q) {
    float xx = q->x * q->x; float yy = q->y * q->y; float zz = q->z * q->z;
    float xy = q->x * q->y; float xz = q->x * q->z; float yz = q->y * q->z;
    float wx = q->w * q->x; float wy = q->w * q->y; float wz = q->w * q->z;
    
    m->m[0][0] = 1.0f - 2.0f * (yy + zz);
    m->m[0][1] = 2.0f * (xy - wz);
    m->m[0][2] = 2.0f * (xz + wy);
    
    m->m[1][0] = 2.0f * (xy + wz);
    m->m[1][1] = 1.0f - 2.0f * (xx + zz);
    m->m[1][2] = 2.0f * (yz - wx);
    
    m->m[2][0] = 2.0f * (xz - wy);
    m->m[2][1] = 2.0f * (yz + wx);
    m->m[2][2] = 1.0f - 2.0f * (xx + yy);
}

static void mat3_invert(Mat3 *out, const Mat3 *m) {
    float det = m->m[0][0] * (m->m[1][1] * m->m[2][2] - m->m[1][2] * m->m[2][1]) -
                m->m[0][1] * (m->m[1][0] * m->m[2][2] - m->m[1][2] * m->m[2][0]) +
                m->m[0][2] * (m->m[1][0] * m->m[2][1] - m->m[1][1] * m->m[2][0]);
                
    if (fabsf(det) < 0.00001f) {
        mat3_identity(out);
        return;
    }
    
    float inv_det = 1.0f / det;
    out->m[0][0] = (m->m[1][1] * m->m[2][2] - m->m[1][2] * m->m[2][1]) * inv_det;
    out->m[0][1] = (m->m[0][2] * m->m[2][1] - m->m[0][1] * m->m[2][2]) * inv_det;
    out->m[0][2] = (m->m[0][1] * m->m[1][2] - m->m[0][2] * m->m[1][1]) * inv_det;
    
    out->m[1][0] = (m->m[1][2] * m->m[2][0] - m->m[1][0] * m->m[2][2]) * inv_det;
    out->m[1][1] = (m->m[0][0] * m->m[2][2] - m->m[0][2] * m->m[2][0]) * inv_det;
    out->m[1][2] = (m->m[0][2] * m->m[1][0] - m->m[0][0] * m->m[1][2]) * inv_det;
    
    out->m[2][0] = (m->m[1][0] * m->m[2][1] - m->m[1][1] * m->m[2][0]) * inv_det;
    out->m[2][1] = (m->m[0][1] * m->m[2][0] - m->m[0][0] * m->m[2][1]) * inv_det;
    out->m[2][2] = (m->m[0][0] * m->m[1][1] - m->m[0][1] * m->m[1][0]) * inv_det;
}

/* === CLOSEST POINTS LINE SEGMENT HELPERS === */

static void closest_points_segment_segment(const Vec3 *p1, const Vec3 *q1,
                                           const Vec3 *p2, const Vec3 *q2,
                                           float *s, float *t,
                                           Vec3 *c1, Vec3 *c2) {
    Vec3 d1, d2, r;
    vec3_sub_to(&d1, q1, p1);
    vec3_sub_to(&d2, q2, p2);
    vec3_sub_to(&r, p1, p2);
    
    float a = vec3_dot_p(&d1, &d1);
    float e = vec3_dot_p(&d2, &d2);
    float f = vec3_dot_p(&d2, &r);
    
    float clamp_s = 0.0f;
    float clamp_t = 0.0f;
    
    if (a <= 0.0001f && e <= 0.0001f) {
        clamp_s = 0.0f;
        clamp_t = 0.0f;
    } else if (a <= 0.0001f) {
        clamp_s = 0.0f;
        clamp_t = f / e;
        if (clamp_t < 0.0f) clamp_t = 0.0f;
        if (clamp_t > 1.0f) clamp_t = 1.0f;
    } else {
        float c = vec3_dot_p(&d1, &r);
        if (e <= 0.0001f) {
            clamp_t = 0.0f;
            clamp_s = -c / a;
            if (clamp_s < 0.0f) clamp_s = 0.0f;
            if (clamp_s > 1.0f) clamp_s = 1.0f;
        } else {
            float b = vec3_dot_p(&d1, &d2);
            float denom = a * e - b * b;
            
            if (denom != 0.0f) {
                clamp_s = (b * f - c * e) / denom;
                if (clamp_s < 0.0f) clamp_s = 0.0f;
                if (clamp_s > 1.0f) clamp_s = 1.0f;
            } else {
                clamp_s = 0.0f;
            }
            
            clamp_t = (b * clamp_s + f) / e;
            if (clamp_t < 0.0f) {
                clamp_t = 0.0f;
                clamp_s = -c / a;
                if (clamp_s < 0.0f) clamp_s = 0.0f;
                if (clamp_s > 1.0f) clamp_s = 1.0f;
            } else if (clamp_t > 1.0f) {
                clamp_t = 1.0f;
                clamp_s = (b - c) / a;
                if (clamp_s < 0.0f) clamp_s = 0.0f;
                if (clamp_s > 1.0f) clamp_s = 1.0f;
            }
        }
    }
    
    *s = clamp_s;
    *t = clamp_t;
    
    Vec3 scaled1, scaled2;
    vec3_scale_to(&scaled1, &d1, clamp_s);
    vec3_add_to(c1, p1, &scaled1);
    
    vec3_scale_to(&scaled2, &d2, clamp_t);
    vec3_add_to(c2, p2, &scaled2);
}

/* === RIGID BODY GEOMETRY DEFINITIONS === */

typedef enum {
    SHAPE_SPHERE,
    SHAPE_OBB,
    SHAPE_CAPSULE
} ShapeKind;

typedef struct {
    ShapeKind kind;
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
    float radius;               /* For Sphere & Capsule */
    Vec3 half_extents;          /* For OBB */
    float capsule_half_height;  /* For Capsule Segment length = 2 * half_height */
    unsigned char r, g, b;
    int is_static;
} RigidBody;

typedef struct {
    Vec3 normal;
    float offset;
} Plane;

typedef struct {
    Vec3 point;
    Vec3 normal;
    float penetration;
    RigidBody *body_a;
    RigidBody *body_b;
} Contact;

typedef enum {
    JOINT_DISTANCE,
    JOINT_BALL_SOCKET,
    JOINT_HINGE
} JointKind;

typedef struct {
    JointKind kind;
    RigidBody *body_a;
    RigidBody *body_b;
    Vec3 local_anchor_a;
    Vec3 local_anchor_b;
    float target_distance;      /* For Distance joints */
    float stiffness;            /* For Distance joints */
    float damping;              /* For Distance joints */
    Vec3 local_axis_a;          /* For Hinge joints */
} Joint;

/* Typeof declarations for compiler static verification */
typedef typeof(RigidBody) body_t;
typedef typeof(Joint) joint_t;

/* === INITIALIZERS === */

static void rigidbody_init_sphere(RigidBody *body, float px, float py, float pz,
                                  float mass, float radius,
                                  unsigned char r, unsigned char g, unsigned char b) {
    vec3_set(&body->position, px, py, pz);
    vec3_set(&body->velocity, 0.0f, 0.0f, 0.0f);
    vec3_set(&body->force, 0.0f, 0.0f, 0.0f);
    body->mass = mass;
    body->inv_mass = mass > 0.0f ? 1.0f / mass : 0.0f;
    body->kind = SHAPE_SPHERE;
    body->radius = radius;
    vec3_set(&body->half_extents, 0.0f, 0.0f, 0.0f);
    body->capsule_half_height = 0.0f;
    
    quat_identity(&body->orientation);
    vec3_set(&body->angular_velocity, 0.0f, 0.0f, 0.0f);
    vec3_set(&body->torque, 0.0f, 0.0f, 0.0f);
    
    mat3_identity(&body->inertia_tensor);
    mat3_identity(&body->inv_inertia_tensor);
    if (mass > 0.0f) {
        float I = 0.4f * mass * radius * radius;
        body->inertia_tensor.m[0][0] = body->inertia_tensor.m[1][1] = body->inertia_tensor.m[2][2] = I;
        float inv_I = 1.0f / I;
        body->inv_inertia_tensor.m[0][0] = body->inv_inertia_tensor.m[1][1] = body->inv_inertia_tensor.m[2][2] = inv_I;
    }
    body->r = r; body->g = g; body->b = b;
    body->is_static = (mass == 0.0f);
}

static void rigidbody_init_obb(RigidBody *body, float px, float py, float pz,
                               float mass, float hx, float hy, float hz,
                               unsigned char r, unsigned char g, unsigned char b) {
    vec3_set(&body->position, px, py, pz);
    vec3_set(&body->velocity, 0.0f, 0.0f, 0.0f);
    vec3_set(&body->force, 0.0f, 0.0f, 0.0f);
    body->mass = mass;
    body->inv_mass = mass > 0.0f ? 1.0f / mass : 0.0f;
    body->kind = SHAPE_OBB;
    body->radius = 0.0f;
    vec3_set(&body->half_extents, hx, hy, hz);
    body->capsule_half_height = 0.0f;
    
    quat_identity(&body->orientation);
    vec3_set(&body->angular_velocity, 0.0f, 0.0f, 0.0f);
    vec3_set(&body->torque, 0.0f, 0.0f, 0.0f);
    
    mat3_identity(&body->inertia_tensor);
    mat3_identity(&body->inv_inertia_tensor);
    if (mass > 0.0f) {
        float ix = (1.0f / 3.0f) * mass * (hy*hy + hz*hz);
        float iy = (1.0f / 3.0f) * mass * (hx*hx + hz*hz);
        float iz = (1.0f / 3.0f) * mass * (hx*hx + hy*hy);
        
        body->inertia_tensor.m[0][0] = ix;
        body->inertia_tensor.m[1][1] = iy;
        body->inertia_tensor.m[2][2] = iz;
        
        body->inv_inertia_tensor.m[0][0] = 1.0f / ix;
        body->inv_inertia_tensor.m[1][1] = 1.0f / iy;
        body->inv_inertia_tensor.m[2][2] = 1.0f / iz;
    }
    body->r = r; body->g = g; body->b = b;
    body->is_static = (mass == 0.0f);
}

static void rigidbody_init_capsule(RigidBody *body, float px, float py, float pz,
                                   float mass, float radius, float half_height,
                                   unsigned char r, unsigned char g, unsigned char b) {
    vec3_set(&body->position, px, py, pz);
    vec3_set(&body->velocity, 0.0f, 0.0f, 0.0f);
    vec3_set(&body->force, 0.0f, 0.0f, 0.0f);
    body->mass = mass;
    body->inv_mass = mass > 0.0f ? 1.0f / mass : 0.0f;
    body->kind = SHAPE_CAPSULE;
    body->radius = radius;
    vec3_set(&body->half_extents, 0.0f, 0.0f, 0.0f);
    body->capsule_half_height = half_height;
    
    quat_identity(&body->orientation);
    vec3_set(&body->angular_velocity, 0.0f, 0.0f, 0.0f);
    vec3_set(&body->torque, 0.0f, 0.0f, 0.0f);
    
    mat3_identity(&body->inertia_tensor);
    mat3_identity(&body->inv_inertia_tensor);
    if (mass > 0.0f) {
        /* Approximate as cylinder of same volume along local y-axis */
        float r2 = radius * radius;
        float h2 = (half_height * 2.0f) * (half_height * 2.0f);
        float ix = (1.0f / 12.0f) * mass * (3.0f * r2 + h2);
        float iy = 0.5f * mass * r2;
        float iz = ix;
        
        body->inertia_tensor.m[0][0] = ix;
        body->inertia_tensor.m[1][1] = iy;
        body->inertia_tensor.m[2][2] = iz;
        
        body->inv_inertia_tensor.m[0][0] = 1.0f / ix;
        body->inv_inertia_tensor.m[1][1] = 1.0f / iy;
        body->inv_inertia_tensor.m[2][2] = 1.0f / iz;
    }
    body->r = r; body->g = g; body->b = b;
    body->is_static = (mass == 0.0f);
}

static void rigidbody_get_world_inv_inertia(Mat3 *out, const RigidBody *body) {
    if (body->is_static) {
        memset(out, 0, sizeof(Mat3));
        return;
    }
    Mat3 R;
    mat3_from_quat(&R, &body->orientation);
    
    Mat3 R_trans;
    mat3_transpose_to(&R_trans, &R);
    
    Mat3 temp;
    int i, j;
    memset(&temp, 0, sizeof(Mat3));
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            temp.m[i][j] = R.m[i][j] * body->inv_inertia_tensor.m[j][j];
        }
    }
    mat3_multiply(out, &temp, &R_trans);
}

static void get_obb_axes(Vec3 *axes, const RigidBody *body) {
    Mat3 R;
    mat3_from_quat(&R, &body->orientation);
    vec3_set(&axes[0], R.m[0][0], R.m[1][0], R.m[2][0]);
    vec3_set(&axes[1], R.m[0][1], R.m[1][1], R.m[2][1]);
    vec3_set(&axes[2], R.m[0][2], R.m[1][2], R.m[2][2]);
}

static void get_obb_vertices(Vec3 *vertices, const RigidBody *body) {
    Vec3 axes[3];
    get_obb_axes(axes, body);
    
    int i;
    for (i = 0; i < 8; i++) {
        float sx = (i & 1) ? 1.0f : -1.0f;
        float sy = (i & 2) ? 1.0f : -1.0f;
        float sz = (i & 4) ? 1.0f : -1.0f;
        
        Vec3 vx, vy, vz, sum;
        vec3_scale_to(&vx, &axes[0], sx * body->half_extents.x);
        vec3_scale_to(&vy, &axes[1], sy * body->half_extents.y);
        vec3_scale_to(&vz, &axes[2], sz * body->half_extents.z);
        
        vec3_add_to(&sum, &vx, &vy);
        vec3_add_to(&vertices[i], &sum, &vz);
        vec3_add_to(&vertices[i], &vertices[i], &body->position);
    }
}

static float get_point_obb_penetration(const Vec3 *point, const RigidBody *body, Vec3 *out_normal) {
    Vec3 d;
    vec3_sub_to(&d, point, &body->position);
    
    Vec3 axes[3];
    get_obb_axes(axes, body);
    
    float px = vec3_dot_p(&d, &axes[0]);
    float py = vec3_dot_p(&d, &axes[1]);
    float pz = vec3_dot_p(&d, &axes[2]);
    
    float dx = body->half_extents.x - fabsf(px);
    float dy = body->half_extents.y - fabsf(py);
    float dz = body->half_extents.z - fabsf(pz);
    
    if (dx > 0.0f && dy > 0.0f && dz > 0.0f) {
        if (dx < dy && dx < dz) {
            float sx = px > 0.0f ? 1.0f : -1.0f;
            vec3_scale_to(out_normal, &axes[0], sx);
            return dx;
        } else if (dy < dz) {
            float sy = py > 0.0f ? 1.0f : -1.0f;
            vec3_scale_to(out_normal, &axes[1], sy);
            return dy;
        } else {
            float sz = pz > 0.0f ? 1.0f : -1.0f;
            vec3_scale_to(out_normal, &axes[2], sz);
            return dz;
        }
    }
    return -1.0f;
}

static void get_capsule_endpoints(const RigidBody *body, Vec3 *a, Vec3 *b) {
    Vec3 local_a, local_b;
    vec3_set(&local_a, 0.0f, -body->capsule_half_height, 0.0f);
    vec3_set(&local_b, 0.0f, body->capsule_half_height, 0.0f);
    
    quat_rotate_vec(a, &body->orientation, &local_a);
    vec3_add_to(a, a, &body->position);
    
    quat_rotate_vec(b, &body->orientation, &local_b);
    vec3_add_to(b, b, &body->position);
}

/* === COLLISION DETECTORS === */

static int detect_sphere_sphere(RigidBody *a, RigidBody *b, Contact *contact) {
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

static int detect_sphere_plane(RigidBody *sphere, const Plane *plane, Contact *contact) {
    float dist = vec3_dot_p(&sphere->position, &plane->normal) - plane->offset;
    if (dist < sphere->radius) {
        contact->penetration = sphere->radius - dist;
        vec3_set(&contact->normal, plane->normal.x, plane->normal.y, plane->normal.z);
        
        Vec3 scaled;
        vec3_scale_to(&scaled, &plane->normal, sphere->radius);
        vec3_sub_to(&contact->point, &sphere->position, &scaled);
        contact->body_a = sphere;
        contact->body_b = NULL;
        return 1;
    }
    return 0;
}

static int detect_sphere_obb(RigidBody *sphere, RigidBody *obb, Contact *contact) {
    Vec3 d;
    vec3_sub_to(&d, &sphere->position, &obb->position);
    Vec3 axes[3];
    get_obb_axes(axes, obb);
    
    Vec3 closest;
    vec3_set(&closest, obb->position.x, obb->position.y, obb->position.z);
    
    int i;
    for (i = 0; i < 3; i++) {
        float dist = vec3_dot_p(&d, &axes[i]);
        float half = i == 0 ? obb->half_extents.x : (i == 1 ? obb->half_extents.y : obb->half_extents.z);
        if (dist > half) dist = half;
        if (dist < -half) dist = -half;
        Vec3 scaled;
        vec3_scale_to(&scaled, &axes[i], dist);
        vec3_add_to(&closest, &closest, &scaled);
    }
    
    float distance = sphere_distance(&sphere->position, &closest);
    if (distance < sphere->radius) {
        contact->penetration = sphere->radius - distance;
        
        Vec3 diff;
        if (distance > 0.001f) {
            vec3_sub_to(&diff, &sphere->position, &closest);
            vec3_normalize_to(&contact->normal, &diff);
        } else {
            vec3_sub_to(&diff, &sphere->position, &obb->position);
            vec3_normalize_to(&contact->normal, &diff);
        }
        vec3_set(&contact->point, closest.x, closest.y, closest.z);
        contact->body_a = sphere;
        contact->body_b = obb;
        return 1;
    }
    return 0;
}

static int detect_obb_plane(RigidBody *obb, const Plane *plane, Contact *contact) {
    Vec3 vertices[8];
    get_obb_vertices(vertices, obb);
    
    Vec3 avg_point;
    vec3_set(&avg_point, 0.0f, 0.0f, 0.0f);
    int count = 0;
    float max_pen = 0.0f;
    
    int i;
    for (i = 0; i < 8; i++) {
        float dist = vec3_dot_p(&vertices[i], &plane->normal) - plane->offset;
        if (dist < 0.0f) {
            float pen = -dist;
            vec3_add_to(&avg_point, &avg_point, &vertices[i]);
            count++;
            if (pen > max_pen) max_pen = pen;
        }
    }
    
    if (count > 0) {
        vec3_scale_to(&contact->point, &avg_point, 1.0f / (float)count);
        contact->penetration = max_pen;
        vec3_set(&contact->normal, plane->normal.x, plane->normal.y, plane->normal.z);
        contact->body_a = obb;
        contact->body_b = NULL;
        return 1;
    }
    return 0;
}

static int detect_obb_obb(RigidBody *a, RigidBody *b, Contact *contact) {
    Vec3 axes_a[3], axes_b[3];
    get_obb_axes(axes_a, a);
    get_obb_axes(axes_b, b);
    
    Vec3 T;
    vec3_sub_to(&T, &b->position, &a->position);
    Vec3 candidate_axes[15];
    int num_axes = 0;
    
    candidate_axes[num_axes++] = axes_a[0];
    candidate_axes[num_axes++] = axes_a[1];
    candidate_axes[num_axes++] = axes_a[2];
    candidate_axes[num_axes++] = axes_b[0];
    candidate_axes[num_axes++] = axes_b[1];
    candidate_axes[num_axes++] = axes_b[2];
    
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            Vec3 axis;
            vec3_cross_to(&axis, &axes_a[i], &axes_b[j]);
            float len = vec3_length_p(&axis);
            if (len > 0.0001f) {
                vec3_scale_to(&candidate_axes[num_axes], &axis, 1.0f / len);
                num_axes++;
            }
        }
    }
    
    float min_overlap = 1e10f;
    int min_axis_idx = -1;
    
    for (i = 0; i < num_axes; i++) {
        Vec3 L = candidate_axes[i];
        float r_a = fabsf(vec3_dot_p(&axes_a[0], &L)) * a->half_extents.x +
                    fabsf(vec3_dot_p(&axes_a[1], &L)) * a->half_extents.y +
                    fabsf(vec3_dot_p(&axes_a[2], &L)) * a->half_extents.z;
        float r_b = fabsf(vec3_dot_p(&axes_b[0], &L)) * b->half_extents.x +
                    fabsf(vec3_dot_p(&axes_b[1], &L)) * b->half_extents.y +
                    fabsf(vec3_dot_p(&axes_b[2], &L)) * b->half_extents.z;
        float d = fabsf(vec3_dot_p(&T, &L));
        float overlap = (r_a + r_b) - d;
        if (overlap < 0.0f) return 0;
        
        if (overlap < min_overlap) {
            min_overlap = overlap;
            min_axis_idx = i;
        }
    }
    
    if (min_axis_idx == -1) return 0;
    
    Vec3 normal = candidate_axes[min_axis_idx];
    if (vec3_dot_p(&T, &normal) < 0.0f) {
        vec3_scale_to(&normal, &normal, -1.0f);
    }
    
    contact->penetration = min_overlap;
    vec3_set(&contact->normal, normal.x, normal.y, normal.z);
    
    Vec3 vertices_a[8], vertices_b[8];
    get_obb_vertices(vertices_a, a);
    get_obb_vertices(vertices_b, b);
    
    Vec3 avg_point;
    vec3_set(&avg_point, 0.0f, 0.0f, 0.0f);
    int count = 0;
    
    for (i = 0; i < 8; i++) {
        Vec3 norm_dummy;
        if (get_point_obb_penetration(&vertices_a[i], b, &norm_dummy) > 0.0f) {
            vec3_add_to(&avg_point, &avg_point, &vertices_a[i]);
            count++;
        }
        if (get_point_obb_penetration(&vertices_b[i], a, &norm_dummy) > 0.0f) {
            vec3_add_to(&avg_point, &avg_point, &vertices_b[i]);
            count++;
        }
    }
    
    if (count > 0) {
        vec3_scale_to(&contact->point, &avg_point, 1.0f / (float)count);
    } else {
        float r_a = fabsf(vec3_dot_p(&axes_a[0], &normal)) * a->half_extents.x +
                    fabsf(vec3_dot_p(&axes_a[1], &normal)) * a->half_extents.y +
                    fabsf(vec3_dot_p(&axes_a[2], &normal)) * a->half_extents.z;
        float r_b = fabsf(vec3_dot_p(&axes_b[0], &normal)) * b->half_extents.x +
                    fabsf(vec3_dot_p(&axes_b[1], &normal)) * b->half_extents.y +
                    fabsf(vec3_dot_p(&axes_b[2], &normal)) * b->half_extents.z;
        
        Vec3 p_a, p_b, scaled_a, scaled_b, sum;
        vec3_scale_to(&scaled_a, &normal, r_a);
        vec3_scale_to(&scaled_b, &normal, -r_b);
        vec3_add_to(&p_a, &a->position, &scaled_a);
        vec3_add_to(&p_b, &b->position, &scaled_b);
        vec3_add_to(&sum, &p_a, &p_b);
        vec3_scale_to(&contact->point, &sum, 0.5f);
    }
    
    contact->body_a = a;
    contact->body_b = b;
    return 1;
}

/* === CAPSULE COLLISION SOLVERS === */

static int detect_capsule_plane(RigidBody *capsule, const Plane *plane, Contact *contacts, int *num_contacts) {
    Vec3 a, b;
    get_capsule_endpoints(capsule, &a, &b);
    
    float da = vec3_dot_p(&a, &plane->normal) - plane->offset;
    float db = vec3_dot_p(&b, &plane->normal) - plane->offset;
    
    int hit = 0;
    if (da < capsule->radius) {
        Contact *c = &contacts[*num_contacts];
        c->penetration = capsule->radius - da;
        vec3_set(&c->normal, plane->normal.x, plane->normal.y, plane->normal.z);
        Vec3 scaled;
        vec3_scale_to(&scaled, &plane->normal, capsule->radius);
        vec3_sub_to(&c->point, &a, &scaled);
        c->body_a = capsule;
        c->body_b = NULL;
        (*num_contacts)++;
        hit = 1;
    }
    if (db < capsule->radius) {
        Contact *c = &contacts[*num_contacts];
        c->penetration = capsule->radius - db;
        vec3_set(&c->normal, plane->normal.x, plane->normal.y, plane->normal.z);
        Vec3 scaled;
        vec3_scale_to(&scaled, &plane->normal, capsule->radius);
        vec3_sub_to(&c->point, &b, &scaled);
        c->body_a = capsule;
        c->body_b = NULL;
        (*num_contacts)++;
        hit = 1;
    }
    return hit;
}

static int detect_capsule_sphere(RigidBody *capsule, RigidBody *sphere, Contact *contact) {
    Vec3 a, b;
    get_capsule_endpoints(capsule, &a, &b);
    
    Vec3 ab, as;
    vec3_sub_to(&ab, &b, &a);
    vec3_sub_to(&as, &sphere->position, &a);
    
    float ab_len_sq = vec3_dot_p(&ab, &ab);
    float t = 0.0f;
    if (ab_len_sq > 0.0001f) {
        t = vec3_dot_p(&as, &ab) / ab_len_sq;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    
    Vec3 closest, scaled;
    vec3_scale_to(&scaled, &ab, t);
    vec3_add_to(&closest, &a, &scaled);
    
    float dist = sphere_distance(&closest, &sphere->position);
    float min_dist = capsule->radius + sphere->radius;
    if (dist < min_dist) {
        contact->penetration = min_dist - dist;
        Vec3 diff;
        if (dist > 0.001f) {
            vec3_sub_to(&diff, &sphere->position, &closest);
            vec3_normalize_to(&contact->normal, &diff);
        } else {
            vec3_set(&contact->normal, 0.0f, 1.0f, 0.0f);
        }
        
        Vec3 offset;
        vec3_scale_to(&offset, &contact->normal, capsule->radius);
        vec3_add_to(&contact->point, &closest, &offset);
        contact->body_a = capsule;
        contact->body_b = sphere;
        return 1;
    }
    return 0;
}

static int detect_capsule_capsule(RigidBody *a, RigidBody *b, Contact *contact) {
    Vec3 a_start, a_end;
    get_capsule_endpoints(a, &a_start, &a_end);
    
    Vec3 b_start, b_end;
    get_capsule_endpoints(b, &b_start, &b_end);
    
    float s, t;
    Vec3 c1, c2;
    closest_points_segment_segment(&a_start, &a_end, &b_start, &b_end, &s, &t, &c1, &c2);
    
    float dist = sphere_distance(&c1, &c2);
    float min_dist = a->radius + b->radius;
    if (dist < min_dist) {
        contact->penetration = min_dist - dist;
        Vec3 diff;
        if (dist > 0.001f) {
            vec3_sub_to(&diff, &c2, &c1);
            vec3_normalize_to(&contact->normal, &diff);
        } else {
            vec3_set(&contact->normal, 0.0f, 1.0f, 0.0f);
        }
        
        Vec3 scaled_a, scaled_b, pt_a, pt_b, sum;
        vec3_scale_to(&scaled_a, &contact->normal, a->radius);
        vec3_scale_to(&scaled_b, &contact->normal, -b->radius);
        vec3_add_to(&pt_a, &c1, &scaled_a);
        vec3_add_to(&pt_b, &c2, &scaled_b);
        vec3_add_to(&sum, &pt_a, &pt_b);
        vec3_scale_to(&contact->point, &sum, 0.5f);
        
        contact->body_a = a;
        contact->body_b = b;
        return 1;
    }
    return 0;
}

static int detect_capsule_obb(RigidBody *capsule, RigidBody *obb, Contact *contact) {
    Vec3 a, b;
    get_capsule_endpoints(capsule, &a, &b);
    
    Vec3 mid, sum;
    vec3_add_to(&sum, &a, &b);
    vec3_scale_to(&mid, &sum, 0.5f);
    
    Vec3 samples[3];
    samples[0] = a; samples[1] = mid; samples[2] = b;
    
    Mat3 R, R_t;
    mat3_from_quat(&R, &obb->orientation);
    mat3_transpose_to(&R_t, &R);
    
    float max_pen = -1.0f;
    Vec3 best_normal, best_point;
    
    int i;
    for (i = 0; i < 3; i++) {
        Vec3 p = samples[i];
        Vec3 diff, local_p;
        vec3_sub_to(&diff, &p, &obb->position);
        mat3_vec3_multiply(&local_p, &R_t, &diff);
        
        Vec3 local_closest;
        local_closest.x = fminf(fmaxf(local_p.x, -obb->half_extents.x), obb->half_extents.x);
        local_closest.y = fminf(fmaxf(local_p.y, -obb->half_extents.y), obb->half_extents.y);
        local_closest.z = fminf(fmaxf(local_p.z, -obb->half_extents.z), obb->half_extents.z);
        
        Vec3 world_closest;
        mat3_vec3_multiply(&world_closest, &R, &local_closest);
        vec3_add_to(&world_closest, &world_closest, &obb->position);
        
        float dist = sphere_distance(&p, &world_closest);
        if (dist < capsule->radius) {
            float pen = capsule->radius - dist;
            if (pen > max_pen) {
                max_pen = pen;
                Vec3 norm_diff;
                if (dist > 0.001f) {
                    vec3_sub_to(&norm_diff, &p, &world_closest);
                    vec3_normalize_to(&best_normal, &norm_diff);
                } else {
                    vec3_set(&best_normal, 0.0f, 1.0f, 0.0f);
                }
                vec3_set(&best_point, world_closest.x, world_closest.y, world_closest.z);
            }
        }
    }
    
    if (max_pen > 0.0f) {
        contact->penetration = max_pen;
        vec3_set(&contact->normal, best_normal.x, best_normal.y, best_normal.z);
        vec3_set(&contact->point, best_point.x, best_point.y, best_point.z);
        contact->body_a = capsule;
        contact->body_b = obb;
        return 1;
    }
    return 0;
}

/* Dispatch routine mapping shapes into correct collision pipelines */
static int detect_collision_dispatch(RigidBody *a, RigidBody *b, const Plane *plane, Contact *contacts, int *num_contacts) {
    if (b == NULL) {
        if (a->kind == SHAPE_SPHERE) {
            if (detect_sphere_plane(a, plane, &contacts[*num_contacts])) {
                (*num_contacts)++;
                return 1;
            }
        } else if (a->kind == SHAPE_OBB) {
            if (detect_obb_plane(a, plane, &contacts[*num_contacts])) {
                (*num_contacts)++;
                return 1;
            }
        } else if (a->kind == SHAPE_CAPSULE) {
            return detect_capsule_plane(a, plane, contacts, num_contacts);
        }
        return 0;
    }
    
    int hit = 0;
    Contact *c = &contacts[*num_contacts];
    
    if (a->kind == SHAPE_SPHERE && b->kind == SHAPE_SPHERE) {
        hit = detect_sphere_sphere(a, b, c);
    } else if (a->kind == SHAPE_SPHERE && b->kind == SHAPE_OBB) {
        hit = detect_sphere_obb(a, b, c);
    } else if (a->kind == SHAPE_OBB && b->kind == SHAPE_SPHERE) {
        hit = detect_sphere_obb(b, a, c);
        if (hit) vec3_scale_to(&c->normal, &c->normal, -1.0f);
    } else if (a->kind == SHAPE_OBB && b->kind == SHAPE_OBB) {
        hit = detect_obb_obb(a, b, c);
    } else if (a->kind == SHAPE_CAPSULE && b->kind == SHAPE_SPHERE) {
        hit = detect_capsule_sphere(a, b, c);
    } else if (a->kind == SHAPE_SPHERE && b->kind == SHAPE_CAPSULE) {
        hit = detect_capsule_sphere(b, a, c);
        if (hit) vec3_scale_to(&c->normal, &c->normal, -1.0f);
    } else if (a->kind == SHAPE_CAPSULE && b->kind == SHAPE_CAPSULE) {
        hit = detect_capsule_capsule(a, b, c);
    } else if (a->kind == SHAPE_CAPSULE && b->kind == SHAPE_OBB) {
        hit = detect_capsule_obb(a, b, c);
    } else if (a->kind == SHAPE_OBB && b->kind == SHAPE_CAPSULE) {
        hit = detect_capsule_obb(b, a, c);
        if (hit) vec3_scale_to(&c->normal, &c->normal, -1.0f);
    }
    
    if (hit) {
        (*num_contacts)++;
        return 1;
    }
    return 0;
}

/* === PROJECTED GAUSS-SEIDEL (PGS) CONSTRAINT SOLVER === */

typedef struct {
    RigidBody *body_a;
    RigidBody *body_b;
    Vec3 J_linear_a;
    Vec3 J_angular_a;
    Vec3 J_linear_b;
    Vec3 J_angular_b;
    float effective_mass;
    float bias;
    float accumulated_impulse;
    float limit_min;
    float limit_max;
    int parent_normal_idx; /* link for friction limits */
} SolverConstraint;

static void init_contact_constraint(SolverConstraint *c, RigidBody *a, RigidBody *b,
                                    Vec3 normal, Vec3 contact_point, float penetration,
                                    float restitution, float dt) {
    c->body_a = a;
    c->body_b = b;
    c->J_linear_a = normal;
    vec3_scale_to(&c->J_linear_a, &c->J_linear_a, -1.0f);
    
    Vec3 ra, rb;
    vec3_sub_to(&ra, &contact_point, &a->position);
    vec3_cross_to(&c->J_angular_a, &ra, &normal);
    vec3_scale_to(&c->J_angular_a, &c->J_angular_a, -1.0f);
    
    if (b) {
        c->J_linear_b = normal;
        vec3_sub_to(&rb, &contact_point, &b->position);
        vec3_cross_to(&c->J_angular_b, &rb, &normal);
    } else {
        vec3_set(&c->J_linear_b, 0.0f, 0.0f, 0.0f);
        vec3_set(&c->J_angular_b, 0.0f, 0.0f, 0.0f);
    }
    
    Mat3 invIa, invIb;
    rigidbody_get_world_inv_inertia(&invIa, a);
    if (b) {
        rigidbody_get_world_inv_inertia(&invIb, b);
    }
    
    float inv_mass_sum = a->inv_mass + (b ? b->inv_mass : 0.0f);
    Vec3 invIa_Jang_a;
    mat3_vec3_multiply(&invIa_Jang_a, &invIa, &c->J_angular_a);
    float ang_term_a = vec3_dot_p(&c->J_angular_a, &invIa_Jang_a);
    
    float ang_term_b = 0.0f;
    if (b) {
        Vec3 invIb_Jang_b;
        mat3_vec3_multiply(&invIb_Jang_b, &invIb, &c->J_angular_b);
        ang_term_b = vec3_dot_p(&c->J_angular_b, &invIb_Jang_b);
    }
    
    float total_mass = inv_mass_sum + ang_term_a + ang_term_b;
    c->effective_mass = total_mass > 0.0f ? 1.0f / total_mass : 0.0f;
    
    float beta = 0.2f;
    float slop = 0.01f;
    float pen_corr = penetration - slop;
    if (pen_corr < 0.0f) pen_corr = 0.0f;
    float bias_term = (pen_corr / dt) * beta;
    
    /* Calculate relative velocity for restitution */
    Vec3 va_contact, wa_cross_ra;
    vec3_cross_to(&wa_cross_ra, &a->angular_velocity, &ra);
    vec3_add_to(&va_contact, &a->velocity, &wa_cross_ra);
    
    Vec3 vb_contact;
    if (b) {
        Vec3 wb_cross_rb;
        vec3_cross_to(&wb_cross_rb, &b->angular_velocity, &rb);
        vec3_add_to(&vb_contact, &b->velocity, &wb_cross_rb);
    } else {
        vec3_set(&vb_contact, 0.0f, 0.0f, 0.0f);
    }
    
    Vec3 relative_vel;
    vec3_sub_to(&relative_vel, &vb_contact, &va_contact);
    float vel_along_n = vec3_dot_p(&relative_vel, &normal);
    if (vel_along_n < -0.5f) {
        bias_term += -restitution * vel_along_n;
    }
    
    c->bias = -bias_term;
    c->accumulated_impulse = 0.0f;
    c->limit_min = 0.0f;
    c->limit_max = 1e10f;
    c->parent_normal_idx = -1;
}

static void init_friction_constraint(SolverConstraint *c, RigidBody *a, RigidBody *b,
                                     Vec3 tangent, Vec3 contact_point, int parent_idx) {
    c->body_a = a;
    c->body_b = b;
    c->J_linear_a = tangent;
    vec3_scale_to(&c->J_linear_a, &c->J_linear_a, -1.0f);
    
    Vec3 ra, rb;
    vec3_sub_to(&ra, &contact_point, &a->position);
    vec3_cross_to(&c->J_angular_a, &ra, &tangent);
    vec3_scale_to(&c->J_angular_a, &c->J_angular_a, -1.0f);
    
    if (b) {
        c->J_linear_b = tangent;
        vec3_sub_to(&rb, &contact_point, &b->position);
        vec3_cross_to(&c->J_angular_b, &rb, &tangent);
    } else {
        vec3_set(&c->J_linear_b, 0.0f, 0.0f, 0.0f);
        vec3_set(&c->J_angular_b, 0.0f, 0.0f, 0.0f);
    }
    
    Mat3 invIa, invIb;
    rigidbody_get_world_inv_inertia(&invIa, a);
    if (b) {
        rigidbody_get_world_inv_inertia(&invIb, b);
    }
    
    float inv_mass_sum = a->inv_mass + (b ? b->inv_mass : 0.0f);
    Vec3 invIa_Jang_a;
    mat3_vec3_multiply(&invIa_Jang_a, &invIa, &c->J_angular_a);
    float ang_term_a = vec3_dot_p(&c->J_angular_a, &invIa_Jang_a);
    
    float ang_term_b = 0.0f;
    if (b) {
        Vec3 invIb_Jang_b;
        mat3_vec3_multiply(&invIb_Jang_b, &invIb, &c->J_angular_b);
        ang_term_b = vec3_dot_p(&c->J_angular_b, &invIb_Jang_b);
    }
    
    float total_mass = inv_mass_sum + ang_term_a + ang_term_b;
    c->effective_mass = total_mass > 0.0f ? 1.0f / total_mass : 0.0f;
    
    c->bias = 0.0f;
    c->accumulated_impulse = 0.0f;
    c->limit_min = 0.0f;
    c->limit_max = 0.0f;
    c->parent_normal_idx = parent_idx;
}

static void init_ball_socket_constraint(SolverConstraint *constraints, int *count, Joint *j, float dt) {
    RigidBody *a = j->body_a;
    RigidBody *b = j->body_b;
    
    Mat3 Ra, Rb;
    mat3_from_quat(&Ra, &a->orientation);
    Vec3 ra;
    mat3_vec3_multiply(&ra, &Ra, &j->local_anchor_a);
    
    Vec3 wa;
    vec3_add_to(&wa, &a->position, &ra);
    Vec3 wb;
    Vec3 rb;
    if (b) {
        mat3_from_quat(&Rb, &b->orientation);
        mat3_vec3_multiply(&rb, &Rb, &j->local_anchor_b);
        vec3_add_to(&wb, &b->position, &rb);
    } else {
        vec3_set(&wb, j->local_anchor_b.x, j->local_anchor_b.y, j->local_anchor_b.z);
        vec3_set(&rb, 0.0f, 0.0f, 0.0f);
    }
    
    Vec3 diff;
    vec3_sub_to(&diff, &wb, &wa);
    Vec3 axes[3];
    vec3_set(&axes[0], 1.0f, 0.0f, 0.0f);
    vec3_set(&axes[1], 0.0f, 1.0f, 0.0f);
    vec3_set(&axes[2], 0.0f, 0.0f, 1.0f);
    
    int i;
    for (i = 0; i < 3; i++) {
        SolverConstraint *c = &constraints[*count];
        c->body_a = a;
        c->body_b = b;
        c->J_linear_a = axes[i];
        vec3_scale_to(&c->J_linear_a, &c->J_linear_a, -1.0f);
        vec3_cross_to(&c->J_angular_a, &ra, &axes[i]);
        vec3_scale_to(&c->J_angular_a, &c->J_angular_a, -1.0f);
        
        if (b) {
            c->J_linear_b = axes[i];
            vec3_cross_to(&c->J_angular_b, &rb, &axes[i]);
        } else {
            vec3_set(&c->J_linear_b, 0.0f, 0.0f, 0.0f);
            vec3_set(&c->J_angular_b, 0.0f, 0.0f, 0.0f);
        }
        
        Mat3 invIa, invIb;
        rigidbody_get_world_inv_inertia(&invIa, a);
        if (b) {
            rigidbody_get_world_inv_inertia(&invIb, b);
        }
        
        float inv_mass_sum = a->inv_mass + (b ? b->inv_mass : 0.0f);
        Vec3 invIa_Jang_a;
        mat3_vec3_multiply(&invIa_Jang_a, &invIa, &c->J_angular_a);
        float ang_term_a = vec3_dot_p(&c->J_angular_a, &invIa_Jang_a);
        
        float ang_term_b = 0.0f;
        if (b) {
            Vec3 invIb_Jang_b;
            mat3_vec3_multiply(&invIb_Jang_b, &invIb, &c->J_angular_b);
            ang_term_b = vec3_dot_p(&c->J_angular_b, &invIb_Jang_b);
        }
        
        float total_mass = inv_mass_sum + ang_term_a + ang_term_b;
        c->effective_mass = total_mass > 0.0f ? 1.0f / total_mass : 0.0f;
        
        float beta = 0.25f;
        float error = vec3_dot_p(&diff, &axes[i]);
        c->bias = (error / dt) * beta;
        c->accumulated_impulse = 0.0f;
        c->limit_min = -1e10f;
        c->limit_max = 1e10f;
        c->parent_normal_idx = -1;
        (*count)++;
    }
}

static void init_hinge_constraint(SolverConstraint *constraints, int *count, Joint *j, float dt) {
    /* First establish Ball-and-Socket components */
    init_ball_socket_constraint(constraints, count, j, dt);
    
    RigidBody *a = j->body_a;
    RigidBody *b = j->body_b;
    
    Mat3 Ra, Rb;
    mat3_from_quat(&Ra, &a->orientation);
    
    Vec3 H_a;
    mat3_vec3_multiply(&H_a, &Ra, &j->local_axis_a);
    vec3_normalize_to(&H_a, &H_a);
    
    Vec3 H_b, U_b, V_b;
    if (b) {
        mat3_from_quat(&Rb, &b->orientation);
        mat3_vec3_multiply(&H_b, &Rb, &j->local_axis_a);
        vec3_normalize_to(&H_b, &H_b);
        
        Vec3 helper;
        if (fabsf(H_b.x) < 0.8f) vec3_set(&helper, 1.0f, 0.0f, 0.0f);
        else vec3_set(&helper, 0.0f, 1.0f, 0.0f);
        
        vec3_cross_to(&U_b, &H_b, &helper);
        vec3_normalize_to(&U_b, &U_b);
        vec3_cross_to(&V_b, &H_b, &U_b);
    } else {
        vec3_set(&H_b, j->local_axis_a.x, j->local_axis_a.y, j->local_axis_a.z);
        vec3_normalize_to(&H_b, &H_b);
        
        Vec3 helper;
        if (fabsf(H_b.x) < 0.8f) vec3_set(&helper, 1.0f, 0.0f, 0.0f);
        else vec3_set(&helper, 0.0f, 1.0f, 0.0f);
        
        vec3_cross_to(&U_b, &H_b, &helper);
        vec3_normalize_to(&U_b, &U_b);
        vec3_cross_to(&V_b, &H_b, &U_b);
    }
    
    Vec3 rot_axes[2];
    rot_axes[0] = U_b;
    rot_axes[1] = V_b;
    
    int i;
    for (i = 0; i < 2; i++) {
        SolverConstraint *c = &constraints[*count];
        c->body_a = a;
        c->body_b = b;
        vec3_set(&c->J_linear_a, 0.0f, 0.0f, 0.0f);
        c->J_angular_a = rot_axes[i];
        vec3_scale_to(&c->J_angular_a, &c->J_angular_a, -1.0f);
        
        vec3_set(&c->J_linear_b, 0.0f, 0.0f, 0.0f);
        if (b) {
            c->J_angular_b = rot_axes[i];
        } else {
            vec3_set(&c->J_angular_b, 0.0f, 0.0f, 0.0f);
        }
        
        Mat3 invIa, invIb;
        rigidbody_get_world_inv_inertia(&invIa, a);
        if (b) {
            rigidbody_get_world_inv_inertia(&invIb, b);
        }
        
        Vec3 invIa_Jang_a;
        mat3_vec3_multiply(&invIa_Jang_a, &invIa, &c->J_angular_a);
        float ang_term_a = vec3_dot_p(&c->J_angular_a, &invIa_Jang_a);
        
        float ang_term_b = 0.0f;
        if (b) {
            Vec3 invIb_Jang_b;
            mat3_vec3_multiply(&invIb_Jang_b, &invIb, &c->J_angular_b);
            ang_term_b = vec3_dot_p(&c->J_angular_b, &invIb_Jang_b);
        }
        
        float total_mass = ang_term_a + ang_term_b;
        c->effective_mass = total_mass > 0.0f ? 1.0f / total_mass : 0.0f;
        
        float error = vec3_dot_p(&H_a, &rot_axes[i]);
        float beta = 0.25f;
        c->bias = (error / dt) * beta;
        c->accumulated_impulse = 0.0f;
        c->limit_min = -1e10f;
        c->limit_max = 1e10f;
        c->parent_normal_idx = -1;
        (*count)++;
    }
}

/* Backward-compatible distance constraint solver using old-style dynamics */
static void distance_constraint_apply(Joint *j) {
    RigidBody *a = j->body_a;
    RigidBody *b = j->body_b;
    
    Mat3 Ra, Rb;
    mat3_from_quat(&Ra, &a->orientation);
    if (b) {
        mat3_from_quat(&Rb, &b->orientation);
    }
    
    Vec3 pa, pb, ra, rb;
    mat3_vec3_multiply(&ra, &Ra, &j->local_anchor_a);
    vec3_add_to(&pa, &a->position, &ra);
    
    if (b) {
        mat3_vec3_multiply(&rb, &Rb, &j->local_anchor_b);
        vec3_add_to(&pb, &b->position, &rb);
    } else {
        vec3_set(&pb, j->local_anchor_b.x, j->local_anchor_b.y, j->local_anchor_b.z);
        vec3_set(&rb, 0.0f, 0.0f, 0.0f);
    }
    
    Vec3 d_vec;
    vec3_sub_to(&d_vec, &pb, &pa);
    float d = vec3_length_p(&d_vec);
    if (d < 0.001f) return;
    
    Vec3 normal;
    vec3_normalize_to(&normal, &d_vec);
    
    float spring_f = j->stiffness * (d - j->target_distance);
    
    Vec3 wa_cross_ra;
    vec3_cross_to(&wa_cross_ra, &a->angular_velocity, &ra);
    Vec3 va_contact;
    vec3_add_to(&va_contact, &a->velocity, &wa_cross_ra);
    
    Vec3 vb_contact;
    if (b) {
        Vec3 wb_cross_rb;
        vec3_cross_to(&wb_cross_rb, &b->angular_velocity, &rb);
        vec3_add_to(&vb_contact, &b->velocity, &wb_cross_rb);
    } else {
        vec3_set(&vb_contact, 0.0f, 0.0f, 0.0f);
    }
    
    Vec3 relative_vel;
    vec3_sub_to(&relative_vel, &vb_contact, &va_contact);
    float damping_f = j->damping * vec3_dot_p(&relative_vel, &normal);
    
    float total_force = spring_f + damping_f;
    Vec3 force_vec;
    vec3_scale_to(&force_vec, &normal, total_force);
    
    if (!a->is_static) {
        vec3_add_to(&a->force, &a->force, &force_vec);
        Vec3 torque;
        vec3_cross_to(&torque, &ra, &force_vec);
        vec3_add_to(&a->torque, &a->torque, &torque);
    }
    if (b && !b->is_static) {
        vec3_sub_to(&b->force, &b->force, &force_vec);
        Vec3 torque;
        vec3_cross_to(&torque, &rb, &force_vec);
        vec3_sub_to(&b->torque, &b->torque, &torque);
    }
}

/* === DYNAMICS INTEGRATION === */

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
    
    Mat3 inv_inertia_world;
    rigidbody_get_world_inv_inertia(&inv_inertia_world, body);
    
    Vec3 angular_accel;
    mat3_vec3_multiply(&angular_accel, &inv_inertia_world, &body->torque);
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
        
        float q_len = sqrtf(new_orient.w * new_orient.w + new_orient.x * new_orient.x +
                            new_orient.y * new_orient.y + new_orient.z * new_orient.z);
        if (q_len > 0.0001f) {
            float inv_q_len = 1.0f / q_len;
            new_orient.w *= inv_q_len;
            new_orient.x *= inv_q_len;
            new_orient.y *= inv_q_len;
            new_orient.z *= inv_q_len;
        }
        body->orientation = new_orient;
    }
    vec3_set(&body->torque, 0.0f, 0.0f, 0.0f);
}

/* === 3D Perspective Rasterizer === */

static int zcc_abs(int x) {
    return x < 0 ? -x : x;
}

static void draw_line(unsigned char *fb, int width, int height,
                       int x0, int y0, int x1, int y1,
                       unsigned char r, unsigned char g, unsigned char b) {
    if (x0 < -4000 || x0 > 4000 || y0 < -4000 || y0 > 4000 ||
        x1 < -4000 || x1 > 4000 || y1 < -4000 || y1 > 4000) {
        return;
    }
    int dx = zcc_abs(x1 - x0);
    int dy = zcc_abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height) {
            int idx = (y0 * width + x0) * 3;
            fb[idx + 0] = r;
            fb[idx + 1] = g;
            fb[idx + 2] = b;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_dashed_line(unsigned char *fb, int width, int height,
                             int x0, int y0, int x1, int y1,
                             unsigned char r, unsigned char g, unsigned char b) {
    if (x0 < -4000 || x0 > 4000 || y0 < -4000 || y0 > 4000 ||
        x1 < -4000 || x1 > 4000 || y1 < -4000 || y1 > 4000) {
        return;
    }
    int dx = zcc_abs(x1 - x0);
    int dy = zcc_abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int count = 0;
    
    while (1) {
        if ((count % 8) < 4) {
            if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height) {
                int idx = (y0 * width + x0) * 3;
                fb[idx + 0] = r; fb[idx + 1] = g; fb[idx + 2] = b;
            }
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
        count++;
    }
}

static void draw_wire_circle(unsigned char *fb, int width, int height,
                             int cx, int cy, int radius,
                             unsigned char r, unsigned char g, unsigned char b) {
    if (radius <= 0 || radius > 4000) return;
    if (cx < -4000 || cx > 4000 || cy < -4000 || cy > 4000) return;
    int x = radius;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        int pxs[8] = { cx + x, cx + y, cx - y, cx - x, cx - x, cx - y, cx + y, cx + x };
        int pys[8] = { cy + y, cy + x, cy + x, cy + y, cy - y, cy - x, cy - x, cy - y };
        
        int i;
        for (i = 0; i < 8; i++) {
            if (pxs[i] >= 0 && pxs[i] < width && pys[i] >= 0 && pys[i] < height) {
                int idx = (pys[i] * width + pxs[i]) * 3;
                fb[idx + 0] = r; fb[idx + 1] = g; fb[idx + 2] = b;
            }
        }
        
        y += 1;
        if (err <= 0) {
            err += 2 * y + 1;
        } else {
            x -= 1;
            err += 2 * (y - x) + 1;
        }
    }
}

static void draw_grid_floor(unsigned char *fb, int width, int height,
                            float cam_x, float cam_y, float cam_z, float scale) {
    float ground_y = -4.0f;
    unsigned char r = 40, g = 40, b = 40;
    float x;
    for (x = -12.0f; x <= 12.0f; x += 2.0f) {
        Vec3 start, end;
        vec3_set(&start, x, ground_y, -24.0f);
        vec3_set(&end, x, ground_y, 0.0f);
        
        float to_z_s = start.z - cam_z;
        float to_z_e = end.z - cam_z;
        if (to_z_s < -0.1f && to_z_e < -0.1f) {
            int sx = (int)((start.x - cam_x) * (scale / (-to_z_s))) + width / 2;
            int sy = (int)(-(start.y - cam_y) * (scale / (-to_z_s))) + height / 2;
            int ex = (int)((end.x - cam_x) * (scale / (-to_z_e))) + width / 2;
            int ey = (int)(-(end.y - cam_y) * (scale / (-to_z_e))) + height / 2;
            draw_line(fb, width, height, sx, sy, ex, ey, r, g, b);
        }
    }
    
    float z;
    for (z = -24.0f; z <= 0.0f; z += 2.0f) {
        Vec3 start, end;
        vec3_set(&start, -12.0f, ground_y, z);
        vec3_set(&end, 12.0f, ground_y, z);
        
        float to_z_s = start.z - cam_z;
        float to_z_e = end.z - cam_z;
        if (to_z_s < -0.1f && to_z_e < -0.1f) {
            int sx = (int)((start.x - cam_x) * (scale / (-to_z_s))) + width / 2;
            int sy = (int)(-(start.y - cam_y) * (scale / (-to_z_s))) + height / 2;
            int ex = (int)((end.x - cam_x) * (scale / (-to_z_e))) + width / 2;
            int ey = (int)(-(end.y - cam_y) * (scale / (-to_z_e))) + height / 2;
            draw_line(fb, width, height, sx, sy, ex, ey, r, g, b);
        }
    }
}

static void render_physics(RigidBody *bodies, int num_bodies,
                           Joint *joints, int num_joints,
                           unsigned char *fb, int height, int width) {
    /* Clear Framebuffer to dark grid space */
    memset(fb, 10, height * width * 3);
    
    float cam_x = 0.0f, cam_y = 5.0f, cam_z = 15.0f;
    float scale = 400.0f;
    
    draw_grid_floor(fb, width, height, cam_x, cam_y, cam_z, scale);
    
    int b;
    for (b = 0; b < num_bodies; b++) {
        RigidBody *body = &bodies[b];
        
        if (body->kind == SHAPE_SPHERE) {
            float to_z = body->position.z - cam_z;
            if (to_z > -0.1f) continue;
            
            float p_scale = scale / (-to_z);
            int px = (int)((body->position.x - cam_x) * p_scale) + width / 2;
            int py = (int)(-(body->position.y - cam_y) * p_scale) + height / 2;
            int radius = (int)(body->radius * p_scale);
            if (radius <= 0 || radius > 4000) continue;
            if (px < -4000 || px > 4000 || py < -4000 || py > 4000) continue;
            
            float r_sq = (float)(radius * radius);
            int dy, dx;
            for (dy = -radius; dy <= radius; dy++) {
                for (dx = -radius; dx <= radius; dx++) {
                    float dist_sq = (float)(dx * dx + dy * dy);
                    if (dist_sq <= r_sq) {
                        int x = px + dx;
                        int y = py + dy;
                        if (x >= 0 && x < width && y >= 0 && y < height) {
                            float dz = sqrtf(r_sq - dist_sq);
                            Vec3 local_normal;
                            vec3_set(&local_normal, (float)dx / (float)radius, (float)-dy / (float)radius, dz / (float)radius);
                            
                            Quat inv_orient;
                            inv_orient.w = body->orientation.w;
                            inv_orient.x = -body->orientation.x;
                            inv_orient.y = -body->orientation.y;
                            inv_orient.z = -body->orientation.z;
                            
                            Vec3 body_normal;
                            quat_rotate_vec(&body_normal, &inv_orient, &local_normal);
                            
                            float freq = 6.0f;
                            float val_x = sinf(body_normal.x * freq);
                            float val_y = sinf(body_normal.y * freq);
                            float val_z = sinf(body_normal.z * freq);
                            
                            float pattern = 1.0f;
                            if ((val_x > 0.0f ? 1 : 0) ^ (val_y > 0.0f ? 1 : 0) ^ (val_z > 0.0f ? 1 : 0)) {
                                pattern = 0.5f;
                            }
                            
                            float light = 1.0f - sqrtf(dist_sq) / (float)radius;
                            light = light * 0.7f + 0.3f;
                            light *= pattern;
                            
                            int idx = (y * width + x) * 3;
                            fb[idx + 0] = (unsigned char)(body->r * light);
                            fb[idx + 1] = (unsigned char)(body->g * light);
                            fb[idx + 2] = (unsigned char)(body->b * light);
                        }
                    }
                }
            }
        } else if (body->kind == SHAPE_OBB) {
            Vec3 vertices[8];
            get_obb_vertices(vertices, body);
            
            int screen_x[8], screen_y[8];
            int visible[8];
            
            int i;
            for (i = 0; i < 8; i++) {
                float to_z = vertices[i].z - cam_z;
                if (to_z > -0.1f) {
                    visible[i] = 0;
                } else {
                    visible[i] = 1;
                    float p_scale = scale / (-to_z);
                    screen_x[i] = (int)((vertices[i].x - cam_x) * p_scale) + width / 2;
                    screen_y[i] = (int)(-(vertices[i].y - cam_y) * p_scale) + height / 2;
                }
            }
            
            int edges[12][2] = {
                {0, 1}, {1, 3}, {3, 2}, {2, 0},
                {4, 5}, {5, 7}, {7, 6}, {6, 4},
                {0, 4}, {1, 5}, {2, 6}, {3, 7}
            };
            
            for (i = 0; i < 12; i++) {
                int u = edges[i][0];
                int v = edges[i][1];
                if (visible[u] && visible[v]) {
                    draw_line(fb, width, height, screen_x[u], screen_y[u], screen_x[v], screen_y[v],
                              body->r, body->g, body->b);
                }
            }
            
            /* Add axis lines at OBB center */
            Vec3 axes[3];
            get_obb_axes(axes, body);
            for (i = 0; i < 3; i++) {
                Vec3 end;
                float extent = (i == 0 ? body->half_extents.x : (i == 1 ? body->half_extents.y : body->half_extents.z));
                vec3_scale_to(&end, &axes[i], extent * 1.5f);
                vec3_add_to(&end, &body->position, &end);
                
                float to_z_c = body->position.z - cam_z;
                float to_z_e = end.z - cam_z;
                if (to_z_c < -0.1f && to_z_e < -0.1f) {
                    int cx = (int)((body->position.x - cam_x) * (scale / (-to_z_c))) + width / 2;
                    int cy = (int)(-(body->position.y - cam_y) * (scale / (-to_z_c))) + height / 2;
                    int ex = (int)((end.x - cam_x) * (scale / (-to_z_e))) + width / 2;
                    int ey = (int)(-(end.y - cam_y) * (scale / (-to_z_e))) + height / 2;
                    
                    unsigned char ar = i == 0 ? 255 : 0;
                    unsigned char ag = i == 1 ? 255 : 0;
                    unsigned char ab = i == 2 ? 255 : 0;
                    draw_line(fb, width, height, cx, cy, ex, ey, ar, ag, ab);
                }
            }
        } else if (body->kind == SHAPE_CAPSULE) {
            Vec3 a, b;
            get_capsule_endpoints(body, &a, &b);
            
            float to_z_a = a.z - cam_z;
            float to_z_b = b.z - cam_z;
            if (to_z_a < -0.1f && to_z_b < -0.1f) {
                float scale_a = scale / (-to_z_a);
                float scale_b = scale / (-to_z_b);
                int ax = (int)((a.x - cam_x) * scale_a) + width / 2;
                int ay = (int)(-(a.y - cam_y) * scale_a) + height / 2;
                int bx = (int)((b.x - cam_x) * scale_b) + width / 2;
                int by = (int)(-(b.y - cam_y) * scale_b) + height / 2;
                
                int rad_a = (int)(body->radius * scale_a);
                int rad_b = (int)(body->radius * scale_b);
                
                /* Draw main segment */
                draw_line(fb, width, height, ax, ay, bx, by, body->r, body->g, body->b);
                
                /* Draw wire endings */
                draw_wire_circle(fb, width, height, ax, ay, rad_a, body->r, body->g, body->b);
                draw_wire_circle(fb, width, height, bx, by, rad_b, body->r, body->g, body->b);
                
                /* Draw side silhouettes */
                Vec3 ab;
                vec3_sub_to(&ab, &b, &a);
                Vec3 normal_ab;
                vec3_normalize_to(&normal_ab, &ab);
                
                Vec3 perp;
                vec3_set(&perp, -normal_ab.y, normal_ab.x, 0.0f);
                vec3_normalize_to(&perp, &perp);
                
                Vec3 p1_a, p1_b, p2_a, p2_b;
                Vec3 offset_a, offset_b;
                vec3_scale_to(&offset_a, &perp, body->radius);
                vec3_scale_to(&offset_b, &perp, body->radius);
                
                vec3_add_to(&p1_a, &a, &offset_a);
                vec3_add_to(&p1_b, &b, &offset_b);
                vec3_sub_to(&p2_a, &a, &offset_a);
                vec3_sub_to(&p2_b, &b, &offset_b);
                
                float to_z_p1a = p1_a.z - cam_z;
                float to_z_p1b = p1_b.z - cam_z;
                if (to_z_p1a < -0.1f && to_z_p1b < -0.1f) {
                    int p1ax = (int)((p1_a.x - cam_x) * (scale / (-to_z_p1a))) + width / 2;
                    int p1ay = (int)(-(p1_a.y - cam_y) * (scale / (-to_z_p1a))) + height / 2;
                    int p1bx = (int)((p1_b.x - cam_x) * (scale / (-to_z_p1b))) + width / 2;
                    int p1by = (int)(-(p1_b.y - cam_y) * (scale / (-to_z_p1b))) + height / 2;
                    draw_line(fb, width, height, p1ax, p1ay, p1bx, p1by, body->r, body->g, body->b);
                }
                
                float to_z_p2a = p2_a.z - cam_z;
                float to_z_p2b = p2_b.z - cam_z;
                if (to_z_p2a < -0.1f && to_z_p2b < -0.1f) {
                    int p2ax = (int)((p2_a.x - cam_x) * (scale / (-to_z_p2a))) + width / 2;
                    int p2ay = (int)(-(p2_a.y - cam_y) * (scale / (-to_z_p2a))) + height / 2;
                    int p2bx = (int)((p2_b.x - cam_x) * (scale / (-to_z_p2b))) + width / 2;
                    int p2by = (int)(-(p2_b.y - cam_y) * (scale / (-to_z_p2b))) + height / 2;
                    draw_line(fb, width, height, p2ax, p2ay, p2bx, p2by, body->r, body->g, body->b);
                }
            }
        }
    }
    
    /* Draw active Joints */
    int j_idx;
    for (j_idx = 0; j_idx < num_joints; j_idx++) {
        Joint *joint = &joints[j_idx];
        RigidBody *a = joint->body_a;
        RigidBody *b = joint->body_b;
        
        Mat3 Ra, Rb;
        mat3_from_quat(&Ra, &a->orientation);
        Vec3 ra;
        mat3_vec3_multiply(&ra, &Ra, &joint->local_anchor_a);
        
        Vec3 wa;
        vec3_add_to(&wa, &a->position, &ra);
        Vec3 wb;
        if (b) {
            mat3_from_quat(&Rb, &b->orientation);
            Vec3 rb;
            mat3_vec3_multiply(&rb, &Rb, &joint->local_anchor_b);
            vec3_add_to(&wb, &b->position, &rb);
        } else {
            vec3_set(&wb, joint->local_anchor_b.x, joint->local_anchor_b.y, joint->local_anchor_b.z);
        }
        
        float to_z_wa = wa.z - cam_z;
        float to_z_wb = wb.z - cam_z;
        if (to_z_wa < -0.1f && to_z_wb < -0.1f) {
            int wax = (int)((wa.x - cam_x) * (scale / (-to_z_wa))) + width / 2;
            int way = (int)(-(wa.y - cam_y) * (scale / (-to_z_wa))) + height / 2;
            int wbx = (int)((wb.x - cam_x) * (scale / (-to_z_wb))) + width / 2;
            int wby = (int)(-(wb.y - cam_y) * (scale / (-to_z_wb))) + height / 2;
            
            unsigned char jr = joint->kind == JOINT_HINGE ? 255 : 200;
            unsigned char jg = joint->kind == JOINT_HINGE ? 255 : 200;
            unsigned char jb = joint->kind == JOINT_HINGE ? 0 : 255;
            
            draw_dashed_line(fb, width, height, wax, way, wbx, wby, jr, jg, jb);
        }
    }
}

/* === MAIN PROGRAM & SCENARIOS === */

#ifndef TESTING_MODE
int main(int argc, char **argv) {
    int width = 640;
    int height = 480;
    float dt = 0.016f;
    int num_steps = 140;
    int scenario_idx = 3; /* Suspension Bridge default */
    
    int mode = 0; /* 0 = final frame, 1 = all frames animation stream */
    if (argc > 1) {
        scenario_idx = argv[1][0] - '0';
        if (scenario_idx < 1 || scenario_idx > 6) scenario_idx = 3;
    }
    if (argc > 2) {
        mode = argv[2][0] - '0';
        if (mode < 0 || mode > 1) mode = 0;
    }
    
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "ZCC EXPERIMENT 5: 3D RIGID BODY SIMULATOR\n");
    fprintf(stderr, "Scenario selected: %d\n", scenario_idx);
    
    RigidBody bodies[24];
    int num_bodies = 0;
    
    Joint joints[16];
    int num_joints = 0;
    
    Plane ground;
    vec3_set(&ground.normal, 0.0f, 1.0f, 0.0f);
    ground.offset = -4.0f;
    
    if (scenario_idx == 1) {
        /* Scenario 1: Tower Collapse & Multi-body playground */
        rigidbody_init_sphere(&bodies[num_bodies++], 0.0f, 8.0f, -8.0f, 1.2f, 0.6f, 255, 50, 50);
        rigidbody_init_obb(&bodies[num_bodies++], 0.0f, -3.0f, -8.0f, 0.0f, 10.0f, 0.5f, 10.0f, 100, 100, 100);
        
        rigidbody_init_obb(&bodies[num_bodies++], -1.0f, 0.0f, -8.0f, 1.0f, 0.5f, 0.8f, 0.5f, 50, 255, 50);
        rigidbody_init_obb(&bodies[num_bodies++], 1.0f, 0.0f, -8.0f, 1.0f, 0.5f, 0.8f, 0.5f, 50, 50, 255);
        rigidbody_init_obb(&bodies[num_bodies++], 0.0f, 1.8f, -8.0f, 1.2f, 1.5f, 0.4f, 0.5f, 255, 255, 50);
        
        rigidbody_init_capsule(&bodies[num_bodies++], -0.5f, 4.0f, -8.0f, 1.0f, 0.3f, 0.8f, 255, 50, 255);
        rigidbody_init_capsule(&bodies[num_bodies++], 0.5f, 4.0f, -8.0f, 1.0f, 0.3f, 0.8f, 50, 255, 255);
        
        bodies[0].velocity.y = -3.0f;
        bodies[5].angular_velocity.x = 2.0f;
        
    } else if (scenario_idx == 2) {
        /* Scenario 2: Rigid Newton's Cradle (Spheres + Ball & Socket joints) */
        float spacing = 1.0f;
        float height_y = 4.0f;
        float length = 4.0f;
        
        int i;
        for (i = 0; i < 4; i++) {
            float x = (i - 1.5f) * spacing;
            /* Static anchor spheres */
            rigidbody_init_sphere(&bodies[num_bodies++], x, height_y, -8.0f, 0.0f, 0.2f, 150, 150, 150);
            
            /* Dynamic suspended spheres */
            unsigned char r = i == 0 ? 255 : 50;
            unsigned char g = i == 0 ? 50 : 150;
            unsigned char b = i == 0 ? 50 : 255;
            
            float py = height_y - length;
            rigidbody_init_sphere(&bodies[num_bodies++], x, py, -8.0f, 1.0f, 0.45f, r, g, b);
            
            /* Set Point-to-Point constraints */
            joints[num_joints].kind = JOINT_BALL_SOCKET;
            joints[num_joints].body_a = &bodies[num_bodies - 2]; /* Anchor */
            joints[num_joints].body_b = &bodies[num_bodies - 1]; /* Bob */
            vec3_set(&joints[num_joints].local_anchor_a, 0.0f, 0.0f, 0.0f);
            vec3_set(&joints[num_joints].local_anchor_b, 0.0f, length, 0.0f);
            num_joints++;
        }
        
        /* Displace first bob to begin dynamic swings */
        bodies[1].position.x -= 2.5f;
        bodies[1].position.y += 1.5f;
        
    } else if (scenario_idx == 3) {
        /* Scenario 3: Joint Suspension Bridge with dynamic sphere crossing */
        float start_x = -7.0f;
        float plank_width = 1.6f;
        float plank_mass = 0.5f;
        
        /* Initial static post */
        rigidbody_init_obb(&bodies[num_bodies++], start_x, 1.0f, -8.0f, 0.0f, 0.5f, 1.0f, 1.0f, 100, 100, 100);
        
        int i;
        for (i = 0; i < 7; i++) {
            float px = start_x + plank_width * (i + 1);
            rigidbody_init_obb(&bodies[num_bodies++], px, 1.0f, -8.0f, plank_mass, plank_width * 0.45f, 0.15f, 1.0f, 200, 150, 100);
            
            /* Ball and socket hinges linking consecutive planks */
            joints[num_joints].kind = JOINT_BALL_SOCKET;
            joints[num_joints].body_a = &bodies[num_bodies - 2];
            joints[num_joints].body_b = &bodies[num_bodies - 1];
            vec3_set(&joints[num_joints].local_anchor_a, plank_width * 0.5f, 0.0f, 0.0f);
            vec3_set(&joints[num_joints].local_anchor_b, -plank_width * 0.5f, 0.0f, 0.0f);
            num_joints++;
        }
        
        /* Ending static post (placed at plank_width * 8 to align anchors perfectly) */
        rigidbody_init_obb(&bodies[num_bodies++], start_x + plank_width * 8, 1.0f, -8.0f, 0.0f, 0.5f, 1.0f, 1.0f, 100, 100, 100);
        joints[num_joints].kind = JOINT_BALL_SOCKET;
        joints[num_joints].body_a = &bodies[num_bodies - 2];
        joints[num_joints].body_b = &bodies[num_bodies - 1];
        vec3_set(&joints[num_joints].local_anchor_a, plank_width * 0.5f, 0.0f, 0.0f);
        vec3_set(&joints[num_joints].local_anchor_b, -plank_width * 0.5f, 0.0f, 0.0f);
        num_joints++;
        
        /* Dynamic sphere rolling across bridge */
        rigidbody_init_sphere(&bodies[num_bodies++], start_x, 4.0f, -8.0f, 2.0f, 0.6f, 255, 50, 50);
        bodies[num_bodies - 1].velocity.x = 3.5f;
        
    } else if (scenario_idx == 4) {
        /* Scenario 4: Capsule Stairs tumbling */
        rigidbody_init_obb(&bodies[num_bodies++], -3.5f, 0.5f, -8.0f, 0.0f, 2.0f, 0.3f, 2.0f, 120, 120, 120);
        rigidbody_init_obb(&bodies[num_bodies++], -0.5f, -1.0f, -8.0f, 0.0f, 2.0f, 0.3f, 2.0f, 120, 120, 120);
        rigidbody_init_obb(&bodies[num_bodies++], 2.5f, -2.5f, -8.0f, 0.0f, 2.0f, 0.3f, 2.0f, 120, 120, 120);
        
        rigidbody_init_capsule(&bodies[num_bodies++], -4.0f, 3.0f, -8.0f, 1.0f, 0.35f, 0.9f, 255, 100, 255);
        rigidbody_init_capsule(&bodies[num_bodies++], -3.0f, 5.5f, -8.0f, 1.0f, 0.35f, 0.9f, 100, 255, 100);
        bodies[num_bodies - 2].angular_velocity.z = -5.0f;
        bodies[num_bodies - 1].angular_velocity.y = 4.0f;
        
    } else if (scenario_idx == 5) {
        /* Scenario 5: Double Hinge Pendulum Constraint Chain */
        rigidbody_init_sphere(&bodies[num_bodies++], 0.0f, 5.0f, -8.0f, 0.0f, 0.3f, 255, 255, 255); /* Pivot */
        
        /* Bar A (placed at y=3.2 with half-height 1.8, endpoint is at y=5.0) */
        rigidbody_init_capsule(&bodies[num_bodies++], 0.0f, 3.2f, -8.0f, 1.5f, 0.25f, 1.8f, 255, 128, 50);
        joints[num_joints].kind = JOINT_HINGE;
        joints[num_joints].body_a = &bodies[num_bodies - 2];
        joints[num_joints].body_b = &bodies[num_bodies - 1];
        vec3_set(&joints[num_joints].local_anchor_a, 0.0f, 0.0f, 0.0f);
        vec3_set(&joints[num_joints].local_anchor_b, 0.0f, 1.8f, 0.0f);
        vec3_set(&joints[num_joints].local_axis_a, 0.0f, 0.0f, 1.0f); /* Hinge rotates along Z axis */
        num_joints++;
        
        /* Bar B (placed at y=-0.4 with half-height 1.8, aligned to bottom of Bar A) */
        rigidbody_init_capsule(&bodies[num_bodies++], 0.0f, -0.4f, -8.0f, 1.2f, 0.25f, 1.8f, 50, 128, 255);
        joints[num_joints].kind = JOINT_HINGE;
        joints[num_joints].body_a = &bodies[num_bodies - 2];
        joints[num_joints].body_b = &bodies[num_bodies - 1];
        vec3_set(&joints[num_joints].local_anchor_a, 0.0f, -1.8f, 0.0f);
        vec3_set(&joints[num_joints].local_anchor_b, 0.0f, 1.8f, 0.0f);
        vec3_set(&joints[num_joints].local_axis_a, 0.0f, 0.0f, 1.0f);
        num_joints++;
        
        /* Force out-of-plane torque */
        bodies[1].angular_velocity.z = 8.0f;
        bodies[2].velocity.x = 5.0f;
        
    } else {
        /* Scenario 6: Friction Slopes test */
        /* Slope boxes */
        rigidbody_init_obb(&bodies[num_bodies++], -3.0f, 0.0f, -8.0f, 0.0f, 3.0f, 0.2f, 2.0f, 150, 150, 150);
        rigidbody_init_obb(&bodies[num_bodies++], 3.0f, -1.0f, -8.0f, 0.0f, 3.0f, 0.2f, 2.0f, 150, 150, 150);
        
        Quat rot_a, rot_b;
        Vec3 rot_axis;
        vec3_set(&rot_axis, 0.0f, 0.0f, 1.0f);
        quat_from_axis_angle(&rot_a, &rot_axis, 0.45f);
        quat_from_axis_angle(&rot_b, &rot_axis, -0.45f);
        
        bodies[0].orientation = rot_a;
        bodies[1].orientation = rot_b;
        
        /* Slipping OBBs and Spheres */
        rigidbody_init_obb(&bodies[num_bodies++], -3.5f, 2.5f, -8.0f, 1.0f, 0.5f, 0.5f, 0.5f, 255, 100, 100);
        rigidbody_init_sphere(&bodies[num_bodies++], 2.5f, 1.5f, -8.0f, 1.0f, 0.5f, 100, 255, 100);
    }
    
    unsigned char *framebuffer = (unsigned char *)malloc(height * width * 3);
    fprintf(stderr, "Broadphase Initialized. Simulated bodies count: %d\n", num_bodies);
    fprintf(stderr, "Running simulation iterations (%d steps)...\n", num_steps);
    
    int step;
    for (step = 0; step < num_steps; step++) {
        int i, j;
        
        /* Apply forces (Gravity) */
        for (i = 0; i < num_bodies; i++) {
            if (!bodies[i].is_static) {
                Vec3 gravity;
                vec3_set(&gravity, 0.0f, -9.81f * bodies[i].mass, 0.0f);
                vec3_add_to(&bodies[i].force, &bodies[i].force, &gravity);
            }
        }
        
        /* Collect Collisions (Broadphase) */
        Contact contacts[256];
        int num_contacts = 0;
        
        for (i = 0; i < num_bodies; i++) {
            /* Ground Plane Collision */
            detect_collision_dispatch(&bodies[i], NULL, &ground, contacts, &num_contacts);
            
            /* Body to Body Collision */
            for (j = i + 1; j < num_bodies; j++) {
                detect_collision_dispatch(&bodies[i], &bodies[j], NULL, contacts, &num_contacts);
            }
        }
        
        /* Build PGS constraints lists */
        SolverConstraint constraints_pool[768];
        int constraints_count = 0;
        
        /* Add contact constraints */
        for (i = 0; i < num_contacts; i++) {
            Contact *c = &contacts[i];
            
            /* Normal constraint */
            int normal_idx = constraints_count;
            init_contact_constraint(&constraints_pool[constraints_count],
                                    c->body_a, c->body_b, c->normal, c->point, c->penetration,
                                    0.25f, dt);
            constraints_count++;
            
            /* Build friction tangents vectors */
            Vec3 t1, t2;
            if (fabsf(c->normal.x) > 0.577f) {
                vec3_set(&t1, c->normal.y, -c->normal.x, 0.0f);
            } else {
                vec3_set(&t1, 0.0f, c->normal.z, -c->normal.y);
            }
            vec3_normalize_to(&t1, &t1);
            vec3_cross_to(&t2, &c->normal, &t1);
            
            /* Tangent 1 constraint */
            init_friction_constraint(&constraints_pool[constraints_count],
                                     c->body_a, c->body_b, t1, c->point, normal_idx);
            constraints_count++;
            
            /* Tangent 2 constraint */
            init_friction_constraint(&constraints_pool[constraints_count],
                                     c->body_a, c->body_b, t2, c->point, normal_idx);
            constraints_count++;
        }
        
        /* Add Joint constraints */
        for (i = 0; i < num_joints; i++) {
            Joint *joint = &joints[i];
            if (joint->kind == JOINT_BALL_SOCKET) {
                init_ball_socket_constraint(constraints_pool, &constraints_count, joint, dt);
            } else if (joint->kind == JOINT_HINGE) {
                init_hinge_constraint(constraints_pool, &constraints_count, joint, dt);
            }
        }
        
        /* Projected Gauss-Seidel Solver Loop (6 iterations) */
        int iter;
        int max_iters = 6;
        for (iter = 0; iter < max_iters; iter++) {
            for (i = 0; i < constraints_count; i++) {
                SolverConstraint *c = &constraints_pool[i];
                RigidBody *ba = c->body_a;
                RigidBody *bb = c->body_b;
                
                /* Relative velocity along Jacobian */
                float v_rel = vec3_dot_p(&c->J_linear_a, &ba->velocity) +
                              vec3_dot_p(&c->J_angular_a, &ba->angular_velocity);
                if (bb) {
                    v_rel += vec3_dot_p(&c->J_linear_b, &bb->velocity) +
                             vec3_dot_p(&c->J_angular_b, &bb->angular_velocity);
                }
                
                float delta_impulse = -(v_rel + c->bias) * c->effective_mass;
                
                /* Handle limits (normal constraint, friction constraint, joints) */
                float old_impulse = c->accumulated_impulse;
                float new_impulse = old_impulse + delta_impulse;
                
                if (c->parent_normal_idx != -1) {
                    /* Friction boundary based on accumulated normal force (Coulomb law) */
                    float normal_impulse = constraints_pool[c->parent_normal_idx].accumulated_impulse;
                    float friction_limit = normal_impulse * 0.4f; /* mu = 0.4 */
                    if (new_impulse < -friction_limit) new_impulse = -friction_limit;
                    if (new_impulse > friction_limit) new_impulse = friction_limit;
                } else {
                    /* Standard clamping limits */
                    if (new_impulse < c->limit_min) new_impulse = c->limit_min;
                    if (new_impulse > c->limit_max) new_impulse = c->limit_max;
                }
                
                delta_impulse = new_impulse - old_impulse;
                c->accumulated_impulse = new_impulse;
                
                /* Apply velocities corrections */
                if (!ba->is_static) {
                    Vec3 dv; vec3_scale_to(&dv, &c->J_linear_a, delta_impulse * ba->inv_mass);
                    vec3_add_to(&ba->velocity, &ba->velocity, &dv);
                    
                    Mat3 invIa; rigidbody_get_world_inv_inertia(&invIa, ba);
                    Vec3 dw; vec3_scale_to(&dw, &c->J_angular_a, delta_impulse);
                    mat3_vec3_multiply(&dw, &invIa, &dw);
                    vec3_add_to(&ba->angular_velocity, &ba->angular_velocity, &dw);
                }
                if (bb && !bb->is_static) {
                    Vec3 dv; vec3_scale_to(&dv, &c->J_linear_b, delta_impulse * bb->inv_mass);
                    vec3_add_to(&bb->velocity, &bb->velocity, &dv);
                    
                    Mat3 invIb; rigidbody_get_world_inv_inertia(&invIb, bb);
                    Vec3 dw; vec3_scale_to(&dw, &c->J_angular_b, delta_impulse);
                    mat3_vec3_multiply(&dw, &invIb, &dw);
                    vec3_add_to(&bb->angular_velocity, &bb->angular_velocity, &dw);
                }
            }
        }
        
        /* Integrate Positions & Rotations */
        for (i = 0; i < num_bodies; i++) {
            rigidbody_integrate(&bodies[i], dt);
        }
        
        if (mode == 1) {
            render_physics(bodies, num_bodies, joints, num_joints, framebuffer, height, width);
            printf("P6\n%d %d\n255\n", width, height);
            for (int j = 0; j < height; j++) {
                for (int i = 0; i < width; i++) {
                    int idx = (j * width + i) * 3;
                    fwrite(&framebuffer[idx], 1, 3, stdout);
                }
            }
        }
        
        if (step % 14 == 0) {
            fprintf(stderr, "\rProgress: %d%%", (step * 100) / num_steps);
        }
    }
    
    fprintf(stderr, "\rProgress: 100%%\n");
    
    if (mode == 0) {
        fprintf(stderr, "Generating final PPM frame render...\n");
        render_physics(bodies, num_bodies, joints, num_joints, framebuffer, height, width);
        printf("P6\n%d %d\n255\n", width, height);
        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                int idx = (j * width + i) * 3;
                fwrite(&framebuffer[idx], 1, 3, stdout);
            }
        }
    }
    
    free(framebuffer);
    fprintf(stderr, "Physics Render Pipeline Finished Successfully.\n");
    return 0;
}
#endif

/* ZCC Static Analysis compiler pass verification function definitions */
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
