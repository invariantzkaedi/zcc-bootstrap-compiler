/* EXPERIMENT 20: Vectorized SPH Fluid Dynamics (Sovereign)
 * 
 * - Structure-of-Arrays (SoA) for perfect alignment
 * - Sovereign zero-dependency AVX-512 struct matrix math
 * - Vectorized distance checks and pressure gradients
 */

#include "zcaedi_avx512_math.h"

#define WIDTH 640
#define HEIGHT 480
#define NUM_PARTICLES 1008
#define SIMD_WIDTH 16

// SPH Constants
#define H 16.0f
#define H2 256.0f
#define MASS 65.0f
#define REST_DENS 1000.0f
#define GAS_CONST 2000.0f
#define VISC 250.0f
#define DT 0.0008f
#define G 12000.0f
#define BOUND_DAMPING -0.5f

// Kernel constants
#define POLY6 (315.0f / (64.0f * 3.141592f * 16777216.0f))
#define SPIKY_GRAD (-45.0f / (3.141592f * 1048576.0f))
#define VISC_LAP (45.0f / (3.141592f * 1048576.0f))

typedef struct {
    float x[NUM_PARTICLES];
    float y[NUM_PARTICLES];
    float vx[NUM_PARTICLES];
    float vy[NUM_PARTICLES];
    float fx[NUM_PARTICLES];
    float fy[NUM_PARTICLES];
    float rho[NUM_PARTICLES];
    float p[NUM_PARTICLES];
} ParticlesSoA;

ParticlesSoA fluid;
unsigned char framebuffer[HEIGHT][WIDTH][3];

void init_particles() {
    int i = 0;
    for (int y = 0; y < 42; y++) {
        for (int x = 0; x < 24; x++) {
            if (i >= NUM_PARTICLES) return;
            fluid.x[i] = 100.0f + (float)x * 8.5f + (float)(y % 3) * 3.5f;
            fluid.y[i] = 100.0f + (float)y * 8.2f;
            fluid.vx[i] = (float)(i % 7 - 3) * 15.0f; 
            fluid.vy[i] = (float)(i % 5 - 2) * 10.0f;
            i++;
        }
    }
}

static inline v16i cmp_lt_mask(v16f a, v16f b) {
    v16i m;
    for(int k=0; k<16; k++) m.v[k] = (a.v[k] < b.v[k]) ? 1 : 0;
    return m;
}

static inline v16i cmp_gt_mask(v16f a, v16f b) {
    v16i m;
    for(int k=0; k<16; k++) m.v[k] = (a.v[k] > b.v[k]) ? 1 : 0;
    return m;
}

