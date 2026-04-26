#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
void *malloc(unsigned long size);
void free(void *ptr);
#ifndef NULL
#define NULL ((void*)0)
#endif
/* #include <math.h> */
#include "models.h"

struct Mesh {
    Vec3 aabb_min, aabb_max;
    int count;
    Triangle *tris;
};

struct Mesh level_meshes[3] = {
    { {0,0,0}, {0,0,0}, 0, NULL }, 
    { {0,0,0}, {0,0,0}, 0, NULL },
    { {0,0,0}, {0,0,0}, 0, NULL }
};

#ifndef make_vec3
#define make_vec3(xx, yy, zz, out) do { (out)->x = (xx); (out)->y = (yy); (out)->z = (zz); } while(0)
#endif
#include "voxel_engine.h"

void init_meshes() {
    int voxel_count = 0;
    Triangle *voxel_tris = synthesize_voxel_geometry(&voxel_count);

    int total_count = MESH_SIZE + voxel_count;
    Triangle *fused_tris = malloc(total_count * sizeof(Triangle));

    /* Copy Model Geometry */
    for(int i=0; i<MESH_SIZE; i++) fused_tris[i] = mesh_triangles[i];
    /* Append Systems Graveyard Voxel Geometry */
    for(int i=0; i<voxel_count; i++) fused_tris[MESH_SIZE + i] = voxel_tris[i];
    free(voxel_tris);

    make_vec3(-1000.0, -1000.0, -1000.0, &level_meshes[0].aabb_min);
    make_vec3(1000.0, 1000.0, 1000.0, &level_meshes[0].aabb_max);
    level_meshes[0].count = total_count;
    level_meshes[0].tris = fused_tris;

    level_meshes[1] = level_meshes[0];
    level_meshes[2] = level_meshes[0];
}
double sqrt(double x);
double floor(double x);
double cos(double x);
double sin(double x);
double pow(double x, double y);   /* added for local_zoom calculation */
char *getenv(const char *name);
int atoi(const char *nptr);

#define PI 3.14159265358979

/* ------------------------------------------------------------------ vec3 */
#define v_add(a, b, out) do { (out)->x = (a)->x+(b)->x; (out)->y = (a)->y+(b)->y; (out)->z = (a)->z+(b)->z; } while(0)
#define v_sub(a, b, out) do { (out)->x = (a)->x-(b)->x; (out)->y = (a)->y-(b)->y; (out)->z = (a)->z-(b)->z; } while(0)
#define v_mul(a, d, out) do { (out)->x = (a)->x*(d); (out)->y = (a)->y*(d); (out)->z = (a)->z*(d); } while(0)
#define v_dot(a, b) ((a)->x*(b)->x + (a)->y*(b)->y + (a)->z*(b)->z)
#define v_len(v) sqrt((v)->x*(v)->x + (v)->y*(v)->y + (v)->z*(v)->z)

#define v_norm(in, out) do { \
    double _l = v_len(in); \
    if(_l>0.0){ (out)->x=(in)->x/_l; (out)->y=(in)->y/_l; (out)->z=(in)->z/_l; } \
    else { (out)->x=0; (out)->y=0; (out)->z=1; } \
} while(0)

#define v_cross(a, b, out) do { \
    double _ax=(a)->x, _ay=(a)->y, _az=(a)->z; \
    double _bx=(b)->x, _by=(b)->y, _bz=(b)->z; \
    (out)->x = _ay*_bz - _az*_by; \
    (out)->y = _az*_bx - _ax*_bz; \
    (out)->z = _ax*_by - _ay*_bx; \
} while(0)

#define v_reflect(I, N, out) do { \
    double _d = v_dot(N, I); \
    (out)->x = (I)->x - 2.0*_d*(N)->x; \
    (out)->y = (I)->y - 2.0*_d*(N)->y; \
    (out)->z = (I)->z - 2.0*_d*(N)->z; \
} while(0)

