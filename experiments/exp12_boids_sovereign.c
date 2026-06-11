/* EXPERIMENT 12-SOVEREIGN: Boids Flocking Simulation
 * 
 * 777JACKPOT777 LUCKY UPGRADE
 * - Vectorized O(N^2) inner loop (16 boids processed per cycle)
 * - SoA layout for Boids (aligned to 64 bytes)
 * - Pointer-based mask comparison for perfect ZCC compatibility
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zcaedi_avx512_math.h"

extern float sqrtf(float);

#define WIDTH 800
#define HEIGHT 600
#define NUM_BOIDS 512 // padded to multiple of 16

typedef struct {
    float px[NUM_BOIDS] __attribute__((aligned(64)));
    float py[NUM_BOIDS] __attribute__((aligned(64)));
    float vx[NUM_BOIDS] __attribute__((aligned(64)));
    float vy[NUM_BOIDS] __attribute__((aligned(64)));
    float ax[NUM_BOIDS] __attribute__((aligned(64)));
    float ay[NUM_BOIDS] __attribute__((aligned(64)));
} BoidsSoA;

BoidsSoA boids;

static unsigned int seed = 12345;
static inline float randf() {
    seed = (1664525 * seed + 1013904223);
    return (float)(seed & 0xFFFFFF) / (float)0xFFFFFF;
}

void init_boids() {
    for (int i = 0; i < NUM_BOIDS; i++) {
        boids.px[i] = randf() * WIDTH;
        boids.py[i] = randf() * HEIGHT;
        boids.vx[i] = randf() * 4.0f - 2.0f;
        boids.vy[i] = randf() * 4.0f - 2.0f;
        boids.ax[i] = 0.0f;
        boids.ay[i] = 0.0f;
    }
}

void flock_avx512() {
    v16f r_sep = _mm512_set1_ps(25.0f);
    v16f r_ali_coh = _mm512_set1_ps(50.0f);
    v16f zero = _mm512_setzero_ps();
    v16f eps = _mm512_set1_ps(0.0001f);
    
    for (int i = 0; i < NUM_BOIDS; i++) {
        v16f px_i = _mm512_set1_ps(boids.px[i]);
        v16f py_i = _mm512_set1_ps(boids.py[i]);
        
        v16f sep_x = zero, sep_y = zero;
        v16f ali_x = zero, ali_y = zero;
        v16f coh_x = zero, coh_y = zero;
        
        v16i count_s = _mm512_setzero_si512();
        v16i count_a = _mm512_setzero_si512();
        v16i count_c = _mm512_setzero_si512();
        v16i one_i = _mm512_set1_epi32(1);
        
        for (int j = 0; j < NUM_BOIDS; j += 32) {
            _mm_prefetch((const char*)&boids.px[j + 32], _MM_HINT_T0);
            _mm_prefetch((const char*)&boids.py[j + 32], _MM_HINT_T0);
            _mm_prefetch((const char*)&boids.vx[j + 32], _MM_HINT_T0);
            _mm_prefetch((const char*)&boids.vy[j + 32], _MM_HINT_T0);
            
            // Block 1
            v16f px_j1 = _mm512_load_ps(&boids.px[j]);
            v16f py_j1 = _mm512_load_ps(&boids.py[j]);
            v16f vx_j1 = _mm512_load_ps(&boids.vx[j]);
            v16f vy_j1 = _mm512_load_ps(&boids.vy[j]);
            
            v16f dx1 = _mm512_sub_ps(px_i, px_j1);
            v16f dy1 = _mm512_sub_ps(py_i, py_j1);
            v16f d1 = _mm512_sqrt_ps(_mm512_fmadd_ps(dx1, dx1, _mm512_mul_ps(dy1, dy1)));
            
            unsigned short mask_valid1 = _mm512_cmp_ps_mask(&d1, &eps, _CMP_GT_OQ);
            int block_start1 = j;
            if (i >= block_start1 && i < block_start1 + 16) mask_valid1 &= ~(1 << (i - block_start1));
            
            if (mask_valid1) {
                unsigned short det_sep1 = _mm512_cmp_ps_mask(&d1, &r_sep, _CMP_LT_OQ);
                unsigned short mask_sep1 = mask_valid1 & det_sep1;
                if (mask_sep1) {
                    v16f inv_d1 = _mm512_rcp_ps(d1);
                    v16f norm_dx1 = _mm512_mul_ps(dx1, inv_d1);
                    v16f norm_dy1 = _mm512_mul_ps(dy1, inv_d1);
                    v16f weighted_dx1 = _mm512_mul_ps(norm_dx1, inv_d1);
                    v16f weighted_dy1 = _mm512_mul_ps(norm_dy1, inv_d1);
                    sep_x = _mm512_mask_add_ps(sep_x, mask_sep1, sep_x, weighted_dx1);
                    sep_y = _mm512_mask_add_ps(sep_y, mask_sep1, sep_y, weighted_dy1);
                    count_s = _mm512_mask_add_epi32(count_s, mask_sep1, count_s, one_i);
                }
                
                unsigned short det_ali1 = _mm512_cmp_ps_mask(&d1, &r_ali_coh, _CMP_LT_OQ);
                unsigned short mask_ali1 = mask_valid1 & det_ali1;
                if (mask_ali1) {
                    ali_x = _mm512_mask_add_ps(ali_x, mask_ali1, ali_x, vx_j1);
                    ali_y = _mm512_mask_add_ps(ali_y, mask_ali1, ali_y, vy_j1);
                    count_a = _mm512_mask_add_epi32(count_a, mask_ali1, count_a, one_i);
                    coh_x = _mm512_mask_add_ps(coh_x, mask_ali1, coh_x, px_j1);
                    coh_y = _mm512_mask_add_ps(coh_y, mask_ali1, coh_y, py_j1);
                    count_c = _mm512_mask_add_epi32(count_c, mask_ali1, count_c, one_i);
                }
            }
            
            // Block 2
            v16f px_j2 = _mm512_load_ps(&boids.px[j+16]);
            v16f py_j2 = _mm512_load_ps(&boids.py[j+16]);
            v16f vx_j2 = _mm512_load_ps(&boids.vx[j+16]);
            v16f vy_j2 = _mm512_load_ps(&boids.vy[j+16]);
            
            v16f dx2 = _mm512_sub_ps(px_i, px_j2);
            v16f dy2 = _mm512_sub_ps(py_i, py_j2);
            v16f d2 = _mm512_sqrt_ps(_mm512_fmadd_ps(dx2, dx2, _mm512_mul_ps(dy2, dy2)));
            
            unsigned short mask_valid2 = _mm512_cmp_ps_mask(&d2, &eps, _CMP_GT_OQ);
            int block_start2 = j + 16;
            if (i >= block_start2 && i < block_start2 + 16) mask_valid2 &= ~(1 << (i - block_start2));
            
            if (mask_valid2) {
                unsigned short det_sep2 = _mm512_cmp_ps_mask(&d2, &r_sep, _CMP_LT_OQ);
                unsigned short mask_sep2 = mask_valid2 & det_sep2;
                if (mask_sep2) {
                    v16f inv_d2 = _mm512_rcp_ps(d2);
                    v16f norm_dx2 = _mm512_mul_ps(dx2, inv_d2);
                    v16f norm_dy2 = _mm512_mul_ps(dy2, inv_d2);
                    v16f weighted_dx2 = _mm512_mul_ps(norm_dx2, inv_d2);
                    v16f weighted_dy2 = _mm512_mul_ps(norm_dy2, inv_d2);
                    sep_x = _mm512_mask_add_ps(sep_x, mask_sep2, sep_x, weighted_dx2);
                    sep_y = _mm512_mask_add_ps(sep_y, mask_sep2, sep_y, weighted_dy2);
                    count_s = _mm512_mask_add_epi32(count_s, mask_sep2, count_s, one_i);
                }
                
                unsigned short det_ali2 = _mm512_cmp_ps_mask(&d2, &r_ali_coh, _CMP_LT_OQ);
                unsigned short mask_ali2 = mask_valid2 & det_ali2;
                if (mask_ali2) {
                    ali_x = _mm512_mask_add_ps(ali_x, mask_ali2, ali_x, vx_j2);
                    ali_y = _mm512_mask_add_ps(ali_y, mask_ali2, ali_y, vy_j2);
                    count_a = _mm512_mask_add_epi32(count_a, mask_ali2, count_a, one_i);
                    coh_x = _mm512_mask_add_ps(coh_x, mask_ali2, coh_x, px_j2);
                    coh_y = _mm512_mask_add_ps(coh_y, mask_ali2, coh_y, py_j2);
                    count_c = _mm512_mask_add_epi32(count_c, mask_ali2, count_c, one_i);
                }
            }
        }
        
        float sx = _mm512_reduce_add_ps(sep_x);
        float sy = _mm512_reduce_add_ps(sep_y);
        int cs = _mm512_reduce_add_epi32(count_s);
        
        float ax = _mm512_reduce_add_ps(ali_x);
        float ay = _mm512_reduce_add_ps(ali_y);
        int ca = _mm512_reduce_add_epi32(count_a);
        
        float cx = _mm512_reduce_add_ps(coh_x);
        float cy = _mm512_reduce_add_ps(coh_y);
        int cc = _mm512_reduce_add_epi32(count_c);
        
        float acc_x = 0, acc_y = 0;
        
        if (cs > 0) {
            sx /= cs; sy /= cs;
            float l = sqrtf(sx*sx + sy*sy);
            if (l > 0) { sx = (sx/l)*4.0f; sy = (sy/l)*4.0f; }
            sx -= boids.vx[i]; sy -= boids.vy[i];
            float l2 = sqrtf(sx*sx + sy*sy);
            if (l2 > 0.1f) { sx = (sx/l2)*0.1f; sy = (sy/l2)*0.1f; }
            acc_x += sx * 1.5f; acc_y += sy * 1.5f;
        }
        
        if (ca > 0) {
            ax /= ca; ay /= ca;
            float l = sqrtf(ax*ax + ay*ay);
            if (l > 0) { ax = (ax/l)*4.0f; ay = (ay/l)*4.0f; }
            ax -= boids.vx[i]; ay -= boids.vy[i];
            float l2 = sqrtf(ax*ax + ay*ay);
            if (l2 > 0.1f) { ax = (ax/l2)*0.1f; ay = (ay/l2)*0.1f; }
            acc_x += ax * 1.0f; acc_y += ay * 1.0f;
        }
        
        if (cc > 0) {
            cx /= cc; cy /= cc;
            float desired_x = cx - boids.px[i];
            float desired_y = cy - boids.py[i];
            float l = sqrtf(desired_x*desired_x + desired_y*desired_y);
            if (l > 0) { desired_x = (desired_x/l)*4.0f; desired_y = (desired_y/l)*4.0f; }
            float dx2 = desired_x - boids.vx[i];
            float dy2 = desired_y - boids.vy[i];
            float l2 = sqrtf(dx2*dx2 + dy2*dy2);
            if (l2 > 0.1f) { dx2 = (dx2/l2)*0.1f; dy2 = (dy2/l2)*0.1f; }
            acc_x += dx2 * 1.0f; acc_y += dy2 * 1.0f;
        }
        
        boids.ax[i] = acc_x;
        boids.ay[i] = acc_y;
    }
}

void update_avx512() {
    v16f limit = _mm512_set1_ps(4.0f);
    v16f zero = _mm512_setzero_ps();
    v16f w = _mm512_set1_ps(WIDTH);
    v16f h = _mm512_set1_ps(HEIGHT);
    v16f lim_val = _mm512_set1_ps(16.0f); // limit^2 = 16.0
    
    for (int i = 0; i < NUM_BOIDS; i += 16) {
        v16f vx = _mm512_load_ps(&boids.vx[i]);
        v16f vy = _mm512_load_ps(&boids.vy[i]);
        v16f ax = _mm512_load_ps(&boids.ax[i]);
        v16f ay = _mm512_load_ps(&boids.ay[i]);
        v16f px = _mm512_load_ps(&boids.px[i]);
        v16f py = _mm512_load_ps(&boids.py[i]);
        
        vx = _mm512_add_ps(vx, ax);
        vy = _mm512_add_ps(vy, ay);
        
        v16f vlen_sq = _mm512_fmadd_ps(vx, vx, _mm512_mul_ps(vy, vy));
        unsigned short mask_lim = _mm512_cmp_ps_mask(&vlen_sq, &lim_val, _CMP_GT_OQ);
        if (mask_lim) {
            v16f inv_l = _mm512_rsqrt_ps(vlen_sq);
            v16f scalar = _mm512_mul_ps(limit, inv_l);
            vx = _mm512_mask_mul_ps(vx, mask_lim, vx, scalar);
            vy = _mm512_mask_mul_ps(vy, mask_lim, vy, scalar);
        }
        
        px = _mm512_add_ps(px, vx);
        py = _mm512_add_ps(py, vy);
        
        // Wrap bounds
        unsigned short m_xl = _mm512_cmp_ps_mask(&px, &zero, _CMP_LT_OQ);
        px = _mm512_mask_add_ps(px, m_xl, px, w);
        unsigned short m_xr = _mm512_cmp_ps_mask(&px, &w, _CMP_GE_OQ);
        px = _mm512_mask_sub_ps(px, m_xr, px, w);
        
        unsigned short m_yl = _mm512_cmp_ps_mask(&py, &zero, _CMP_LT_OQ);
        py = _mm512_mask_add_ps(py, m_yl, py, h);
        unsigned short m_yr = _mm512_cmp_ps_mask(&py, &h, _CMP_GE_OQ);
        py = _mm512_mask_sub_ps(py, m_yr, py, h);
        
        _mm512_stream_ps(&boids.vx[i], vx);
        _mm512_stream_ps(&boids.vy[i], vy);
        _mm512_stream_ps(&boids.px[i], px);
        _mm512_stream_ps(&boids.py[i], py);
        _mm512_stream_ps(&boids.ax[i], zero);
        _mm512_stream_ps(&boids.ay[i], zero);
    }
}

int main(void) {
    unsigned char (*fb)[WIDTH][3] = malloc(HEIGHT * WIDTH * 3);
    memset(fb, 10, HEIGHT * WIDTH * 3);
    
    init_boids();
    
    fprintf(stderr, "Simulating Boids Exp12 Sovereign...\n");
    for(int f=0; f<200; f++) {
        flock_avx512();
        update_avx512();
    }
    
    for(int i=0; i<NUM_BOIDS; i++) {
        int x = (int)boids.px[i];
        int y = (int)boids.py[i];
        if(x>=0 && x<WIDTH && y>=0 && y<HEIGHT) {
            fb[y][x][0] = 255; fb[y][x][1] = 200; fb[y][x][2] = 50;
            if(x+1 < WIDTH) fb[y][x+1][0]=255;
            if(y+1 < HEIGHT) fb[y+1][x][0]=255;
            if(x-1 >=0) fb[y][x-1][0]=255;
            if(y-1 >=0) fb[y-1][x][0]=255;
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
    fprintf(stderr, "Exp12 Boids Verification Hash: 0x%08X\n", hash);

    printf("P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int y = 0; y < HEIGHT; y++) fwrite(fb[y], 1, WIDTH * 3, stdout);
    
    free(fb);
    fprintf(stderr, "Exp12 Sovereign done!\n");
    return 0;
}