void compute_density_pressure_avx512() {
    v16f v_h2 = _mm512_set1_ps(H2);
    v16f v_poly6 = _mm512_set1_ps(MASS * POLY6);
    
    for (int i = 0; i < NUM_PARTICLES; i++) {
        v16f v_xi = _mm512_set1_ps(fluid.x[i]);
        v16f v_yi = _mm512_set1_ps(fluid.y[i]);
        v16f v_rho = _mm512_setzero_ps();
        
        if (i == 0) {
            printf("i=0 INIT v_rho.v[0]=%f\n", v_rho.v[0]);
        }
        
        for (int j = 0; j < NUM_PARTICLES; j += 32) {
            // Block 1
            v16f v_xj1 = _mm512_loadu_ps(&fluid.x[j]);
            v16f v_yj1 = _mm512_loadu_ps(&fluid.y[j]);
            v16f dx1 = _mm512_sub_ps(v_xi, v_xj1);
            v16f dy1 = _mm512_sub_ps(v_yi, v_yj1);
            v16f r2_1 = _mm512_fmadd_ps(dx1, dx1, _mm512_mul_ps(dy1, dy1));
            
            v16i mask1 = cmp_lt_mask(r2_1, v_h2);
            int m1_any = 0;
            for(int k=0; k<16; k++) if(mask1.v[k]) m1_any = 1;
            
            if (m1_any) {
                v16f term1 = _mm512_sub_ps(v_h2, r2_1);
                v16f term3_1 = _mm512_mul_ps(term1, _mm512_mul_ps(term1, term1));
                v16f rho_contrib1 = _mm512_mul_ps(v_poly6, term3_1);
                v_rho = _mm512_mask_add_ps_v16i(v_rho, mask1, v_rho, rho_contrib1);
                
                if (i == 0 && j == 0) {
                    printf("i=0 j=0 term1=%f term3=%f poly6=%f rho_contrib=%f\n", 
                           term1.v[0], term3_1.v[0], v_poly6.v[0], rho_contrib1.v[0]);
                }
                for (int k = 0; k < 16; k++) {
                    if (mask1.v[k] && (rho_contrib1.v[k] > 10000.0f || rho_contrib1.v[k] < -10000.0f)) {
                        printf("HUGE RHO: i=%d j=%d k=%d rho_contrib=%f r2=%f\n", i, j, k, rho_contrib1.v[k], r2_1.v[k]);
                    }
                }
            }
            
            // Block 2
            if (j + 16 < NUM_PARTICLES) {
                v16f v_xj2 = _mm512_loadu_ps(&fluid.x[j+16]);
                v16f v_yj2 = _mm512_loadu_ps(&fluid.y[j+16]);
                v16f dx2 = _mm512_sub_ps(v_xi, v_xj2);
                v16f dy2 = _mm512_sub_ps(v_yi, v_yj2);
                v16f r2_2 = _mm512_fmadd_ps(dx2, dx2, _mm512_mul_ps(dy2, dy2));
                
                v16i mask2 = cmp_lt_mask(r2_2, v_h2);
                int m2_any = 0;
                for(int k=0; k<16; k++) if(mask2.v[k]) m2_any = 1;
                
                if (m2_any) {
                    v16f term2 = _mm512_sub_ps(v_h2, r2_2);
                    v16f term3_2 = _mm512_mul_ps(term2, _mm512_mul_ps(term2, term2));
                    v16f rho_contrib2 = _mm512_mul_ps(v_poly6, term3_2);
                    v_rho = _mm512_mask_add_ps_v16i(v_rho, mask2, v_rho, rho_contrib2);
                }
            }
        }
        
        float total_rho = _mm512_reduce_add_ps(v_rho);
        if (i == 0) {
            printf("i=0 v_rho.v[0]=%f v[1]=%f total_rho=%f\n", v_rho.v[0], v_rho.v[1], total_rho);
        }
        fluid.rho[i] = total_rho;
        fluid.p[i] = GAS_CONST * (total_rho - REST_DENS);
    }
}