/* ------------------------------------------------------ AABB intersection */
double hit_aabb_dist(Vec3 *mn, Vec3 *mx, Vec3 *ro, Vec3 *rdinv){
    double tx0=(mn->x-ro->x)*rdinv->x, tx1=(mx->x-ro->x)*rdinv->x;
    double tmin = tx0<tx1?tx0:tx1, tmax = tx0<tx1?tx1:tx0;
    double ty0=(mn->y-ro->y)*rdinv->y, ty1=(mx->y-ro->y)*rdinv->y;
    double tymin = ty0<ty1?ty0:ty1, tymax = ty0<ty1?ty1:ty0;
    if (tmin > tymax || tymin > tmax) return 1e18;
    if (tymin > tmin) tmin = tymin;
    if (tymax < tmax) tmax = tymax;
    double tz0=(mn->z-ro->z)*rdinv->z, tz1=(mx->z-ro->z)*rdinv->z;
    double tzmin = tz0<tz1?tz0:tz1, tzmax = tz0<tz1?tz1:tz0;
    if (tmin > tzmax || tzmin > tmax) return 1e18;
    if (tzmin > tmin) tmin = tzmin;
    if (tzmax < tmax) tmax = tzmax;
    if (tmax >= tmin && tmax > 0.0) return tmin > 0.0 ? tmin : 0.0;
    return 1e18;
}

int hit_aabb_shadow(Vec3 *mn, Vec3 *mx, Vec3 *ro, Vec3 *rdinv){
    double tx0=(mn->x-ro->x)*rdinv->x, tx1=(mx->x-ro->x)*rdinv->x;
    double tmin = tx0<tx1?tx0:tx1, tmax = tx0<tx1?tx1:tx0;
    double ty0=(mn->y-ro->y)*rdinv->y, ty1=(mx->y-ro->y)*rdinv->y;
    double tymin = ty0<ty1?ty0:ty1, tymax = ty0<ty1?ty1:ty0;
    if (tmin > tymax || tymin > tmax) return 0;
    if (tymin > tmin) tmin = tymin;
    if (tymax < tmax) tmax = tymax;
    double tz0=(mn->z-ro->z)*rdinv->z, tz1=(mx->z-ro->z)*rdinv->z;
    double tzmin = tz0<tz1?tz0:tz1, tzmax = tz0<tz1?tz1:tz0;
    if (tmin > tzmax || tzmin > tmax) return 0;
    if (tzmax < tmax) tmax = tzmax;
    return tmax > 0.001;
}

/* ------------------------------------------------------------------ BVH */
typedef struct {
    Vec3 aabb_min, aabb_max;
    int left_first, tri_count;
} BVHNode;

#define MAX_BVH_NODES 15000000
BVHNode bvh_nodes[MAX_BVH_NODES];
int bvh_alloc = 0;
int mesh_bvh_roots[3];

void update_node_bounds(int node_idx, Triangle *tris, int first_tri, int tri_count) {
    BVHNode *node = &bvh_nodes[node_idx];
    int i;
    make_vec3(1e30, 1e30, 1e30, &node->aabb_min);
    make_vec3(-1e30, -1e30, -1e30, &node->aabb_max);
    for (i = 0; i < tri_count; i++) {
        Triangle *t = &tris[first_tri + i];
        node->aabb_min.x = t->v0.x < node->aabb_min.x ? t->v0.x : node->aabb_min.x;
        node->aabb_min.y = t->v0.y < node->aabb_min.y ? t->v0.y : node->aabb_min.y;
        node->aabb_min.z = t->v0.z < node->aabb_min.z ? t->v0.z : node->aabb_min.z;
        node->aabb_max.x = t->v0.x > node->aabb_max.x ? t->v0.x : node->aabb_max.x;
        node->aabb_max.y = t->v0.y > node->aabb_max.y ? t->v0.y : node->aabb_max.y;
        node->aabb_max.z = t->v0.z > node->aabb_max.z ? t->v0.z : node->aabb_max.z;

        node->aabb_min.x = t->v1.x < node->aabb_min.x ? t->v1.x : node->aabb_min.x;
        node->aabb_min.y = t->v1.y < node->aabb_min.y ? t->v1.y : node->aabb_min.y;
        node->aabb_min.z = t->v1.z < node->aabb_min.z ? t->v1.z : node->aabb_min.z;
        node->aabb_max.x = t->v1.x > node->aabb_max.x ? t->v1.x : node->aabb_max.x;
        node->aabb_max.y = t->v1.y > node->aabb_max.y ? t->v1.y : node->aabb_max.y;
        node->aabb_max.z = t->v1.z > node->aabb_max.z ? t->v1.z : node->aabb_max.z;

        node->aabb_min.x = t->v2.x < node->aabb_min.x ? t->v2.x : node->aabb_min.x;
        node->aabb_min.y = t->v2.y < node->aabb_min.y ? t->v2.y : node->aabb_min.y;
        node->aabb_min.z = t->v2.z < node->aabb_min.z ? t->v2.z : node->aabb_min.z;
        node->aabb_max.x = t->v2.x > node->aabb_max.x ? t->v2.x : node->aabb_max.x;
        node->aabb_max.y = t->v2.y > node->aabb_max.y ? t->v2.y : node->aabb_max.y;
        node->aabb_max.z = t->v2.z > node->aabb_max.z ? t->v2.z : node->aabb_max.z;
    }
}

