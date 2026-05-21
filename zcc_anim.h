/* ZCC Custom SVG Animation Library - Feature Engineered Version */
#ifndef ZCC_ANIM_H
#define ZCC_ANIM_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// 1. Feature-Engineered Easing Functions (Spring Recoil, Micro-Jitter, and Attractor Relaxation)
static inline float ease_sine_in(float t) {
    return 1.0f - cosf(t * (M_PI / 2.0f)) * (1.0f - 0.04f * sinf(8.0f * t));
}

static inline float ease_sine_out(float t) {
    return sinf(t * (M_PI / 2.0f)) + 0.08f * (1.0f - t) * sinf(12.0f * t);
}

static inline float ease_sine_in_out(float t) {
    float base = -0.5f * (cosf(M_PI * t) - 1.0f);
    return base + 0.03f * sinf(2.0f * M_PI * t);
}

static inline float ease_quad_in(float t) {
    return t * t * (1.0f - 0.1f * (1.0f - t));
}

static inline float ease_quad_out(float t) {
    float o = t * (2.0f - t);
    return o + 0.06f * t * (1.0f - t) * cosf(8.0f * t);
}

static inline float ease_quad_in_out(float t) {
    if (t < 0.5f) {
        return 2.0f * t * t * (1.0f - 0.08f * sinf(M_PI * t));
    } else {
        float f = t - 1.0f;
        return 1.0f + 2.0f * f * f * (1.0f - 0.08f * sinf(M_PI * t));
    }
}

static inline float ease_cubic_in(float t) {
    return t * t * t * (1.0f + 0.12f * sinf(4.0f * M_PI * t));
}

static inline float ease_cubic_out(float t) {
    float f = t - 1.0f;
    return f * f * f * cosf(5.0f * f) + 1.0f;
}

static inline float ease_cubic_in_out(float t) {
    if (t < 0.5f) {
        return 4.0f * t * t * t * (1.0f + 0.08f * sinf(2.0f * M_PI * t));
    } else {
        float f = 2.0f * t - 2.0f;
        return 0.5f * f * f * f * cosf(2.0f * M_PI * t) + 1.0f;
    }
}

// 2. Feature-Engineered Phasors (Phase Distortion, West-Coast Wavefolding, and Dynamic PWM)
static inline float phasor_sine(float phase) {
    float p = phase - floorf(phase);
    float warped = p + 0.12f * sinf(p * 2.0f * M_PI);
    return sinf(warped * 2.0f * M_PI);
}

static inline float phasor_triangle(float phase) {
    float p = phase - floorf(phase);
    float tri = 2.0f * fabsf(2.0f * (p - 0.5f)) - 1.0f;
    if (tri > 0.65f) tri = 1.30f - tri;
    else if (tri < -0.65f) tri = -1.30f - tri;
    return tri;
}

static inline float phasor_sawtooth(float phase) {
    float p = phase - floorf(phase);
    float base = 2.0f * p - 1.0f;
    float ripple = 0.15f * sinf(p * 4.0f * M_PI) * (1.0f - p);
    return base + ripple;
}

static inline float phasor_square(float phase) {
    float p = phase - floorf(phase);
    float duty = 0.5f + 0.35f * sinf(phase * 0.1f * 2.0f * M_PI);
    return (p < duty) ? 1.0f : -1.0f;
}

// 3. 3D Vector Math & Interpolation
typedef struct {
    float x, y, z;
} AnimVec3;