void integrate_avx512() {
    v16f v_dt = _mm512_set1_ps(DT);
    v16f v_bound_damp = _mm512_set1_ps(BOUND_DAMPING);
    v16f v_zero = _mm512_setzero_ps();
    v16f v_w = _mm512_set1_ps((float)WIDTH);
    v16f v_h = _mm512_set1_ps((float)HEIGHT);
    
    for (int i = 0; i < NUM_PARTICLES; i += SIMD_WIDTH) {
        v16f v_fx = _mm512_loadu_ps(&fluid.fx[i]);
        v16f v_fy = _mm512_loadu_ps(&fluid.fy[i]);
        v16f v_rho = _mm512_loadu_ps(&fluid.rho[i]);
        v16f v_vx = _mm512_loadu_ps(&fluid.vx[i]);
        v16f v_vy = _mm512_loadu_ps(&fluid.vy[i]);
        v16f v_x = _mm512_loadu_ps(&fluid.x[i]);
        v16f v_y = _mm512_loadu_ps(&fluid.y[i]);

        v_fy = _mm512_fmadd_ps(_mm512_set1_ps(G), v_rho, v_fy);

        v16f ax = _mm512_div_ps(v_fx, v_rho);
        v16f ay = _mm512_div_ps(v_fy, v_rho);
        
        v_vx = _mm512_fmadd_ps(ax, v_dt, v_vx);
        v_vy = _mm512_fmadd_ps(ay, v_dt, v_vy);
        
        v_x = _mm512_fmadd_ps(v_vx, v_dt, v_x);
        v_y = _mm512_fmadd_ps(v_vy, v_dt, v_y);

        v16i mask_xl = cmp_lt_mask(v_x, v_zero);
        v_vx = _mm512_mask_mul_ps_v16i(v_vx, mask_xl, v_vx, v_bound_damp);
        v_x = _mm512_mask_mov_ps(v_x, mask_xl, v_zero);

        v16i mask_xr = cmp_gt_mask(v_x, v_w);
        v_vx = _mm512_mask_mul_ps_v16i(v_vx, mask_xr, v_vx, v_bound_damp);
        v_x = _mm512_mask_mov_ps(v_x, mask_xr, v_w);

        v16i mask_yl = cmp_lt_mask(v_y, v_zero);
        v_vy = _mm512_mask_mul_ps_v16i(v_vy, mask_yl, v_vy, v_bound_damp);
        v_y = _mm512_mask_mov_ps(v_y, mask_yl, v_zero);

        v16i mask_yr = cmp_gt_mask(v_y, v_h);
        v_vy = _mm512_mask_mul_ps_v16i(v_vy, mask_yr, v_vy, v_bound_damp);
        v_y = _mm512_mask_mov_ps(v_y, mask_yr, v_h);

        _mm512_storeu_ps(&fluid.x[i], v_x);
        _mm512_storeu_ps(&fluid.y[i], v_y);
        _mm512_storeu_ps(&fluid.vx[i], v_vx);
        _mm512_storeu_ps(&fluid.vy[i], v_vy);
    }
}