void subdivide_bvh(int node_idx, Triangle *tris, int first_tri, int tri_count) {
    BVHNode *node = &bvh_nodes[node_idx];
    Vec3 extent;
    int axis, i, j, left_count, left_idx, right_idx;
    double split_pos, center;

    update_node_bounds(node_idx, tris, first_tri, tri_count);
    if (tri_count <= 2) {
        node->left_first = first_tri;
        node->tri_count = tri_count;
        return;
    }

    v_sub(&node->aabb_max, &node->aabb_min, &extent);
    axis = 0;
    if (extent.y > extent.x) axis = 1;
    if (extent.z > extent.x && extent.z > extent.y) axis = 2;

    split_pos = ((axis == 0 ? node->aabb_min.x : (axis == 1 ? node->aabb_min.y : node->aabb_min.z)) + 
                 (axis == 0 ? node->aabb_max.x : (axis == 1 ? node->aabb_max.y : node->aabb_max.z))) * 0.5;

    i = first_tri;
    j = first_tri + tri_count - 1;
    while (i <= j) {
        center = ((axis == 0 ? tris[i].v0.x : (axis == 1 ? tris[i].v0.y : tris[i].v0.z)) +
                  (axis == 0 ? tris[i].v1.x : (axis == 1 ? tris[i].v1.y : tris[i].v1.z)) +
                  (axis == 0 ? tris[i].v2.x : (axis == 1 ? tris[i].v2.y : tris[i].v2.z))) / 3.0;
        if (center < split_pos) {
            i++;
        } else {
            Triangle temp = tris[i];
            tris[i] = tris[j];
            tris[j] = temp;
            j--;
        }
    }

    left_count = i - first_tri;
    if (left_count == 0 || left_count == tri_count) {
        node->left_first = first_tri;
        node->tri_count = tri_count;
        return;
    }

    left_idx = bvh_alloc++;
    right_idx = bvh_alloc++;
    node->left_first = left_idx;
    node->tri_count = 0;
    subdivide_bvh(left_idx, tris, first_tri, left_count);
    subdivide_bvh(right_idx, tris, i, tri_count - left_count);
}

/* build_bvh removed — inlined in main for recursive levels */

/* ---------------------------------------------- Moller-Trumbore triangle */
double hit_tri(Triangle *tri, Vec3 *ro, Vec3 *rd){
    Vec3 e1,e2,h,s,q;
    double a,f,u,vv;
    v_sub(&tri->v1,&tri->v0,&e1);
    v_sub(&tri->v2,&tri->v0,&e2);
    v_cross(rd,&e2,&h);
    a=v_dot(&e1,&h);
    if(a>-1e-8&&a<1e-8) return -1.0;
    f=1.0/a;
    v_sub(ro,&tri->v0,&s);
    u=f*v_dot(&s,&h);
    if(u<0.0||u>1.0) return -1.0;
    v_cross(&s,&e1,&q);
    vv=f*v_dot(rd,&q);
    if(vv<0.0||u+vv>1.0) return -1.0;
    return f*v_dot(&e2,&q);
}

/* ---------------------------------------------- Determininistic Math */
double zcc_pow_fixed(double base, int exp) {
    double res = 1.0;
    while (exp > 0) {
        if (exp & 1) res *= base;
        base *= base;
        exp >>= 1;
    }
    return res;
}

/* --------------------------------------- Implicit checkerboard floor plane */
/* Plane: Y = FLOOR_Y. Returns t>0 on hit, -1.0 on miss.                     */
#define FLOOR_Y       (-2.5)
#define FLOOR_REFL     0.35
#define FLOOR_SHIN     0.0

double hit_floor(Vec3 *ro, Vec3 *rd){
    double t;
    if(rd->y > -1e-8 && rd->y < 1e-8) return -1.0; /* parallel */
    t = (FLOOR_Y - ro->y) / rd->y;
    if(t <= 0.001) return -1.0;
    return t;
}

void floor_albedo(Vec3 *p, Vec3 *out){
    /* Deterministic 1-unit checker: (floor(x)+floor(z)) parity */
    int cx = (int)floor(p->x);
    int cz = (int)floor(p->z);
    int parity = ((cx + cz) & 1);
    if(parity) make_vec3(0.85, 0.85, 0.88, out);        /* light tile */
    else       make_vec3(0.08, 0.08, 0.10, out);        /* dark  tile */
}