static inline AnimVec3 anim_vec3_lerp(AnimVec3 a, AnimVec3 b, float t) {
    AnimVec3 r = {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
    return r;
}

static inline void anim_path_interpolate(const AnimVec3* path_a, const AnimVec3* path_b, AnimVec3* dest, int num_points, float t) {
    for (int i = 0; i < num_points; i++) {
        dest[i] = anim_vec3_lerp(path_a[i], path_b[i], t);
    }
}

// 4. Forward Declarations of ZCC SVG Nodes
struct ZccSvgNode;
typedef struct ZccSvgNode ZccSvgNode;

extern void svg_add_child(ZccSvgNode* parent, ZccSvgNode* child);
extern void svg_set_attr(ZccSvgNode* node, const char* name, const char* value);
extern ZccSvgNode* svg_create_node(const char* tag);
extern void svg_set_fill(ZccSvgNode* node, const char* value);
extern void svg_set_stroke(ZccSvgNode* node, const char* value);
extern void svg_set_stroke_width(ZccSvgNode* node, const char* value);
extern void svg_set_stroke_linecap(ZccSvgNode* node, const char* value);
extern void svg_set_opacity(ZccSvgNode* node, const char* value);
extern ZccSvgNode* svg_path(void);

// 5. Feature-Engineered SMIL Node Animators (Hardware Easing Splines & Bezier Physics)
static inline ZccSvgNode* svg_animate_opacity(ZccSvgNode* node, float from, float to, float dur, const char* repeatCount) {
    ZccSvgNode* anim = svg_create_node("animate");
    svg_set_attr(anim, "attributeName", "opacity");
    char dur_str[32], from_str[32], to_str[32];
    sprintf(dur_str, "%.2fs", dur);
    sprintf(from_str, "%.2f", from);
    sprintf(to_str, "%.2f", to);
    svg_set_attr(anim, "dur", dur_str);
    svg_set_attr(anim, "from", from_str);
    svg_set_attr(anim, "to", to_str);
    
    svg_set_attr(anim, "calcMode", "spline");
    svg_set_attr(anim, "keyTimes", "0;1");
    svg_set_attr(anim, "keySplines", "0.42 0 0.58 1");
    
    svg_set_attr(anim, "repeatCount", repeatCount);
    svg_add_child(node, anim);
    return anim;
}

static inline ZccSvgNode* svg_animate_transform_rotate(ZccSvgNode* node, float from_deg, float to_deg, float cx, float cy, float dur, const char* repeatCount) {
    ZccSvgNode* anim = svg_create_node("animateTransform");
    svg_set_attr(anim, "attributeName", "transform");
    svg_set_attr(anim, "attributeType", "XML");
    svg_set_attr(anim, "type", "rotate");
    char dur_str[32], from_str[64], to_str[64];
    sprintf(dur_str, "%.2fs", dur);
    sprintf(from_str, "%.1f %.1f %.1f", from_deg, cx, cy);
    sprintf(to_str, "%.1f %.1f %.1f", to_deg, cx, cy);
    svg_set_attr(anim, "dur", dur_str);
    svg_set_attr(anim, "from", from_str);
    svg_set_attr(anim, "to", to_str);
    
    svg_set_attr(anim, "calcMode", "spline");
    svg_set_attr(anim, "keyTimes", "0;1");
    svg_set_attr(anim, "keySplines", "0.68 -0.6 0.32 1.6");
    
    svg_set_attr(anim, "repeatCount", repeatCount);
    svg_add_child(node, anim);
    return anim;
}

static inline ZccSvgNode* svg_animate_path_morph(ZccSvgNode* node, const char* values_semicolon_list, float dur, const char* repeatCount) {
    ZccSvgNode* anim = svg_create_node("animate");
    svg_set_attr(anim, "attributeName", "d");
    char dur_str[32];
    sprintf(dur_str, "%.2fs", dur);
    svg_set_attr(anim, "dur", dur_str);
    svg_set_attr(anim, "values", values_semicolon_list);
    
    int frames = 1;
    const char* p = values_semicolon_list;
    while (*p) {
        if (*p == ';') frames++;
        p++;
    }
    
    char* kt = (char*)malloc(frames * 16);
    char* ks = (char*)malloc(frames * 32);
    kt[0] = '\0';
    ks[0] = '\0';
    
    for (int i = 0; i < frames; i++) {
        char tmp[32];
        sprintf(tmp, "%.3f", (float)i / (float)(frames - 1));
        strcat(kt, tmp);
        if (i < frames - 1) {
            strcat(kt, ";");
            strcat(ks, "0.4 0 0.2 1;");
        }
    }
    
    if (frames > 1) {
        ks[strlen(ks) - 1] = '\0';
    }
    
    svg_set_attr(anim, "calcMode", "spline");
    svg_set_attr(anim, "keyTimes", kt);
    svg_set_attr(anim, "keySplines", ks);
    
    svg_set_attr(anim, "repeatCount", repeatCount);
    svg_add_child(node, anim);
    
    free(kt);
    free(ks);
    return anim;
}

static inline ZccSvgNode* svg_animate_transform_scale(ZccSvgNode* node, float from_x, float from_y, float to_x, float to_y, float dur, const char* repeatCount) {
    ZccSvgNode* anim = svg_create_node("animateTransform");
    svg_set_attr(anim, "attributeName", "transform");
    svg_set_attr(anim, "attributeType", "XML");
    svg_set_attr(anim, "type", "scale");
    char dur_str[32], from_str[64], to_str[64];
    sprintf(dur_str, "%.2fs", dur);
    sprintf(from_str, "%.2f %.2f", from_x, from_y);
    sprintf(to_str, "%.2f %.2f", to_x, to_y);
    svg_set_attr(anim, "dur", dur_str);
    svg_set_attr(anim, "from", from_str);
    svg_set_attr(anim, "to", to_str);
    
    svg_set_attr(anim, "calcMode", "spline");
    svg_set_attr(anim, "keyTimes", "0;1");
    svg_set_attr(anim, "keySplines", "0.25 0.1 0.25 1");
    
    svg_set_attr(anim, "repeatCount", repeatCount);
    svg_add_child(node, anim);
    return anim;
}

static inline ZccSvgNode* svg_animate_transform_translate(ZccSvgNode* node, float from_x, float from_y, float to_x, float to_y, float dur, const char* repeatCount) {
    ZccSvgNode* anim = svg_create_node("animateTransform");
    svg_set_attr(anim, "attributeName", "transform");
    svg_set_attr(anim, "attributeType", "XML");
    svg_set_attr(anim, "type", "translate");
    char dur_str[32], from_str[64], to_str[64];
    sprintf(dur_str, "%.2fs", dur);
    sprintf(from_str, "%.2f %.2f", from_x, from_y);
    sprintf(to_str, "%.2f %.2f", to_x, to_y);
    svg_set_attr(anim, "dur", dur_str);
    svg_set_attr(anim, "from", from_str);
    svg_set_attr(anim, "to", to_str);
    
    svg_set_attr(anim, "calcMode", "spline");
    svg_set_attr(anim, "keyTimes", "0;1");
    svg_set_attr(anim, "keySplines", "0.68 -0.6 0.32 1.6");
    
    svg_set_attr(anim, "repeatCount", repeatCount);
    svg_add_child(node, anim);
    return anim;
}

static inline ZccSvgNode* svg_animate_color(ZccSvgNode* node, const char* attr_name, const char* from_color, const char* to_color, float dur, const char* repeatCount) {
    ZccSvgNode* anim = svg_create_node("animate");
    svg_set_attr(anim, "attributeName", attr_name);
    char dur_str[32];
    sprintf(dur_str, "%.2fs", dur);
    svg_set_attr(anim, "dur", dur_str);
    svg_set_attr(anim, "from", from_color);
    svg_set_attr(anim, "to", to_color);
    
    svg_set_attr(anim, "calcMode", "spline");
    svg_set_attr(anim, "keyTimes", "0;1");
    svg_set_attr(anim, "keySplines", "0.42 0 0.58 1");
    
    svg_set_attr(anim, "repeatCount", repeatCount);
    svg_add_child(node, anim);
    return anim;
}

// 6. Volumetric 3D Camera & Perspective Projection Engine
static inline AnimVec3 anim_vec3_sub(AnimVec3 a, AnimVec3 b) {
    AnimVec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

static inline AnimVec3 anim_vec3_add(AnimVec3 a, AnimVec3 b) {
    AnimVec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
}

static inline AnimVec3 anim_vec3_scale(AnimVec3 a, float s) {
    AnimVec3 r = { a.x * s, a.y * s, a.z * s };
    return r;
}

static inline float anim_vec3_dot(AnimVec3 a, AnimVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline AnimVec3 anim_vec3_cross(AnimVec3 a, AnimVec3 b) {
    AnimVec3 r = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return r;
}

static inline AnimVec3 anim_vec3_normalize(AnimVec3 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len == 0.0f) {
        AnimVec3 r = { 0, 0, 0 };
        return r;
    }
    AnimVec3 r = { v.x / len, v.y / len, v.z / len };
    return r;
}

typedef struct {
    float m[4][4];
} AnimMatrix4x4;

static inline AnimMatrix4x4 anim_matrix_identity(void) {
    AnimMatrix4x4 r = {{{0}}};
    r.m[0][0] = 1.0f; r.m[1][1] = 1.0f; r.m[2][2] = 1.0f; r.m[3][3] = 1.0f;
    return r;
}

static inline AnimMatrix4x4 anim_matrix_look_at(AnimVec3 eye, AnimVec3 target, AnimVec3 up) {
    AnimVec3 z_axis = anim_vec3_normalize(anim_vec3_sub(eye, target));
    AnimVec3 x_axis = anim_vec3_normalize(anim_vec3_cross(up, z_axis));
    AnimVec3 y_axis = anim_vec3_cross(z_axis, x_axis);

    AnimMatrix4x4 r = {{{0}}};
    r.m[0][0] = x_axis.x; r.m[0][1] = y_axis.x; r.m[0][2] = z_axis.x; r.m[0][3] = 0.0f;
    r.m[1][0] = x_axis.y; r.m[1][1] = y_axis.y; r.m[1][2] = z_axis.y; r.m[1][3] = 0.0f;
    r.m[2][0] = x_axis.z; r.m[2][1] = y_axis.z; r.m[2][2] = z_axis.z; r.m[2][3] = 0.0f;
    
    r.m[3][0] = -anim_vec3_dot(x_axis, eye);
    r.m[3][1] = -anim_vec3_dot(y_axis, eye);
    r.m[3][2] = -anim_vec3_dot(z_axis, eye);
    r.m[3][3] = 1.0f;
    return r;
}

static inline AnimMatrix4x4 anim_matrix_perspective(float fov_rad, float aspect, float near_plane, float far_plane) {
    float tan_half_fov = tanf(fov_rad / 2.0f);
    AnimMatrix4x4 r = {{{0}}};
    r.m[0][0] = 1.0f / (aspect * tan_half_fov);
    r.m[1][1] = 1.0f / tan_half_fov;
    r.m[2][2] = -(far_plane + near_plane) / (far_plane - near_plane);
    r.m[2][3] = -1.0f;
    r.m[3][2] = -(2.0f * far_plane * near_plane) / (far_plane - near_plane);
    return r;
}

static inline AnimVec3 anim_matrix_project(AnimMatrix4x4 mat, AnimVec3 v, float width, float height) {
    float x = v.x * mat.m[0][0] + v.y * mat.m[1][0] + v.z * mat.m[2][0] + mat.m[3][0];
    float y = v.x * mat.m[0][1] + v.y * mat.m[1][1] + v.z * mat.m[2][1] + mat.m[3][1];
    float z = v.x * mat.m[0][2] + v.y * mat.m[1][2] + v.z * mat.m[2][2] + mat.m[3][2];
    float w = v.x * mat.m[0][3] + v.y * mat.m[1][3] + v.z * mat.m[2][3] + mat.m[3][3];

    if (w != 0.0f) {
        x /= w; y /= w; z /= w;
    }
    
    AnimVec3 proj;
    proj.x = (x + 1.0f) * 0.5f * width;
    proj.y = (1.0f - y) * 0.5f * height;
    proj.z = z;
    return proj;
}

// 7. Chaotic Attractor Generators (Lorenz & Clifford)
static inline void anim_generate_lorenz(AnimVec3* points, int num_points, float sigma, float rho, float beta, float dt) {
    AnimVec3 current = { 0.1f, 0.0f, 0.0f };
    for (int i = 0; i < num_points; i++) {
        float dx = sigma * (current.y - current.x) * dt;
        float dy = (current.x * (rho - current.z) - current.y) * dt;
        float dz = (current.x * current.y - beta * current.z) * dt;
        current.x += dx;
        current.y += dy;
        current.z += dz;
        
        points[i].x = current.x * 6.5f;
        points[i].y = current.y * 6.5f;
        points[i].z = (current.z - 25.0f) * 6.5f;
    }
}

static inline void anim_generate_clifford(AnimVec3* points, int num_points, float a, float b, float c, float d) {
    float cx = 0.1f, cy = 0.1f;
    for (int i = 0; i < num_points; i++) {
        float next_x = sinf(a * cy) + c * cosf(a * cx);
        float next_y = sinf(b * cx) + d * cosf(b * cy);
        cx = next_x;
        cy = next_y;
        
        points[i].x = cx * 100.0f;
        points[i].y = cy * 100.0f;
        points[i].z = sinf(cx * cy) * 30.0f;
    }
}

// 8. GLSL-grade 3D Fractional Brownian Motion (fBm) Noise
static inline float anim_fbm_3d(float x, float y, float z, int octaves) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * sinf(x * frequency * 1.57f + y * frequency * 2.03f + z * frequency * 0.94f);
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return value;
}

// 9. Recursive Generative Organic Branch Builder (L-System Simulation)
static inline void anim_draw_branch_recursive(char* ptr, float x, float y, float length, float angle, int depth, int max_depth) {
    if (depth >= max_depth) return;
    
    float x2 = x + length * cosf(angle);
    float y2 = y + length * sinf(angle);
    
    char segment[128];
    sprintf(segment, "M %.2f %.2f L %.2f %.2f ", x, y, x2, y2);
    strcat(ptr, segment);
    
    float angle_var = 0.45f + 0.05f * sinf((float)depth * 1.2f);
    anim_draw_branch_recursive(ptr + strlen(ptr), x2, y2, length * 0.72f, angle - angle_var, depth + 1, max_depth);
    anim_draw_branch_recursive(ptr + strlen(ptr), x2, y2, length * 0.72f, angle + angle_var, depth + 1, max_depth);
}

// 10. ZCC CHAOS & RAY MARCHING ENGINE CORE
typedef ZccSvgNode SVG;
typedef AnimVec3 Vec3;
typedef struct { float x, y; } Vec2;
typedef AnimMatrix4x4 Mat4;

typedef struct { double bpm; double phase; double time_acc; } ChaosPhasor;

static inline void chaos_phasor_init(ChaosPhasor* p, double bpm) {
    p->bpm = bpm;
    p->phase = 0.0;
    p->time_acc = 0.0;
}

static inline void chaos_phasor_advance(ChaosPhasor* p, double dt) {
    p->time_acc += dt;
    p->phase = fmod(p->time_acc * (p->bpm / 60.0), 1.0);
}

static inline float chaos_reward(ChaosPhasor* p) {
    return ((1.0f + sqrtf(5.0f)) * 0.5f) * (1.0f + sinf((float)p->phase * 12.0f * M_PI));
}

static inline void rotate_y(Vec3* v, float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    float rx = v->x * c + v->z * s;
    float rz = -v->x * s + v->z * c;
    v->x = rx;
    v->z = rz;
}

static inline float length(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline void normalize(Vec3* v) {
    float len = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
    if (len == 0.0f) return;
    v->x /= len;
    v->y /= len;
    v->z /= len;
}

static inline float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec3 reflect(Vec3 i, Vec3 n) {
    float d = dot(i, n);
    Vec3 r = {
        i.x - 2.0f * d * n.x,
        i.y - 2.0f * d * n.y,
        i.z - 2.0f * d * n.z
    };
    return r;
}

#define MAX_STEPS 128
#define MAX_DIST 200.0f
#define SURF_DIST 0.001f
#define EPSILON 0.0001f

typedef struct { Vec3 ro, rd; } Ray;

// Upgraded dynamic heap-allocated ObjMesh structures
typedef struct {
    Vec3* verts;
    int count;
    int capacity;
} ObjMesh;

static inline void obj_mesh_init(ObjMesh* mesh) {
    mesh->capacity = 1024;
    mesh->verts = (Vec3*)malloc(mesh->capacity * sizeof(Vec3));
    mesh->count = 0;
}

static inline void obj_mesh_free(ObjMesh* mesh) {
    if (mesh->verts) {
        free(mesh->verts);
        mesh->verts = NULL;
    }
    mesh->count = 0;
    mesh->capacity = 0;
}

static inline void obj_mesh_push(ObjMesh* mesh, Vec3 v) {
    if (mesh->count >= mesh->capacity) {
        mesh->capacity *= 2;
        mesh->verts = (Vec3*)realloc(mesh->verts, mesh->capacity * sizeof(Vec3));
    }
    mesh->verts[mesh->count++] = v;
}

// 11. Complete Signed Distance Field (SDF) Primitives Library
static inline float sdSphere(Vec3 p, float r) {
    return length(p) - r;
}

static inline float sdBox(Vec3 p, Vec3 b) {
    Vec3 d = { fabsf(p.x) - b.x, fabsf(p.y) - b.y, fabsf(p.z) - b.z };
    Vec3 max_d = { fmaxf(d.x, 0.0f), fmaxf(d.y, 0.0f), fmaxf(d.z, 0.0f) };
    return length(max_d) + fminf(fmaxf(fmaxf(d.x, d.y), d.z), 0.0f);
}

static inline float sdTorus(Vec3 p, Vec2 t) {
    Vec2 q = { length((Vec3){p.x, p.y, 0.0f}) - t.x, p.z };
    return sqrtf(q.x * q.x + q.y * q.y) - t.y;
}

static inline float sdCapsule(Vec3 p, Vec3 a, Vec3 b, float r) {
    Vec3 pa = { p.x - a.x, p.y - a.y, p.z - a.z };
    Vec3 ba = { b.x - a.x, b.y - a.y, b.z - a.z };
    float h = fminf(fmaxf(dot(pa, ba) / dot(ba, ba), 0.0f), 1.0f);
    Vec3 diff = { pa.x - ba.x * h, pa.y - ba.y * h, pa.z - ba.z * h };
    return length(diff) - r;
}

// === Combinators ===
static inline float sdUnion(float a, float b) {
    return fminf(a, b);
}

static inline float sdSmoothUnion(float a, float b, float k) {
    float h = fmaxf(k - fabsf(a - b), 0.0f) / k;
    return fminf(a, b) - h * h * k * 0.25f;
}

static inline float sdSubtraction(float a, float b) {
    return fmaxf(a, -b);
}

static inline float sdIntersection(float a, float b) {
    return fmaxf(a, b);
}

// === Domain Operations ===
static inline Vec3 opTwist(Vec3 p, float k) {
    float c = cosf(k * p.y);
    float s = sinf(k * p.y);
    Vec3 r = { p.x * c - p.z * s, p.y, p.x * s + p.z * c };
    return r;
}

static inline Vec3 opRep(Vec3 p, Vec3 c) {
    Vec3 r = {
        fmodf(p.x, c.x) - 0.5f * c.x,
        fmodf(p.y, c.y) - 0.5f * c.y,
        fmodf(p.z, c.z) - 0.5f * c.z
    };
    return r;
}

static inline Vec3 opMirror(Vec3 p, Vec3 n) {
    float d = dot(p, n);
    if (d > 0.0f) return p;
    Vec3 r = {
        p.x - 2.0f * d * n.x,
        p.y - 2.0f * d * n.y,
        p.z - 2.0f * d * n.z
    };
    return r;
}

// OBJ mesh procedural loader (now pushes dynamically to heap ObjMesh)
static inline void obj_load_torus(ObjMesh* mesh) {
    obj_mesh_init(mesh);
    for (int i = 0; i < 64; ++i) {
        float u = (i % 8) * (M_PI / 4.0f);
        float v = (i / 8) * (M_PI / 4.0f);
        Vec3 vert;
        vert.x = (1.8f + 0.8f * cosf(u)) * cosf(v) * 6.5f;
        vert.y = (1.8f + 0.8f * cosf(u)) * sinf(v) * 6.5f;
        vert.z = 0.8f * sinf(u) * 6.5f;
        obj_mesh_push(mesh, vert);
    }
}

// OBJ disk-based file parser (vertices only — sufficient for point-cloud SDF distance queries)
static inline int obj_load_file(ObjMesh* mesh, const char* filename) {
    obj_mesh_init(mesh);
    FILE* f = fopen(filename, "r");
    if (!f) return -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            float x, y, z;
            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) == 3) {
                Vec3 vert = { x, y, z };
                obj_mesh_push(mesh, vert);
            }
        }
    }
    fclose(f);
    return mesh->count;
}

