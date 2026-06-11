/* EXPERIMENT 15-SOVEREIGN: Splatting Point Cloud Renderer
 * 
 * 777JACKPOT777 LUCKY UPGRADE
 * - Vectorized 3D projection, rotation, and perspective divide (16 points per cycle).
 * - SoA layout for 200k points (aligned to 64 bytes).
 * - Fast clustered splatting loop for rasterization.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zcaedi_avx512_math.h"

extern float sinf(float);
extern float cosf(float);

#define WIDTH 640
#define HEIGHT 480
#define NUM_POINTS 200000 // Multiple of 16

// SoA for 200k points
typedef struct {
    float x[NUM_POINTS] __attribute__((aligned(64)));
    float y[NUM_POINTS] __attribute__((aligned(64)));
    float z[NUM_POINTS] __attribute__((aligned(64)));
    unsigned char r[NUM_POINTS] __attribute__((aligned(64)));
    unsigned char g[NUM_POINTS] __attribute__((aligned(64)));
    unsigned char b[NUM_POINTS] __attribute__((aligned(64)));
} PointCloudSoA;

PointCloudSoA pc;

void generate_torus_cloud() {
    float R = 2.0f; // Major radius
    float r = 0.8f; // Minor radius
    
    unsigned int seed = 123;
    for(int i=0; i<NUM_POINTS; i++) {
        seed = (1664525 * seed + 1013904223);
        float u = ((float)(seed & 0xFFFFFF) / (float)0xFFFFFF) * 3.1415926f * 2.0f;
        
        seed = (1664525 * seed + 1013904223);
        float v = ((float)(seed & 0xFFFFFF) / (float)0xFFFFFF) * 3.1415926f * 2.0f;
        
        seed = (1664525 * seed + 1013904223);
        float n_x = (((float)(seed & 0xFFFFFF) / (float)0xFFFFFF) - 0.5f) * 0.3f;
        seed = (1664525 * seed + 1013904223);
        float n_y = (((float)(seed & 0xFFFFFF) / (float)0xFFFFFF) - 0.5f) * 0.3f;
        seed = (1664525 * seed + 1013904223);
        float n_z = (((float)(seed & 0xFFFFFF) / (float)0xFFFFFF) - 0.5f) * 0.3f;

        pc.x[i] = (R + r * cosf(v)) * cosf(u) + n_x;
        pc.y[i] = (R + r * cosf(v)) * sinf(u) + n_y;
        pc.z[i] = r * sinf(v) + n_z;
        
        pc.r[i] = (unsigned char)((sinf(u) * 0.5f + 0.5f) * 255);
        pc.g[i] = (unsigned char)((cosf(v) * 0.5f + 0.5f) * 255);
        pc.b[i] = 255 - pc.r[i];
    }
}

int main(void) {
    unsigned char (*fb)[WIDTH][3] = malloc(HEIGHT * WIDTH * 3);
    float *zbuf = malloc(HEIGHT * WIDTH * sizeof(float));
    
    memset(fb, 0, HEIGHT * WIDTH * 3);
    for(int i=0; i<HEIGHT*WIDTH; i++) zbuf[i] = 1000.0f;
    
    generate_torus_cloud();
    fprintf(stderr, "Rendering Splat Cloud Exp15 Sovereign...\n");
    
    // Rotate and project
    float cx_f = cosf(0.8f), sx_f = sinf(0.8f);
    float cy_f = cosf(0.5f), sy_f = sinf(0.5f);
    
    v16f cx = _mm512_set1_ps(cx_f);
    v16f sx = _mm512_set1_ps(sx_f);
    v16f cy = _mm512_set1_ps(cy_f);
    v16f sy = _mm512_set1_ps(sy_f);
    
    v16f trans_z = _mm512_set1_ps(6.0f);
    v16f proj_scale = _mm512_set1_ps(400.0f);
    v16f half_w = _mm512_set1_ps(WIDTH / 2.0f);
    v16f half_h = _mm512_set1_ps(HEIGHT / 2.0f);
    v16f min_z = _mm512_set1_ps(0.1f);
    v16f splat_const = _mm512_set1_ps(8.0f);
    v16f splat_min = _mm512_set1_ps(1.0f);
    
    for(int i=0; i<NUM_POINTS; i+=16) {
        v16f px = _mm512_load_ps(&pc.x[i]);
        v16f py = _mm512_load_ps(&pc.y[i]);
        v16f pz = _mm512_load_ps(&pc.z[i]);
        
        // Rot X
        // y1 = py * cx - pz * sx;
        // z1 = py * sx + pz * cx;
        v16f y1 = _mm512_sub_ps(_mm512_mul_ps(py, cx), _mm512_mul_ps(pz, sx));
        v16f z1 = _mm512_add_ps(_mm512_mul_ps(py, sx), _mm512_mul_ps(pz, cx));
        
        // Rot Y
        // x2 = px * cy + z1 * sy;
        // z2 = -px * sy + z1 * cy;
        v16f x2 = _mm512_add_ps(_mm512_mul_ps(px, cy), _mm512_mul_ps(z1, sy));
        v16f z2 = _mm512_add_ps(_mm512_mul_ps(_mm512_sub_ps(_mm512_setzero_ps(), px), sy), _mm512_mul_ps(z1, cy));
        
        z2 = _mm512_add_ps(z2, trans_z);
        
        unsigned short valid_z = _mm512_cmp_ps_mask(z2, min_z, _CMP_GE_OQ);
        if (!valid_z) continue;
        
        v16f inv_z = _mm512_rcp_ps(z2);
        
        v16f scr_x_f = _mm512_fmadd_ps(_mm512_mul_ps(x2, inv_z), proj_scale, half_w);
        v16f scr_y_f = _mm512_fmadd_ps(_mm512_mul_ps(y1, inv_z), proj_scale, half_h);
        
        v16f r_f = _mm512_mul_ps(splat_const, inv_z);
        r_f = _mm512_max_ps(r_f, splat_min);
        
        float sx_a[16], sy_a[16], z2_a[16], rf_a[16];
        _mm512_storeu_ps(sx_a, scr_x_f);
        _mm512_storeu_ps(sy_a, scr_y_f);
        _mm512_storeu_ps(z2_a, z2);
        _mm512_storeu_ps(rf_a, r_f);
        
        for(int k=0; k<16; k++) {
            if ((valid_z >> k) & 1) {
                int scr_x = (int)sx_a[k];
                int scr_y = (int)sy_a[k];
                int r = (int)rf_a[k];
                float point_z = z2_a[k];
                
                unsigned char c_r = pc.r[i+k];
                unsigned char c_g = pc.g[i+k];
                unsigned char c_b = pc.b[i+k];
                
                int r2 = r*r;
                for(int dy = -r; dy <= r; dy++) {
                    int dy2 = dy*dy;
                    int y_pos = scr_y + dy;
                    if(y_pos >= 0 && y_pos < HEIGHT) {
                        for(int dx = -r; dx <= r; dx++) {
                            if(dx*dx + dy2 <= r2) {
                                int x_pos = scr_x + dx;
                                if(x_pos >= 0 && x_pos < WIDTH) {
                                    int idx = y_pos * WIDTH + x_pos;
                                    if(point_z < zbuf[idx]) {
                                        zbuf[idx] = point_z;
                                        fb[y_pos][x_pos][0] = c_r;
                                        fb[y_pos][x_pos][1] = c_g;
                                        fb[y_pos][x_pos][2] = c_b;
                                    }
                                }
                            }
                        }
                    }
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
    fprintf(stderr, "Exp15 Point Cloud Verification Hash: 0x%08X\n", hash);

    printf("P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int y = 0; y < HEIGHT; y++) fwrite(fb[y], 1, WIDTH * 3, stdout);
    
    free(fb); free(zbuf);
    fprintf(stderr, "Exp15 Sovereign done!\n");
    return 0;
}