/* ---------------------------------- Deterministic 4-tap shadow attenuation */
/* Returns shadow density in [0,1]: 0.0 = fully lit, 1.0 = fully occluded.   */
double shadow_attenuation(Vec3 *hit, Vec3 *light_pos, int level_idx, int frame, double treble){
    /* 16-tap Stratified Sampling (4x4 jittered grid) */
    int k, i, blocked = 0;
    /* Bass-reactive spread: higher energy -> softer/more diffuse shadow border */
    double offset = 0.45 + treble * 0.2; 
    for(k = 0; k < 16; k++){
        int row = k / 4;
        int col = k % 4;
        /* Simple deterministic jitter based on frame/tap */
        double jx = ((double)col - 1.5 + (double)((frame*13 + k*7)%100)/100.0) * offset;
        double jz = ((double)row - 1.5 + (double)((frame*17 + k*3)%100)/100.0) * offset;
        
        Vec3 lp, ld, ldinv;
        struct Mesh *m;
        int hit_any = 0;
        make_vec3(light_pos->x + jx, light_pos->y, light_pos->z + jz, &lp);
        v_sub(&lp, hit, &ld); v_norm(&ld, &ld);
        make_vec3(ld.x!=0?1.0/ld.x:1e18, ld.y!=0?1.0/ld.y:1e18, ld.z!=0?1.0/ld.z:1e18, &ldinv);

        {
            int stack[64], stack_ptr;
            m = &level_meshes[level_idx];
            if(!hit_aabb_shadow(&m->aabb_min,&m->aabb_max,hit,&ldinv)) {
                /* miss */
            } else {
                stack_ptr = 0;
                stack[stack_ptr++] = mesh_bvh_roots[level_idx];
                while(stack_ptr > 0 && !hit_any) {
                    int ni = stack[--stack_ptr];
                    BVHNode *node = &bvh_nodes[ni];
                    if(!hit_aabb_shadow(&node->aabb_min, &node->aabb_max, hit, &ldinv)) continue;
                    if(node->tri_count > 0) {
                        for(i=0; i<node->tri_count; i++){
                            if(hit_tri(&m->tris[node->left_first+i], hit, &ld) > 0.001){ hit_any=1; break; }
                        }
                    } else {
                        stack[stack_ptr++] = node->left_first;
                        stack[stack_ptr++] = node->left_first + 1;
                    }
                }
            }
        }
        if(hit_any) blocked++;
    }
    return (double)blocked / 16.0;
}