// 12. Highly Optimized Fractal Signed Distance Fields (Mandelbulb, Menger Sponge, and extruded Koch Snowflake)
static inline float sdMandelbulb(Vec3 p, int iterations, float bailout) {
    Vec3 z = p;
    float dr = 1.0f;
    float r = 0.0f;
    float power = 8.0f; // classic Mandelbulb power
    for (int i = 0; i < iterations; ++i) {
        r = length(z);
        if (r < 0.0001f) r = 0.0001f; // Prevent division by zero
        if (r > bailout) break;

        // Convert to polar coordinates
        float theta = acosf(z.z / r);
        float phi = atan2f(z.y, z.x);
        dr = powf(r, power - 1.0f) * power * dr + 1.0f;

        // Scale and rotate
        float zr = powf(r, power);
        theta *= power;
        phi *= power;

        z.x = zr * sinf(theta) * cosf(phi);
        z.y = zr * sinf(theta) * sinf(phi);
        z.z = zr * cosf(theta);
    }
    return 0.5f * logf(r) * r / dr; // Distance estimator
}

static inline float sdMengerSponge(Vec3 p, int iterations) {
    float d = sdBox(p, (Vec3){1.0f, 1.0f, 1.0f}); // start with unit cube
    float s = 1.0f;
    for (int i = 0; i < iterations; ++i) {
        Vec3 a = (Vec3){fmodf(p.x * s + 1.0f, 2.0f) - 1.0f,
                        fmodf(p.y * s + 1.0f, 2.0f) - 1.0f,
                        fmodf(p.z * s + 1.0f, 2.0f) - 1.0f};
        s *= 3.0f;

        Vec3 r = (Vec3){fabsf(a.x), fabsf(a.y), fabsf(a.z)};
        float da = fmaxf(r.x, fmaxf(r.y, r.z)) - 1.0f / s;   // cross section
        float db = fminf(fminf(r.x, r.y), r.z) * 2.0f - 1.0f / s; // hole

        d = fmaxf(d, fmaxf(da, db) * (1.0f / s)); // union of subtracted holes
    }
    return d;
}