void compute_forces_avx512() {
    v16f v_h2 = _mm512_set1_ps(H2);
    v16f v_h = _mm512_set1_ps(H);
    v16f v_mass = _mm512_set1_ps(MASS);
    v16f v_spiky_grad = _mm512_set1_ps(SPIKY_GRAD);
    v16f v_visc_lap = _mm512_set1_ps(VISC_LAP);
    v16f v_visc = _mm512_set1_ps(VISC);
    
    for (int i = 0; i < NUM_PARTICLES; i++) {
        v16f v_fpx = _mm512_setzero_ps();
        v16f v_fpy = _mm512_setzero_ps();
        v16f v_fvx = _mm512_setzero_ps();
        v16f v_fvy = _mm512_setzero_ps();
        
        v16f v_xi = _mm512_set1_ps(fluid.x[i]);
        v16f v_yi = _mm512_set1_ps(fluid.y[i]);
        v16f v_vxi = _mm512_set1_ps(fluid.vx[i]);
        v16f v_vyi = _mm512_set1_ps(fluid.vy[i]);
        v16f v_pi = _mm512_set1_ps(fluid.p[i]);
        
        for (int j = 0; j < NUM_PARTICLES; j += 32) {
            // Block 1
            v16f v_xj1 = _mm512_loadu_ps(&fluid.x[j]);
            v16f v_yj1 = _mm512_loadu_ps(&fluid.y[j]);
            v16f dx1 = _mm512_sub_ps(v_xj1, v_xi);
            v16f dy1 = _mm512_sub_ps(v_yj1, v_yi);
            v16f r2_1 = _mm512_fmadd_ps(dx1, dx1, _mm512_mul_ps(dy1, dy1));
            
            v16i mask1 = cmp_lt_mask(r2_1, v_h2);
            int block_start1 = j;
            if (i >= block_start1 && i < block_start1 + 16) mask1.v[i - block_start1] = 0;
            
            int m1_any = 0;
            for(int k=0; k<16; k++) if(mask1.v[k]) m1_any = 1;

            if (m1_any) {
                v16f safe_r2_1 = _mm512_max_ps(r2_1, _mm512_set1_ps(0.0001f));
                v16f r1 = _mm512_sqrt_ps(safe_r2_1);
                v16f inv_r1 = _mm512_rsqrt_ps(safe_r2_1);
                
                v16f v_pj1 = _mm512_loadu_ps(&fluid.p[j]);
                v16f v_rhoj1 = _mm512_loadu_ps(&fluid.rho[j]);
                v16f inv_rhoj1 = _mm512_rsqrt_ps(_mm512_mul_ps(v_rhoj1, v_rhoj1));
                
                v16f term1 = _mm512_sub_ps(v_h, r1);
                v16f term2_1 = _mm512_mul_ps(term1, term1);
                
                v16f p_factor1 = _mm512_mul_ps(_mm512_add_ps(v_pi, v_pj1), _mm512_mul_ps(_mm512_set1_ps(0.5f), inv_rhoj1));
                p_factor1 = _mm512_mul_ps(p_factor1, _mm512_mul_ps(v_mass, v_spiky_grad));
                p_factor1 = _mm512_mul_ps(p_factor1, term2_1);
                p_factor1 = _mm512_mul_ps(p_factor1, inv_r1);
                v16f neg_p_factor1 = _mm512_sub_ps(_mm512_setzero_ps(), p_factor1);
                
                v_fpx = sovereign_mask_fmadd_ps(v_fpx, mask1, neg_p_factor1, dx1);
                v_fpy = sovereign_mask_fmadd_ps(v_fpy, mask1, neg_p_factor1, dy1);
                
                v16f v_vxj1 = _mm512_loadu_ps(&fluid.vx[j]);
                v16f v_vyj1 = _mm512_loadu_ps(&fluid.vy[j]);
                
                v16f v_factor1 = _mm512_mul_ps(_mm512_mul_ps(v_visc, v_mass), inv_rhoj1);
                v_factor1 = _mm512_mul_ps(v_factor1, _mm512_mul_ps(v_visc_lap, term1));
                
                v_fvx = sovereign_mask_fmadd_ps(v_fvx, mask1, v_factor1, _mm512_sub_ps(v_vxj1, v_vxi));
                v_fvy = sovereign_mask_fmadd_ps(v_fvy, mask1, v_factor1, _mm512_sub_ps(v_vyj1, v_vyi));
            }
            
            // Block 2
            if (j + 16 < NUM_PARTICLES) {
                v16f v_xj2 = _mm512_loadu_ps(&fluid.x[j+16]);
                v16f v_yj2 = _mm512_loadu_ps(&fluid.y[j+16]);
                v16f dx2 = _mm512_sub_ps(v_xj2, v_xi);
                v16f dy2 = _mm512_sub_ps(v_yj2, v_yi);
                v16f r2_2 = _mm512_fmadd_ps(dx2, dx2, _mm512_mul_ps(dy2, dy2));
                
                v16i mask2 = cmp_lt_mask(r2_2, v_h2);
                int block_start2 = j+16;
                if (i >= block_start2 && i < block_start2 + 16) mask2.v[i - block_start2] = 0;

                int m2_any = 0;
                for(int k=0; k<16; k++) if(mask2.v[k]) m2_any = 1;
                
                if (m2_any) {
                    v16f safe_r2_2 = _mm512_max_ps(r2_2, _mm512_set1_ps(0.0001f));
                    v16f r2_sqrt = _mm512_sqrt_ps(safe_r2_2);
                    v16f inv_r2 = _mm512_rsqrt_ps(safe_r2_2);
                    
                    v16f v_pj2 = _mm512_loadu_ps(&fluid.p[j+16]);
                    v16f v_rhoj2 = _mm512_loadu_ps(&fluid.rho[j+16]);
                    v16f inv_rhoj2 = _mm512_rsqrt_ps(_mm512_mul_ps(v_rhoj2, v_rhoj2));
                    
                    v16f term2 = _mm512_sub_ps(v_h, r2_sqrt);
                    v16f term2_2 = _mm512_mul_ps(term2, term2);
                    
                    v16f p_factor2 = _mm512_mul_ps(_mm512_add_ps(v_pi, v_pj2), _mm512_mul_ps(_mm512_set1_ps(0.5f), inv_rhoj2));
                    p_factor2 = _mm512_mul_ps(p_factor2, _mm512_mul_ps(v_mass, v_spiky_grad));
                    p_factor2 = _mm512_mul_ps(p_factor2, term2_2);
                    p_factor2 = _mm512_mul_ps(p_factor2, inv_r2);
                    v16f neg_p_factor2 = _mm512_sub_ps(_mm512_setzero_ps(), p_factor2);
                    
                    v_fpx = sovereign_mask_fmadd_ps(v_fpx, mask2, neg_p_factor2, dx2);
                    v_fpy = sovereign_mask_fmadd_ps(v_fpy, mask2, neg_p_factor2, dy2);
                    
                    v16f v_vxj2 = _mm512_loadu_ps(&fluid.vx[j+16]);
                    v16f v_vyj2 = _mm512_loadu_ps(&fluid.vy[j+16]);
                    
                    v16f v_factor2 = _mm512_mul_ps(_mm512_mul_ps(v_visc, v_mass), inv_rhoj2);
                    v_factor2 = _mm512_mul_ps(v_factor2, _mm512_mul_ps(v_visc_lap, term2));
                    
                    v_fvx = sovereign_mask_fmadd_ps(v_fvx, mask2, v_factor2, _mm512_sub_ps(v_vxj2, v_vxi));
                    v_fvy = sovereign_mask_fmadd_ps(v_fvy, mask2, v_factor2, _mm512_sub_ps(v_vyj2, v_vyi));
                }
            }
        }
        
        fluid.fx[i] = _mm512_reduce_add_ps(v_fpx) + _mm512_reduce_add_ps(v_fvx);
        fluid.fy[i] = _mm512_reduce_add_ps(v_fpy) + _mm512_reduce_add_ps(v_fvy);
    }
}