/* ----------------------------------------------------------------- trace */
void trace(Vec3 *ro, Vec3 *rd, int depth, int level_idx, int frame, double bass, double mid, double treble, double level_t, Vec3 *out){
    int mi, i, best_mi, best_ti, shadow, hit_kind = 0;
    double best_t, dk, df, sa, si, refl, shin, t_floor;
    Vec3 rdinv, td, hit, e1,e2,n, nudge, color, rl, ld, rf, fd, amb, diff_k, diff_f, spec, direct, tmp, neg_rd, vd, half, lv, rd_r, rc, t1, t2;
    struct Triangle *tri;
    struct Mesh *m;

    best_t  = 1e18;
    best_mi = -1;
    best_ti = -1;

    make_vec3(rd->x!=0?1.0/rd->x:1e18,
              rd->y!=0?1.0/rd->y:1e18,
              rd->z!=0?1.0/rd->z:1e18, &rdinv);

    /* --- Only check the current level's mesh --- */
    {
        int stack[64], stack_ptr;
        m = &level_meshes[level_idx];
        if(hit_aabb_dist(&m->aabb_min, &m->aabb_max, ro, &rdinv) < best_t) {
            stack_ptr = 0;
            stack[stack_ptr++] = mesh_bvh_roots[level_idx];
            
            while(stack_ptr > 0) {
                int ni = stack[--stack_ptr];
                BVHNode *node = &bvh_nodes[ni];
                if(hit_aabb_dist(&node->aabb_min, &node->aabb_max, ro, &rdinv) >= best_t) continue;
                
                if(node->tri_count > 0) {
                    for(i=0; i<node->tri_count; i++){
                        int ti = node->left_first + i;
                        double t = hit_tri(&m->tris[ti], ro, rd);
                        if(t>0.001 && t<best_t){ best_t=t; best_mi=level_idx; best_ti=ti; }
                    }
                } else {
                    double t1 = hit_aabb_dist(&bvh_nodes[node->left_first].aabb_min, &bvh_nodes[node->left_first].aabb_max, ro, &rdinv);
                    double t2 = hit_aabb_dist(&bvh_nodes[node->left_first+1].aabb_min, &bvh_nodes[node->left_first+1].aabb_max, ro, &rdinv);
                    if(t1 > t2) {
                        if(t1 < best_t) stack[stack_ptr++] = node->left_first;
                        if(t2 < best_t) stack[stack_ptr++] = node->left_first + 1;
                    } else {
                        if(t2 < best_t) stack[stack_ptr++] = node->left_first + 1;
                        if(t1 < best_t) stack[stack_ptr++] = node->left_first;
                    }
                }
            }
        }
    }

    hit_kind = (best_mi != -1) ? 1 : 0;

    /* --- emissive short-circuit: lighting-independent glow --- */
    if(hit_kind == 1) {
        Triangle *et = &level_meshes[best_mi].tris[best_ti];
        if(et->emission > 0.0) {
            out->x = et->color.x * et->emission;
            out->y = et->color.y * et->emission;
            out->z = et->color.z * et->emission;
            return;
        }
    }

    /* --- horizon convergence / sky miss --- */
    if(hit_kind == 0){
        double a = 0.5*(rd->y+1.0);
        Vec3 lo,hi,t1,t2,sky;
        make_vec3(0.01,0.01,0.03,&lo); /* Deeper space */
        make_vec3(0.04,0.03,0.12,&hi);
        v_mul(&lo,1.0-a,&t1); v_mul(&hi,a,&t2); v_add(&t1,&t2,&sky);
        
        /* PORTAL BLEED: Interpolate sky color with the next universe level color as we zoom */
        if(level_t > 0.5) {
            Vec3 next_color;
            double bleed = (level_t - 0.5) * 2.0; 
            /* Robot Cyan -> Core Magenta -> City Gold */
            if(level_idx == 0) make_vec3(0.2, 0.0, 0.2, &next_color);
            else if(level_idx == 1) make_vec3(0.3, 0.2, 0.0, &next_color);
            else make_vec3(0.0, 0.1, 0.1, &next_color);
            
            v_mul(&sky, 1.0-bleed, &t1); v_mul(&next_color, bleed, &t2); v_add(&t1, &t2, out);
        } else {
            *out = sky;
        }
        return;
    }

    /* --- compute hit pos, normal, material --- */
    v_mul(rd, best_t, &td); v_add(ro, &td, &hit);

    tri = &level_meshes[best_mi].tris[best_ti];
    v_sub(&tri->v1,&tri->v0,&e1);
    v_sub(&tri->v2,&tri->v0,&e2);
    v_cross(&e1,&e2,&n); v_norm(&n,&n);
    if(v_dot(&n,rd)>0.0) v_mul(&n,-1.0,&n);
    color = tri->color;
    refl  = tri->reflectivity;
    shin  = tri->shininess;
    v_mul(&n,0.001,&nudge); v_add(&hit,&nudge,&hit);

    /* --- Dive Synergy soft shadow (16-tap) --- */
    make_vec3(4.0,6.0,8.0,&rl); v_norm(&rl,&ld);
    {
        Vec3 light_pos; make_vec3(4.0, 6.0, 8.0, &light_pos);
        sa = shadow_attenuation(&hit, &light_pos, level_idx, frame, treble);   /* 0..1 occlusion */
    }
    /* reuse 'shadow' as a legacy boolean for spec gate */
    shadow = (sa >= 0.999) ? 1 : 0;

    make_vec3(-3.0,2.0,-1.0,&rf); v_norm(&rf,&fd);
    /* SPECTRAL PUMPING: modulate illumination by bass energy */
    double intensity = 1.0 + bass * 0.8;
    v_mul(&color,intensity*0.30,&amb);
    dk = v_dot(&n,&ld); if(dk<0.0)dk=0.0;
    dk *= (1.0 - sa);                                /* penumbra scaling */
    v_mul(&color,intensity*dk*0.80,&diff_k);
    df = v_dot(&n,&fd); if(df<0.0)df=0.0;
    v_mul(&color,intensity*df*0.45,&diff_f);

    make_vec3(0,0,0,&spec);
    if(!shadow && shin>0.0 && dk>0.0){
        v_mul(rd,-1.0,&neg_rd); v_norm(&neg_rd,&vd);
        v_add(&ld,&vd,&lv); v_norm(&lv,&half);
        sa=v_dot(&n,&half);
        if(sa>0.0){ si=zcc_pow_fixed(sa,(int)shin); v_mul(&color,si*0.5,&spec); }
    }

    v_add(&amb,&diff_k,&tmp); v_add(&tmp,&diff_f,&direct); v_add(&direct,&spec,&direct);
    if(refl>0.0 && depth<8){
        v_reflect(rd,&n,&rd_r); v_norm(&rd_r,&rd_r);
        trace(&hit,&rd_r,depth+1,level_idx,frame,bass,mid,treble,level_t,&rc);
        v_mul(&direct,1.0-refl,&t1); v_mul(&rc,refl,&t2); v_add(&t1,&t2,out);
    } else {
        *out = direct;
    }
}