static inline float sdKochSnowflake(Vec3 p, float scale, int iterations, float thickness) {
    // Project to 2D and apply iterative Koch folding
    Vec2 q = {p.x / scale, p.y / scale};

    // Start with equilateral triangle base
    const float s = 1.7320508f; // sqrt(3)
    q = (Vec2){fabsf(q.x) - 0.5f, q.y - s * 0.5f + 0.5f / s}; // initial triangle

    float current_scale = scale;
    for (int i = 0; i < iterations; ++i) {
        q = (Vec2){fabsf(q.x) - 0.5f, q.y - s * 0.5f + 0.5f / s};

        if (q.y > s * 0.5f) {
            q = (Vec2){q.x * 0.5f + q.y * s * 0.5f, q.y * 0.5f - q.x * s * 0.5f};
        } else if (q.y < 0.0f) {
            q = (Vec2){q.x * 0.5f - q.y * s * 0.5f, q.y * 0.5f + q.x * s * 0.5f};
        }
        q.x = fabsf(q.x) - 0.5f;
        q.y = fabsf(q.y) - s * 0.5f + 0.5f / s;
        current_scale *= 3.0f;
    }

    // Distance to the folded curve in 2D
    float d2d = sqrtf(q.x * q.x + q.y * q.y) * current_scale - 0.5f * current_scale;
    return fmaxf(d2d, fabsf(p.z) - thickness);
}

