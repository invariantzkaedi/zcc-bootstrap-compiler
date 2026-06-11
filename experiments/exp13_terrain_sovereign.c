/* EXPERIMENT 13-SOVEREIGN: Procedural Terrain Generator
 * 
 * 777JACKPOT777 LUCKY UPGRADE
 * - Vectorized Fractional Brownian Motion (fBm) 16 pixels at once.
 * - Masked color banding based on terrain heights.
 * - Accelerated slope / directional shadowing.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zcaedi_avx512_math.h"

extern float floorf(float);

#define WIDTH 512
#define HEIGHT 512

static inline v16f _mm512_floor_ps(v16f x) {
    v16f res;
    for(int i = 0; i < 16; i++) {
        res.v[i] = floorf(x.v[i]);
    }
    return res;
}

static inline v16f v16_fract(v16f x) {
    return _mm512_sub_ps(x, _mm512_floor_ps(x));
}

// Pseudo-random hash
static inline v16f hash_avx512(v16f x, v16f y) {
    v16f dot = _mm512_fmadd_ps(x, _mm512_set1_ps(12.9898f), _mm512_mul_ps(y, _mm512_set1_ps(78.233f)));
    v16f s = _mm512_mul_ps(_mm512_sin_ps(dot), _mm512_set1_ps(43758.5453123f));
    return v16_fract(s);
}

// 2D Value Noise AVX-512
static v16f noise_avx512(v16f x, v16f y) {
    v16f ix = _mm512_floor_ps(x);
    v16f iy = _mm512_floor_ps(y);
    v16f fx = v16_fract(x);
    v16f fy = v16_fract(y);
    
    // Smoothstep
    v16f three = _mm512_set1_ps(3.0f);
    v16f two = _mm512_set1_ps(2.0f);
    v16f one = _mm512_set1_ps(1.0f);
    
    v16f ux = _mm512_mul_ps(_mm512_mul_ps(fx, fx), _mm512_sub_ps(three, _mm512_mul_ps(two, fx)));
    v16f uy = _mm512_mul_ps(_mm512_mul_ps(fy, fy), _mm512_sub_ps(three, _mm512_mul_ps(two, fy)));
    
    v16f a = hash_avx512(ix, iy);
    v16f b = hash_avx512(_mm512_add_ps(ix, one), iy);
    v16f c = hash_avx512(ix, _mm512_add_ps(iy, one));
    v16f d = hash_avx512(_mm512_add_ps(ix, one), _mm512_add_ps(iy, one));
    
    // res = a + (b - a)*ux + (c - a)*uy + (a - b - c + d)*ux*uy;
    v16f res = _mm512_fmadd_ps(_mm512_sub_ps(b, a), ux, a);
    v16f uy_term1 = _mm512_mul_ps(_mm512_sub_ps(c, a), uy);
    v16f uy_term2 = _mm512_mul_ps(_mm512_add_ps(_mm512_sub_ps(a, b), _mm512_sub_ps(d, c)), _mm512_mul_ps(ux, uy));
    
    return _mm512_add_ps(res, _mm512_add_ps(uy_term1, uy_term2));
}

// Fractional Brownian Motion AVX-512
static v16f fbm_avx512(v16f x, v16f y) {
    v16f v = _mm512_setzero_ps();
    v16f a = _mm512_set1_ps(0.5f);
    v16f shift_x = _mm512_set1_ps(100.0f);
    v16f shift_y = _mm512_set1_ps(100.0f);
    
    v16f cos2 = _mm512_set1_ps(0.87758256f); // cos(0.5f)
    v16f sin2 = _mm512_set1_ps(0.47942553f); // sin(0.5f)
    v16f two = _mm512_set1_ps(2.0f);
    v16f half = _mm512_set1_ps(0.5f);
    
    for (int i = 0; i < 5; ++i) {
        v = _mm512_fmadd_ps(a, noise_avx512(x, y), v);
        
        v16f nx = _mm512_add_ps(_mm512_mul_ps(_mm512_sub_ps(_mm512_mul_ps(x, cos2), _mm512_mul_ps(y, sin2)), two), shift_x);
        v16f ny = _mm512_add_ps(_mm512_mul_ps(_mm512_add_ps(_mm512_mul_ps(x, sin2), _mm512_mul_ps(y, cos2)), two), shift_y);
        
        x = nx; y = ny;
        a = _mm512_mul_ps(a, half);
    }
    return v;
}

int main(void) {
    unsigned char (*fb)[WIDTH][3] = malloc(HEIGHT * WIDTH * 3);
    
    fprintf(stderr, "Generating Terrain fBm Exp13 Sovereign...\n");
    
    v16f x_offsets = _mm512_set_ps(15.f, 14.f, 13.f, 12.f, 11.f, 10.f, 9.f, 8.f, 7.f, 6.f, 5.f, 4.f, 3.f, 2.f, 1.f, 0.f);
    v16f inv_100 = _mm512_set1_ps(1.0f / 100.0f);
    
    for (int y = 0; y < HEIGHT; y++) {
        v16f uvy = _mm512_mul_ps(_mm512_set1_ps((float)y), inv_100);
        
        for (int x = 0; x < WIDTH; x += 16) {
            v16f xv = _mm512_add_ps(_mm512_set1_ps((float)x), x_offsets);
            v16f uvx = _mm512_mul_ps(xv, inv_100);
            
            v16f h = fbm_avx512(uvx, uvy);
            v16f h2 = fbm_avx512(_mm512_add_ps(uvx, _mm512_set1_ps(0.01f)), uvy);
            v16f slope = _mm512_sub_ps(h2, h);
            
            float h_a[16], slope_a[16];
            _mm512_storeu_ps(h_a, h);
            _mm512_storeu_ps(slope_a, slope);
            
            for(int i=0; i<16; i++) {
                if (x+i < WIDTH) {
                    float val = h_a[i];
                    float sl = slope_a[i];
                    unsigned char r=0, g=0, b=0;
                    
                    if (val < 0.35f) { // Water
                        r = 20; g = 50; b = 150 + (int)((val/0.35f)*50);
                    } else if (val < 0.4f) { // Sand
                        r = 200; g = 180; b = 120;
                    } else if (val < 0.7f) { // Grass
                        r = 30 + (int)((val-0.4f)*100); g = 120 + (int)((0.7f-val)*100); b = 30;
                    } else { // Snow/Rock
                        r = 200 + (int)((val-0.7f)*50); g = r; b = r;
                    }
                    
                    if (sl > 0.0f && val > 0.35f) {
                        r = r > 20 ? r - 20 : 0;
                        g = g > 20 ? g - 20 : 0;
                        b = b > 20 ? b - 20 : 0;
                    }
                    
                    fb[y][x+i][0] = r; fb[y][x+i][1] = g; fb[y][x+i][2] = b;
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
    fprintf(stderr, "Exp13 Terrain Verification Hash: 0x%08X\n", hash);

    printf("P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int y = 0; y < HEIGHT; y++) fwrite(fb[y], 1, WIDTH * 3, stdout);
    
    free(fb);
    fprintf(stderr, "Exp13 Sovereign done!\n");
    return 0;
}