static void pulse_vertex(Vec3 *v, double b, double m, double t_freq) {
    /* Phase keyed off position hash - deterministic deep dive */
    double phase1 = v->x * 2.7 + v->y * 3.1 + v->z * 1.9;
    double phase2 = v->x * -1.8 + v->y * 4.4 + v->z * -2.2;
    Vec3 n; v_norm(v, &n);
    
    /* Deep synergistic parallel displacement */
    double displace = b * 0.45 * sin(phase1)               /* Deep heavy bass wave */
                    + m * 0.25 * cos(phase2 * 2.5)         /* Mid-range transverse ripples */
                    + t_freq * 0.15 * sin(phase1 * 6.0);   /* Treble chaotic micro-fractures */
                    
    v->x += n.x * displace;
    v->y += n.y * displace;
    v->z += n.z * displace;
}

/* ------------------------------------------------------------------ Audio Telemetry Buffer */
typedef struct { double b, m, t; } AudioEntry;
AudioEntry audio_buffer[1200];

/* ------------------------------------------------------------------ Hamiltonian Navigation */
void prime_step(Vec3 *pos, Vec3 *target, int frame) {
    /* Dynamic Hamiltonian parameters modulated by audio telemetry */
    AudioEntry *ae = &audio_buffer[frame];
    
    double total_t = (double)frame / 1200.0;
    double level_t = (total_t * 3.0) - (int)(total_t * 3.0);

    /* eta (attraction) scales with bass, epsilon (exploration) with treble, gamma (sharpness) with mid */
    double base_eta = 0.4, base_gamma = 0.5, base_epsilon = 0.08, beta = 0.1;
    double eta = base_eta * (1.0 + ae->b * 1.5) + 0.3 * level_t;
    double gamma = base_gamma * (1.0 + ae->m);
    double epsilon = base_epsilon * (1.0 + ae->t * 3.0); 
    if (level_t > 0.7) epsilon *= 1.4; /* Amplified exploration during bleed */

    Vec3 dir, noise, attractor;
    v_sub(target, pos, &attractor); v_norm(&attractor, &attractor);
    
    double H_prev = v_len(pos);
    double sigma = 1.0 / (1.0 + 1.0/(1.0 + gamma * H_prev)); 
    
    double rx = (double)((frame*137 + 123)%1000)/1000.0 - 0.5;
    double ry = (double)((frame*149 + 456)%1000)/1000.0 - 0.5;
    double rz = (double)((frame*163 + 789)%1000)/1000.0 - 0.5;
    make_vec3(rx, ry, rz, &noise);
    
    /* Advanced Inter-Level Bleed Optimized (lambda = 0.150) */
    double bleed_val = 0.0;
    if (level_t > 0.7) {
        double bleed_factor = 0.150 * (level_t - 0.7) / 0.3;
        bleed_val = bleed_factor * (1.0 + ae->m * 0.5); /* bleed influence */
    }

    /* Evolution: Step = H0 + eta * Sigma + epsilon * Noise + Bleed */
    v_mul(&attractor, 1.0 + bleed_val, &dir);
    v_mul(&attractor, eta * sigma, &attractor);
    v_add(&dir, &attractor, &dir);
    v_mul(&noise, epsilon * (1.0 + beta * H_prev), &noise);
    v_add(&dir, &noise, &dir);
    
    double step_size = 0.05 + 0.1 * ae->b;
    v_mul(&dir, step_size, &dir);
    v_add(pos, &dir, pos);
}

void get_h_field(Vec3 *pos, Vec3 *target, double *h_val) {
    Vec3 diff; v_sub(target, pos, &diff);
    *h_val = v_len(&diff);
}

void deform_liquid(double bass, double mid, double treble, int level_idx) {
    int ti;
    struct Mesh *m = &level_meshes[level_idx];
    for (ti = 0; ti < m->count; ti++) {
        struct Triangle *t = &m->tris[ti];
        pulse_vertex(&t->v0, bass, mid, treble);
        pulse_vertex(&t->v1, bass, mid, treble);
        pulse_vertex(&t->v2, bass, mid, treble);
    }
}

/* ------------------------------------------------------------------ main */