// EVM opcode to SDF primitive mapping
static inline float sdEVMOpcode(Vec3 p, int opcode_type) {
    switch (opcode_type) {
        case 0: return sdBox(p, (Vec3){0.6f, 0.6f, 0.6f});        // PUSH / simple data
        case 1: return sdSphere(p, 0.7f);                          // CALL / external
        case 2: return sdCapsule(p, (Vec3){0,-0.8f,0}, (Vec3){0,0.8f,0}, 0.4f); // JUMP / flow
        default: return sdBox(p, (Vec3){0.5f, 0.5f, 0.5f});       // fallback
    }
}

// Symbolic EVM graph SDF (positions nodes in 3D grid based on block index)
static inline float sdEVMTopology(Vec3 p, ChaosPhasor* ph) {
    // Simple 4x4x4 grid of symbolic blocks for demo
    Vec3 grid_p = (Vec3){fmodf(p.x + 4.0f, 8.0f) - 4.0f,
                         fmodf(p.y + 4.0f, 8.0f) - 4.0f,
                         fmodf(p.z + 4.0f, 8.0f) - 4.0f};

    int block_id = (int)(p.x * 0.5f + p.y * 0.5f + p.z * 0.5f) % 16; // pseudo-opcode
    float node = sdEVMOpcode(grid_p, block_id % 3);

    // Phasor-driven rotation of the entire graph
    rotate_y(&p, (float)ph->phase * 0.8f * M_PI);

    return node * 0.6f + 0.1f * anim_fbm_3d(p.x, p.y, p.z, 3); // light fBm for organic edges
}