int main(void) {
    printf("ZKAEDI PRIME SOVEREIGN SPH (Exp20) initializing...\n");
    printf("Constants: POLY6=%f SPIKY_GRAD=%f VISC_LAP=%f\n", POLY6, SPIKY_GRAD, VISC_LAP);
    init_particles();

    for (int frame = 0; frame < 50; frame++) {
        compute_density_pressure_avx512();
        compute_forces_avx512();
        integrate_avx512();
        if (frame == 0 || frame == 49) {
            printf("Frame %d: p0=(%.2f, %.2f) rho=%.2f p=%.2f\n", frame, fluid.x[0], fluid.y[0], fluid.rho[0], fluid.p[0]);
        }
    }
    
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            framebuffer[y][x][0] = 0;
            framebuffer[y][x][1] = 0;
            framebuffer[y][x][2] = 0;
        }
    }
    for (int i = 0; i < NUM_PARTICLES; i++) {
        int px = (int)fluid.x[i];
        int py = (int)fluid.y[i];
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                if (dx*dx + dy*dy <= 4) {
                    int x = px + dx; int y = py + dy;
                    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
                        framebuffer[y][x][0] = 50; 
                        framebuffer[y][x][1] = 255; 
                        framebuffer[y][x][2] = 200;
                    }
                }
            }
        }
    }

    unsigned int hash = 0;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            unsigned int val = (framebuffer[y][x][0] << 16) | (framebuffer[y][x][1] << 8) | framebuffer[y][x][2];
            hash ^= val;
            hash = (hash << 1) | (hash >> 31);
        }
    }

    printf("Exp20 SPH Verification Hash: 0x%08X\n", hash);
    printf("Sovereign SPH Established. Zero External Dependencies linked.\n");

    return 0;
}