struct ThreadArg {
    int start_j, end_j;
    int width, height, current_level, frame;
    double env_bass, env_mid, env_treble, level_t;
    Vec3 ro, target, forward, up, right;
    Vec3 *framebuffer;
};

void *render_tile(void *arg) {
    struct ThreadArg *ta = (struct ThreadArg *)arg;
    int i, j, s;
    double focal_factor = 0.6 - (ta->level_t * ta->level_t * 0.35);
    for(j = ta->start_j; j < ta->end_j; j++){
        for(i = 0; i < ta->width; i++){
            Vec3 accum; make_vec3(0,0,0,&accum);
            double tx = ta->ro.x + ((double)((ta->frame*53+j*3)%100)/5000.0) * ta->env_treble;
            double ty = ta->ro.y + ((double)((ta->frame*59+i*7)%100)/5000.0) * ta->env_treble;
            Vec3 ro_j; make_vec3(tx, ty, ta->ro.z, &ro_j);
            
            for(s=0; s<16; s++){
                double jx = ((double)((ta->frame*31 + s*13 + i*7)%100)/100.0) - 0.5;
                double jy = ((double)((ta->frame*37 + s*17 + j*11)%100)/100.0) - 0.5;
                double u = ((double)i + jx) / (double)(ta->width  - 1);
                double v = ((double)j + jy) / (double)(ta->height - 1);
                double dx = (u*2.0-1.0) * focal_factor * (double)ta->width/(double)ta->height;
                double dy = (v*2.0-1.0) * focal_factor;
                
                Vec3 rd, right_comp, up_comp, forward_comp, rdn, col;
                v_mul(&ta->right,   dx,  &right_comp);
                v_mul(&ta->up,      dy,  &up_comp);
                v_mul(&ta->forward, 1.0, &forward_comp);
                v_add(&right_comp, &up_comp,      &rd);
                v_add(&rd,         &forward_comp, &rd);
                v_norm(&rd, &rdn);
                
                trace(&ro_j, &rdn, 0, ta->current_level, ta->frame, ta->env_bass, ta->env_mid, ta->env_treble, ta->level_t, &col);
                v_add(&accum, &col, &accum);
            }
            v_mul(&accum, 0.0625, &accum); /* average 16 samples for 16x SSAA */

            /* 3-COLOR SPECTRAL ENERGY GRID OVERLAY */
            if (j < (int)(ta->height * 0.15)) {
                double h_val; get_h_field(&ta->ro, &ta->target, &h_val);
                double energy = h_val / 10.0; if(energy > 1.0) energy = 1.0;
                double pulse = 0.2 + ta->env_bass * 0.8;
                Vec3 level_color, next_color, final_color;
                if (ta->current_level == 0)      { make_vec3(0, 1.0, 1.0, &level_color); make_vec3(1.0, 0, 1.0, &next_color); }
                else if (ta->current_level == 1) { make_vec3(1.0, 0, 1.0, &level_color); make_vec3(1.0, 0.84, 0, &next_color); }
                else                             { make_vec3(1.0, 0.84, 0, &level_color); make_vec3(0, 1.0, 1.0, &next_color); }
                if (ta->level_t > 0.7) {
                    double bf = (ta->level_t - 0.7) / 0.3;
                    v_mul(&level_color, 1.0-bf, &level_color); v_mul(&next_color, bf, &next_color);
                    v_add(&level_color, &next_color, &final_color);
                } else { final_color = level_color; }
                accum.x = accum.x * (1.0 - pulse*0.3) + (energy * final_color.x + (1.0-energy) * 0.2) * pulse * 0.3;
                accum.y = accum.y * (1.0 - pulse*0.3) + (energy * final_color.y + (1.0-energy) * 0.2) * pulse * 0.3;
                accum.z = accum.z * (1.0 - pulse*0.3) + (energy * final_color.z + (1.0-energy) * 0.2) * pulse * 0.3;
            }
            ta->framebuffer[j * ta->width + i] = accum;
        }
    }
    return NULL;
}