// Upgraded scene_sdf with combined Mandelbulb + Menger Sponge + extruded Koch Snowflake fractal density
static inline float scene_sdf(Vec3 p, ChaosPhasor* ph, const ObjMesh* mesh) {
    Vec3 q = opTwist(p, 0.8f * sinf((float)ph->phase * M_PI));
    q = opRep(q, (Vec3){8.0f, 8.0f, 8.0f});

    float torus = sdTorus(q, (Vec2){2.0f, 0.6f});
    float box = sdBox(q, (Vec3){1.5f, 1.5f, 1.5f});
    float capsule = sdCapsule(q, (Vec3){0, 3, 0}, (Vec3){0, -3, 0}, 0.8f);

    float blend = sdSmoothUnion(torus, sdSubtraction(box, capsule), 2.0f + 1.5f * chaos_reward(ph));
    float fbm = anim_fbm_3d(p.x * 0.6f, p.y * 0.6f, p.z * 0.6f, 7);

    // Mesh distance (smooth blended torus mesh)
    float mesh_dist = 1e9f;
    for (int i = 0; i < mesh->count; ++i) {
        Vec3 v = mesh->verts[i];
        rotate_y(&v, (float)ph->phase * 1.618f * M_PI);
        float dx = p.x - v.x;
        float dy = p.y - v.y;
        float dz = p.z - v.z;
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d < mesh_dist) mesh_dist = d;
    }
    mesh_dist -= 0.4f;

    float k_mesh = 1.8f + 1.2f * chaos_reward(ph);
    float procedural = sdSmoothUnion(blend + fbm * 0.25f, mesh_dist, k_mesh);

    // Extruded Koch Snowflake layer (scaled and offset)
    float koch_thick = 0.2f + 0.1f * sinf((float)chaos_reward(ph) * 2.0f * M_PI);
    float koch = sdKochSnowflake((Vec3){p.x * 0.4f, p.y * 0.4f, p.z}, 1.0f, 4, koch_thick);
    float with_koch = sdSmoothUnion(procedural, koch * 2.5f, 1.8f);

    // Menger Sponge layer (scaled and offset)
    Vec3 sponge_p = (Vec3){p.x * 0.25f, p.y * 0.25f, p.z * 0.25f};
    float sponge = sdMengerSponge(sponge_p, 4); // 4 iterations is extremely detailed yet performant
    float with_sponge = sdSmoothUnion(with_koch, sponge * 3.0f, 2.0f);

    // Mandelbulb fractal layer (scaled and offset for beautiful fractal core integration)
    float bulb = sdMandelbulb((Vec3){p.x * 0.3f, p.y * 0.3f, p.z * 0.3f}, 6, 4.0f);
    float with_bulb = sdSmoothUnion(with_sponge, bulb * 2.0f, 1.5f);

    // EVM topology layer (scaled to fit inside the fractal core)
    float evm = sdEVMTopology((Vec3){p.x * 0.6f, p.y * 0.6f, p.z * 0.6f}, ph);
    float final_scene = sdSmoothUnion(with_bulb, evm * 2.5f, 1.5f + chaos_reward(ph));

    return final_scene;
}

