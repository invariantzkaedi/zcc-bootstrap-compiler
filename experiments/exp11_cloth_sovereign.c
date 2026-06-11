/* EXPERIMENT 11-SOVEREIGN: Real-time Cloth Simulation
 * 
 * 777JACKPOT777 LUCKY UPGRADE
 * - Parallel execution of 16 independent cloth simulations using Sovereign Vector Engine.
 * - Structure of Arrays (SoA) across 16 parallel simulation universes.
 * - Verlet numerical integration and Constraint satisfaction vectorized across 16 instances.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zcaedi_avx512_math.h"

extern float sqrtf(float);

#define WIDTH 512
#define HEIGHT 512
#define GRID_SIZE 30
#define NUM_PARTICLES (GRID_SIZE * GRID_SIZE)
#define ITERATIONS 5
#define NUM_CLOTHS 16

// 16 instances per particle
typedef struct {
    v16f x, y;
    v16f oldx, oldy;
    int pinned;
} Particle16;

typedef struct {
    int p1, p2;
    float rest_len;
} Constraint;

Particle16 particles[NUM_PARTICLES];
Constraint *constraints = NULL;
int num_constraints = 0;

static inline v16f v16_len(v16f x, v16f y) {
    return _mm512_sqrt_ps(_mm512_fmadd_ps(x, x, _mm512_mul_ps(y, y)));
}

void init_cloth() {
    float start_x_base = WIDTH / 2.0f - (GRID_SIZE * 10) / 2.0f;
    float start_y_base = 50.0f;
    
    constraints = malloc(sizeof(Constraint) * NUM_PARTICLES * 4);
    
    float x_offsets[16];
    float y_offsets[16];
    for(int k=0; k<16; k++) {
        x_offsets[k] = (k%4 - 1.5f) * 20.0f; // spread 16 cloths around
        y_offsets[k] = (k/4 - 1.5f) * 20.0f;
    }
    v16f v_x_off = _mm512_loadu_ps(x_offsets);
    v16f v_y_off = _mm512_loadu_ps(y_offsets);
    
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int idx = y * GRID_SIZE + x;
            
            v16f px = _mm512_add_ps(_mm512_set1_ps(start_x_base + x * 10.0f), v_x_off);
            v16f py = _mm512_add_ps(_mm512_set1_ps(start_y_base + y * 10.0f), v_y_off);
            
            particles[idx].x = px;
            particles[idx].y = py;
            particles[idx].oldx = px;
            particles[idx].oldy = py;
            particles[idx].pinned = (y == 0 && (x == 0 || x == GRID_SIZE - 1 || x == GRID_SIZE / 2));
            
            if (x < GRID_SIZE - 1) {
                constraints[num_constraints].p1 = idx;
                constraints[num_constraints].p2 = idx + 1;
                constraints[num_constraints].rest_len = 10.0f;
                num_constraints++;
            }
            if (y < GRID_SIZE - 1) {
                constraints[num_constraints].p1 = idx;
                constraints[num_constraints].p2 = idx + GRID_SIZE;
                constraints[num_constraints].rest_len = 10.0f;
                num_constraints++;
            }
            if (x < GRID_SIZE - 1 && y < GRID_SIZE - 1) {
                constraints[num_constraints].p1 = idx;
                constraints[num_constraints].p2 = idx + GRID_SIZE + 1;
                constraints[num_constraints].rest_len = 14.14f;
                num_constraints++;
                
                constraints[num_constraints].p1 = idx + 1;
                constraints[num_constraints].p2 = idx + GRID_SIZE;
                constraints[num_constraints].rest_len = 14.14f;
                num_constraints++;
            }
        }
    }
}

void simulate_avx512(float dt) {
    v16f v_dt2_wind = _mm512_set1_ps(0.5f * dt * dt);
    v16f v_dt2_grav = _mm512_set1_ps(9.8f * dt * dt);
    v16f v_damp = _mm512_set1_ps(0.99f);
    
    // Simulate some random wind variations across the 16 cloths
    float wind_a[16];
    for(int k=0; k<16; k++) wind_a[k] = (k%3)*0.5f * dt * dt;
    v16f v_wind = _mm512_loadu_ps(wind_a);
    v_dt2_wind = _mm512_add_ps(v_dt2_wind, v_wind);
    
    // Integrate
    for (int i = 0; i < NUM_PARTICLES; i++) {
        if (!particles[i].pinned) {
            v16f vx = _mm512_sub_ps(particles[i].x, particles[i].oldx);
            v16f vy = _mm512_sub_ps(particles[i].y, particles[i].oldy);
            
            particles[i].oldx = particles[i].x;
            particles[i].oldy = particles[i].y;
            
            particles[i].x = _mm512_fmadd_ps(vx, v_damp, _mm512_add_ps(particles[i].x, v_dt2_wind));
            particles[i].y = _mm512_fmadd_ps(vy, v_damp, _mm512_add_ps(particles[i].y, v_dt2_grav));
        }
    }
    
    v16f v_half = _mm512_set1_ps(0.5f);
    
    // Satisfy Constraints
    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (int i = 0; i < num_constraints; i++) {
            Particle16 *p1 = &particles[constraints[i].p1];
            Particle16 *p2 = &particles[constraints[i].p2];
            
            v16f dx = _mm512_sub_ps(p2->x, p1->x);
            v16f dy = _mm512_sub_ps(p2->y, p1->y);
            v16f current_dist = v16_len(dx, dy);
            
            unsigned short valid = _mm512_cmp_ps_mask(current_dist, _mm512_setzero_ps(), _CMP_GT_OQ);
            
            if (valid) {
                v16f v_rest = _mm512_set1_ps(constraints[i].rest_len);
                v16f inv_dist = _mm512_rcp_ps(current_dist);
                v16f diff = _mm512_mask_mul_ps(_mm512_setzero_ps(), valid, _mm512_sub_ps(current_dist, v_rest), inv_dist);
                
                v16f offset_x = _mm512_mul_ps(_mm512_mul_ps(dx, v_half), diff);
                v16f offset_y = _mm512_mul_ps(_mm512_mul_ps(dy, v_half), diff);
                
                if (!p1->pinned) {
                    p1->x = _mm512_mask_add_ps(p1->x, valid, p1->x, offset_x);
                    p1->y = _mm512_mask_add_ps(p1->y, valid, p1->y, offset_y);
                }
                if (!p2->pinned) {
                    p2->x = _mm512_mask_sub_ps(p2->x, valid, p2->x, offset_x);
                    p2->y = _mm512_mask_sub_ps(p2->y, valid, p2->y, offset_y);
                }
            }
        }
    }
}

void draw_line(unsigned char (*fb)[WIDTH][3], int x0, int y0, int x1, int y1, unsigned char r, unsigned char g, unsigned char b) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    
    while (1) {
        if (x0 >= 0 && x0 < WIDTH && y0 >= 0 && y0 < HEIGHT) {
            fb[y0][x0][0] = r; fb[y0][x0][1] = g; fb[y0][x0][2] = b;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

int main(void) {
    unsigned char (*fb)[WIDTH][3] = malloc(HEIGHT * WIDTH * 3);
    memset(fb, 30, HEIGHT * WIDTH * 3);
    
    init_cloth();
    
    fprintf(stderr, "Simulating 16 Cloths Exp11 Sovereign...\n");
    for(int frame = 0; frame < 300; frame++) {
        simulate_avx512(0.2f);
    }
    
    for (int i = 0; i < num_constraints; i++) {
        float p1x[16], p1y[16], p2x[16], p2y[16];
        _mm512_storeu_ps(p1x, particles[constraints[i].p1].x);
        _mm512_storeu_ps(p1y, particles[constraints[i].p1].y);
        _mm512_storeu_ps(p2x, particles[constraints[i].p2].x);
        _mm512_storeu_ps(p2y, particles[constraints[i].p2].y);
        
        for(int k=0; k<16; k++) {
            unsigned char r = (k*30)%255;
            unsigned char b = 255 - r;
            draw_line(fb, (int)p1x[k], (int)p1y[k], (int)p2x[k], (int)p2y[k], r, 200, b);
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
    fprintf(stderr, "Exp11 Cloth Verification Hash: 0x%08X\n", hash);

    printf("P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int y = 0; y < HEIGHT; y++) fwrite(fb[y], 1, WIDTH * 3, stdout);
    
    free(fb);
    free(constraints);
    fprintf(stderr, "Exp11 Sovereign done!\n");
    return 0;
}