int main(){
    int width=1280, height=720, i, j;
    int frame=0, total_frames=450;
    char *f_env  = getenv("FRAME");
    char *w_env  = getenv("WIDTH");
    char *h_env  = getenv("HEIGHT");
    double env_bass = 0.0, env_mid = 0.0, env_treble = 0.0;
    FILE *f_csv;
    init_meshes();

    if(f_env) frame  = atoi(f_env);
    if(w_env) width  = atoi(w_env);
    if(h_env) height = atoi(h_env);

    if (width < 64 || height < 64 || width > 4096 || height > 4096) {
        width = 1280; height = 720;
    }
    if (frame < 0 || frame > 1199) {
        frame = 0;
    }

    /* --- Universal Recursion Calculations --- */
    total_frames = 1200; 
    double total_t = (double)frame / (double)total_frames;
    if(total_t > 0.999) total_t = 0.999;
    int current_level = (int)(total_t * 3.0);
    double level_t = (total_t * 3.0) - (double)current_level;

    /* Initialize Audio Buffer with defaults */
    for(i=0; i<1200; i++) { audio_buffer[i].b = 0.22; audio_buffer[i].m = 0.5; audio_buffer[i].t = 0.12; }

    f_csv = fopen("telemetry_audio.csv", "r");
    if(f_csv) {
        char line[256];
        if(fgets(line, sizeof(line), f_csv)) {} /* skip header */
        while(fgets(line, sizeof(line), f_csv)) {
            int f_idx; double b, m, t_freq;
            if(sscanf(line, "%d,%lf,%lf,%lf", &f_idx, &b, &m, &t_freq) == 4) {
                if(f_idx >= 0 && f_idx < 1200) {
                    audio_buffer[f_idx].b = b;
                    audio_buffer[f_idx].m = m;
                    audio_buffer[f_idx].t = t_freq;
                }
            }
        }
        fclose(f_csv);
    }
    env_bass = audio_buffer[frame].b;
    env_mid = audio_buffer[frame].m;
    env_treble = audio_buffer[frame].t;

    /* RECURSIVE PREDATION: Resolve camera origin by re-simulating Hamiltonian path */
    Vec3 ro, target; 
    make_vec3(0, 0, 0, &target);
    make_vec3(0, 1.5, 8.0, &ro); /* Initial seed at t=0 */

    for(int f=0; f<=frame; f++) {
        prime_step(&ro, &target, f);
    }
    
    struct Mesh *m;
    /* deform_liquid(env_bass, env_mid, env_treble, current_level); */

    for(i=0; i<3; i++){
        fprintf(stderr, "Building BVH for Universe Level %d...\n", i);
        m = &level_meshes[i];
        int root_idx = bvh_alloc++;
        subdivide_bvh(root_idx, m->tris, 0, m->count);
        mesh_bvh_roots[i] = root_idx;
    }

    double angle = (double)frame * 0.5 * PI / (double)total_frames;
    Vec3 forward, up_tmp, up, right;
    /* ro is already determined by prime_step simulation */
    
    v_sub(&target, &ro, &forward); v_norm(&forward, &forward);
    make_vec3(0, 1.0, 0, &up_tmp);
    v_cross(&forward, &up_tmp, &right); v_norm(&right, &right);
    v_cross(&right, &forward, &up); v_norm(&up, &up);

    /* Ordered 2x2 sub-pixel grid offsets (deterministic, no jitter). */
    static const double SSAA_OFFX[4] = { -0.25,  0.25, -0.25,  0.25 };
    static const double SSAA_OFFY[4] = { -0.25, -0.25,  0.25,  0.25 };


    printf("P3\n%d %d\n255\n", width, height);

    Vec3 *framebuffer = (Vec3 *)malloc(width * height * sizeof(Vec3));
    int num_threads = 16;
    pthread_t threads[16];
    struct ThreadArg args[16];
    
    int chunk = height / num_threads;
    for(i=0; i<num_threads; i++) {
        args[i].start_j = i * chunk;
        args[i].end_j = (i == num_threads - 1) ? height : (i + 1) * chunk;
        args[i].width = width; args[i].height = height; args[i].current_level = current_level; args[i].frame = frame;
        args[i].env_bass = env_bass; args[i].env_mid = env_mid; args[i].env_treble = env_treble; args[i].level_t = level_t;
        args[i].ro = ro; args[i].target = target; args[i].forward = forward; args[i].up = up; args[i].right = right;
        args[i].framebuffer = framebuffer;
        pthread_create(&threads[i], NULL, render_tile, &args[i]);
    }
    
    for(i=0; i<num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    


    for(j=height-1; j>=0; j--){
        for(i=0; i<width; i++){
            Vec3 accum = framebuffer[j * width + i];
            int ir=(int)(255.99*accum.x), ig=(int)(255.99*accum.y), ib=(int)(255.99*accum.z);
            if(ir>255)ir=255; if(ig>255)ig=255; if(ib>255)ib=255;
            if(ir<0)ir=0;     if(ig<0)ig=0;     if(ib<0)ib=0;
            printf("%d %d %d\n", ir, ig, ib);
        }
    }
    free(framebuffer);
    
    return 0;
}
