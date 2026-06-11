/* EXPERIMENT 9-SOVEREIGN: SDF Boolean Raymarcher
 * 
 * 777JACKPOT777 LUCKY UPGRADE
 * - Vectorized raymarching loop handling 16 pixels at once.
 * - Masked fminf/fmaxf and absolute values using AVX-512 ops.
 * - CSG (Constructive Solid Geometry) evaluation across 16 rays natively.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zcaedi_avx512_math.h"

extern float sqrtf(float);

#define WIDTH 640
#define HEIGHT 480
#define MAX_STEPS 100
#define SURF_DIST 0.01f
#define MAX_DIST 100.0f

static inline v16f v16_abs(v16f a) {
    v16f res;
    for(int i = 0; i < 16; i++) {
        res.v[i] = (a.v[i] < 0.0f) ? -a.v[i] : a.v[i];
    }
    return res;
}

static inline v16f v16_len(v16f x, v16f y, v16f z) {
    return _mm512_sqrt_ps(_mm512_fmadd_ps(x, x, _mm512_fmadd_ps(y, y, _mm512_mul_ps(z, z))));
}

// SDF Primitives Sovereign
static inline v16f sdSphere_avx512(v16f px, v16f py, v16f pz, v16f s) {
    return _mm512_sub_ps(v16_len(px, py, pz), s);
}

static inline v16f sdBox_avx512(v16f px, v16f py, v16f pz, v16f bx, v16f by, v16f bz) {
    v16f qx = _mm512_sub_ps(v16_abs(px), bx);
    v16f qy = _mm512_sub_ps(v16_abs(py), by);
    v16f qz = _mm512_sub_ps(v16_abs(pz), bz);
    
    v16f zero = _mm512_setzero_ps();
    v16f dx = _mm512_max_ps(qx, zero);
    v16f dy = _mm512_max_ps(qy, zero);
    v16f dz = _mm512_max_ps(qz, zero);
    
    v16f d_out = v16_len(dx, dy, dz);
    v16f d_in = _mm512_min_ps(_mm512_max_ps(qx, _mm512_max_ps(qy, qz)), zero);
    
    return _mm512_add_ps(d_out, d_in);
}

// Scene Distance Field Sovereign
static inline v16f get_dist_avx512(v16f px, v16f py, v16f pz) {
    v16f box_px = px;
    v16f box_py = _mm512_sub_ps(py, _mm512_set1_ps(1.0f));
    v16f box_pz = _mm512_sub_ps(pz, _mm512_set1_ps(6.0f));
    
    v16f dBox = sdBox_avx512(box_px, box_py, box_pz, 
                             _mm512_set1_ps(1.0f), _mm512_set1_ps(1.0f), _mm512_set1_ps(1.0f));
                             
    v16f dSphere = sdSphere_avx512(box_px, box_py, box_pz, _mm512_set1_ps(1.3f));
    
    v16f neg_dSphere = _mm512_sub_ps(_mm512_setzero_ps(), dSphere);
    v16f dObj = _mm512_max_ps(dBox, neg_dSphere); // Subtraction
    
    v16f dPlane = py; // Ground
    
    return _mm512_min_ps(dObj, dPlane);
}

int main(void) {
    unsigned char (*fb)[WIDTH][3] = malloc(HEIGHT * WIDTH * 3);
    fprintf(stderr, "Raymarching Exp9 SDF Sovereign...\n");
    
    v16f ro_x = _mm512_setzero_ps();
    v16f ro_y = _mm512_set1_ps(2.0f);
    v16f ro_z = _mm512_setzero_ps();
    
    v16f max_dist = _mm512_set1_ps(MAX_DIST);
    v16f surf_dist = _mm512_set1_ps(SURF_DIST);
    v16f lightPos_x = _mm512_setzero_ps();
    v16f lightPos_y = _mm512_set1_ps(5.0f);
    v16f lightPos_z = _mm512_set1_ps(6.0f);
    
    v16f x_offsets = _mm512_set_ps(15.f, 14.f, 13.f, 12.f, 11.f, 10.f, 9.f, 8.f, 7.f, 6.f, 5.f, 4.f, 3.f, 2.f, 1.f, 0.f);

    for (int y = 0; y < HEIGHT; y++) {
        v16f uvy = _mm512_set1_ps((0.5f * HEIGHT - y) / HEIGHT);
        
        for (int x = 0; x < WIDTH; x += 16) {
            v16f xv = _mm512_add_ps(_mm512_set1_ps(x), x_offsets);
            v16f uvx = _mm512_div_ps(_mm512_sub_ps(xv, _mm512_set1_ps(0.5f * WIDTH)), _mm512_set1_ps(HEIGHT));
            
            v16f rd_z = _mm512_set1_ps(1.0f);
            v16f rd_len = v16_len(uvx, uvy, rd_z);
            v16f inv_rd = _mm512_rcp_ps(rd_len);
            v16f rd_x = _mm512_mul_ps(uvx, inv_rd);
            v16f rd_y = _mm512_mul_ps(uvy, inv_rd);
            rd_z = _mm512_mul_ps(rd_z, inv_rd);
            
            v16f dO = _mm512_setzero_ps();
            unsigned short active = 0xFFFF; // All 16 rays active
            
            for(int i=0; i<MAX_STEPS; i++) {
                if(!active) break;
                
                v16f px = _mm512_fmadd_ps(rd_x, dO, ro_x);
                v16f py = _mm512_fmadd_ps(rd_y, dO, ro_y);
                v16f pz = _mm512_fmadd_ps(rd_z, dO, ro_z);
                
                v16f dS = get_dist_avx512(px, py, pz);
                
                dO = _mm512_mask_add_ps(dO, active, dO, dS);
                
                unsigned short hit = _mm512_cmp_ps_mask(dS, surf_dist, _CMP_LT_OQ);
                unsigned short miss = _mm512_cmp_ps_mask(dO, max_dist, _CMP_GT_OQ);
                
                active &= ~(hit | miss);
            }
            
            unsigned short hit_mask = _mm512_cmp_ps_mask(dO, max_dist, _CMP_LT_OQ);
            v16f dif = _mm512_setzero_ps();
            
            if (hit_mask) {
                v16f px = _mm512_fmadd_ps(rd_x, dO, ro_x);
                v16f py = _mm512_fmadd_ps(rd_y, dO, ro_y);
                v16f pz = _mm512_fmadd_ps(rd_z, dO, ro_z);
                
                // Get normal (d is already known, but let's re-eval for simplicity or use epsilon)
                v16f d = get_dist_avx512(px, py, pz);
                v16f eps = _mm512_set1_ps(0.01f);
                
                v16f nx = _mm512_sub_ps(d, get_dist_avx512(_mm512_sub_ps(px, eps), py, pz));
                v16f ny = _mm512_sub_ps(d, get_dist_avx512(px, _mm512_sub_ps(py, eps), pz));
                v16f nz = _mm512_sub_ps(d, get_dist_avx512(px, py, _mm512_sub_ps(pz, eps)));
                
                v16f nlen = v16_len(nx, ny, nz);
                v16f inv_nlen = _mm512_rcp_ps(nlen);
                nx = _mm512_mul_ps(nx, inv_nlen);
                ny = _mm512_mul_ps(ny, inv_nlen);
                nz = _mm512_mul_ps(nz, inv_nlen);
                
                // Get light
                v16f lx = _mm512_sub_ps(lightPos_x, px);
                v16f ly = _mm512_sub_ps(lightPos_y, py);
                v16f lz = _mm512_sub_ps(lightPos_z, pz);
                
                v16f light_dist = v16_len(lx, ly, lz);
                v16f inv_ldist = _mm512_rcp_ps(light_dist);
                lx = _mm512_mul_ps(lx, inv_ldist);
                ly = _mm512_mul_ps(ly, inv_ldist);
                lz = _mm512_mul_ps(lz, inv_ldist);
                
                dif = _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(nx, lx, _mm512_fmadd_ps(ny, ly, _mm512_mul_ps(nz, lz))));
                
                // Shadows
                v16f s_ro_x = _mm512_fmadd_ps(nx, _mm512_set1_ps(SURF_DIST * 2.0f), px);
                v16f s_ro_y = _mm512_fmadd_ps(ny, _mm512_set1_ps(SURF_DIST * 2.0f), py);
                v16f s_ro_z = _mm512_fmadd_ps(nz, _mm512_set1_ps(SURF_DIST * 2.0f), pz);
                
                v16f s_dO = _mm512_setzero_ps();
                unsigned short s_active = hit_mask;
                
                for(int i=0; i<MAX_STEPS; i++) {
                    if(!s_active) break;
                    
                    v16f s_px = _mm512_fmadd_ps(lx, s_dO, s_ro_x);
                    v16f s_py = _mm512_fmadd_ps(ly, s_dO, s_ro_y);
                    v16f s_pz = _mm512_fmadd_ps(lz, s_dO, s_ro_z);
                    
                    v16f s_dS = get_dist_avx512(s_px, s_py, s_pz);
                    s_dO = _mm512_mask_add_ps(s_dO, s_active, s_dO, s_dS);
                    
                    unsigned short s_hit = _mm512_cmp_ps_mask(s_dS, surf_dist, _CMP_LT_OQ);
                    unsigned short s_miss = _mm512_cmp_ps_mask(s_dO, max_dist, _CMP_GT_OQ);
                    
                    s_active &= ~(s_hit | s_miss);
                }
                
                unsigned short shadow_mask = _mm512_cmp_ps_mask(s_dO, light_dist, _CMP_LT_OQ);
                shadow_mask &= hit_mask; // only if we actually hit the main object
                
                dif = _mm512_mask_mul_ps(dif, shadow_mask, dif, _mm512_set1_ps(0.1f));
            }
            
            v16f col_f = _mm512_mul_ps(dif, _mm512_set1_ps(255.0f));
            float col_f_a[16];
            _mm512_storeu_ps(col_f_a, col_f);
            
            for(int i=0; i<16; i++) {
                if (x+i < WIDTH) {
                    float val = col_f_a[i];
                    unsigned char col = (unsigned char)(val > 255.0f ? 255.0f : (val < 0.0f ? 0.0f : val));
                    fb[y][x+i][0] = col;
                    fb[y][x+i][1] = col;
                    fb[y][x+i][2] = col;
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
    fprintf(stderr, "Exp9 SDF Verification Hash: 0x%08X\n", hash);

    printf("P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int y = 0; y < HEIGHT; y++) fwrite(fb[y], 1, WIDTH * 3, stdout);
    
    free(fb);
    return 0;
}