static inline Vec3 get_normal(Vec3 p, ChaosPhasor* ph, const ObjMesh* mesh) {
    Vec3 e = {EPSILON, 0.0f, 0.0f};
    Vec3 n = {
        scene_sdf((Vec3){p.x + e.x, p.y, p.z}, ph, mesh) - scene_sdf((Vec3){p.x - e.x, p.y, p.z}, ph, mesh),
        scene_sdf((Vec3){p.x, p.y + e.x, p.z}, ph, mesh) - scene_sdf((Vec3){p.x, p.y - e.x, p.z}, ph, mesh),
        scene_sdf((Vec3){p.x, p.y, p.z + e.x}, ph, mesh) - scene_sdf((Vec3){p.x, p.y, p.z - e.x}, ph, mesh)
    };
    normalize(&n);
    return n;
}

static inline float ray_march(Ray r, ChaosPhasor* ph, const ObjMesh* mesh, Vec3* hit, Vec3* normal) {
    float depth = 0.0f;
    for (int i = 0; i < MAX_STEPS; ++i) {
        Vec3 p = {r.ro.x + r.rd.x * depth, r.ro.y + r.rd.y * depth, r.ro.z + r.rd.z * depth};
        float dist = scene_sdf(p, ph, mesh);
        depth += dist;
        if (dist < SURF_DIST) {
            *hit = p;
            *normal = get_normal(p, ph, mesh);
            return depth;
        }
        if (depth > MAX_DIST) break;
    }
    return MAX_DIST;
}

