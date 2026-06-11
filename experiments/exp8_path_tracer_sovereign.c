/* EXPERIMENT 8-SOVEREIGN: Monte Carlo Path Tracer
 * 
 * 777JACKPOT777 LUCKY UPGRADE
 * - Vectorized ray generation and intersection (16-wide)
 * - Iterative path tracing to avoid ZMM stack spills
 * - SIMD Cosine-weighted hemisphere sampling
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zcaedi_avx512_math.h"

extern float sqrtf(float);
extern float sinf(float);
extern float cosf(float);
extern float fmaxf(float, float);
extern float fminf(float, float);

#define WIDTH 256
#define HEIGHT 256
#define SAMPLES 16
#define MAX_DEPTH 4

static unsigned int seed = 12345;
static inline float randf() {
    seed = (1664525 * seed + 1013904223);
    return (float)(seed & 0xFFFFFF) / (float)0xFFFFFF;
}

// Struct of Arrays for 16 rays
typedef struct {
    v16f ox, oy, oz;
    v16f dx, dy, dz;
} Ray16;

typedef struct {
    v16f x, y, z;
} Vec3_16;

typedef struct { float x, y, z; } vec3;

typedef struct {
    float radius;
    vec3 p, e, c; 
} Sphere;

Sphere spheres[] = {
    {1e5f, { 1e5f+1.0f,40.8f,81.6f}, {0,0,0}, {0.75f,0.25f,0.25f}},
    {1e5f, {-1e5f+99.0f,40.8f,81.6f},{0,0,0}, {0.25f,0.25f,0.75f}},
    {1e5f, {50.0f,40.8f, 1e5f},      {0,0,0}, {0.75f,0.75f,0.75f}},
    {1e5f, {50.0f,40.8f,-1e5f+170.0f},{0,0,0},{0,0,0}},            
    {1e5f, {50.0f, 1e5f, 81.6f},     {0,0,0}, {0.75f,0.75f,0.75f}},
    {1e5f, {50.0f,-1e5f+81.6f,81.6f},{0,0,0}, {0.75f,0.75f,0.75f}},
    {16.5f,{27.0f,16.5f,47.0f},      {0,0,0}, {0.999f,0.999f,0.999f}},
    {16.5f,{73.0f,16.5f,78.0f},      {0,0,0}, {0.999f,0.999f,0.999f}},
    {600.0f,{50.0f,681.6f-0.27f,81.6f},{12.0f,12.0f,12.0f}, {0,0,0}} 
};

#define _CMP_NEQ_OQ 4

static inline unsigned short _mm512_cmp_epi32_mask(const v16i *a, const v16i *b, int pred) {
    unsigned short m = 0;
    for(int i=0; i<16; i++) {
        int hit = 0;
        if(pred == _CMP_NEQ_OQ) hit = (a->v[i] != b->v[i]);
        else hit = (a->v[i] == b->v[i]);
        m |= (hit ? 1 : 0) << i;
    }
    return m;
}

static inline v16i _mm512_mask_set1_epi32(const v16i *src, unsigned short mask, int val) {
    v16i res;
    for(int i=0; i<16; i++) {
        res.v[i] = (mask >> i & 1) ? val : src->v[i];
    }
    return res;
}

static inline v16f _mm512_abs_ps(v16f a) {
    v16f res;
    for(int i=0; i<16; i++) {
        res.v[i] = (a.v[i] < 0.0f) ? -a.v[i] : a.v[i];
    }
    return res;
}

static inline void _mm512_storeu_si512(int *dst, v16i src) {
    for(int i=0; i<16; i++) dst[i] = src.v[i];
}

static inline unsigned short sovereign_mask_cmp_ps_mask(unsigned short mask, v16f a, v16f b, int pred) {
    unsigned short m = 0;
    for(int i=0; i<16; i++) {
        int hit = 0;
        if (mask >> i & 1) {
            if(pred == _CMP_GT_OQ) hit = (a.v[i] > b.v[i]);
            else if(pred == _CMP_LT_OQ) hit = (a.v[i] < b.v[i]);
            else if(pred == _CMP_GE_OQ) hit = (a.v[i] >= b.v[i]);
        }
        m |= (hit ? 1 : 0) << i;
    }
    return m;
}

static inline v16f sovereign_mask_blend_ps(unsigned short mask, v16f a, v16f b) {
    v16f res;
    for(int i=0; i<16; i++) {
        res.v[i] = (mask >> i & 1) ? b.v[i] : a.v[i];
    }
    return res;
}

static inline v16f sovereign_mask_mov_ps(v16f src, unsigned short mask, v16f a) {
    v16f res;
    for(int i=0; i<16; i++) {
        res.v[i] = (mask >> i & 1) ? a.v[i] : src.v[i];
    }
    return res;
}

// 16-wide intersection (Ray16 passed by pointer to avoid stack copies)
static inline void intersect_spheres(const Ray16 *r, unsigned short active, v16f *t_out, v16i *id_out) {
    v16f t = _mm512_set1_ps(1e20f);
    v16i id = _mm512_set1_epi32(-1);
    int num_spheres = sizeof(spheres)/sizeof(Sphere);
    
    for (int i = 0; i < num_spheres; i++) {
        v16f opx = _mm512_sub_ps(_mm512_set1_ps(spheres[i].p.x), r->ox);
        v16f opy = _mm512_sub_ps(_mm512_set1_ps(spheres[i].p.y), r->oy);
        v16f opz = _mm512_sub_ps(_mm512_set1_ps(spheres[i].p.z), r->oz);
        
        v16f b = _mm512_fmadd_ps(opx, r->dx, _mm512_fmadd_ps(opy, r->dy, _mm512_mul_ps(opz, r->dz)));
        v16f op_dot = _mm512_fmadd_ps(opx, opx, _mm512_fmadd_ps(opy, opy, _mm512_mul_ps(opz, opz)));
        v16f rad2 = _mm512_set1_ps(spheres[i].radius * spheres[i].radius);
        
        v16f det = _mm512_sub_ps(_mm512_add_ps(_mm512_mul_ps(b, b), rad2), op_dot);
        v16f v_zero = _mm512_setzero_ps();
        unsigned short det_mask = _mm512_cmp_ps_mask(&det, &v_zero, _CMP_GE_OQ);
        det_mask &= active;
        
        if (det_mask) {
            v16f sqdet = _mm512_sqrt_ps(det);
            v16f t1 = _mm512_sub_ps(b, sqdet);
            v16f t2 = _mm512_add_ps(b, sqdet);
            v16f eps = _mm512_set1_ps(1e-4f);
            
            unsigned short mask1 = sovereign_mask_cmp_ps_mask(det_mask, t1, eps, _CMP_GT_OQ);
            unsigned short mask2 = sovereign_mask_cmp_ps_mask(det_mask & ~mask1, t2, eps, _CMP_GT_OQ);
            
            v16f cur_t = sovereign_mask_blend_ps(mask1, sovereign_mask_blend_ps(mask2, _mm512_set1_ps(1e20f), t2), t1);
            unsigned short hit_mask = (mask1 | mask2) & _mm512_cmp_ps_mask(&cur_t, &t, _CMP_LT_OQ);
            
            if (hit_mask) {
                t = sovereign_mask_mov_ps(t, hit_mask, cur_t);
                id = _mm512_mask_set1_epi32(&id, hit_mask, i);
            }
        }
    }
    
    *t_out = t;
    *id_out = id;
}

int main(void) {
    unsigned char (*fb)[WIDTH][3] = malloc(HEIGHT * WIDTH * 3);
    memset(fb, 0, HEIGHT * WIDTH * 3);
    
    vec3 cam_o = {50.0f, 52.0f, 295.6f};
    vec3 cam_d = {0.0f, -0.042612f, -1.0f};
    
    float len = sqrtf(cam_d.x*cam_d.x + cam_d.y*cam_d.y + cam_d.z*cam_d.z);
    cam_d.x /= len; cam_d.y /= len; cam_d.z /= len;

    vec3 cx = {WIDTH * 0.5135f / HEIGHT, 0.0f, 0.0f};
    vec3 cy = {cam_d.y*cx.z - cam_d.z*cx.y, cam_d.z*cx.x - cam_d.x*cx.z, cam_d.x*cx.y - cam_d.y*cx.x};
    len = sqrtf(cy.x*cy.x + cy.y*cy.y + cy.z*cy.z);
    cy.x = (cy.x/len)*0.5135f; cy.y = (cy.y/len)*0.5135f; cy.z = (cy.z/len)*0.5135f;

    fprintf(stderr, "Path Tracing Exp8 Sovereign...\n");
    
    for (int y = 0; y < HEIGHT; y++) {
        fprintf(stderr, "\rRow %d/%d", y, HEIGHT);
        for (int x = 0; x < WIDTH; x += 16) {
            
            v16f acc_r = _mm512_setzero_ps();
            v16f acc_g = _mm512_setzero_ps();
            v16f acc_b = _mm512_setzero_ps();
            
            for (int s = 0; s < SAMPLES; s++) {
                Ray16 ray;
                v16f mask_r = _mm512_set1_ps(1.0f);
                v16f mask_g = _mm512_set1_ps(1.0f);
                v16f mask_b = _mm512_set1_ps(1.0f);
                v16f L_r = _mm512_setzero_ps();
                v16f L_g = _mm512_setzero_ps();
                v16f L_b = _mm512_setzero_ps();
                
                float dx_arr[16], dy_arr[16];
                for(int i=0; i<16; i++) {
                    float r1 = 2.0f * randf(); dx_arr[i] = r1 < 1.0f ? sqrtf(r1)-1.0f : 1.0f-sqrtf(2.0f-r1);
                    float r2 = 2.0f * randf(); dy_arr[i] = r2 < 1.0f ? sqrtf(r2)-1.0f : 1.0f-sqrtf(2.0f-r2);
                }
                
                v16f xv = _mm512_add_ps(_mm512_set1_ps((float)x), _mm512_set_ps(15.f,14.f,13.f,12.f,11.f,10.f,9.f,8.f,7.f,6.f,5.f,4.f,3.f,2.f,1.f,0.f));
                v16f dx_v = _mm512_loadu_ps(dx_arr);
                v16f dy_v = _mm512_loadu_ps(dy_arr);
                
                v16f sx = _mm512_sub_ps(_mm512_div_ps(_mm512_add_ps(_mm512_add_ps(xv, _mm512_set1_ps(0.5f)), dx_v), _mm512_set1_ps((float)WIDTH)), _mm512_set1_ps(0.5f));
                v16f sy = _mm512_sub_ps(_mm512_div_ps(_mm512_add_ps(_mm512_set1_ps((float)(y) + 0.5f), dy_v), _mm512_set1_ps((float)HEIGHT)), _mm512_set1_ps(0.5f));
                
                v16f dir_x = _mm512_fmadd_ps(sx, _mm512_set1_ps(cx.x), _mm512_fmadd_ps(sy, _mm512_set1_ps(cy.x), _mm512_set1_ps(cam_d.x)));
                v16f dir_y = _mm512_fmadd_ps(sx, _mm512_set1_ps(cx.y), _mm512_fmadd_ps(sy, _mm512_set1_ps(cy.y), _mm512_set1_ps(cam_d.y)));
                v16f dir_z = _mm512_fmadd_ps(sx, _mm512_set1_ps(cx.z), _mm512_fmadd_ps(sy, _mm512_set1_ps(cy.z), _mm512_set1_ps(cam_d.z)));
                
                v16f dlen = _mm512_sqrt_ps(_mm512_fmadd_ps(dir_x, dir_x, _mm512_fmadd_ps(dir_y, dir_y, _mm512_mul_ps(dir_z, dir_z))));
                
                v16f new_dx = _mm512_div_ps(dir_x, dlen);
                v16f new_dy = _mm512_div_ps(dir_y, dlen);
                v16f new_dz = _mm512_div_ps(dir_z, dlen);
                for(int k=0; k<16; k++) {
                    ray.dx.v[k] = new_dx.v[k];
                    ray.dy.v[k] = new_dy.v[k];
                    ray.dz.v[k] = new_dz.v[k];
                }
                
                v16f ox_val = _mm512_fmadd_ps(ray.dx, _mm512_set1_ps(140.0f), _mm512_set1_ps(cam_o.x));
                v16f oy_val = _mm512_fmadd_ps(ray.dy, _mm512_set1_ps(140.0f), _mm512_set1_ps(cam_o.y));
                v16f oz_val = _mm512_fmadd_ps(ray.dz, _mm512_set1_ps(140.0f), _mm512_set1_ps(cam_o.z));
                for(int k=0; k<16; k++) {
                    ray.ox.v[k] = ox_val.v[k];
                    ray.oy.v[k] = oy_val.v[k];
                    ray.oz.v[k] = oz_val.v[k];
                }
                
                unsigned short active = 0xFFFF;
                
                for (int depth = 0; depth < MAX_DEPTH; depth++) {
                    if (!active) break;
                    
                    v16f t;
                    v16i id;
                    intersect_spheres(&ray, active, &t, &id);
                    
                    v16i neg_one = _mm512_set1_epi32(-1);
                    active &= _mm512_cmp_epi32_mask(&id, &neg_one, _CMP_NEQ_OQ);
                    if (!active) break;
                    
                    v16f hx = _mm512_fmadd_ps(ray.dx, t, ray.ox);
                    v16f hy = _mm512_fmadd_ps(ray.dy, t, ray.oy);
                    v16f hz = _mm512_fmadd_ps(ray.dz, t, ray.oz);
                    
                    int id_arr[16];
                    _mm512_storeu_si512(id_arr, id);
                    
                    float nx_arr[16], ny_arr[16], nz_arr[16];
                    float ex_arr[16], ey_arr[16], ez_arr[16];
                    float cx_arr[16], cy_arr[16], cz_arr[16];
                    
                    for(int i=0; i<16; i++) {
                        if ((active & (1<<i))) {
                            int obj_id = id_arr[i];
                            nx_arr[i] = spheres[obj_id].p.x;
                            ny_arr[i] = spheres[obj_id].p.y;
                            nz_arr[i] = spheres[obj_id].p.z;
                            ex_arr[i] = spheres[obj_id].e.x;
                            ey_arr[i] = spheres[obj_id].e.y;
                            ez_arr[i] = spheres[obj_id].e.z;
                            cx_arr[i] = spheres[obj_id].c.x;
                            cy_arr[i] = spheres[obj_id].c.y;
                            cz_arr[i] = spheres[obj_id].c.z;
                        } else {
                            ex_arr[i] = ey_arr[i] = ez_arr[i] = 0.0f;
                            cx_arr[i] = cy_arr[i] = cz_arr[i] = 0.0f;
                        }
                    }
                    
                    v16f nx = _mm512_sub_ps(hx, _mm512_loadu_ps(nx_arr));
                    v16f ny = _mm512_sub_ps(hy, _mm512_loadu_ps(ny_arr));
                    v16f nz = _mm512_sub_ps(hz, _mm512_loadu_ps(nz_arr));
                    v16f nlen = _mm512_sqrt_ps(_mm512_fmadd_ps(nx, nx, _mm512_fmadd_ps(ny, ny, _mm512_mul_ps(nz, nz))));
                    nx = _mm512_div_ps(nx, nlen); ny = _mm512_div_ps(ny, nlen); nz = _mm512_div_ps(nz, nlen);
                    
                    v16f dot_nd = _mm512_fmadd_ps(nx, ray.dx, _mm512_fmadd_ps(ny, ray.dy, _mm512_mul_ps(nz, ray.dz)));
                    v16f v_zero = _mm512_setzero_ps();
                    unsigned short nl_mask = _mm512_cmp_ps_mask(&dot_nd, &v_zero, _CMP_LT_OQ);
                    v16f nlx = sovereign_mask_blend_ps(nl_mask, _mm512_sub_ps(_mm512_setzero_ps(), nx), nx);
                    v16f nly = sovereign_mask_blend_ps(nl_mask, _mm512_sub_ps(_mm512_setzero_ps(), ny), ny);
                    v16f nlz = sovereign_mask_blend_ps(nl_mask, _mm512_sub_ps(_mm512_setzero_ps(), nz), nz);
                    
                    v16f ev_x = _mm512_loadu_ps(ex_arr);
                    v16f ev_y = _mm512_loadu_ps(ey_arr);
                    v16f ev_z = _mm512_loadu_ps(ez_arr);
                    
                    v16f cv_x = _mm512_loadu_ps(cx_arr);
                    v16f cv_y = _mm512_loadu_ps(cy_arr);
                    v16f cv_z = _mm512_loadu_ps(cz_arr);
                    
                    L_r = _mm512_mask_add_ps(L_r, active, L_r, _mm512_mul_ps(mask_r, ev_x));
                    L_g = _mm512_mask_add_ps(L_g, active, L_g, _mm512_mul_ps(mask_g, ev_y));
                    L_b = _mm512_mask_add_ps(L_b, active, L_b, _mm512_mul_ps(mask_b, ev_z));
                    
                    mask_r = _mm512_mul_ps(mask_r, cv_x);
                    mask_g = _mm512_mul_ps(mask_g, cv_y);
                    mask_b = _mm512_mul_ps(mask_b, cv_z);
                    
                    float r1_arr[16], r2_arr[16];
                    for(int i=0; i<16; i++) {
                        r1_arr[i] = 2.0f * 3.1415926f * randf();
                        r2_arr[i] = randf();
                    }
                    v16f r1 = _mm512_loadu_ps(r1_arr);
                    v16f r2 = _mm512_loadu_ps(r2_arr);
                    v16f r2s = _mm512_sqrt_ps(r2);
                    
                    v16f abs_nlx = _mm512_abs_ps(nlx);
                    v16f v_01 = _mm512_set1_ps(0.1f);
                    unsigned short w_mask = _mm512_cmp_ps_mask(&abs_nlx, &v_01, _CMP_GT_OQ);
                    v16f up_x = sovereign_mask_blend_ps(w_mask, _mm512_set1_ps(1.0f), _mm512_setzero_ps());
                    v16f up_y = sovereign_mask_blend_ps(w_mask, _mm512_setzero_ps(), _mm512_set1_ps(1.0f));
                    v16f up_z = _mm512_setzero_ps();
                    
                    v16f ux = _mm512_sub_ps(_mm512_mul_ps(up_y, nlz), _mm512_mul_ps(up_z, nly));
                    v16f uy = _mm512_sub_ps(_mm512_mul_ps(up_z, nlx), _mm512_mul_ps(up_x, nlz));
                    v16f uz = _mm512_sub_ps(_mm512_mul_ps(up_x, nly), _mm512_mul_ps(up_y, nlx));
                    
                    v16f ulen = _mm512_sqrt_ps(_mm512_fmadd_ps(ux, ux, _mm512_fmadd_ps(uy, uy, _mm512_mul_ps(uz, uz))));
                    ux = _mm512_div_ps(ux, ulen); uy = _mm512_div_ps(uy, ulen); uz = _mm512_div_ps(uz, ulen);
                    
                    v16f vx = _mm512_sub_ps(_mm512_mul_ps(nly, uz), _mm512_mul_ps(nlz, uy));
                    v16f vy = _mm512_sub_ps(_mm512_mul_ps(nlz, ux), _mm512_mul_ps(nlx, uz));
                    v16f vz = _mm512_sub_ps(_mm512_mul_ps(nlx, uy), _mm512_mul_ps(nly, ux));
                    
                    float r1_arr_val[16], r2s_arr_val[16], r2_arr_val[16];
                    float d_new_x[16], d_new_y[16], d_new_z[16];
                    _mm512_storeu_ps(r1_arr_val, r1);
                    _mm512_storeu_ps(r2s_arr_val, r2s);
                    _mm512_storeu_ps(r2_arr_val, r2);
                    
                    float ux_a[16], uy_a[16], uz_a[16];
                    float vx_a[16], vy_a[16], vz_a[16];
                    float wx_a[16], wy_a[16], wz_a[16];
                    _mm512_storeu_ps(ux_a, ux); _mm512_storeu_ps(uy_a, uy); _mm512_storeu_ps(uz_a, uz);
                    _mm512_storeu_ps(vx_a, vx); _mm512_storeu_ps(vy_a, vy); _mm512_storeu_ps(vz_a, vz);
                    _mm512_storeu_ps(wx_a, nlx); _mm512_storeu_ps(wy_a, nly); _mm512_storeu_ps(wz_a, nlz);
                    
                    for(int i=0; i<16; i++) {
                        if (active & (1<<i)) {
                            float cr = cosf(r1_arr_val[i]) * r2s_arr_val[i];
                            float sr = sinf(r1_arr_val[i]) * r2s_arr_val[i];
                            float wr = sqrtf(1.0f - r2_arr_val[i]);
                            
                            float p_dx = ux_a[i]*cr + vx_a[i]*sr + wx_a[i]*wr;
                            float p_dy = uy_a[i]*cr + vy_a[i]*sr + wy_a[i]*wr;
                            float p_dz = uz_a[i]*cr + vz_a[i]*sr + wz_a[i]*wr;
                            
                            float len2 = sqrtf(p_dx*p_dx + p_dy*p_dy + p_dz*p_dz);
                            d_new_x[i] = p_dx/len2; d_new_y[i] = p_dy/len2; d_new_z[i] = p_dz/len2;
                        }
                    }
                    
                    for(int k=0; k<16; k++) {
                        ray.ox.v[k] = hx.v[k];
                        ray.oy.v[k] = hy.v[k];
                        ray.oz.v[k] = hz.v[k];
                        ray.dx.v[k] = d_new_x[k];
                        ray.dy.v[k] = d_new_y[k];
                        ray.dz.v[k] = d_new_z[k];
                    }
                }
                
                acc_r = _mm512_add_ps(acc_r, _mm512_mul_ps(L_r, _mm512_set1_ps(1.0f/SAMPLES)));
                acc_g = _mm512_add_ps(acc_g, _mm512_mul_ps(L_g, _mm512_set1_ps(1.0f/SAMPLES)));
                acc_b = _mm512_add_ps(acc_b, _mm512_mul_ps(L_b, _mm512_set1_ps(1.0f/SAMPLES)));
            }
            
            float out_r[16], out_g[16], out_b[16];
            _mm512_storeu_ps(out_r, acc_r);
            _mm512_storeu_ps(out_g, acc_g);
            _mm512_storeu_ps(out_b, acc_b);
            
            for(int i=0; i<16; i++) {
                if (x+i < WIDTH) {
                    fb[HEIGHT-1-y][x+i][0] = (unsigned char)(fminf(1.0f, sqrtf(out_r[i])) * 255.0f);
                    fb[HEIGHT-1-y][x+i][1] = (unsigned char)(fminf(1.0f, sqrtf(out_g[i])) * 255.0f);
                    fb[HEIGHT-1-y][x+i][2] = (unsigned char)(fminf(1.0f, sqrtf(out_b[i])) * 255.0f);
                }
            }
        }
    }
    
    // Hash calculations
    unsigned int hash = 0;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            hash ^= (fb[y][x][0] << 16) | (fb[y][x][1] << 8) | fb[y][x][2];
            hash = (hash << 1) | (hash >> 31);
        }
    }
    fprintf(stderr, "\nExp8 Path Tracer Verification Hash: 0x%08X\n", hash);

    printf("P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int y = 0; y < HEIGHT; y++) fwrite(fb[y], 1, WIDTH * 3, stdout);
    
    free(fb);
    fprintf(stderr, "\nExp8 Sovereign done!\n");
    return 0;
}