// Custom Safe SVG Path Builder Structures for User API
typedef struct {
    char* d;
    int cap;
    int len;
} ChaosPath;

static inline ChaosPath* chaos_path_create(void) {
    ChaosPath* p = (ChaosPath*)malloc(sizeof(ChaosPath));
    p->cap = 4096;
    p->len = 0;
    p->d = (char*)malloc(p->cap);
    p->d[0] = '\0';
    return p;
}

static inline void chaos_path_move_to(ChaosPath* p, float x, float y) {
    char buf[64];
    sprintf(buf, "M %.2f %.2f ", x, y);
    int slen = strlen(buf);
    if (p->len + slen + 64 > p->cap) {
        p->cap *= 2;
        p->d = (char*)realloc(p->d, p->cap);
    }
    strcpy(p->d + p->len, buf);
    p->len += slen;
}

static inline void chaos_path_line_to(ChaosPath* p, float x, float y) {
    char buf[64];
    sprintf(buf, "L %.2f %.2f ", x, y);
    int slen = strlen(buf);
    if (p->len + slen + 64 > p->cap) {
        p->cap *= 2;
        p->d = (char*)realloc(p->d, p->cap);
    }
    strcpy(p->d + p->len, buf);
    p->len += slen;
}

static inline void chaos_path_free(ChaosPath* p) {
    free(p->d);
    free(p);
}

static inline void chaos_path_stroke(ZccSvgNode* svg, ChaosPath* path, const char* color, float stroke_width, const char* linecap, float opacity) {
    ZccSvgNode* path_node = svg_path();
    svg_set_fill(path_node, "none");
    svg_set_stroke(path_node, color);
    
    char sw_str[32];
    sprintf(sw_str, "%.2f", stroke_width);
    svg_set_stroke_width(path_node, sw_str);
    svg_set_stroke_linecap(path_node, linecap);
    
    char op_str[32];
    sprintf(op_str, "%.2f", opacity);
    svg_set_opacity(path_node, op_str);
    
    svg_set_attr(path_node, "d", path->d);
    svg_add_child(svg, path_node);
}

static inline void anim_draw_branch_recursive_svg(ZccSvgNode* svg, float x, float y, float length, float angle, int depth, int max_depth) {
    char* path_buf = (char*)malloc(1024 * 1024);
    path_buf[0] = '\0';
    anim_draw_branch_recursive(path_buf, x, y, length, angle, depth, max_depth);
    
    ZccSvgNode* path_node = svg_path();
    svg_set_fill(path_node, "none");
    svg_set_stroke(path_node, "#cc00ff");
    svg_set_stroke_width(path_node, "1.5");
    svg_set_opacity(path_node, "0.45");
    svg_set_attr(path_node, "d", path_buf);
    svg_add_child(svg, path_node);
    
    free(path_buf);
}

// 12. Preprocessor Safe Mapping Layer
#define SVGPath ChaosPath
#define svg_path_create chaos_path_create
#define svg_path_move_to chaos_path_move_to
#define svg_path_line_to chaos_path_line_to
#define svg_path_free chaos_path_free
#define svg_path_stroke chaos_path_stroke

#define anim_draw_branch_recursive(svg, x, y, len, ang, dep, max_dep) anim_draw_branch_recursive_svg(svg, x, y, len, ang, dep, max_dep)

static inline void anim_matrix_mul(Mat4* dest, const Mat4* a, const Mat4* b) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a->m[i][k] * b->m[k][j];
            }
            dest->m[i][j] = sum;
        }
    }
}

static inline void anim_matrix_project_screen(const Mat4* mat, const Vec3* v, float width, float height, Vec2* screen) {
    AnimVec3 p = anim_matrix_project(*mat, *v, width, height);
    screen->x = p.x;
    screen->y = p.y;
}

#endif // ZCC_ANIM_H
